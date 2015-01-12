#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>

#if defined(linux)
#define HAVE_SETAFFINITY
#include <sched.h>
#endif

#ifndef CPU_SETSIZE
#undef HAVE_SETAFFINITY
#endif

#include "photon.h"
#include "test_cfg.h"

#define PHOTON_BUF_SIZE (1024*64) // 64k
#define PHOTON_TAG UINT32_MAX
#define SQ_SIZE 100

static int ITERS = 10000;

static int sizes[] = {
  0,
  1,
  8,
  64,
  128,
  192,
  256,
  2048,
  4096,
  8192,
  12288,
  16384
};

static int DONE = 0;
static int *recvCompT;
static int myrank;
static sem_t sem;

#ifdef HAVE_SETAFFINITY
static int ncores = 1;                 /* number of CPU cores */
static cpu_set_t cpu_set;              /* processor CPU set */
static cpu_set_t def_set;
#endif

void *test_thread() {
  photon_rid request;
  int flag, rc;
  
  do {
    rc = photon_wait_any(&flag, &request);
    if (rc != PHOTON_OK) {
      fprintf(stderr, "Error in photon_wait_any\n");
      exit(1);
    }
  } while (!DONE);

  pthread_exit(NULL);
}

// Have one thread poll local completion only, PROTON_PROBE_EVQ
void *wait_local_completion_thread() {
  photon_rid request;
  int flag, rc;

  do {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_EVQ);
    if (rc < 0) {
      exit(1);
    }
    if ((flag > 0) && (request == PHOTON_TAG)) {
      // Increments the counter  
      sem_post(&sem);
    }
  } while (!DONE);
  
  pthread_exit(NULL);
}

// Have multiple threads each polling for ledger completions (i.e., revs) 
// PHOTON_PROBE_LEDGER from a given rank instead of any source
void *wait_ledger_completions_thread(void *arg) {
  photon_rid request;
  long inputrank = (long)arg;
  int flag;
  
  do {
    //photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_LEDGER);
    photon_probe_completion(inputrank, &flag, &request, PHOTON_PROBE_LEDGER);
    if (flag && request == 0xcafebabe)
      recvCompT[inputrank]++;
  } while (!DONE);
  
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  int i, j, k, ns, val;
  int rank, nproc;
  int aff_main, aff_evq, aff_ledg;
  long t;

  MPI_Init(&argc,&argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  myrank = rank;

  cfg.nproc = nproc;
  cfg.address = rank;

  photon_init(&cfg);

  aff_main = aff_evq = aff_ledg = -1;
  if (argc > 1)
    aff_main = atoi(argv[1]);
  if (argc > 2)
    aff_evq = atoi(argv[2]);
  if (argc > 3)
    aff_ledg = atoi(argv[3]);

  struct photon_buffer_t rbuf[nproc];
  photon_rid recvReq[nproc], sendReq[nproc];
  char *send, *recv[nproc];
  pthread_t th, th2, recv_threads[nproc];

  recvCompT = calloc(nproc, sizeof(int));

  // only need one send buffer
  //posix_memalign((void **) &send, 8, PHOTON_BUF_SIZE*sizeof(uint8_t));
  send = malloc(PHOTON_BUF_SIZE);
  memset(send, 1, PHOTON_BUF_SIZE);
  photon_register_buffer(send, PHOTON_BUF_SIZE);

  // ... but recv buffers for each potential sender
  for (i=0; i<nproc; i++) {
    //posix_memalign((void **) &recv[i], 8, PHOTON_BUF_SIZE*sizeof(uint8_t));
    recv[i] = malloc(PHOTON_BUF_SIZE);
    photon_register_buffer(recv[i], PHOTON_BUF_SIZE);
  }

  for (i=0; i<nproc; i++) {
    // everyone posts their recv buffers
    photon_post_recv_buffer_rdma(i, recv[i], PHOTON_BUF_SIZE, PHOTON_TAG, &recvReq[i]);
  }

  for (i=0; i<nproc; i++) {
    // wait for a recv buffer that was posted
    photon_wait_recv_buffer_rdma(i, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq[i]);
    // get the remote buffer info so we can do our own put
    photon_get_buffer_remote(sendReq[i], &rbuf[i]);
    photon_send_FIN(sendReq[i], i, PHOTON_REQ_COMPLETED);
    photon_wait(recvReq[i]);
  }

  sem_init(&sem, 0, SQ_SIZE);

  // Create a thread to wait for local completions 
  pthread_create(&th, NULL, wait_local_completion_thread, NULL);

  // Create a thread that simultaneously tests for a rendezvous completion
  //pthread_create(&th2, NULL, test_thread, NULL);
  
  // Create receive threads one per rank
  for (t=0; t<nproc; t++) {
    pthread_create(&recv_threads[t], NULL, wait_ledger_completions_thread, (void*)t);
  }
  
  // set affinity as requested
#ifdef HAVE_SETAFFINITY
  if ((ncores = sysconf(_SC_NPROCESSORS_CONF)) <= 0)
    err(1, "sysconf: couldn't get _SC_NPROCESSORS_CONF");
  CPU_ZERO(&def_set);
  for (i=0; i<ncores; i++)
    CPU_SET(i, &def_set);
  if (aff_main >= 0) {
    //printf("Setting main thread affinity to core %d\n", aff_main);
    CPU_ZERO(&cpu_set);
    CPU_SET(aff_main, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0)
      err(1, "couldn't change CPU affinity");
  }
  else
    sched_setaffinity(0, sizeof(cpu_set_t), &def_set);
  if (aff_evq >= 0) {
    //printf("Setting EVQ probe thread affinity to core %d\n", aff_evq);
    CPU_ZERO(&cpu_set);
    CPU_SET(aff_evq, &cpu_set);
    pthread_setaffinity_np(th, sizeof(cpu_set_t), &cpu_set);
  }
  else
    sched_setaffinity(th, sizeof(cpu_set_t), &def_set);
  if (aff_evq >= 0) {
    //printf("Setting LEDGER probe thread affinity to core %d\n", aff_ledg);
    CPU_ZERO(&cpu_set);
    CPU_SET(aff_ledg, &cpu_set);
    pthread_setaffinity_np(recv_threads[0], sizeof(cpu_set_t), &cpu_set);
  }
  else
    sched_setaffinity(recv_threads[0], sizeof(cpu_set_t), &def_set);
#endif
  
  // now we can proceed with our benchmark
  if (rank == 0)
    printf("%-7s%-9s%-7s%-11s%-12s\n", "Ranks", "Senders", "Bytes", "Sync PUT", "Sync GET");

  struct timespec time_s, time_e;
  for (ns = 0; ns < nproc; ns++) {
    for (i=0; i<sizeof(sizes)/sizeof(sizes[0]); i++) {
      if (rank == 0) {
        printf("%-7d", nproc);
        printf("%-9u", ns + 1);
        printf("%-7u", sizes[i]);
        fflush(stdout);
      }
      
      j = rand() % nproc;
      // send to random rank, excluding self
      while (j == rank)
        j = rand() % nproc;

      // PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<ITERS; k++) {
	  if (sem_wait(&sem) == 0) {
	    int rc;
	    rc = photon_put_with_completion(j, send, sizes[i], (void*)rbuf[j].addr, rbuf[j].priv, PHOTON_TAG, 0xcafebabe, 0);
	    if (rc == PHOTON_ERROR) {
	      fprintf(stderr, "Error doing PWC\n");
	      exit(1);
	    }
	  }
	}
      }
      
      // clear remaining local completions
      do {
	if (sem_getvalue(&sem, &val)) continue;
      } while (val < SQ_SIZE);

      clock_gettime(CLOCK_MONOTONIC, &time_e);

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f     ", latency);
        fflush(stdout);
      }

      if (rank <= ns) {
        if (i && !(sizes[i] % 8)) {
          clock_gettime(CLOCK_MONOTONIC, &time_s);
          for (k=0; k<ITERS; k++) {
              if (sem_wait(&sem) == 0) {
                if (photon_get_with_completion(j, send, sizes[i], (void*)rbuf[j].addr, rbuf[j].priv, PHOTON_TAG, 0)) {
		  fprintf(stderr, "Error doing GWC\n");
		  exit(1);
		}
              }
          }
        }
      }
      
      // clear remaining local completions
      do {
	if (sem_getvalue(&sem, &val)) continue;
      } while (val < SQ_SIZE);
      
      clock_gettime(CLOCK_MONOTONIC, &time_e);

      MPI_Barrier(MPI_COMM_WORLD);
      
      if (rank == 0 && i && !(sizes[i] % 8)) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f\n", latency);
        fflush(stdout);
      }
      else if (rank == 0) {
        printf("N/A\n");
        fflush(stdout);
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  DONE = 1;
  // Wait for all threads to complete
  pthread_join(th, NULL);

  for (i=0; i<nproc; i++) {
    printf("%d received %d\n", i, recvCompT[i]);
  }
  
  // Clean up and destroy the mutex
  photon_unregister_buffer(send, PHOTON_BUF_SIZE);
  free(send);

  for (i=0; i<nproc; i++) {
    photon_unregister_buffer(recv[i], PHOTON_BUF_SIZE);
    free(recv[i]);
  }

  photon_finalize();
  MPI_Finalize();

  return 0;
}

