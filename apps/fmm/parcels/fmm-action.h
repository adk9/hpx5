/// ---------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Declare FMM actions 
/// ---------------------------------------------------------------------------

#include "hpx/hpx.h"

extern hpx_action_t _fmm_main; 
extern hpx_action_t _init_sources;
extern hpx_action_t _init_targets; 
extern hpx_action_t _init_param; 
extern hpx_action_t _init_source_root; 
extern hpx_action_t _init_target_root; 
extern hpx_action_t _partition_box; 
extern hpx_action_t _swap; 
extern hpx_action_t _set_box; 
extern hpx_action_t _aggregate; 
extern hpx_action_t _source_to_mpole; 
extern hpx_action_t _mpole_to_mpole;
extern hpx_action_t _mpole_to_expo; 
extern hpx_action_t _disaggregate; 
extern hpx_action_t _build_list5; 
extern hpx_action_t _query_box; 
extern hpx_action_t _source_to_local;
extern hpx_action_t _delete_box; 
extern hpx_action_t _merge_expo; 
extern hpx_action_t _merge_expo_zp; 
extern hpx_action_t _merge_expo_zm; 
extern hpx_action_t _merge_update; 
extern hpx_action_t _shift_expo_c1; 
extern hpx_action_t _shift_expo_c2; 
extern hpx_action_t _shift_expo_c3; 
extern hpx_action_t _shift_expo_c4; 
extern hpx_action_t _shift_expo_c5; 
extern hpx_action_t _shift_expo_c6; 
extern hpx_action_t _shift_expo_c7; 
extern hpx_action_t _shift_expo_c8; 
extern hpx_action_t _merge_local; 

/// ---------------------------------------------------------------------------
/// @brief The main FMM action
/// ---------------------------------------------------------------------------
int _fmm_main_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Initialize sources action
/// ---------------------------------------------------------------------------
int _init_sources_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Initialize targets action
/// ---------------------------------------------------------------------------
int _init_targets_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Initialize root box of the source tree
/// ---------------------------------------------------------------------------
int _init_source_root_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Initialize root box of the target tree
/// ---------------------------------------------------------------------------
int _init_target_root_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Initialize FMM param action
/// ---------------------------------------------------------------------------
int _init_param_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Construct FMM DAG action
/// ---------------------------------------------------------------------------
int _partition_box_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Swap points action
/// ---------------------------------------------------------------------------
int _swap_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Set box action
/// ---------------------------------------------------------------------------
int _set_box_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Source to multipole action
/// ---------------------------------------------------------------------------
int _aggregate_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Source to multipole action helper
/// ---------------------------------------------------------------------------
int _source_to_multipole_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Multipole to multipole action
/// ---------------------------------------------------------------------------
int _multipole_to_multipole_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Multipole to exponential action
/// ---------------------------------------------------------------------------
int _multipole_to_exponential_action(void *args); 

void multipole_to_exponential_p1(const double complex *multipole, 
                                 double complex *mexpu, 
                                 double complex *mexpd); 

void multipole_to_exponential_p2(const double complex *mexpf, 
                                 double complex *mexpphys); 

/// ---------------------------------------------------------------------------
/// @brief Disaggregate operation
/// ---------------------------------------------------------------------------
int _disaggregate_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Build list5 action
/// ---------------------------------------------------------------------------
int _build_list5_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Query box information action
/// ---------------------------------------------------------------------------
int _query_box_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Source to local action
/// ---------------------------------------------------------------------------
int _source_to_local_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Delete box action
/// ---------------------------------------------------------------------------
int _delete_box_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Merge exponential action
/// ---------------------------------------------------------------------------
int _merge_exponential_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Process '+' direction exponential expansion action
/// ---------------------------------------------------------------------------
int _merge_exponential_zp_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Process '-' direction exponential expansion action
/// ---------------------------------------------------------------------------
int _merge_exponential_zm_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Merge update action
/// ---------------------------------------------------------------------------
int _merge_update_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 1
/// ---------------------------------------------------------------------------
int _shift_exponential_c1_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 2
/// ---------------------------------------------------------------------------
int _shift_exponential_c2_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 3
/// ---------------------------------------------------------------------------
int _shift_exponential_c3_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 4
/// ---------------------------------------------------------------------------
int _shift_exponential_c4_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 5
/// ---------------------------------------------------------------------------
int _shift_exponential_c5_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 6
/// ---------------------------------------------------------------------------
int _shift_exponential_c6_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 7
/// ---------------------------------------------------------------------------
int _shift_exponential_c7_action(void); 

/// ---------------------------------------------------------------------------
/// @brief Shift merged exponential expansion to child 8
/// ---------------------------------------------------------------------------
int _shift_exponential_c8_action(void); 

void exponential_to_local_p1(const double complex *mexpphys, 
                             double complex *mexpf);

void exponential_to_local_p2(const double complex *mexpu,
                             const double complex *mexpd, 
                             double complex *local); 

/// ---------------------------------------------------------------------------
/// @brief Merge local action
/// ---------------------------------------------------------------------------
int _merge_local_action(void *args); 

/// ---------------------------------------------------------------------------
/// @brief Evaluates Lengndre polynomial 
/// ---------------------------------------------------------------------------
void lgndr(int nmax, double x, double *y); 

/// ---------------------------------------------------------------------------
/// @brief Rotation z->y
/// ---------------------------------------------------------------------------
void rotz2y(const double complex *multipole, const double *rd, 
            double complex *mrotate); 

/// ---------------------------------------------------------------------------
/// @brief Rotation y->z
/// ---------------------------------------------------------------------------
void roty2z(const double complex *multipole, const double *rd, 
            double complex *mrotate); 

/// ---------------------------------------------------------------------------
/// @brief Rotation z->x
/// ---------------------------------------------------------------------------
void rotz2x(const double complex *multipole, const double *rd, 
            double complex *mrotate); 

