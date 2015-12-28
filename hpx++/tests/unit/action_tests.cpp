#include <iostream>
#include "hpx++/hpx++.h"
#include "hpx/rpc.h"

using namespace std;

// static int _my_interrupt_handler(void) {
//   printf("Hi, I am an interrupt!\n");
//   return HPX_SUCCESS;
// }
// static HPX_ACTION(HPX_INTERRUPT, 0, _my_interrupt, _my_interrupt_handler);
// 
// static int _my_task_handler(void) {
//   printf("Hi, I am a task!\n");
//   hpx_call_cc(HPX_HERE, _my_interrupt, NULL, NULL);
//   return HPX_SUCCESS;
// }
// static HPX_ACTION(HPX_TASK, 0, _my_task, _my_task_handler);
// 
// static int _my_action_handler(void) {
//   printf("Hi, I am an action!\n");
//   hpx_call_cc(HPX_HERE, _my_task, NULL, NULL);
//   return HPX_SUCCESS;
// }
// HPXPP_REGISTER_ACTION(_my_action_handler);

static int _my_typed_handler(int i, float f, char c) {
  printf("Hi, I am a typed action with args: %d %f %c!\n", i, f, c);
//   hpx_call_cc(HPX_HERE, _my_action_handler_action_struct::id, NULL, NULL);
  return HPX_SUCCESS;
}
HPXPP_REGISTER_ACTION(_my_typed_handler);

int hello(int a) {
  std::cout << "Rank#" << hpx_get_my_rank() << " received " << a << "." << std::endl;
  return HPX_SUCCESS;
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



int main_act(int arg) {
  std::cout << "main action begin..." << std::endl;
  
  test1(arg);
  
  _my_typed_handler_action_struct typed_o;
  int r, i = 1; float f = 3.0; char c = 'b';
  typed_o.call_sync(HPX_HERE, r, i, f, c);
  
  hpx::exit(HPX_SUCCESS);
}

HPXPP_REGISTER_ACTION(main_act);

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  int a = hpx_get_my_rank() + 1;
  main_act_action_struct obj;
  obj(a);
  
  test1();
  
  hpx::finalize();
  return 0;
}