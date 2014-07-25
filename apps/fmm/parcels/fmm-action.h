/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Declare FMM actions 
/// ----------------------------------------------------------------------------

#include "hpx/hpx.h"

extern hpx_action_t _fmm_main; 
extern hpx_action_t _init_sources;
extern hpx_action_t _init_charges; 
extern hpx_action_t _init_targets; 
extern hpx_action_t _init_mapsrc; 
extern hpx_action_t _init_maptar; 
extern hpx_action_t _init_param; 
extern hpx_action_t _init_source_root; 
extern hpx_action_t _init_target_root; 


extern hpx_action_t _partition_box; 
extern hpx_action_t _source_to_mpole; 
extern hpx_action_t _mpole_to_mpole;
extern hpx_action_t _mpole_reduction; 
extern hpx_action_t _mpole_to_expo; 
extern hpx_action_t _disaggregate; 

/// ----------------------------------------------------------------------------
/// @brief The main FMM action
/// ----------------------------------------------------------------------------
int _fmm_main_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize sources action
/// ----------------------------------------------------------------------------
int _init_sources_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize charges action
/// ----------------------------------------------------------------------------
int _init_charges_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize targets action
/// ----------------------------------------------------------------------------
int _init_targets_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize source mapping action
/// ----------------------------------------------------------------------------
int _init_mapsrc_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize target mapping action
/// ----------------------------------------------------------------------------
int _init_maptar_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize root box of the source tree
/// ----------------------------------------------------------------------------
int _init_source_root_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Initialize root box of the target tree
/// ----------------------------------------------------------------------------
int _init_target_root_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Initialize FMM param action
/// ----------------------------------------------------------------------------
int _init_param_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Construct the FMM DAG
/// ----------------------------------------------------------------------------
int _partition_box_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Source to multipole action
/// ----------------------------------------------------------------------------
int _source_to_multipole_action(void); 

/// ----------------------------------------------------------------------------
/// @brief Multipole to multipole action
/// ----------------------------------------------------------------------------
int _multipole_to_multipole_action(void *args); 

void multipole_to_exponential_p1(const double complex *multipole, 
                                 double complex *mexpu, 
                                 double complex *mexpd); 

void multipole_to_exponential_p2(const double complex *mexpf, 
                                 double complex *mexpphys); 

/// ----------------------------------------------------------------------------
/// @brief Reduction on multipole expansion
/// ----------------------------------------------------------------------------
int _multipole_reduction_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Multipole to exponential action
/// ----------------------------------------------------------------------------
int _multipole_to_exponential_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Disaggregate operation
/// ----------------------------------------------------------------------------
int _disaggregate_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Evaluates Lengndre polynomial 
/// ----------------------------------------------------------------------------
void lgndr(int nmax, double x, double *y); 

void rotz2y(const double complex *multipole, const double *rd, 
            double complex *mrotate); 

void roty2z(const double complex *multipole, const double *rd, 
            double complex *mrotate); 

void rotz2x(const double complex *multipole, const double *rd, 
            double complex *mrotate); 
