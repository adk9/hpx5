// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_WORKER_H
#define LIBHPX_WORKER_H

#include "libhpx/Network.h"
#include "libhpx/padding.h"
#include "libsync/deques.h"
#include "libsync/queues.h"
#include "hpx/hpx.h"
#include "hpx/attributes.h"
#include <pthread.h>
#include <atomic>
#include <functional>

/// Forward declarations.
/// @{
struct ustack;
struct Scheduler;
/// @}

// namespace libhpx {
// namespace scheduler {
/// Class representing a worker thread's state.
///
/// Worker threads are "object-oriented" insofar as that goes, but each native
/// thread has exactly one, thread-local worker structure, so the interface
/// doesn't take a "this" pointer and instead grabs the "self" structure using
/// __thread local storage.
///
/// @{
struct PaddedDequeue {
  PaddedDequeue() : work(), _pad() {
    sync_chase_lev_ws_deque_init(&work, 32);
  }

  ~PaddedDequeue() {
    sync_chase_lev_ws_deque_fini(&work);
  }

  size_t size() const {
    return sync_chase_lev_ws_deque_size(&work);
  }

  hpx_parcel_t* pop() {
    return static_cast<hpx_parcel_t*>(sync_chase_lev_ws_deque_pop(&work));
  }

  hpx_parcel_t* steal() {
    return static_cast<hpx_parcel_t*>(sync_chase_lev_ws_deque_steal(&work));
  }

  size_t push(hpx_parcel_t *p) {
    return sync_chase_lev_ws_deque_push(&work, p);
  }

  chase_lev_ws_deque_t work;                    // my work
  const char _pad[_BYTES(HPX_CACHELINE_SIZE, sizeof(work))];
};

struct Worker {

 public:

  /// Initialize a worker structure.
  ///
  /// This initializes a worker.
  ///
  /// @param        sched The scheduler associated with this worker.
  /// @param           id The worker's id.
  Worker(Scheduler& sched, libhpx::Network& network, int id);

  /// Finalize a worker structure.
  ///
  /// This will cleanup any queues and free any stacks and parcels associated with
  /// the worker. This should only be called once *all* of the workers have been
  /// joined so that an _in-flight_ mail message doesn't get missed.
  ~Worker();

  /// Workers need to be aligned on cacheline boundaries.
  static void* operator new(size_t bytes) {
    void* addr;
    if (posix_memalign(&addr, HPX_CACHELINE_SIZE, bytes)) {
      abort();
    }
    return addr;
  }

  static void operator delete(void* worker) {
    free(worker);
  }

  /// Create a scheduler worker thread.
  ///
  /// This starts an underlying system thread for the scheduler. Assuming the
  /// scheduler is stopped the underlying thread will immediately sleep waiting
  /// for a run condition.
  ///
  /// @returns          true if the thread was created successfully
  ///                  false otherwise
  bool create();

  /// Join with a scheduler worker thread.
  ///
  /// This will block waiting for the designated worker thread to exit. It should
  /// be used before calling worker_fini() on this thread in order to avoid race
  /// conditions on the mailbox and scheduling and whatnot.
  ///
  /// @param            w The worker to join (should be active).
  void join();

  void stop();

  void start();

  void shutdown();

  void pushMail(hpx_parcel_t* p) {
    sync_two_lock_queue_enqueue(&inbox, p);
  }

  void pushYield(hpx_parcel_t* p) {
    queues[1 - work_id].push(p);
  }

 public:

  /// Process a mail queue.
  ///
  /// This processes all of the parcels in the mailbox of the worker, moving them
  /// into the work queue of the designated worker. It will return a parcel if
  /// there was one.
  ///
  /// @returns            A parcel from the mailbox if there is one.
  hpx_parcel_t* handleMail();

  hpx_parcel_t* handleEpoch() {
    work_id = 1 - work_id;
    return NULL;
  }

  /// Handle the network.
  ///
  /// This will return a parcel from the network if it finds any. It will also
  /// refill the local work queue.
  ///
  /// @returns            A parcel from the network if there is one.
  hpx_parcel_t* handleNetwork();

  hpx_parcel_t* handleSteal();

  /// Pop the next available parcel from our lifo work queue.
  hpx_parcel_t* popLIFO();

  /// Push a parcel into the lifo queue.
  void pushLIFO(hpx_parcel_t *p);

  using Continuation = std::function<void(hpx_parcel_t*)>;

  void checkpoint(hpx_parcel_t* p, Continuation& f, void *sp);

  static void Checkpoint(hpx_parcel_t* p, Continuation& f, Worker* w, void *sp)
    asm("worker_checkpoint");

  static void ContextSwitch(hpx_parcel_t* p, Continuation& f, Worker* w)
    asm("thread_transfer");

  /// Local wrapper for the thread transfer call.
  ///
  /// This wrapper will reset the signal mask as part of the transfer if it is
  /// necessary, and it will always checkpoint the stack for the thread that we
  /// are transferring away from.
  ///
  /// @param            p The parcel to transfer to.
  /// @param            f The checkpoint continuation.
  void transfer(hpx_parcel_t* p, Continuation& f);

  template <typename Lambda>
  void transfer(hpx_parcel_t* p, Lambda&& l) {
    Continuation f(std::forward<Lambda>(l));
    transfer(p, f);
  }

  /// The non-blocking schedule operation.
  ///
  /// This will schedule new work relatively quickly, in order to avoid delaying
  /// the execution of the user's continuation. If there is no local work we can
  /// find quickly we'll transfer back to the main pthread stack and go through an
  /// extended transfer time.
  ///
  /// @param            f The continuation function.
  void schedule(Continuation& f);

  template <typename Lambda>
  void schedule(Lambda&& l) {
    Continuation f(std::forward<Lambda>(l));
    schedule(f);
  }

  void spawn(hpx_parcel_t* p);

 private:

  /// The main entry point for the worker thread.
  void enter();

  /// The primary schedule loop.
  ///
  /// This will continue to try and schedule lightweight threads while the
  /// worker's state is SCHED_RUN.
  void run();

  /// The sleep loop.
  ///
  /// This will continue to sleep the scheduler until the worker's state is no
  /// longer SCHED_STOP.
  void sleep();

  /// Try to bind a stack to the parcel.
  ///
  /// This uses the worker's stack caching infrastructure to find a stack, or
  /// allocates a new stack if necessary. The newly created thread is runnable,
  /// and can be thread_transfer()ed to in the same way as any other lightweight
  /// thread can be. Calling bind() on a parcel that already has a stack
  /// (i.e., a thread) is permissible and has no effect.
  ///
  /// @param            p The parcel to which we are binding.
  void bind(hpx_parcel_t* p);

 public:
  /// This returns the parcel's stack to the stack cache.
  ///
  /// This will push the parcel's stack onto the local worker's freelist, and
  /// possibly trigger a freelist flush if there are too many parcels cached
  /// locally.
  ///
  /// @param            p The parcel that has the stack that we are freeing.
  void unbind(hpx_parcel_t* p);

 private:
  /// Just used through the pthread interface during create to bounce to the
  /// worker's run member.
  static void* Enter(void *worker) {
    static_cast<Worker*>(worker)->enter();
    return nullptr;
  }

  /// The thread entry function that the worker uses to start a thread.
  ///
  /// This is the function that sits at the outermost stack frame for a
  /// lightweight thread, and deals with dispatching the parcel's action and
  /// handling the action's return value.
  ///
  /// It does not return.
  ///
  /// @param            p The parcel to execute.
  [[ noreturn ]] static void Execute(hpx_parcel_t *p);


 public:
#ifdef ENABLE_INSTRUMENTATION
  void EVENT_THREAD_RUN(struct hpx_parcel *p);
  void EVENT_THREAD_END(struct hpx_parcel *p);
  void EVENT_THREAD_SUSPEND(struct hpx_parcel *p);
  void EVENT_THREAD_RESUME(struct hpx_parcel *p);
#else
  void EVENT_THREAD_RUN(struct hpx_parcel *p) {}
  void EVENT_THREAD_END(struct hpx_parcel *p) {}
  void EVENT_THREAD_SUSPEND(struct hpx_parcel *p) {}
  void EVENT_THREAD_RESUME(struct hpx_parcel *p) {}
#endif

 public:
  pthread_t        thread;                      //!< this worker's native thread
  int                  id;                      //!< this worker's id
  unsigned           seed;                      //!< my random seed
  int          work_first;                      //!< this worker's mode
 private:
  int             nstacks_;                      //!< count of freelisted stacks
 public:
  int             yielded;                      //!< used by APEX
  int              active;                      //!< used by APEX
  int         last_victim;                      //!< last successful victim
  int           numa_node;                      //!< this worker's numa node
  void          *profiler_;                     //!< reference to the profiler
  void               *bst;                      //!< the block statistics table
  libhpx::Network& network_;                      //!< reference to the network
  struct logtable   *logs;                      //!< reference to tracer data
  uint64_t         *stats;                      //!< reference to statistics data
  Scheduler&        sched_;                      //!< pointer to the scheduler
 private:
  hpx_parcel_t    *system_;                      //!< this worker's native parcel
 public:
  hpx_parcel_t   *current;                      //!< current thread
 private:
  struct ustack   *stacks_;                      //!< freelisted stacks
  const char _pad1[_BYTES(HPX_CACHELINE_SIZE,
                          sizeof(pthread_t) +
                          sizeof(int) * 8 +
                          sizeof(void*) * 9)];
 public:
  pthread_mutex_t     lock;                     //!< state lock
  pthread_cond_t   running;                     //!< local condition for sleep
  std::atomic<int>   state;                     //!< what state are we in
  std::atomic<int> work_id;                     //!< which queue are we using
  const char _pad2[_BYTES(HPX_CACHELINE_SIZE,
                          sizeof(pthread_mutex_t) +
                          sizeof(pthread_cond_t) +
                          sizeof(int) * 2)];
  PaddedDequeue    queues[2];                   //!< work and yield queues
  two_lock_queue_t  inbox;                      //!< mail sent to me
};

static_assert((sizeof(Worker) & (HPX_CACHELINE_SIZE - 1)) == 0,
              "Poor alignment for worker structure");

// } // namespace scheduler
// } // namespace libhpx

/// @}

extern __thread Worker * volatile self;


#endif // LIBHPX_WORKER_H
