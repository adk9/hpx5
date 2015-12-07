#include <iostream>
#include "hpx++/hpx++.h"
#include "hpx/rpc.h"

int hello(int a) {
  std::cout << "Hello World from " << hpx_get_my_rank() << "." << std::endl;
  return 0;
}

HPXPP_REGISTER_ACTION(hello);

hpx_status_t test1() {
  
  hello_action_struct obj;
  int r;
  obj(HPX_HERE, r, 1);
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