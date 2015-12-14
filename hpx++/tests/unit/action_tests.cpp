#include <iostream>
#include "hpx++/hpx++.h"

using namespace std;

int hello(int a) {
  std::cout << "Rank#" << hpx_get_my_rank() << " received " << a << "." << std::endl;
  hpx::exit(HPX_SUCCESS);
}

HPXPP_REGISTER_ACTION(hello);

hpx_status_t test1(int arg) {
  
  hello_action_struct obj;
  int r;
  
  // commented code should fail to compile
//   obj(HPX_HERE, r);
//   obj(HPX_HERE, r, "");
  
  obj.call_sync(HPX_HERE, r, arg);
  
  return HPX_SUCCESS;
}

int main_act(int* arg, size_t sz) {
  std::cout << "main action begin..." << std::endl;
  
//   hello_action_struct obj;
//   int r;
//   obj.call_sync(HPX_HERE, r, arg);
  
  cout << "main " << arg << endl;
  
  test1(arg);
  
  hpx::exit(HPX_SUCCESS);
}

HPXPP_REGISTER_ACTION(main_act);
// static hpx_action_t m_a_id = 0;

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  int a = hpx_get_my_rank();
  main_act_action_struct obj;
  obj(a);
  
//   HPX_REGISTER_ACTION(HPX_DEFAULT, 0, m_a_id, main_act);
//   hpx::run(&m_a_id);
  hpx::finalize();
  return 0;
}