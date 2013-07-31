#include "mpi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "photon.h"
#include "logging.h"

// All functions exist with one and two underscores for all compilers to be happy

//////// Function prototypes ////////

////////-- Library initialization and finalization functions --////////
void photon_init_(int *np, int *rank, int *comm, int *ret_val);
void photon_init__(int *np, int *rank, int *comm, int *ret_val);
void photon_finalize_(int *ret_val);
void photon_finalize__(int *ret_val);

////////-- Memory registration functions --////////
void photon_register_buffer_(void *buff, int *buff_size, int *ret_val);
void photon_register_buffer__(void *buff, int *buff_size, int *ret_val);
void photon_unregister_buffer_(void *buff, int *buff_size, int *ret_val);
void photon_unregister_buffer__(void *buff, int *buff_size, int *ret_val);

////////-- Two sided send/recv operations build on-top of one-sided RDMA --////////
  // receiver side post recv request (rendezvous start)
void photon_post_recv_buffer_rdma_(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val);
void photon_post_recv_buffer_rdma__(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val);
  // sender side post send request (rendezvous start)
void photon_post_send_buffer_rdma_(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val);
void photon_post_send_buffer_rdma__(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val);
  // sender side post send request (rendezvous pre-start)
void photon_post_send_request_rdma_(int *proc, int *size, int *tag, int *request, int *ret_val);
void photon_post_send_request_rdma__(int *proc, int *size, int *tag, int *request, int *ret_val);
  // sender side receive rendezvous start (blocking)
void photon_wait_recv_buffer_rdma_(int *proc, int *tag, int *ret_val);
void photon_wait_recv_buffer_rdma__(int *proc, int *tag, int *ret_val);
  // sender side receive rendezvous start (blocking)
void photon_wait_send_buffer_rdma_(int *proc, int *tag, int *ret_val);
void photon_wait_send_buffer_rdma__(int *proc, int *tag, int *ret_val);
  // receiver side receive rendezvous pre-start (blocking)
void photon_wait_send_request_rdma_(int *tag, int *proc, int *ret_val);
void photon_wait_send_request_rdma__(int *tag, int *proc, int *ret_val);
  // sender side start rdma transfer
void photon_post_os_put_(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
void photon_post_os_put__(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
  // receiver side start rdma transfer
void photon_post_os_get_(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
void photon_post_os_get__(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
  // sender side rdma write into receiver's send ledger
void photon_send_fin_(int *proc, int *ret_val);
void photon_send_fin__(int *proc, int *ret_val);
  // convenience function that wraps the three steps of sending a buffer
int photon_rdma_send(int proc, char *ptr, int size, int tag, int remote_offset, uint32_t *request); // C interface
void photon_rdma_send_(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
void photon_rdma_send__(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val);
  // convenience *blocking* function that wraps the three steps of sending a buffer and waits for completion
void photon_rdma_bsend_(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *ret_val);
void photon_rdma_bsend__(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *ret_val);

////////-- Pure two sided send/recv operations --////////
//void photon_post_recv_(int *, char *, uint32_t *, int *, int *);
//void photon_post_recv__(int *, char *, uint32_t *, int *, int *);
//void photon_post_send_(int *, char *,  uint32_t *, int *, int *);
//void photon_post_send__(int *, char *, uint32_t *, int *, int *);

////////-- Non-Blocking test functions --////////
void photon_test_(int *request, int *flag, int *type, int *status, int *ret_val);
void photon_test__(int *request, int *flag, int *type, int *status, int *ret_val);

////////-- Blocking wait functions --////////
void photon_wait_(int *request, int *ret_val);
void photon_wait__(int *request, int *ret_val);
void photon_waitall_(int *count, int *requestVec, int *ret_val);
void photon_waitall__(int *count, int *requestVec, int *ret_val);
void photon_wait_any_(int *proc, uint32_t *req, int *ret_val);
void photon_wait_any__(int *proc, uint32_t *req, int *ret_val);
//void photon_wait_remaining_(int *ret_val);
//void photon_wait_remaining__(int *ret_val);

////////-- Convenience functions --////////
void photon_gettime_(double *time);
void photon_gettime__(double *time);
void photon_set_debug_(int *boolean);
void photon_set_debug__(int *boolean);

// This macro generate code that satisfies any
// fortran linker by making both _ and __
// versions of the same function.
#define FORTRAN(ret_type,name,args_and_code) \
  ret_type name##_ args_and_code \
  ret_type name##__ args_and_code
// Note: when you use the FORTRAN macro you cannot
// declare variables like this:
//     uint64_t offset, size;
// because the preprocessor will think the ,
// separates macro arguments. You have to write
//     uint64_t offset;
//     uint64_t size;
// instead.

//////// Global variables ////////
#ifdef DEBUG
int _photon_start_debugging=1;
#endif
#if defined(DEBUG) || defined(CALLTRACE)
FILE *_phot_ofp;
extern int _photon_nproc, _photon_myrank;
#endif

//////// Functions ////////

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_init,(int *np, int *rank, int *comm, int *ret_val) {
	*ret_val = photon_init(*np, *rank, *((MPI_Comm *)comm));
	return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_finalize,(int *ret_val){
   *ret_val = photon_finalize();
})


///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_register_buffer,(void *sbuff, int *sbuff_size, int *ret_val){
	*ret_val = photon_register_buffer(sbuff, *sbuff_size);
	return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_unregister_buffer,(void *sbuff, int *size, int *ret_val){
	*ret_val = photon_unregister_buffer(sbuff, *size);
	return;
})


///////////////////////////////////////////////////////////////////////////////
//////  Two sided send/recv operations build on-top of one-sided RDMA  ////////

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_post_recv_buffer_rdma,(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val) {
        *ret_val = photon_post_recv_buffer_rdma(*proc, ptr, (uint32_t)*size, *tag, (uint32_t *)request);
        return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_post_send_buffer_rdma,(int *proc, char *ptr, int *size, int *tag, int *request, int *ret_val) {
        *ret_val = photon_post_send_buffer_rdma(*proc, ptr, (uint32_t)*size, *tag, (uint32_t *)request);
        return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_post_send_request_rdma,(int *proc, int *size, int *tag, int *request, int *ret_val) {
        *ret_val = photon_post_send_request_rdma(*proc, (uint32_t)*size, *tag, (uint32_t *)request);
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_wait_recv_buffer_rdma,(int *proc, int *tag, int *ret_val) {
        *ret_val = photon_wait_recv_buffer_rdma(*proc, *tag);
        return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_wait_send_buffer_rdma,(int *proc, int *tag, int *ret_val) {
        *ret_val = photon_wait_send_buffer_rdma(*proc, *tag);
        return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_wait_send_request_rdma,(int *tag, int *proc, int *ret_val) {
    int tmp;
	tmp = photon_wait_send_request_rdma(*tag);
    if( tmp < 0 ){
	    *ret_val = tmp;
    }else{
	    *proc = tmp;
	    *ret_val = 0;
    }
    return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_post_os_put,(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val) {
        *ret_val = photon_post_os_put(*proc, ptr, (uint32_t)*size, *tag, (uint32_t)*remote_offset, (uint32_t *)request);
	return;
})

///////////////////////////////////////////////////////////////////////////////

FORTRAN(void,photon_post_os_get,(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val) {
        *ret_val = photon_post_os_get(*proc, ptr, (uint32_t)*size, *tag, (uint32_t)*remote_offset, (uint32_t *)request);
	return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void, photon_send_fin,(int *proc, int *ret_val){
	*ret_val = photon_send_FIN(*proc);
	return;
})

//// C Interface
int photon_rdma_send(int proc, char *ptr, int size, int tag, int remote_offset, uint32_t *request) {
    int err=0;

    err = photon_wait_recv_buffer_rdma(proc, tag);
    if( err ) goto end;
    err = photon_post_os_put(proc, ptr, (uint32_t)size, tag, (uint32_t)remote_offset, (uint32_t *)request);
    if( err ) goto end;
    err = photon_send_FIN(proc);

end:
    return err;
}

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_rdma_send,(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *request, int *ret_val) {
    int err=0;

    err = photon_wait_recv_buffer_rdma(*proc, *tag);
    if( err ) goto end;
    err = photon_post_os_put(*proc, ptr, (uint32_t)*size, *tag, (uint32_t)*remote_offset, (uint32_t *)request);
    if( err ) goto end;
    err = photon_send_FIN(*proc);

end:
    *ret_val = err;
    return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_rdma_bsend,(int *proc, char *ptr, int *size, int *tag, int *remote_offset, int *ret_val) {
    int err=0;
    uint32_t request=0;

    err = photon_wait_recv_buffer_rdma(*proc, *tag);
    if( err ) goto end;
    err = photon_post_os_put(*proc, ptr, (uint32_t)*size, *tag, (uint32_t)*remote_offset, &request);
    if( err ) goto end;
    err = photon_send_FIN(*proc);
    if( err ) goto end;
    err = photon_wait(request);

end:
    *ret_val = err;
    return;
})


///////////////////////////////////////////////////////////////////////////////
//////  Pure two sided send/recv operations  //////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//FORTRAN(void,photon_post_recv,(int *proc, char *rbuff, uint32_t *size, int *request, int *ret_val){
//	*ret_val = photon_post_recv(*proc, rbuff, *size, (uint32_t *)request);
//	return;
//})

///////////////////////////////////////////////////////////////////////////////
//FORTRAN(void,photon_post_send,(int *proc, char *sbuff, uint32_t *size, int *request, int *ret_val){
//	*ret_val = photon_post_send(*proc, sbuff, *size, (uint32_t *)request);
//	return;
//})


///////////////////////////////////////////////////////////////////////////////
//////  Non-Blocking test functions  //////////////////////////////////////////

FORTRAN(void,photon_test,(int *request, int *flag, int *type, int *status, int *ret_val){
    *ret_val = photon_test((uint32_t)*request, flag, type, (MPI_Status *)status);
    return;
})


///////////////////////////////////////////////////////////////////////////////
//////  Blocking wait functions  //////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_waitall,(int *count, int *requestVec, int *ret_val) {
    int cnt=*count;
    int pending=*count;
    char *bitMap;

    bitMap = (char *)malloc(cnt*sizeof(char));
    bitMap = memset(bitMap, 0, cnt*sizeof(char));

    while( pending > 0 ){
        int i;
        for(i=0; i<cnt; i++){
            int flag;
            int type;
            int ret;
            MPI_Status status;

            if( bitMap[i] ){ continue; }
            ret = photon_test(requestVec[i], &flag, &type, &status);
            if( ret < 0 ){
                *ret_val = ret;
                return;
            }else if( !ret && (flag == 1) ){
                --pending;
                bitMap[i] = 1;
            }

        }
    }

    free(bitMap);
    *ret_val = 0;
	return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_wait,(int *request, int *ret_val){
	*ret_val = photon_wait((uint32_t) *request);
	return;
})

///////////////////////////////////////////////////////////////////////////////
/*
FORTRAN(void,photon_wait_remaining,(int *ret_val) {
	*ret_val = photon_wait_remaining();
	return;
})
*/

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_wait_any,(int *proc, uint32_t *req, int *ret_val) {
	*ret_val = photon_wait_any(proc, req);
	return;
})

///////////////////////////////////////////////////////////////////////////////
//////   Convenience Functions  ///////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_gettime,(double *s){
    struct timeval tp;

    gettimeofday(&tp, NULL);
    *s = ((double)tp.tv_sec) + ( ((double)tp.tv_usec) / 1000000.0);
    return;
})

///////////////////////////////////////////////////////////////////////////////
FORTRAN(void,photon_set_debug,(int *bln){
#ifdef DEBUG
    _photon_start_debugging=*bln;
#endif
    return;
})


