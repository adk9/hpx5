#ifndef LIBHPX_UTIL_PRIORITY_QUEUE_H
#define LIBHPX_UTIL_PRIORITY_QUEUE_H

#include "hpx/hpx.h"
#include "libhpx/config.h"

namespace libhpx {
namespace util {
class PriorityQueue {
 public:
  static PriorityQueue* Create(const config_t * cfg);

  virtual ~PriorityQueue();

  virtual void insert(int key, hpx_parcel_t* p) = 0;
  virtual hpx_parcel_t* deleteMin() = 0;
}; // class PriorityQueue
} // namespace util
} // namespace libhpx

#endif // #define LIBHPX_UTIL_PRIORITY_QUEUE_H
