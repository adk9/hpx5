#include <iostream>
#include "hpx++/hpx++.h"

int t1(int a1, double a2) {
  return a1;
}

HPXPP_REGISTER_ACTION(t1);

hpx_status_t test1() {
  
  hpx_addr_t addr;
  
  t1_action_struct obj;
  
  int r;
  obj(addr, r, 1, 3.0);
  
  return HPX_SUCCESS;
}

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  
  test1();
  
  hpx::finalize();
  return 0;
}