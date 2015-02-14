// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <unistd.h>
#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libsync/barriers.h"
#include "tests.h"

static hpx_action_t _typed_task1;
static int _typed_task1_action(int i, float f, char c) {
  printf("Typed task 1 %d %f %c!\n", i, f, c);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(TASK, _typed_task1_action, _typed_task1,
                      HPX_INT, HPX_FLOAT, HPX_CHAR);

static hpx_action_t _typed_task2;
static int _typed_task2_action(int i, float f, char c) {
  printf("Typed task 2 %d %f %c!\n", i, f, c);
  sleep(1);
  return HPX_SUCCESS;
}
static HPX_ACTION_DEF(TASK, _typed_task2_action, _typed_task2,
                      HPX_INT, HPX_FLOAT, HPX_CHAR);

static HPX_ACTION(test_libhpx_task, void *UNUSED) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  printf("Test hpx typed task\n");
  hpx_call_sync(HPX_HERE, _typed_task1, NULL, 0, &i, &f, &c);
  hpx_call_sync(HPX_HERE, _typed_task2, NULL, 0, &i, &f, &c);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_task2, void *UNUSED) {
  int i = 42;
  float f = 1.0;
  char c = 'a';

  printf("Test hpx typed task 2\n");
  hpx_addr_t and = hpx_lco_and_new(2);
  hpx_call(HPX_HERE, _typed_task1, and, &i, &f, &c);
  hpx_call(HPX_HERE, _typed_task2, and, &i, &f, &c);
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
}


static barrier_t *barrier = NULL;
static volatile int n = 0;
static char * volatile task_sp = NULL;

static HPX_TASK(_test_task, int *id) {
  char local;

  printf("thread %d running the subtask %d on stack %p\n", HPX_THREAD_ID, *id,
         &local);
  fflush(stdout);

  // Record my stack address so that we can verify that an eager transfer really
  // did take place.
  task_sp = &local;

  // Let everyone else make progress---one of them should start running my
  // parent, if it's been exposed to the world. Otherwise, no one else will
  // run.
  sync_barrier_join(barrier, *id);

  // Wait for a while. This give the rest of the threads plenty of time to steal
  // my parent if they're going to.
  sleep(1);

  // Bump the counter.
  sync_store(&n, 1, SYNC_RELEASE);

  // At this point my parent was stolen.
  return HPX_SUCCESS;
}

static HPX_ACTION(_test_action, int *id) {
  char local;

  if (sync_barrier_join(barrier, *id)) {
    // I win the race.
    printf("thread %d running action %d on stack %p\n", HPX_THREAD_ID, *id,
           &local);

    // This will push the task onto my queue, then I have to induce myself to
    // transfer to it---everyone else is blocked, so all I have to do is call
    // yield, which should do the transfer on the same stack, and make this
    // thread available to whoever wakes up.
    //
    // Note that the _test_task task actually releases the lock here, this
    // prevents anyone from stealing the parent thread (or getting it from the
    // yield queue) until I have already transferred to the child.
    hpx_call(HPX_HERE, _test_task, HPX_NULL, id, sizeof(*id));
    hpx_thread_yield();
    printf("action %d stolen by %d\n", *id, HPX_THREAD_ID);

    // Now, this thread should have been "stolen" or taken from the yield queue
    // or whatnot. We expect that we're running concurrent with, and on the same
    // stack, as the _test_task. Verify that we're on the same stack.
    ptrdiff_t d = &local - task_sp;

    if (0 < d && d < 1000) {
      // We're on the same stack---for this to be safe, the _test_task MUST have
      // already run, which implies that the value for n must be 1.
      int v = sync_load(&n, SYNC_ACQUIRE);
      printf("stack difference is %ld, value is %d\n", d, v);
      assert(v == 1 && "work-first task test failed\n");
    }
    else {
      printf("test indeterminate, task spawned with new stack, d=%ld\n", d);
    }

    printf("work-first task test success\n");
  }
  else {
    // I lost the race, wait for the entire thing to be set up before
    // returning and becoming a "stealer".
    sync_barrier_join(barrier, *id);
  }
  printf("finishing %d\n", *id);
  return HPX_SUCCESS;
}

static HPX_ACTION(_test_try_task, void *UNUSED) {
  barrier = sr_barrier_new(HPX_THREADS);
  assert(barrier);
  hpx_addr_t and = hpx_lco_and_new(HPX_THREADS);
  assert(and);
  for (int i = 0; i < HPX_THREADS; ++i) {
    hpx_call(HPX_HERE, _test_action, and, &i, sizeof(i));
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  sync_barrier_delete(barrier);
  return HPX_SUCCESS;
}

TEST_MAIN({
 ADD_TEST(_test_try_task);
});
