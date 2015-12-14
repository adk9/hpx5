#include <iostream>
#include "hpx++/hpx++.h"

int hello(int a) {
  std::cout << "Hello World from " << hpx_get_my_rank() << "." << std::endl;
  hpx::exit(HPX_SUCCESS);
}

HPXPP_REGISTER_ACTION(hello);

hpx_status_t test1() {
  
  hello_action_struct obj;
  int r;
  
  // commented code should fail to compile
//   obj(HPX_HERE, r);
//   obj(HPX_HERE, r, "");
  
  int a = 1;
  obj(HPX_HERE, r, a);
//   hpx::run(&hello_action_struct::id, &a, sizeof(int));
  
  return HPX_SUCCESS;
}

int main_act() {
  hello_action_struct obj;
  int r, a = 1;
  obj(HPX_HERE, r, a);
//   hpx::run(&hello_action_struct::id, &a, sizeof(int));
  hpx::exit(HPX_SUCCESS);
}

// HPXPP_REGISTER_ACTION(main_act);
static hpx_action_t m_a_id = 0;

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  
//   hpx::run(&(main_act_action_struct::id));
  HPX_REGISTER_ACTION(HPX_DEFAULT, 0, m_a_id, main_act);
  hpx::run(&m_a_id);
  hpx::finalize();
  return 0;
}