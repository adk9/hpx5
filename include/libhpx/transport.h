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
#ifndef LIBHPX_TRANSPORT_H
#define LIBHPX_TRANSPORT_H

#include "hpx/hpx.h"

#define TRANSPORT_ANY_SOURCE -1

typedef struct transport_class transport_class_t;

struct transport_class {
  hpx_transport_t type;
  void (*barrier)(void);

  int (*request_size)(void);

  int (*request_cancel)(void *request);

  int (*adjust_size)(int size);

  uint32_t (*get_send_limit)(void);
  uint32_t (*get_recv_limit)(void);

  const char *(*id)(void)
    HPX_RETURNS_NON_NULL;

  void (*delete)(transport_class_t*)
    HPX_NON_NULL(1);

  int (*pin)(transport_class_t*, const void* buffer, size_t len)
    HPX_NON_NULL(1);

  void (*unpin)(transport_class_t*, const void* buffer, size_t len)
    HPX_NON_NULL(1);

  // We can assume the transports have a mechanism to acquire the buffer
  // metadata needed for a put/get operation.
  int (*put)(transport_class_t*, int dest, const void *buffer, size_t size,
             void *rbuffer, size_t rsize, void *rid, void *request)
    HPX_NON_NULL(1, 3);

  int (*get)(transport_class_t*, int dest, void *buffer, size_t size,
             const void *rbuffer, size_t rsize, void *rid, void *request)
    HPX_NON_NULL(1, 3);

  int (*send)(transport_class_t*, int dest, const void *buffer, size_t size,
              void *request)
    HPX_NON_NULL(1, 3);

  size_t (*probe)(transport_class_t *t, int *src)
    HPX_NON_NULL(1, 2);

  int (*recv)(transport_class_t *t, int src, void *buffer, size_t size, void *request)
    HPX_NON_NULL(1, 3);

  int (*test)(transport_class_t *t, void *request, int *out)
    HPX_NON_NULL(1, 2, 3);

  /// Tests the array of requests to see if they are complete.
  ///
  /// Will output the indices of the elements that are complete in the array,
  /// and output the number of completed requests in the output array. The
  /// output array is dense.
  ///
  /// @param[in]         t - the transport to test
  /// @param[in]         n - the number of requests in @p requests
  /// @param[in]  requests - an @p n-element array of requests
  /// @param[out] complete - an @p n-element array of integers
  /// @returns             - the number of valid entries in complete
  int (*testsome)(transport_class_t *t, int n, char requests[], int complete[])
    HPX_NON_NULL(1, 3, 4);

  void (*progress)(transport_class_t *t, bool flush)
    HPX_NON_NULL(1);
};


HPX_INTERNAL transport_class_t *transport_new_photon(void);
HPX_INTERNAL transport_class_t *transport_new_mpi(void);
HPX_INTERNAL transport_class_t *transport_new_portals(void);
HPX_INTERNAL transport_class_t *transport_new_smp(void);
HPX_INTERNAL transport_class_t *transport_new(hpx_transport_t transport);


inline static const char *
transport_id(transport_class_t *t) {
  return t->id();
}


inline static void
transport_delete(transport_class_t *t) {
  t->delete(t);
}

inline static hpx_transport_t transport_type(transport_class_t *t) {
  return t->type;
}

inline static int
transport_request_size(const transport_class_t *t) {
  return t->request_size();
}


inline static int
transport_request_cancel(const transport_class_t *t, void *request) {
  return t->request_cancel(request);
}


inline static int
transport_pin(transport_class_t *t, const void *buffer, size_t len) {
  return t->pin(t, buffer, len);
}


inline static void
transport_unpin(transport_class_t *t, const void *buffer, size_t len) {
  t->unpin(t, buffer, len);
}


inline static int
transport_send(transport_class_t *t, int dest, const void *buffer, size_t size,
               void *request) {
  return t->send(t, dest, buffer, size, request);
}


inline static size_t
transport_probe(transport_class_t *t, int *src) {
  return t->probe(t, src);
}


inline static int
transport_recv(transport_class_t *t, int src, void *buffer, size_t n, void *r) {
  return t->recv(t, src, buffer, n, r);
}


inline static void
transport_progress(transport_class_t *t, bool flush) {
  t->progress(t, flush);
}


inline static int
transport_test(transport_class_t *t, void *request, int *out) {
  return t->test(t, request, out);
}


inline static int
transport_testsome(transport_class_t *t, int n, char requests[], int complete[])
{
  return t->testsome(t, n, requests, complete);
}


inline static int
transport_adjust_size(transport_class_t *t, int size) {
  return t->adjust_size(size);
}


inline static void
transport_barrier(transport_class_t *t) {
  t->barrier();
}


#endif // LIBHPX_TRANSPORT_H
