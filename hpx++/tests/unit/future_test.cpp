#include <iostream>
#include <hpx++/hpx++.h>

int main(int argc, char* argv[]) {
  
  int e = hpx::init(&argc, &argv);
  if (e) {
    std::cerr << "HPX: failed to initialize." << std::endl;
    return e;
  }
  
  hpx::lco::Future<double> f1;
  
  hpx::finalize();
  return 0;
}

