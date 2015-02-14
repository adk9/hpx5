// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/util/options.c
/// @brief Implements libhpx configuration parsing and handling.
///

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "hpx/hpx.h"
#include "libhpx/action.h"
#include "libhpx/boot.h"
#include "libhpx/config.h"
#include "libhpx/gas.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"

#include "utstring.h"
#include "parser.h"

typedef struct hpx_options_t hpx_options_t;

/// Special case handling for a config file option.
static char *_config_file = NULL;

/// The default configuration.
static const config_t _default_cfg = {
#define LIBHPX_OPT(group, id, init, UNUSED2) .group##id = init,
# include "libhpx/options.def"
#undef LIBHPX_OPT
};

/// Getenv, but with an upper-case version of @p var.
static const char *_getenv_upper(const char * const var) {
  const char *c = NULL;
  const size_t len = strlen(var);
  char *uvar = malloc(len + 1);
  dbg_assert_str(uvar, "Could not malloc %lu bytes during option parsing", len);
  for (int i = 0; i < len; ++i) {
    uvar[i] = toupper(var[i]);
  }
  uvar[len] = '\0';
  c = getenv(uvar);
  free(uvar);
  return c;
}

/// Get a configuration value from an environment variable.
///
/// This function looks up a configuration value for an environment variable @p
/// var. If a value is not found it will look for an uppercase version. If a
/// value was found, we append a command-line-option form of the variable to @p
/// str so that it can be later parsed by our gengetopt infrastructure.
///
/// We need two different versions of the key we're looking for, one with
/// underscores to pass to getopt, and one with hyphens to pass to gengetopt.
///
/// @param[out]     str A dynamic string to store the output into.
/// @param          var The key we are looking for in the environment.
/// @param          opt The key we will use for gengetopt.
static void _from_env(UT_string *str, const char * const var,
                      const char * const arg) {
  const char *c = getenv(var);
  if (!c) {
    c = _getenv_upper(var);
  }
  if (!c) {
    return;
  }
  utstring_printf(str, "--%s=%s ", arg, c);
}

/// Get values from the environment for the options declared in options.def.
///
/// This uses multiple-inclusion of options.def to call _get_env() for all of
/// the options declared there. Each found option will be appended to @p str in
/// a form that can be parsed by the gengetopt infrastructure.
///
/// @param[out]     str This will contain the collected values.
static void _from_env_all(UT_string *str) {
  char *cfgstr, *substr;
#define LIBHPX_OPT(g, id, u3, u4)			\
  cfgstr = malloc(strlen("hpx-"#g"-"#id));		\
  if (strlen(#g)) {					\
    substr = malloc(strlen(#g));			\
    strncpy(substr, #g, strlen(#g)-1);			\
    sprintf(cfgstr, "hpx-%s-"#id, substr);		\
    free(substr);					\
  }							\
  else {						\
    sprintf(cfgstr, "hpx-"#id);				\
  }							\
  _from_env(str, "hpx_"#g#id, cfgstr);			\
  free(cfgstr);
# include "libhpx/options.def"
#undef LIBHPX_OPT
}

/// Accumulate configuration options from the environment.
///
/// This will go through the environment and create a command-line-like string
/// that we can parse using the gengetopt command-line infrastructure.
///
/// @param[out]    opts The option structure we will fill from the environment.
/// @param     progname Required by the gengetopt parser.
static void _process_env(hpx_options_t *opts, const char *progname) {
  UT_string *hpx_opts = NULL;
  utstring_new(hpx_opts);
  _from_env_all(hpx_opts);

  const char *cmdline = utstring_body(hpx_opts);
  if (cmdline) {
    int e = hpx_option_parser_string(cmdline, opts, progname);
    dbg_check(e, "failed to parse environment options: %s.\n", cmdline);
  }
  utstring_free(hpx_opts);
}

/// Accumulate configuration options from the command line.
///
/// This will go through the command line, splitting out options that are
/// prefixed with "--hpx-" and processing them into the hpx_options_t.
///
/// @param[out]    opts The option structure we will fill from the environment.
/// @param         argc The number of arguments to process.
/// @param         argv The arguments to process.
static void _process_cmdline(hpx_options_t *opts, int *argc, char ***argv) {
  // Split the arguments into those that should be parsed as --hpx- options
  // and those that are application level options
  UT_string *hpx_opts = NULL;
  utstring_new(hpx_opts);
  for (int i = 0, n = 0, e = *argc; i < e; ++i) {
    UT_string *tmp = NULL;
    utstring_new(tmp);
    utstring_printf(tmp, "%s ", (*argv)[i]);
    if (utstring_find(tmp, 0, "--hpx-", 6) < 0) {
      (*argv)[n++] = (*argv)[i];
    }
    else {
      utstring_concat(hpx_opts, tmp);
      *argc = *argc - 1;
    }
    utstring_free(tmp);
  }

  const char *cmdline = utstring_body(hpx_opts);
  if (cmdline) {
    int e = hpx_option_parser_string(cmdline, opts, argv[0][0]);
    dbg_check(e, "failed to parse command-line options %s.\n", cmdline);
  }
  utstring_free(hpx_opts);
}

/// Process a bitvector opt.
///
/// @param            n The number of args we saw.
/// @param         args The args from gengetopt.
/// @param          all An arg that we should interpret as meaning "all bits".
///
/// @returns            A bitvector set with the bits specified in @p args.
static uint64_t _merge_bitvector(int n, uint32_t args[n], uint64_t all) {
  uint64_t bits = 0;
  for (int i = 0; i < n; ++i) {
    if (args[i] == all) {
      return LIBHPX_OPT_BITSET_ALL;
    }
    bits |= 1 << args[i];
  }
  return bits;
}

/// Process a vector opt.
///
/// @param            n The number of option args we need to process.
/// @param         args The option args to process.
/// @param         init A default value.
/// @param         term A value that means "no more values".
///
/// @returns            A @p term-terminated vector populated with the @p
///                       args. This vector must be deleted at shutdown.
static int *_merge_vector(int n, int args[n], int init, int term) {
  int *vector = calloc(n + 1, sizeof(int));
  for (int i = 0; i < n; ++i) {
    vector[i] = args[i];
  }
  vector[n] = (n) ? term : init;
  return vector;
}

/// Transform a set of options from gengetopt into a config structure.
///
/// This will only overwrite parts of the config structure for which options
/// were actually given.
///
/// @param          cfg The configuration object we are writing to.
/// @param         opts The gengetopt options we are reading from.
static void _merge_opts(config_t *cfg, const hpx_options_t *opts) {

#define LIBHPX_OPT_FLAG(group, id, UNUSED2)         \
  if (opts->hpx_##group##id##_given) {              \
    cfg->group##id = opts->hpx_##group##id##_flag;  \
  }

#define LIBHPX_OPT_SCALAR(group, id, UNUSED2, UNUSED3)  \
  if (opts->hpx_##group##id##_given) {                  \
    cfg->group##id = opts->hpx_##group##id##_arg;       \
  }

#define LIBHPX_OPT_STRING(group, id, init)                  \
  if (opts->hpx_##group##id##_given) {                      \
    cfg->group##id = strdup(opts->hpx_##group##id##_arg);   \
  }

#define LIBHPX_OPT_BITSET(group, id, init)                              \
  if (opts->hpx_##group##id##_given) {                                  \
    cfg->group##id = _merge_bitvector(opts->hpx_##group##id##_given,    \
                                      (uint32_t*)opts->hpx_##group##id##_arg, \
                                      hpx_##group##id##_arg_all);       \
  }

#define LIBHPX_OPT_INTSET(group, id, init, none, all)               \
  if (opts->hpx_##group##id##_given) {                              \
    cfg->group##id = _merge_vector(opts->hpx_##group##id##_given,   \
                                   opts->hpx_##group##id##_arg,     \
                                   init, none);                     \
  }
# include "libhpx/options.def"
#undef LIBHPX_OPT_INTSET
#undef LIBHPX_OPT_BITSET
#undef LIBHPX_OPT_STRING
#undef LIBHPX_OPT_SCALAR
#undef LIBHPX_OPT_STRING
#undef LIBHPX_OPT_FLAG

  // Special case handling for the config file option, the
  // opts->hpx_configfile_arg is deleted so we have to duplicate it.
  if (opts->hpx_configfile_given) {
    dbg_assert(opts->hpx_configfile_arg);
    if (_config_file) {
      free(_config_file);
    }
    _config_file = strdup(opts->hpx_configfile_arg);
  }
}

/// Print the help associated with the HPX runtime options
void hpx_print_help(void) {
  hpx_option_parser_print_help();
}

#define LIBHPX_OPT_INTSET(group, id, init, none, all)           \
  int config_##group##id##_isset(const config_t *cfg, int element) {    \
    if (!cfg->group##id) {                      \
      return (init == all);                     \
    }                                   \
    for (int i = 0; cfg->group##id[i] != none; ++i) {           \
      if (cfg->group##id[i] == all) {                   \
        return 1;                           \
      }                                 \
      if (cfg->group##id[i] == element) {               \
        return 1;                           \
      }                                 \
    }                                   \
    return 0;                               \
  }
# include "libhpx/options.def"
#undef LIBHPX_OPT_INTSET

/// Parse HPX command-line options to create a new config object.
config_t *config_new(int *argc, char ***argv) {

  // first, initialize to the default configuration
  config_t *cfg = malloc(sizeof(*cfg));
  dbg_assert(cfg);
  *cfg = _default_cfg;

  if (!argc || !argv) {
    log("hpx_init(NULL, NULL) called, using default configuration\n");
    return cfg;
  }

  dbg_assert(*argc > 0 && *argv);

  // The executable is used by the gengetopt parser internally.
  const char *progname = (*argv)[0];

  // Process the environment.
  hpx_options_t opts;
  _process_env(&opts, progname);
  _merge_opts(cfg, &opts);
  hpx_option_parser_free(&opts);

  // Use the command line arguments to override the environment values.
  _process_cmdline(&opts, argc, argv);
  _merge_opts(cfg, &opts);
  hpx_option_parser_free(&opts);


  // the config file takes the highest precedence in determining the
  // runtime parameters
  if (_config_file) {
    struct hpx_option_parser_params *params = hpx_option_parser_params_create();
    params->initialize = 0;
    params->override = 0;
    int e = hpx_option_parser_config_file(_config_file, &opts, params);
    dbg_check(e, "could not parse HPX configuration file: %s\n", _config_file);
    _merge_opts(cfg, &opts);

    free(_config_file);
    free(params);
    hpx_option_parser_free(&opts);
  }

#ifdef __GLIBC__
  optind = 0;
#else
  optind = 1;
#endif
  return cfg;
}

void config_delete(config_t *cfg) {
  if (!cfg) {
    return;
  }

  if (cfg->log_at) {
    free(cfg->log_at);
  }

  if (cfg->dbg_waitat) {
    free(cfg->dbg_waitat);
  }

  free(cfg);
}
