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

#ifndef LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H
#define LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H

#include <mpi.h>
#include <exception>
#include <cassert>
#include <cstddef>

namespace libhpx {
namespace network {
namespace isir {
class MPITransport {
 public:
  class Request {
   public:
    Request() : request_(MPI_REQUEST_NULL) {
    }

    void reset() {
      request_ = MPI_REQUEST_NULL;
    }

    bool cancel() {
      if (request_ == MPI_REQUEST_NULL) {
        return true;
      }

      int cancelled;
      MPI_Status status;
      MPITransport::Check(MPI_Cancel(&request_));
      MPITransport::Check(MPI_Wait(&request_, &status));
      MPITransport::Check(MPI_Test_cancelled(&status, &cancelled));
      request_ = MPI_REQUEST_NULL;
      return cancelled;
    }

   private:
    friend class MPITransport;
    MPI_Request request_;
  };

  class Status {
   public:
    int tag() const {
      return status_.MPI_TAG;
    }

    int source() const {
      return status_.MPI_SOURCE;
    }

    int bytes() const {
      int bytes;
      MPITransport::Check(MPI_Get_count(&status_, MPI_BYTE, &bytes));
      return bytes;
    }

   private:
    friend class MPITransport;
    MPI_Status status_;
  };

  MPITransport() : world_(MPI_COMM_NULL), finalize_(false) {
    int initialized;
    Check(MPI_Initialized(&initialized));
    if (!initialized) {
      int level;
      Check(MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &level));
      assert(level >= MPI_THREAD_SERIALIZED);
      finalize_ = true;
    }
    Check(MPI_Comm_dup(MPI_COMM_WORLD, &world_));
  }

  ~MPITransport() {
    Check(MPI_Comm_free(&world_));
    if (finalize_) {
      MPI_Finalize();
    }
  }

  int iprobe() {
    Status s;
    int flag;
    Check(MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, world_, &flag, &s.status_));
    return (flag) ? s.tag() : -1;
  }

  Request isend(int to, const void *from, size_t n, int tag) {
    Request r;
    Check(MPI_Isend(from, n, MPI_BYTE, to, tag, world_, &r.request_));
    return r;
  }

  Request irecv(void *to, size_t n, int tag) {
    Request r;
    Check(MPI_Irecv(to, n, MPI_BYTE, MPI_ANY_SOURCE, tag, world_, &r.request_));
    return r;
  }

  static int Testsome(int n, Request* reqs, int* out) {
    if (!n) return 0;
    int ncomplete;
    MPI_Request *requests = reinterpret_cast<MPI_Request*>(reqs);
    Check(MPI_Testsome(n, requests, &ncomplete, out, MPI_STATUS_IGNORE));
    if (ncomplete == MPI_UNDEFINED)  {
      throw std::exception();
    }
    return ncomplete;
  }

  static int Testsome(int n, Request* reqs, int* out, Status* stats) {
    if (!n) {
      return 0;
    }

    int ncomplete;
    MPI_Request *requests = reinterpret_cast<MPI_Request*>(reqs);
    MPI_Status  *statuses = reinterpret_cast<MPI_Status*>(stats);
    Check(MPI_Testsome(n, requests, &ncomplete, out, statuses));
    if (ncomplete == MPI_UNDEFINED)  {
      throw std::exception();
    }
    return ncomplete;
  }

  void *comm() {
    return &world_;
  }

 private:
  static void Check(int e) {
    if (e != MPI_SUCCESS) {
      throw std::exception();
    }
  }

 private:
  MPI_Comm world_;
  bool  finalize_;
};

} // namespace isir
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H
