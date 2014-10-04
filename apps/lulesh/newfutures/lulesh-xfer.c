#include "lulesh-hpx.h"

void
SBN1(Domain *domain, hpx_newfuture_t *sbn1)
{
  const int    nsTF = domain->sendTF[0];
  const int *sendTF = &domain->sendTF[1]; 

  int i,destLocalIdx;
  int           nx = domain->sizeX + 1;
  int           ny = domain->sizeY + 1;
  int           nz = domain->sizeZ + 1;
  for (i = 0; i < nsTF; ++i) {
    destLocalIdx = sendTF[i];
    double *data = malloc(BUFSZ[destLocalIdx]);
    send_t      pack = SENDER[destLocalIdx];
    pack(nx, ny, nz, domain->nodalMass, data);
    hpx_lco_newfuture_setat(sbn1, destLocalIdx + domain->rank*26, BUFSZ[destLocalIdx], data, HPX_NULL, HPX_NULL);
    free(data);
  }

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int fromDomain = OFFSET[srcLocalIdx] + domain->rank;
    int srcRemoteIdx = 25 - srcLocalIdx;
    double *src = malloc(BUFSZ[srcRemoteIdx]);
    hpx_lco_newfuture_getat(sbn1, srcRemoteIdx + fromDomain*26, BUFSZ[srcRemoteIdx], src);
    recv_t unpack = RECEIVER[srcLocalIdx];
    unpack(nx, ny, nz, src, domain->nodalMass, 0);
    free(src);
  }
}

void
SBN3(hpx_newfuture_t *sbn3,Domain *domain, int rank)
{
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int tag = domain->cycle;
  int i,destLocalIdx;

  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];
  
  int gen = tag%2;
  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    int sendcnt = XFERCNT[destLocalIdx];
    double *data = malloc(BUFSZ[destLocalIdx]);
    send_t pack = SENDER[destLocalIdx];

    pack(nx, ny, nz, domain->fx, data);
    pack(nx, ny, nz, domain->fy, data + sendcnt);
    pack(nx, ny, nz, domain->fz, data + sendcnt*2);

    hpx_lco_newfuture_setat(sbn3, destLocalIdx + 26*(domain->rank + gen*2), BUFSZ[destLocalIdx], data, HPX_NULL, HPX_NULL);

    free(data);
  } 

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int fromDomain = OFFSET[srcLocalIdx] + rank;
    int srcRemoteIdx = 25 - srcLocalIdx;
    double *src = malloc(BUFSZ[srcRemoteIdx]);
    hpx_lco_newfuture_getat(sbn3, srcRemoteIdx + 26*(fromDomain + gen*2), BUFSZ[srcRemoteIdx], src);
    int recvcnt = XFERCNT[srcLocalIdx];
    recv_t unpack = RECEIVER[srcLocalIdx];

    unpack(nx, ny, nz, src, domain->fx, 0);
    unpack(nx, ny, nz, src + recvcnt, domain->fy, 0);
    unpack(nx, ny, nz, src + recvcnt*2, domain->fz, 0);

    // free the buffer
    free(src);
  }


}

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
