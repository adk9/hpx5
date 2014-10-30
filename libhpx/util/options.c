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
#include "config.h"
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


#define HPX_SET_VAR(cfg,opts,option)                       \
  { if (opts->hpx_##option##_given)                        \
      cfg->option = opts->hpx_##option##_arg; }

#define HPX_SET_FLAG(cfg,opts,option)                      \
  { if (opts->hpx_##option##_given)                        \
      cfg->option = opts->hpx_##option##_flag; }

#define HPX_GET_CONFIG_ENV(str,opt) _get_config_env(str, "hpx_" #opt, "hpx-" #opt)

/// The default configuration.
static const hpx_config_t _default_cfg = HPX_CONFIG_DEFAULTS;


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
  }

  if (c)
    utstring_printf(str, "--%s=%s ", optstr, c);
}

/// Try to read values of all of the configuration variables that we
/// support from the environment.
int _read_options_from_env(hpx_options_t *env_args, const char *progname) {
  UT_string *str;

  utstring_new(str);
  HPX_GET_CONFIG_ENV(str, cores);
  HPX_GET_CONFIG_ENV(str, threads);
  HPX_GET_CONFIG_ENV(str, backoffmax);
  HPX_GET_CONFIG_ENV(str, stacksize);
  HPX_GET_CONFIG_ENV(str, heapsize);
  HPX_GET_CONFIG_ENV(str, gas);
  HPX_GET_CONFIG_ENV(str, boot);
  HPX_GET_CONFIG_ENV(str, transport);
  HPX_GET_CONFIG_ENV(str, waitat);
  HPX_GET_CONFIG_ENV(str, loglevel);
  HPX_GET_CONFIG_ENV(str, statistics);
  HPX_GET_CONFIG_ENV(str, reqlimit);

  const char *cmdline = utstring_body(str);

  if (cmdline)
    hpx_option_parser_string(cmdline, env_args, progname);

  utstring_free(str);
  return 0;
}


/// Update the configuration structure @p cfg with the option values
/// specified in @p opts.
static void _set_config_options(hpx_config_t *cfg, hpx_options_t *opts) {
  HPX_SET_VAR(cfg, opts, cores);
  HPX_SET_VAR(cfg, opts, threads);
  HPX_SET_VAR(cfg, opts, backoffmax);
  HPX_SET_VAR(cfg, opts, stacksize);
  HPX_SET_VAR(cfg, opts, heapsize);
  HPX_SET_VAR(cfg, opts, gas);
  HPX_SET_VAR(cfg, opts, boot);
  HPX_SET_VAR(cfg, opts, transport);
  HPX_SET_VAR(cfg, opts, waitat);
  HPX_SET_FLAG(cfg, opts, statistics);
  HPX_SET_VAR(cfg, opts, reqlimit);

  if (opts->hpx_loglevel_given) {
    if (opts->hpx_loglevel_arg == hpx_loglevel_arg_all)
      cfg->loglevel = -1;
    else
      cfg->loglevel = (1 << opts->hpx_loglevel_arg);
  }
}

/// Print the help associated with the HPX runtime options
void hpx_print_help(void) {
  hpx_option_parser_print_help();
}

/// Parse HPX command-line options
hpx_config_t *hpx_parse_options(int *argc, char ***argv) {

  // first, initialize to the default configuration
  hpx_config_t *cfg = malloc(sizeof(*cfg));
  *cfg = _default_cfg;

  char *progname = (argv) ? (*argv)[0] : "";

  // then, read the environment for the specified configuration values
  hpx_options_t env_opts;
  int e = _read_options_from_env(&env_opts, progname);
  if (e)
    fprintf(stderr, "failed to read options from the environment variables.\n");

  _set_config_options(cfg, &env_opts);

  // finally, use the CLI-specified options to override the above
  // values
  if (!argc || !argv)
    return cfg;
  
  int nargs = 0;
  UT_string *str;
  UT_string *arg;

  utstring_new(str);
  utstring_new(arg);

  for (int i = 1; i < *argc; ++i) {
    utstring_printf(arg, "%s ", (*argv)[i]);
    if (utstring_find(arg, 0, "--hpx-", 6) != -1) {
      utstring_concat(str, arg);
      nargs++;
    }
    utstring_clear(arg);
  }

  hpx_options_t cli_opts;
  const char *cmdline = utstring_body(str);
  e = hpx_option_parser_string(cmdline, &cli_opts, progname);
  if (e)
    fprintf(stderr, "failed to parse options specified on the command-line.\n");

  _set_config_options(cfg, &cli_opts);
  utstring_free(str);

  if (nargs) {
    *argc -= nargs;
    for (int i = 1; i < *argc; ++i)
      (*argv)[i] = (*argv)[nargs+i];
  }

  optind = 0;
  return cfg;
}
