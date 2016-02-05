// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <iostream>
#include <hpx/hpx++.h>

using namespace std;

int cont2(int arg) {
  std::cout << "cont2 arg: " << arg << std::endl;
  return hpx::SUCCESS;
}
// HPXPP_MAKE_ACTION(cont2, void);
// static hpx_action_t cont2_id;
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, cont2_id, cont2, HPX_INT);
int cont1(int arg) {
  std::cout << "cont1 arg: " << arg << std::endl;
//   hpx_call_cc(HPX_HERE, cont2_action_struct::id, &arg);
  return hpx_thread_continue(&arg);
}
// HPXPP_MAKE_ACTION(cont1, void);
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, cont1_id, cont1, HPX_INT);

int main_act(int arg) {
  std::cout << "main_act arg: " << arg << std::endl;
  
  hpx_call_with_continuation(HPX_HERE, cont1_id, HPX_HERE, cont2_id, &arg);
  
  hpx::exit(hpx::SUCCESS);
}
// HPXPP_MAKE_ACTION(main_act, void, int);
HPX_ACTION(HPX_DEFAULT, HPX_ATTR_NONE, main_act_id, main_act, HPX_INT);

int main(int argc, char* argv[]) {

  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  int a = hpx_get_my_rank() + 1;

//   main_act_action_struct::run(a);
  hpx::run(&main_act_id, &a);

  hpx::finalize();
  return 0;
}
