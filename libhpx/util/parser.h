/** @file parser.h
 *  @brief The header file for the command line option parser
 *  generated by GNU Gengetopt version 2.22
 *  http://www.gnu.org/software/gengetopt.
 *  DO NOT modify this file, since it can be overwritten
 *  @author GNU Gengetopt by Lorenzo Bettini */

#ifndef PARSER_H
#define PARSER_H

/* If we use autoconf.  */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h> /* for FILE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef HPX_OPTION_PARSER_PACKAGE
/** @brief the program name */
#define HPX_OPTION_PARSER_PACKAGE PACKAGE
#endif

#ifndef HPX_OPTION_PARSER_VERSION
/** @brief the program version */
#define HPX_OPTION_PARSER_VERSION VERSION
#endif

enum enum_hpx_gas { hpx_gas_arg_default = 0 , hpx_gas_arg_smp, hpx_gas_arg_pgas, hpx_gas_arg_agas, hpx_gas_arg_pgas_switch, hpx_gas_arg_agas_switch };
enum enum_hpx_boot { hpx_boot_arg_default = 0 , hpx_boot_arg_smp, hpx_boot_arg_mpi, hpx_boot_arg_pmi };
enum enum_hpx_transport { hpx_transport_arg_default = 0 , hpx_transport_arg_smp, hpx_transport_arg_mpi, hpx_transport_arg_portals, hpx_transport_arg_photon };
enum enum_hpx_loglevel { hpx_loglevel_arg_default = 0 , hpx_loglevel_arg_boot, hpx_loglevel_arg_sched, hpx_loglevel_arg_gas, hpx_loglevel_arg_lco, hpx_loglevel_arg_net, hpx_loglevel_arg_trans, hpx_loglevel_arg_parcel, hpx_loglevel_arg_all };

/** @brief Where the command line options are stored */
struct hpx_options_t
{
  int hpx_cores_arg;	/**< @brief number of cores to run on.  */
  char * hpx_cores_orig;	/**< @brief number of cores to run on original value given at command line.  */
  const char *hpx_cores_help; /**< @brief number of cores to run on help description.  */
  int hpx_threads_arg;	/**< @brief number of scheduler threads.  */
  char * hpx_threads_orig;	/**< @brief number of scheduler threads original value given at command line.  */
  const char *hpx_threads_help; /**< @brief number of scheduler threads help description.  */
  int hpx_backoffmax_arg;	/**< @brief upper bound for worker-thread backoff.  */
  char * hpx_backoffmax_orig;	/**< @brief upper bound for worker-thread backoff original value given at command line.  */
  const char *hpx_backoffmax_help; /**< @brief upper bound for worker-thread backoff help description.  */
  long hpx_stacksize_arg;	/**< @brief set HPX stack size.  */
  char * hpx_stacksize_orig;	/**< @brief set HPX stack size original value given at command line.  */
  const char *hpx_stacksize_help; /**< @brief set HPX stack size help description.  */
  long hpx_heapsize_arg;	/**< @brief set HPX per-PE global heap size.  */
  char * hpx_heapsize_orig;	/**< @brief set HPX per-PE global heap size original value given at command line.  */
  const char *hpx_heapsize_help; /**< @brief set HPX per-PE global heap size help description.  */
  enum enum_hpx_gas hpx_gas_arg;	/**< @brief type of Global Address Space (GAS).  */
  char * hpx_gas_orig;	/**< @brief type of Global Address Space (GAS) original value given at command line.  */
  const char *hpx_gas_help; /**< @brief type of Global Address Space (GAS) help description.  */
  enum enum_hpx_boot hpx_boot_arg;	/**< @brief HPX bootstrap method to use.  */
  char * hpx_boot_orig;	/**< @brief HPX bootstrap method to use original value given at command line.  */
  const char *hpx_boot_help; /**< @brief HPX bootstrap method to use help description.  */
  enum enum_hpx_transport hpx_transport_arg;	/**< @brief type of transport to use.  */
  char * hpx_transport_orig;	/**< @brief type of transport to use original value given at command line.  */
  const char *hpx_transport_help; /**< @brief type of transport to use help description.  */
  int hpx_waitat_arg;	/**< @brief wait for debugger at specific locality.  */
  char * hpx_waitat_orig;	/**< @brief wait for debugger at specific locality original value given at command line.  */
  const char *hpx_waitat_help; /**< @brief wait for debugger at specific locality help description.  */
  enum enum_hpx_loglevel hpx_loglevel_arg;	/**< @brief set the logging level.  */
  char * hpx_loglevel_orig;	/**< @brief set the logging level original value given at command line.  */
  const char *hpx_loglevel_help; /**< @brief set the logging level help description.  */
  int hpx_statistics_flag;	/**< @brief print HPX runtime statistics (default=off).  */
  const char *hpx_statistics_help; /**< @brief print HPX runtime statistics help description.  */
  long hpx_reqlimit_arg;	/**< @brief HPX transport-specific request limit.  */
  char * hpx_reqlimit_orig;	/**< @brief HPX transport-specific request limit original value given at command line.  */
  const char *hpx_reqlimit_help; /**< @brief HPX transport-specific request limit help description.  */
  char * hpx_configfile_arg;	/**< @brief HPX runtime configuration file.  */
  char * hpx_configfile_orig;	/**< @brief HPX runtime configuration file original value given at command line.  */
  const char *hpx_configfile_help; /**< @brief HPX runtime configuration file help description.  */
  int hpx_mprotectstacks_flag;	/**< @brief use mprotect() to bracket stacks to look for stack overflows (default=off).  */
  const char *hpx_mprotectstacks_help; /**< @brief use mprotect() to bracket stacks to look for stack overflows help description.  */
  int hpx_waitonabort_flag;	/**< @brief call hpx_wait() inside of hpx_abort() for debugging (default=off).  */
  const char *hpx_waitonabort_help; /**< @brief call hpx_wait() inside of hpx_abort() for debugging help description.  */
  
  unsigned int hpx_cores_given ;	/**< @brief Whether hpx-cores was given.  */
  unsigned int hpx_threads_given ;	/**< @brief Whether hpx-threads was given.  */
  unsigned int hpx_backoffmax_given ;	/**< @brief Whether hpx-backoffmax was given.  */
  unsigned int hpx_stacksize_given ;	/**< @brief Whether hpx-stacksize was given.  */
  unsigned int hpx_heapsize_given ;	/**< @brief Whether hpx-heapsize was given.  */
  unsigned int hpx_gas_given ;	/**< @brief Whether hpx-gas was given.  */
  unsigned int hpx_boot_given ;	/**< @brief Whether hpx-boot was given.  */
  unsigned int hpx_transport_given ;	/**< @brief Whether hpx-transport was given.  */
  unsigned int hpx_waitat_given ;	/**< @brief Whether hpx-waitat was given.  */
  unsigned int hpx_loglevel_given ;	/**< @brief Whether hpx-loglevel was given.  */
  unsigned int hpx_statistics_given ;	/**< @brief Whether hpx-statistics was given.  */
  unsigned int hpx_reqlimit_given ;	/**< @brief Whether hpx-reqlimit was given.  */
  unsigned int hpx_configfile_given ;	/**< @brief Whether hpx-configfile was given.  */
  unsigned int hpx_mprotectstacks_given ;	/**< @brief Whether hpx-mprotectstacks was given.  */
  unsigned int hpx_waitonabort_given ;	/**< @brief Whether hpx-waitonabort was given.  */

} ;

/** @brief The additional parameters to pass to parser functions */
struct hpx_option_parser_params
{
  int override; /**< @brief whether to override possibly already present options (default 0) */
  int initialize; /**< @brief whether to initialize the option structure hpx_options_t (default 1) */
  int check_required; /**< @brief whether to check that all required options were provided (default 1) */
  int check_ambiguity; /**< @brief whether to check for options already specified in the option structure hpx_options_t (default 0) */
  int print_errors; /**< @brief whether getopt_long should print an error message for a bad option (default 1) */
} ;

/** @brief the purpose string of the program */
extern const char *hpx_options_t_purpose;
/** @brief the usage string of the program */
extern const char *hpx_options_t_usage;
/** @brief all the lines making the help output */
extern const char *hpx_options_t_help[];

/**
 * The command line parser
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser (int argc, char * const *argv,
  struct hpx_options_t *args_info);

/**
 * The command line parser (version with additional parameters - deprecated)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use hpx_option_parser_ext() instead
 */
int hpx_option_parser2 (int argc, char * const *argv,
  struct hpx_options_t *args_info,
  int override, int initialize, int check_required);

/**
 * The command line parser (version with additional parameters)
 * @param argc the number of command line options
 * @param argv the command line options
 * @param args_info the structure where option information will be stored
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_ext (int argc, char * const *argv,
  struct hpx_options_t *args_info,
  struct hpx_option_parser_params *params);

/**
 * Save the contents of the option struct into an already open FILE stream.
 * @param outfile the stream where to dump options
 * @param args_info the option struct to dump
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_dump(FILE *outfile,
  struct hpx_options_t *args_info);

/**
 * Save the contents of the option struct into a (text) file.
 * This file can be read by the config file parser (if generated by gengetopt)
 * @param filename the file where to save
 * @param args_info the option struct to save
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_file_save(const char *filename,
  struct hpx_options_t *args_info);

/**
 * Print the help
 */
void hpx_option_parser_print_help(void);
/**
 * Print the version
 */
void hpx_option_parser_print_version(void);

/**
 * Initializes all the fields a hpx_option_parser_params structure 
 * to their default values
 * @param params the structure to initialize
 */
void hpx_option_parser_params_init(struct hpx_option_parser_params *params);

/**
 * Allocates dynamically a hpx_option_parser_params structure and initializes
 * all its fields to their default values
 * @return the created and initialized hpx_option_parser_params structure
 */
struct hpx_option_parser_params *hpx_option_parser_params_create(void);

/**
 * Initializes the passed hpx_options_t structure's fields
 * (also set default values for options that have a default)
 * @param args_info the structure to initialize
 */
void hpx_option_parser_init (struct hpx_options_t *args_info);
/**
 * Deallocates the string fields of the hpx_options_t structure
 * (but does not deallocate the structure itself)
 * @param args_info the structure to deallocate
 */
void hpx_option_parser_free (struct hpx_options_t *args_info);

/**
 * The config file parser (deprecated version)
 * @param filename the name of the config file
 * @param args_info the structure where option information will be stored
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use hpx_option_parser_config_file() instead
 */
int hpx_option_parser_configfile (char * const filename,
  struct hpx_options_t *args_info,
  int override, int initialize, int check_required);

/**
 * The config file parser
 * @param filename the name of the config file
 * @param args_info the structure where option information will be stored
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_config_file (char * const filename,
  struct hpx_options_t *args_info,
  struct hpx_option_parser_params *params);

/**
 * The string parser (interprets the passed string as a command line)
 * @param cmdline the command line stirng
 * @param args_info the structure where option information will be stored
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_string (const char *cmdline, struct hpx_options_t *args_info,
  const char *prog_name);
/**
 * The string parser (version with additional parameters - deprecated)
 * @param cmdline the command line stirng
 * @param args_info the structure where option information will be stored
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @param override whether to override possibly already present options
 * @param initialize whether to initialize the option structure my_args_info
 * @param check_required whether to check that all required options were provided
 * @return 0 if everything went fine, NON 0 if an error took place
 * @deprecated use hpx_option_parser_string_ext() instead
 */
int hpx_option_parser_string2 (const char *cmdline, struct hpx_options_t *args_info,
  const char *prog_name,
  int override, int initialize, int check_required);
/**
 * The string parser (version with additional parameters)
 * @param cmdline the command line stirng
 * @param args_info the structure where option information will be stored
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @param params additional parameters for the parser
 * @return 0 if everything went fine, NON 0 if an error took place
 */
int hpx_option_parser_string_ext (const char *cmdline, struct hpx_options_t *args_info,
  const char *prog_name,
  struct hpx_option_parser_params *params);

/**
 * Checks that all the required options were specified
 * @param args_info the structure to check
 * @param prog_name the name of the program that will be used to print
 *   possible errors
 * @return
 */
int hpx_option_parser_required (struct hpx_options_t *args_info,
  const char *prog_name);

extern char *hpx_option_parser_hpx_gas_values[] ;	/**< @brief Possible values for hpx-gas.  */
extern char *hpx_option_parser_hpx_boot_values[] ;	/**< @brief Possible values for hpx-boot.  */
extern char *hpx_option_parser_hpx_transport_values[] ;	/**< @brief Possible values for hpx-transport.  */
extern char *hpx_option_parser_hpx_loglevel_values[] ;	/**< @brief Possible values for hpx-loglevel.  */


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PARSER_H */
