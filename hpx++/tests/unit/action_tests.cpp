#include <iostream>
#include "hpx++/hpx++.h"

hpx_status_t l_tst() {
  auto l = [](int x) {
    return x;
  };
  
  return hpx::action::register_action(l);
}

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  
  l_tst();
  
  hpx::finalize();
  return 0;
}