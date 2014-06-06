#include "lulesh-hpx.h"

int _SBN1_result_action(Nodal *nodal) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int srcLocalIdx = nodal->srcLocalIdx;
  double *src = nodal->buf;

  recv_t unpack = RECEIVER[srcLocalIdx];

  int nx = ld->sizeX + 1;
  int ny = ld->sizeY + 1;
  int nz = ld->sizeZ + 1;

  hpx_lco_sema_p(ld->sem_sbn1);

  unpack(nx, ny, nz, src, ld->nodalMass, 0);

  hpx_lco_sema_v(ld->sem_sbn1, HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _SBN1_sends_action(pSBN *psbn)
{
  Domain *domain;
  domain = psbn->domain;
  hpx_addr_t local = hpx_thread_current_target();
  int destLocalIdx = psbn->destLocalIdx;
  hpx_addr_t done = psbn->done;
  int rank = psbn->rank;

  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(Nodal) +
                                         BUFSZ[destLocalIdx]);
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  Nodal *nodal = hpx_parcel_get_data(p);

  send_t pack = SENDER[destLocalIdx];

  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  pack(nx, ny, nz, domain->nodalMass, nodal->buf);

  // the neighbor this is being sent to
  int srcRemoteIdx = destLocalIdx;
  int srcLocalIdx = 25 - srcRemoteIdx;
  int to_rank = rank - OFFSET[srcLocalIdx];
  //int distance = to_rank - domain->rank;
  int distance = -OFFSET[srcLocalIdx];
  hpx_addr_t neighbor = hpx_addr_add(local, sizeof(Domain) * distance);

  nodal->srcLocalIdx = srcLocalIdx;

  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _SBN1_result);
  hpx_parcel_set_cont(p, done);
  hpx_parcel_send(p, HPX_NULL);
  return HPX_SUCCESS;
}

void SBN1(hpx_addr_t local, Domain *domain, int index)
{
  int i;
  int rank = index;

  // protect the domain
  //hpx_lco_sema_p(domain->sem_sbn1);

  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];

  // for completing the entire loop of _SBN1_result_action operations
  hpx_addr_t done = hpx_lco_and_new(nsTF);

  // for completing the parallel _SBN1_sends operations, so that we don't
  // release the lock too early
  hpx_addr_t sends = hpx_lco_and_new(nsTF);

  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);
    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _SBN1_sends);
    hpx_parcel_set_cont(p, sends);

    pSBN *psbn = hpx_parcel_get_data(p);
    psbn->domain = domain;
    psbn->destLocalIdx = destLocalIdx;
    psbn->done = done;
    psbn->rank = rank;

    // async is fine, since we're waiting on sends below
    hpx_parcel_send(p, HPX_NULL);
  }

  // Make sure the parallel spawn loop above is done so that we can release the
  // domain lock.
  hpx_lco_wait(sends);
  hpx_lco_delete(sends, HPX_NULL);

  // release the domain lock here, so we don't get deadlock when a
  // _SBN1_result_action tries to acquire it
  hpx_lco_sema_v(domain->sem_sbn1, HPX_NULL);

  // wait for all of the _SBN1_result_action to complete
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // get ready for the next generation
  hpx_lco_sema_p(domain->sem_sbn1);
}

int _SBN3_result_action(Nodal *nodal) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int srcLocalIdx = nodal->srcLocalIdx;
  double *src = nodal->buf;

  int recvcnt = XFERCNT[srcLocalIdx]; 
  recv_t unpack = RECEIVER[srcLocalIdx];

  int nx = ld->sizeX + 1;
  int ny = ld->sizeY + 1;
  int nz = ld->sizeZ + 1;

  hpx_lco_sema_p(ld->sem_sbn3);

  unpack(nx, ny, nz, src, ld->fx, 0); 
  unpack(nx, ny, nz, src + recvcnt, ld->fy, 0);
  unpack(nx, ny, nz, src + recvcnt*2, ld->fz, 0); 

  hpx_lco_sema_v(ld->sem_sbn3, HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _SBN3_sends_action(pSBN *psbn)
{
  Domain *domain;
  domain = psbn->domain;
  hpx_addr_t local = hpx_thread_current_target();
  int destLocalIdx = psbn->destLocalIdx;
  hpx_addr_t done = psbn->done;
  int rank = psbn->rank;

  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(Nodal) +
                                         BUFSZ[destLocalIdx]);
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  Nodal *nodal = hpx_parcel_get_data(p);

  send_t pack = SENDER[destLocalIdx];

  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int sendcnt = XFERCNT[destLocalIdx];
  pack(nx, ny, nz, domain->fx, nodal->buf);
  pack(nx, ny, nz, domain->fy, nodal->buf + sendcnt);
  pack(nx, ny, nz, domain->fz, nodal->buf + sendcnt*2);

  // the neighbor this is being sent to
  int srcRemoteIdx = destLocalIdx;
  int srcLocalIdx = 25 - srcRemoteIdx;
  int to_rank = rank - OFFSET[srcLocalIdx];
  //int distance = to_rank - domain->rank;
  int distance = -OFFSET[srcLocalIdx];
  hpx_addr_t neighbor = hpx_addr_add(local, sizeof(Domain) * distance);

  nodal->srcLocalIdx = srcLocalIdx;

  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _SBN3_result);
  hpx_parcel_set_cont(p, done);
  hpx_parcel_send(p, HPX_NULL);
  return HPX_SUCCESS;
}

void SBN3(hpx_addr_t local,Domain *domain,int rank)
{
  int i;

  // protect the domain
  //hpx_lco_sema_p(domain->sem_sbn3);

  // pack outgoing data
  int nsTF = domain->sendTF[0]; 
  int *sendTF = &domain->sendTF[1]; 

  // for completing the entire loop of _updateSBN3_action operations
  hpx_addr_t done = hpx_lco_and_new(nsTF);

  // for completing the parallel _SBN3_sends operations, so that we don't
  // release the lock too early
  hpx_addr_t sends = hpx_lco_and_new(nsTF);

  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);
    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _SBN3_sends);
    hpx_parcel_set_cont(p, sends);

    pSBN *psbn = hpx_parcel_get_data(p);
    psbn->domain = domain;
    psbn->destLocalIdx = destLocalIdx;
    psbn->done = done;
    psbn->rank = rank;

    // async is fine, since we're waiting on sends below
    hpx_parcel_send(p, HPX_NULL);
  }

  // Make sure the parallel spawn loop above is done so that we can release the
  // domain lock.
  hpx_lco_wait(sends);
  hpx_lco_delete(sends, HPX_NULL);

  // release the domain lock here, so we don't get deadlock when a
  // _SBN3_result_action tries to acquire it
  hpx_lco_sema_v(domain->sem_sbn3, HPX_NULL);

  // wait for all of the _SBN3_result_action to complete
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // get ready for the next generation
  hpx_lco_sema_p(domain->sem_sbn3);
}
#if 0
void PosVel(int rank)
{
  // doRecv = F, doSend = F, planeOnly = F, data = x, y, z, xd, yd, zd
  Domain *domain = &DOMAINS[rank]; 
  int nx = domain->sizeX + 1; 
  int ny = domain->sizeY + 1; 
  int nz = domain->sizeZ + 1; 
  int tag = domain->cycle; 
  int i; 

  // pack outgoing data
  int nsFF = domain->sendFF[0];
  int *sendFF = &domain->sendFF[1]; 

  for (i = 0; i < nsFF; i++) {
    int destLocalIdx = sendFF[i]; 
    int sendcnt = XFERCNT[destLocalIdx]; 
    double *data = hpx_alloc(BUFSZ[destLocalIdx]);
    send_t pack = SENDER[destLocalIdx]; 

    pack(nx, ny, nz, domain->x, data);
    pack(nx, ny, nz, domain->y, data + sendcnt); 
    pack(nx, ny, nz, domain->z, data + sendcnt*2);
    pack(nx, ny, nz, domain->xd, data + sendcnt*3);
    pack(nx, ny, nz, domain->yd, data + sendcnt*4);
    pack(nx, ny, nz, domain->zd, data + sendcnt*5);

    hpx_future_t *fut = &(domain->dataSendFF[destLocalIdx][tag]); 
    hpx_lco_future_set(fut, 0, (void *)data);
  }

  // wait for incoming data
  int nrFF = domain->recvFF[0]; 
  int *recvFF = &domain->recvFF[1]; 

  for (i = 0; i < nrFF; i++) {
    int srcLocalIdx = recvFF[i]; 
    int fromDomain = OFFSET[srcLocalIdx] + rank; 
    int srcRemoteIdx = 25 - srcLocalIdx; 
    hpx_future_t *fut = &(DOMAINS[fromDomain].dataSendFF[srcRemoteIdx][tag]);

    hpx_thread_wait(fut); 
    double *src = (double *) hpx_lco_future_get_value(fut); 
    int recvcnt = XFERCNT[srcLocalIdx]; 
    recv_t unpack = RECEIVER[srcLocalIdx]; 
    
    unpack(nx, ny, nz, src, domain->x, 1); 
    unpack(nx, ny, nz, src + recvcnt, domain->y, 1);
    unpack(nx, ny, nz, src + recvcnt*2, domain->z, 1);
    unpack(nx, ny, nz, src + recvcnt*3, domain->xd, 1);
    unpack(nx, ny, nz, src + recvcnt*4, domain->yd, 1);
    unpack(nx, ny, nz, src + recvcnt*5, domain->zd, 1);

    // free the buffer
    hpx_free(src);

    // destroy the future
    hpx_lco_future_destroy(fut);
  }
}

void MonoQ(int rank)
{
  // doRecv = T, doSend = T, planeOnly = T, data = delv_xi, delv_eta, delv_zeta
  // MonoQ communicates with face-adjacent neighboring domains, the message size for each
  // field is nx*nx (assuming cubic domain). 

  Domain *domain = &DOMAINS[rank]; 
  int nx = domain->sizeX; 
  int ny = domain->sizeY; 
  int nz = domain->sizeZ; 
  int tag = domain->cycle; 
  int numElem = domain->numElem;
  int planeElem = nx*nx;
  int i, j; 

  double *delv_xi = domain->delv_xi;
  double *delv_eta = domain->delv_eta; 
  double *delv_zeta = domain->delv_zeta; 

  // pack outgoing data
  int nsTT = domain->sendTT[0]; 
  int *sendTT = &domain->sendTT[1]; 

  for (i = 0; i < nsTT; i++) {
    int destLocalIdx = sendTT[i]; 
    double *data = hpx_alloc(BUFSZ[destLocalIdx]);
    send_t pack = SENDER[destLocalIdx];    
    pack(nx, ny, nz, delv_xi, data);
    pack(nx, ny, nz, delv_eta, data + planeElem);
    pack(nx, ny, nz, delv_zeta, data + planeElem*2);
    hpx_future_t *fut = &(domain->dataSendTT[destLocalIdx][tag]); 
    hpx_lco_future_set(fut, 0, (void *)data);
    
  }

  // wait for incoming data
  int nrTT = domain->recvTT[0]; 
  int *recvTT = &domain->recvTT[1]; 

  // move pointers to the ghost area
  delv_xi += numElem;
  delv_eta += numElem;
  delv_zeta += numElem;

  for (i = 0; i < nrTT; i++) {
    int srcLocalIdx = recvTT[i]; 
    int fromDomain = OFFSET[srcLocalIdx] + rank; 
    int srcRemoteIdx = 25 - srcLocalIdx; 
    hpx_future_t *fut = &(DOMAINS[fromDomain].dataSendTT[srcRemoteIdx][tag]);
    hpx_thread_wait(fut); 
    double *src = (double *) hpx_lco_future_get_value(fut); 

    memcpy(delv_xi + i*planeElem, src, sizeof(double)*planeElem);
    memcpy(delv_eta + i*planeElem, src + planeElem, sizeof(double)*planeElem);
    memcpy(delv_zeta + i*planeElem, src + planeElem*2, sizeof(double)*planeElem);

    // free the buffer
    hpx_free(src); 

    // destroy the future
    hpx_lco_future_destroy(fut);
  }
}
#endif
void send1(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[0];
}

void send2(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  for (i = 0; i < nx; i++)
    dest[i] = src[i];
}

void send3(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx - 1];
}

void send4(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  for (i = 0; i < ny; i++) 
    dest[i] = src[i*nx];
}

void send5(int nx, int ny, int nz, double *src, double *dest)
{
  memcpy(dest, src, nx*ny*sizeof(double));
}

void send6(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx - 1]; 
  for (i = 0; i < ny; i++) 
    dest[i] = offsrc[i*nx];
}

void send7(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*(ny - 1)];
}

void send8(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*(ny - 1)]; 
  for (i = 0; i < nx; i++) 
    dest[i] = offsrc[i];
}

void send9(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny - 1];
}

void send10(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  for (i = 0; i < nz; i++) 
    dest[i] = src[i*nx*ny];
}

void send11(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  for (i = 0; i < nz; i++) 
    memcpy(&dest[i*nx], &src[i*nx*ny], nx*sizeof(double));
}

void send12(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx - 1];
  for (i = 0; i < nz; i++) 
    dest[i] = offsrc[i*nx*ny];
}

void send13(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j; 
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny]; 
    for (j = 0; j < ny; j++) 
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send14(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j; 
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny + nx - 1];
    for (j = 0; j < ny; j++) 
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send15(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*(ny - 1)]; 
  for (i = 0; i < nz; i++) 
    dest[i] = offsrc[i*nx*ny];
}

void send16(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*(ny - 1)]; 
  for (i = 0; i < nz; i++) 
    memcpy(&dest[i*nx], &offsrc[i*nx*ny], nx*sizeof(double));
}

void send17(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*ny - 1]; 
  for (i = 0; i < nz; i++) 
    dest[i] = offsrc[i*nx*ny]; 
}

void send18(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1)];
}

void send19(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*ny*(nz - 1)]; 
  for (i = 0; i < nx; i++) 
    dest[i] = offsrc[i];
}

void send20(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx - 1];
}

void send21(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*ny*(nz - 1)];
  for (i = 0; i < ny; i++) 
    dest[i] = offsrc[i*nx]; 
}

void send22(int nx, int ny, int nz, double *src, double *dest)
{
  double *offsrc = &src[nx*ny*(nz - 1)]; 
  memcpy(dest, offsrc, nx*ny*sizeof(double));
}

void send23(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1) + nx - 1];
  for (i = 0; i < ny; i++) 
    dest[i] = offsrc[i*nx];
}

void send24(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx*(ny - 1)];
}

void send25(int nx, int ny, int nz, double *src, double *dest)
{
  int i; 
  double *offsrc = &src[nx*(ny - 1) + nx*ny*(nz - 1)]; 
  for (i = 0; i < nx; i++) 
    dest[i] = offsrc[i];
}

void send26(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*nz - 1];
}

void recv1(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[0] = src[0];
  } else {
    dest[0] += src[0];
  }
}

void recv2(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  if (type) {
    for (i = 0; i < nx; i++) 
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      dest[i] += src[i];
  }
}

void recv3(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx - 1] = src[0];
  } else {
    dest[nx - 1] += src[0];
  }
}

void recv4(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  if (type) {
    for (i = 0; i < ny; i++) 
      dest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      dest[i*nx] += src[i];
  }
}

void recv5(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  if (type) {
    for (i = 0; i < nx*ny; i++) 
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx*ny; i++)
      dest[i] += src[i];
  }
}

void recv6(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx - 1]; 

  if (type) {
    for (i = 0; i < ny; i++) 
      offdest[i*nx] = src[i]; 
  } else {
    for (i = 0; i < ny; i++) 
      offdest[i*nx] += src[i]; 
  }
}
void recv7(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*(ny - 1)] = src[0]; 
  } else {
    dest[nx*(ny - 1)] += src[0];
  }
}

void recv8(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*(ny - 1)];
  
  if (type) {
    for (i = 0; i < nx; i++) 
      offdest[i] = src[i]; 
  } else {
    for (i = 0; i < nx; i++) 
      offdest[i] += src[i];
  }
}

void recv9(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny - 1] = src[0]; 
  } else {
    dest[nx*ny - 1] += src[0];
  }
}

void recv10(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  if (type) {
    for (i = 0; i < nz; i++) 
      dest[i*nx*ny] = src[i]; 
  } else {
    for (i = 0; i < nz; i++) 
      dest[i*nx*ny] += src[i];
  }
}

void recv11(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j; 
  if (type) {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++) 
	dest[i*nx*ny + j] = src[i*nx + j]; 
    }
  } else {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++) 
	dest[i*nx*ny + j] += src[i*nx + j];
    }
  }
}

void recv12(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx - 1]; 

  if (type) {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] = src[i]; 
  } else {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] += src[i];
  }
}

void recv13(int nx, int ny, int nz, double *src, double *dest, int type)
{  
  int i, j; 
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny]; 
      for (j = 0; j < ny; j++) 
	offdest[j*nx] = src[i*ny + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny]; 
      for (j = 0; j < ny; j++) 
	offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv14(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1]; 
      for (j = 0; j < ny; j++) 
	offdest[j*nx] = src[i*ny + j]; 
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1];
      for (j = 0; j < ny; j++)
	offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv15(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*(ny - 1)]; 

  if (type) {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] += src[i]; 
  }
}

void recv16(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny]; 
      for (j = 0; j < nx; j++) 
	offdest[j] = src[i*nx + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny]; 
      for (j = 0; j < nx; j++) 
	offdest[j] += src[i*nx + j];
    }
  }
}

void recv17(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny - 1]; 

  if (type) {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] = src[i]; 
  } else {
    for (i = 0; i < nz; i++) 
      offdest[i*nx*ny] += src[i]; 
  }
}

void recv18(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1)] += src[0];
  }
}

void recv19(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*ny*(nz - 1)]; 
  
  if (type) {
    for (i = 0; i < nx; i++) 
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++) 
      offdest[i] += src[i];
  }
}

void recv20(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx - 1] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx - 1] += src[0];
  }
}

void recv21(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++) 
      offdest[i*nx] += src[i];
  }
}

void recv22(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*ny*(nz - 1)]; 
  
  if (type) {
    for (i = 0; i < nx*ny; i++) 
      offdest[i] = src[i]; 
  } else {
    for (i = 0; i < nx*ny; i++) 
      offdest[i] += src[i];
  }
}

void recv23(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*ny*(nz - 1) + nx - 1];

  if (type) {
    for (i = 0; i < ny; i++) 
      offdest[i*nx] = src[i]; 
  } else {
    for (i = 0; i < ny; i++) 
      offdest[i*nx] += src[i];
  }
}

void recv24(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] += src[0];
  }
}

void recv25(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i; 
  double *offdest = &dest[nx*(ny - 1) + nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx; i++) 
      offdest[i] = src[i]; 
  } else {
    for (i = 0; i < nx; i++) 
      offdest[i] += src[i];
  }
}

void recv26(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*nz - 1] = src[0];
  } else {
    dest[nx*ny*nz - 1] += src[0];
  }
}
