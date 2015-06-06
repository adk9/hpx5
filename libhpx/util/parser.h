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

enum enum_hpx_gas { hpx_gas_arg_default = 0 , hpx_gas_arg_smp, hpx_gas_arg_pgas, hpx_gas_arg_agas };
enum enum_hpx_boot { hpx_boot_arg_default = 0 , hpx_boot_arg_smp, hpx_boot_arg_mpi, hpx_boot_arg_pmi };
enum enum_hpx_transport { hpx_transport_arg_default = 0 , hpx_transport_arg_mpi, hpx_transport_arg_portals, hpx_transport_arg_photon };
enum enum_hpx_network { hpx_network_arg_default = 0 , hpx_network_arg_smp, hpx_network_arg_pwc, hpx_network_arg_isir };
enum enum_hpx_log_level { hpx_log_level_arg_default = 0 , hpx_log_level_arg_boot, hpx_log_level_arg_sched, hpx_log_level_arg_gas, hpx_log_level_arg_lco, hpx_log_level_arg_net, hpx_log_level_arg_trans, hpx_log_level_arg_parcel, hpx_log_level_arg_action, hpx_log_level_arg_config, hpx_log_level_arg_memory, hpx_log_level_arg_all };
enum enum_hpx_trace_classes { hpx_trace_classes_arg_parcel = 0 , hpx_trace_classes_arg_pwc, hpx_trace_classes_arg_sched, hpx_trace_classes_arg_all };
enum enum_hpx_photon_backend { hpx_photon_backend_arg_default = 0 , hpx_photon_backend_arg_verbs, hpx_photon_backend_arg_ugni };

/** @brief Where the command line options are stored */
struct hpx_options_t
{
  int hpx_help_flag;	/**< @brief print HPX help (default=off).  */
  const char *hpx_help_help; /**< @brief print HPX help help description.  */
  int hpx_version_flag;	/**< @brief print HPX version (default=off).  */
  const char *hpx_version_help; /**< @brief print HPX version help description.  */
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
  enum enum_hpx_network hpx_network_arg;	/**< @brief type of network to use.  */
  char * hpx_network_orig;	/**< @brief type of network to use original value given at command line.  */
  const char *hpx_network_help; /**< @brief type of network to use help description.  */
  int hpx_statistics_flag;	/**< @brief print HPX runtime statistics (default=off).  */
  const char *hpx_statistics_help; /**< @brief print HPX runtime statistics help description.  */
  char * hpx_configfile_arg;	/**< @brief HPX runtime configuration file.  */
  char * hpx_configfile_orig;	/**< @brief HPX runtime configuration file original value given at command line.  */
  const char *hpx_configfile_help; /**< @brief HPX runtime configuration file help description.  */
  int hpx_cores_arg;	/**< @brief number of scheduler cores (deprecated).  */
  char * hpx_cores_orig;	/**< @brief number of scheduler cores (deprecated) original value given at command line.  */
  const char *hpx_cores_help; /**< @brief number of scheduler cores (deprecated) help description.  */
  int hpx_threads_arg;	/**< @brief number of scheduler threads.  */
  char * hpx_threads_orig;	/**< @brief number of scheduler threads original value given at command line.  */
  const char *hpx_threads_help; /**< @brief number of scheduler threads help description.  */
  long hpx_stacksize_arg;	/**< @brief set HPX stack size.  */
  char * hpx_stacksize_orig;	/**< @brief set HPX stack size original value given at command line.  */
  const char *hpx_stacksize_help; /**< @brief set HPX stack size help description.  */
  long hpx_wfthreshold_arg;	/**< @brief bound on help-first tasks before work-first scheduling.  */
  char * hpx_wfthreshold_orig;	/**< @brief bound on help-first tasks before work-first scheduling original value given at command line.  */
  const char *hpx_wfthreshold_help; /**< @brief bound on help-first tasks before work-first scheduling help description.  */
  int hpx_sched_stackcachelimit_arg;	/**< @brief bound on the number of stacks to cache.  */
  char * hpx_sched_stackcachelimit_orig;	/**< @brief bound on the number of stacks to cache original value given at command line.  */
  const char *hpx_sched_stackcachelimit_help; /**< @brief bound on the number of stacks to cache help description.  */
  int* hpx_log_at_arg;	/**< @brief selectively output log information.  */
  char ** hpx_log_at_orig;	/**< @brief selectively output log information original value given at command line.  */
  int hpx_log_at_min; /**< @brief selectively output log information's minimum occurreces */
  int hpx_log_at_max; /**< @brief selectively output log information's maximum occurreces */
  const char *hpx_log_at_help; /**< @brief selectively output log information help description.  */
  enum enum_hpx_log_level *hpx_log_level_arg;	/**< @brief set the logging level.  */
  char ** hpx_log_level_orig;	/**< @brief set the logging level original value given at command line.  */
  int hpx_log_level_min; /**< @brief set the logging level's minimum occurreces */
  int hpx_log_level_max; /**< @brief set the logging level's maximum occurreces */
  const char *hpx_log_level_help; /**< @brief set the logging level help description.  */
  int* hpx_dbg_waitat_arg;	/**< @brief wait for debugger at specific locality.  */
  char ** hpx_dbg_waitat_orig;	/**< @brief wait for debugger at specific locality original value given at command line.  */
  int hpx_dbg_waitat_min; /**< @brief wait for debugger at specific locality's minimum occurreces */
  int hpx_dbg_waitat_max; /**< @brief wait for debugger at specific locality's maximum occurreces */
  const char *hpx_dbg_waitat_help; /**< @brief wait for debugger at specific locality help description.  */
  int hpx_dbg_waitonabort_flag;	/**< @brief call hpx_wait() inside of hpx_abort() for debugging (default=off).  */
  const char *hpx_dbg_waitonabort_help; /**< @brief call hpx_wait() inside of hpx_abort() for debugging help description.  */
  int hpx_dbg_waitonsegv_flag;	/**< @brief call hpx_wait() for SIGSEGV for debugging (unreliable) (default=off).  */
  const char *hpx_dbg_waitonsegv_help; /**< @brief call hpx_wait() for SIGSEGV for debugging (unreliable) help description.  */
  int hpx_dbg_mprotectstacks_flag;	/**< @brief use mprotect() to bracket stacks to look for stack overflows (default=off).  */
  const char *hpx_dbg_mprotectstacks_help; /**< @brief use mprotect() to bracket stacks to look for stack overflows help description.  */
  enum enum_hpx_trace_classes *hpx_trace_classes_arg;	/**< @brief set the event classes to trace.  */
  char ** hpx_trace_classes_orig;	/**< @brief set the event classes to trace original value given at command line.  */
  int hpx_trace_classes_min; /**< @brief set the event classes to trace's minimum occurreces */
  int hpx_trace_classes_max; /**< @brief set the event classes to trace's maximum occurreces */
  const char *hpx_trace_classes_help; /**< @brief set the event classes to trace help description.  */
  char * hpx_trace_dir_arg;	/**< @brief directory to output trace files.  */
  char * hpx_trace_dir_orig;	/**< @brief directory to output trace files original value given at command line.  */
  const char *hpx_trace_dir_help; /**< @brief directory to output trace files help description.  */
  long hpx_trace_filesize_arg;	/**< @brief set the size of each trace file.  */
  char * hpx_trace_filesize_orig;	/**< @brief set the size of each trace file original value given at command line.  */
  const char *hpx_trace_filesize_help; /**< @brief set the size of each trace file help description.  */
  int* hpx_trace_at_arg;	/**< @brief set the localities to trace at.  */
  char ** hpx_trace_at_orig;	/**< @brief set the localities to trace at original value given at command line.  */
  int hpx_trace_at_min; /**< @brief set the localities to trace at's minimum occurreces */
  int hpx_trace_at_max; /**< @brief set the localities to trace at's maximum occurreces */
  const char *hpx_trace_at_help; /**< @brief set the localities to trace at help description.  */
  long hpx_isir_testwindow_arg;	/**< @brief number of ISIR requests to test in progress loop.  */
  char * hpx_isir_testwindow_orig;	/**< @brief number of ISIR requests to test in progress loop original value given at command line.  */
  const char *hpx_isir_testwindow_help; /**< @brief number of ISIR requests to test in progress loop help description.  */
  long hpx_isir_sendlimit_arg;	/**< @brief ISIR network send limit.  */
  char * hpx_isir_sendlimit_orig;	/**< @brief ISIR network send limit original value given at command line.  */
  const char *hpx_isir_sendlimit_help; /**< @brief ISIR network send limit help description.  */
  long hpx_isir_recvlimit_arg;	/**< @brief ISIR network recv limit.  */
  char * hpx_isir_recvlimit_orig;	/**< @brief ISIR network recv limit original value given at command line.  */
  const char *hpx_isir_recvlimit_help; /**< @brief ISIR network recv limit help description.  */
  long hpx_pwc_parcelbuffersize_arg;	/**< @brief set the size of p2p recv buffers for parcel sends.  */
  char * hpx_pwc_parcelbuffersize_orig;	/**< @brief set the size of p2p recv buffers for parcel sends original value given at command line.  */
  const char *hpx_pwc_parcelbuffersize_help; /**< @brief set the size of p2p recv buffers for parcel sends help description.  */
  long hpx_pwc_parceleagerlimit_arg;	/**< @brief set the largest eager parcel size (header inclusive).  */
  char * hpx_pwc_parceleagerlimit_orig;	/**< @brief set the largest eager parcel size (header inclusive) original value given at command line.  */
  const char *hpx_pwc_parceleagerlimit_help; /**< @brief set the largest eager parcel size (header inclusive) help description.  */
  enum enum_hpx_photon_backend hpx_photon_backend_arg;	/**< @brief set the underlying network API to use.  */
  char * hpx_photon_backend_orig;	/**< @brief set the underlying network API to use original value given at command line.  */
  const char *hpx_photon_backend_help; /**< @brief set the underlying network API to use help description.  */
  char * hpx_photon_ibdev_arg;	/**< @brief set a particular IB device (also a filter for device and port discovery, e.g. qib0:1+mlx4_0:2).  */
  char * hpx_photon_ibdev_orig;	/**< @brief set a particular IB device (also a filter for device and port discovery, e.g. qib0:1+mlx4_0:2) original value given at command line.  */
  const char *hpx_photon_ibdev_help; /**< @brief set a particular IB device (also a filter for device and port discovery, e.g. qib0:1+mlx4_0:2) help description.  */
  char * hpx_photon_ethdev_arg;	/**< @brief set a particular ETH device (for CMA mode only).  */
  char * hpx_photon_ethdev_orig;	/**< @brief set a particular ETH device (for CMA mode only) original value given at command line.  */
  const char *hpx_photon_ethdev_help; /**< @brief set a particular ETH device (for CMA mode only) help description.  */
  int hpx_photon_ibport_arg;	/**< @brief set a particular IB port.  */
  char * hpx_photon_ibport_orig;	/**< @brief set a particular IB port original value given at command line.  */
  const char *hpx_photon_ibport_help; /**< @brief set a particular IB port help description.  */
  int hpx_photon_usecma_flag;	/**< @brief enable CMA connection mode (default=off).  */
  const char *hpx_photon_usecma_help; /**< @brief enable CMA connection mode help description.  */
  int hpx_photon_ledgersize_arg;	/**< @brief set number of ledger entries.  */
  char * hpx_photon_ledgersize_orig;	/**< @brief set number of ledger entries original value given at command line.  */
  const char *hpx_photon_ledgersize_help; /**< @brief set number of ledger entries help description.  */
  int hpx_photon_eagerbufsize_arg;	/**< @brief set size of eager buffers.  */
  char * hpx_photon_eagerbufsize_orig;	/**< @brief set size of eager buffers original value given at command line.  */
  const char *hpx_photon_eagerbufsize_help; /**< @brief set size of eager buffers help description.  */
  int hpx_photon_smallpwcsize_arg;	/**< @brief set PWC small msg limit.  */
  char * hpx_photon_smallpwcsize_orig;	/**< @brief set PWC small msg limit original value given at command line.  */
  const char *hpx_photon_smallpwcsize_help; /**< @brief set PWC small msg limit help description.  */
  int hpx_photon_maxrd_arg;	/**< @brief set max number of request descriptors.  */
  char * hpx_photon_maxrd_orig;	/**< @brief set max number of request descriptors original value given at command line.  */
  const char *hpx_photon_maxrd_help; /**< @brief set max number of request descriptors help description.  */
  int hpx_photon_defaultrd_arg;	/**< @brief set default number of allocated descriptors.  */
  char * hpx_photon_defaultrd_orig;	/**< @brief set default number of allocated descriptors original value given at command line.  */
  const char *hpx_photon_defaultrd_help; /**< @brief set default number of allocated descriptors help description.  */
  int hpx_photon_numcq_arg;	/**< @brief set number of completion queues to use (cyclic assignment to ranks).  */
  char * hpx_photon_numcq_orig;	/**< @brief set number of completion queues to use (cyclic assignment to ranks) original value given at command line.  */
  const char *hpx_photon_numcq_help; /**< @brief set number of completion queues to use (cyclic assignment to ranks) help description.  */
  int hpx_opt_smp_arg;	/**< @brief optimize for SMP execution.  */
  char * hpx_opt_smp_orig;	/**< @brief optimize for SMP execution original value given at command line.  */
  const char *hpx_opt_smp_help; /**< @brief optimize for SMP execution help description.  */
  
  unsigned int hpx_help_given ;	/**< @brief Whether hpx-help was given.  */
  unsigned int hpx_version_given ;	/**< @brief Whether hpx-version was given.  */
  unsigned int hpx_heapsize_given ;	/**< @brief Whether hpx-heapsize was given.  */
  unsigned int hpx_gas_given ;	/**< @brief Whether hpx-gas was given.  */
  unsigned int hpx_boot_given ;	/**< @brief Whether hpx-boot was given.  */
  unsigned int hpx_transport_given ;	/**< @brief Whether hpx-transport was given.  */
  unsigned int hpx_network_given ;	/**< @brief Whether hpx-network was given.  */
  unsigned int hpx_statistics_given ;	/**< @brief Whether hpx-statistics was given.  */
  unsigned int hpx_configfile_given ;	/**< @brief Whether hpx-configfile was given.  */
  unsigned int hpx_cores_given ;	/**< @brief Whether hpx-cores was given.  */
  unsigned int hpx_threads_given ;	/**< @brief Whether hpx-threads was given.  */
  unsigned int hpx_stacksize_given ;	/**< @brief Whether hpx-stacksize was given.  */
  unsigned int hpx_wfthreshold_given ;	/**< @brief Whether hpx-wfthreshold was given.  */
  unsigned int hpx_sched_stackcachelimit_given ;	/**< @brief Whether hpx-sched-stackcachelimit was given.  */
  unsigned int hpx_log_at_given ;	/**< @brief Whether hpx-log-at was given.  */
  unsigned int hpx_log_level_given ;	/**< @brief Whether hpx-log-level was given.  */
  unsigned int hpx_dbg_waitat_given ;	/**< @brief Whether hpx-dbg-waitat was given.  */
  unsigned int hpx_dbg_waitonabort_given ;	/**< @brief Whether hpx-dbg-waitonabort was given.  */
  unsigned int hpx_dbg_waitonsegv_given ;	/**< @brief Whether hpx-dbg-waitonsegv was given.  */
  unsigned int hpx_dbg_mprotectstacks_given ;	/**< @brief Whether hpx-dbg-mprotectstacks was given.  */
  unsigned int hpx_trace_classes_given ;	/**< @brief Whether hpx-trace-classes was given.  */
  unsigned int hpx_trace_dir_given ;	/**< @brief Whether hpx-trace-dir was given.  */
  unsigned int hpx_trace_filesize_given ;	/**< @brief Whether hpx-trace-filesize was given.  */
  unsigned int hpx_trace_at_given ;	/**< @brief Whether hpx-trace-at was given.  */
  unsigned int hpx_isir_testwindow_given ;	/**< @brief Whether hpx-isir-testwindow was given.  */
  unsigned int hpx_isir_sendlimit_given ;	/**< @brief Whether hpx-isir-sendlimit was given.  */
  unsigned int hpx_isir_recvlimit_given ;	/**< @brief Whether hpx-isir-recvlimit was given.  */
  unsigned int hpx_pwc_parcelbuffersize_given ;	/**< @brief Whether hpx-pwc-parcelbuffersize was given.  */
  unsigned int hpx_pwc_parceleagerlimit_given ;	/**< @brief Whether hpx-pwc-parceleagerlimit was given.  */
  unsigned int hpx_photon_backend_given ;	/**< @brief Whether hpx-photon-backend was given.  */
  unsigned int hpx_photon_ibdev_given ;	/**< @brief Whether hpx-photon-ibdev was given.  */
  unsigned int hpx_photon_ethdev_given ;	/**< @brief Whether hpx-photon-ethdev was given.  */
  unsigned int hpx_photon_ibport_given ;	/**< @brief Whether hpx-photon-ibport was given.  */
  unsigned int hpx_photon_usecma_given ;	/**< @brief Whether hpx-photon-usecma was given.  */
  unsigned int hpx_photon_ledgersize_given ;	/**< @brief Whether hpx-photon-ledgersize was given.  */
  unsigned int hpx_photon_eagerbufsize_given ;	/**< @brief Whether hpx-photon-eagerbufsize was given.  */
  unsigned int hpx_photon_smallpwcsize_given ;	/**< @brief Whether hpx-photon-smallpwcsize was given.  */
  unsigned int hpx_photon_maxrd_given ;	/**< @brief Whether hpx-photon-maxrd was given.  */
  unsigned int hpx_photon_defaultrd_given ;	/**< @brief Whether hpx-photon-defaultrd was given.  */
  unsigned int hpx_photon_numcq_given ;	/**< @brief Whether hpx-photon-numcq was given.  */
  unsigned int hpx_opt_smp_given ;	/**< @brief Whether hpx-opt-smp was given.  */

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
extern char *hpx_option_parser_hpx_network_values[] ;	/**< @brief Possible values for hpx-network.  */
extern char *hpx_option_parser_hpx_log_level_values[] ;	/**< @brief Possible values for hpx-log-level.  */
extern char *hpx_option_parser_hpx_trace_classes_values[] ;	/**< @brief Possible values for hpx-trace-classes.  */
extern char *hpx_option_parser_hpx_photon_backend_values[] ;	/**< @brief Possible values for hpx-photon-backend.  */


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PARSER_H */
