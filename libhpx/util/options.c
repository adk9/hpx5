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

/// The default configuration.
static const hpx_config_t _default_cfg = {
#define LIBHPX_DECL_OPTION(group, type, ctype, id, default) .id = default,
# include "libhpx/options.def"
#undef LIBHPX_DECL_OPTION
};


/// Get a configuration value from an environment variable.
///
/// This function looks up a configuration value for an environment
/// variable @p var (of the form hpx_foo). Since environment variables
/// could be potentially case-sensitive, if it does not find an
/// associated configuration value, it turns @p var into uppercase (of
/// the form (HPX_FOO) and retries. If a value was found eventually,
/// it is appended to a dynamic string @p str (as --hpx-foo=<val>).

static void _get_config_env(UT_string *str, const char *var, const char *optstr) {
  char *c = getenv(var);
  if (!c) {
    int len = strlen(var);
    char *uvar = malloc(sizeof(*uvar)*(len+1));
    uvar[len] = '\0';
    for (int i = 0; i < len; ++i) {
      assert(var[i]);
      uvar[i] = toupper(var[i]);
    }
    c = getenv(uvar);
    free(uvar);
  }

  if (c)
    utstring_printf(str, "--%s=%s ", optstr, c);
}

/// Try to read values of all of the configuration variables that we
/// support from the environment.
int _read_options_from_env(hpx_options_t *env_args, const char *progname) {
  UT_string *str;

  utstring_new(str);

#define _GET_CONFIG_ENV(str,opt) _get_config_env(str, "hpx_" #opt, "hpx-" #opt)
#define LIBHPX_DECL_OPTION(group, type, ctype, id, default) _GET_CONFIG_ENV(str, id);
# include "libhpx/options.def"
#undef LIBHPX_DECL_OPTION
#undef _GET_CONFIG_ENV

  const char *cmdline = utstring_body(str);

  if (cmdline)
    hpx_option_parser_string(cmdline, env_args, progname);

  utstring_free(str);
  return 0;
}


/// Update the configuration structure @p cfg with the option values
/// specified in @p opts.
static void _set_config_options(hpx_config_t *cfg, hpx_options_t *opts) {

#define LIBHPX_DECL_OPTION(group, type, ctype, id, default)   \
  {                                                           \
    if (opts->hpx_##id##_given)                               \
      cfg->id = opts->hpx_##id##_##type;                      \
  }
# include "libhpx/options.def"
#undef LIBHPX_DECL_OPTION
#undef _SET_VAR

  if (opts->hpx_loglevel_given) {
    cfg->loglevel = 0;
    for (int i = 0; i < opts->hpx_loglevel_given; ++i) {
      if (opts->hpx_loglevel_arg[i] == hpx_loglevel_arg_all) {
        cfg->loglevel = -1;
        break;
      }
      cfg->loglevel |= (1 << opts->hpx_loglevel_arg[i]);
    }
  }

  if (opts->hpx_logat_given) {
    if (opts->hpx_logat_arg[0] == HPX_LOCALITY_ALL) {
      cfg->logat = (int*)HPX_LOCALITY_ALL;
    } else {
      cfg->logat = malloc(sizeof(*cfg->logat) * (opts->hpx_logat_given+1));
      cfg->logat[0] = opts->hpx_logat_given;
      for (int i = 0; i < opts->hpx_logat_given; ++i) {
        cfg->logat[i+1] = opts->hpx_logat_arg[i];
      }
    }
  }

  if (opts->hpx_configfile_given) {
    if (cfg->configfile)
      free(cfg->configfile);
    cfg->configfile = strdup(opts->hpx_configfile_arg);
  }
}

/// Print the help associated with the HPX runtime options
void hpx_print_help(void) {
  hpx_option_parser_print_help();
}

/// Parse HPX command-line options
hpx_config_t *parse_options(int *argc, char ***argv) {

  // first, initialize to the default configuration
  hpx_config_t *cfg = malloc(sizeof(*cfg));
  *cfg = _default_cfg;

  char *progname = (argv) ? (*argv)[0] : "";

  // then, read the environment for the specified configuration values
  hpx_options_t opts;
  int e = _read_options_from_env(&opts, progname);
  if (e)
    fprintf(stderr, "failed to read options from the environment variables.\n");

  _set_config_options(cfg, &opts);
  hpx_option_parser_free(&opts);

  // finally, use the CLI-specified options to override the above
  // values
  if (!argc || !argv)
    return cfg;

  UT_string *str;
  UT_string *arg;

  utstring_new(str);
  utstring_new(arg);

  int nargs = *argc;
  for (int i = 1, n = 1; i < *argc; ++i) {
    utstring_printf(arg, "%s ", (*argv)[i]);
    if (utstring_find(arg, 0, "--hpx-", 6) != -1) {
      utstring_concat(str, arg);
      nargs--;
    } else {
      (*argv)[n++] = (*argv)[i];
    }
    utstring_clear(arg);
  }
  *argc = nargs;
  utstring_free(arg);

  const char *cmdline = utstring_body(str);
  e = hpx_option_parser_string(cmdline, &opts, progname);
  if (e)
    fprintf(stderr, "failed to parse options specified on the command-line.\n");

  _set_config_options(cfg, &opts);
  utstring_free(str);
  hpx_option_parser_free(&opts);

  // the config file takes the highest precedence in determining the
  // runtime parameters
  if (cfg->configfile) {
    struct hpx_option_parser_params *params = hpx_option_parser_params_create();
    params->initialize = 0;
    params->override = 0;
    int e = hpx_option_parser_config_file(cfg->configfile, &opts, params);
    if (!e)
      _set_config_options(cfg, &opts);

    free(params);
    hpx_option_parser_free(&opts);
  }

  optind = 0;
  return cfg;
}


void config_free(hpx_config_t *config) {
  if (config->configfile)
    free(config->configfile);
  if (config->logat && config->logat != (int*)HPX_LOCALITY_ALL)
    free(config->logat);
  free(config);
}
