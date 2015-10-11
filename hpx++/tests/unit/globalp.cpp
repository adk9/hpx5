#include <iostream>
#include <hpx++/hpx++.h>

using namespace std;

hpx_status_t test_addr_arith() {
  int n1 = 10, n2 = 20;
  hpx::global_ptr<uint64_t> ptr1 = hpx::gas::alloc_cyclic<uint64_t>(n1);
  
  // TODO populate ptr1 and dereference at some index and check the value
  
  auto ptr2 = hpx::gas::alloc_cyclic<uint64_t>(n2,2);
  
  auto dist = ptr2 - ptr1;
  // is dist guaranteed to be > 0?
  cout << "dist: " << dist << endl;
  
  return HPX_SUCCESS;
}

hpx_status_t test_pin_unpin() {
  int n1 = 10;
  auto ptr1 = hpx::gas::alloc_local<int>(n1);
  int* local = ptr1.pin();
  // ...
  ptr1.unpin();
  return HPX_SUCCESS;
}

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    cerr << "HPX: failed to initialize." << endl;
    return e;
  }
  
  // TODO use hpx test framework
  test_addr_arith();
  
  hpx::finalize();
  return 0;
}
