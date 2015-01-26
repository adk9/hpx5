#include "lulesh-hpx.h"

static hpx_action_t _main          = 0;
static hpx_action_t _advanceDomain = 0;

void SBN1(hpx_addr_t *address,int index,Domain *domain)
{
  hpx_addr_t channels = *address;
  printf(" TEST in SBN1\n");
  // doRecv = T, doSend = T, planeOnly = F, data = nodalMass
  int rank = index;
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int i;

  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];
#if 0
  for (i = 0; i < nsTF; i++) {
    int destLocalIdx = sendTF[i];
    printf(" TEST sendTF %d\n",sendTF[i]);
    double *data = malloc(BUFSZ[destLocalIdx]);

    send_t pack = SENDER[destLocalIdx];
    pack(nx, ny, nz, domain->nodalMass, data);

    // QUESTION:  Where should I free data ?

    //hpx_future_t *fut = &domain->SBN1[destLocalIdx];
    //hpx_lco_future_set(fut, 0, (void *)data);
  }
#endif

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int fromDomain = OFFSET[srcLocalIdx] + rank;
    int srcRemoteIdx = 25 - srcLocalIdx;

    printf(" TEST fromDomain %d srcRemoteIdx %d\n",fromDomain,srcRemoteIdx);
    //hpx_future_t *fut = &DOMAINS[fromDomain].SBN1[srcRemoteIdx];
    if (index == fromDomain) {
      double *data = malloc(1*sizeof(double));
      data[0] = 1.0*index;
      hpx_addr_t dest = hpx_lco_chan_array_at(channels,srcRemoteIdx);
      hpx_lco_chan_send(dest, &data, sizeof(data), HPX_NULL);
    } else if ( index == srcRemoteIdx ) {
      hpx_addr_t here = hpx_lco_chan_array_at(channels, srcRemoteIdx);
      double *src = hpx_lco_chan_recv(here, sizeof(1*sizeof(double)));

      recv_t unpack = RECEIVER[srcLocalIdx];
      unpack(nx, ny, nz, src, domain->nodalMass, 0);

      free(src);
    }

    //hpx_thread_wait(fut);
    //double *src = (double *) hpx_lco_future_get_value(fut);

    //recv_t unpack = RECEIVER[srcLocalIdx];
    //unpack(nx, ny, nz, src, domain->nodalMass, 0);

    //hpx_free(src);
  }
  //hpx_thread_exit(NULL);
}


static int _advanceDomain_action(Advance *advance) {
  hpx_addr_t channels = *(advance->address);
  int nx = advance->nx;
  int nDoms = advance->nDoms;
  int maxcycles = advance->maxcycles;
  int cores = advance->cores;
  int index = advance->index;
  int i;

  int tp = (int) (cbrt(nDoms) + 0.5);

  Init(tp,nx);
  Domain domain;
  int col = index%tp;
  int row = (index/tp)%tp;
  int plane = index/(tp*tp);
  SetDomain(index, col, row, plane, nx, tp, nDoms, maxcycles,&domain);

  SBN1(&channels,index,&domain);

  // send recv example
  //if ( index == 1 ) {
  //  double buf = 3.14159;
  //  hpx_addr_t root = hpx_lco_chan_array_at(channels, 7);
  //  hpx_lco_chan_send(root, &buf, sizeof(buf), HPX_NULL);
  //} else if ( index == 7 ) {
  //  hpx_addr_t here = hpx_lco_chan_array_at(channels, 7);
  //  double *result = hpx_lco_chan_recv(here, sizeof(double));
  //  printf(" TEST buf %g\n",*result);
  //}

  //int nrTT = domain.recvTT[0];
  //int *recvTT = &domain.recvTT[1];
  //int srcLocalIdx = recvTT[0];
  //int fromDomain = OFFSET[srcLocalIdx] + index;
  //int srcRemoteIdx = 25 - srcLocalIdx;
  // 4 3 21
  //printf(" TEST %d %d %d\n",srcLocalIdx,fromDomain,srcRemoteIdx);
#if 0
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
#endif

  hpx_thread_continue(0, NULL);
}

static int _main_action(int *input)
{
  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_addr_t rank;
  int ranks = HPX_LOCALITIES;

  hpx_time_t t1 = hpx_time_now();

  int nDoms, nx, maxcycles, cores, tp, i, j, k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];
  cores = input[3];

  tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  // initialize persistent threads---one per domain.
  hpx_addr_t channels = hpx_lco_chan_array_new(nDoms, 1);
  hpx_addr_t and = hpx_lco_and_new(nDoms);
  Advance advance[nDoms];
  for (k=0; k<nDoms; ++k) {
    advance[k].index = k;
    advance[k].nDoms = nDoms;
    advance[k].nx = nx;
    advance[k].maxcycles = maxcycles;
    advance[k].cores = cores;
    advance[k].address = &channels;
    rank = hpx_lco_chan_array_at(channels, k);
    hpx_call(rank, _advanceDomain, &advance[k], sizeof(advance[k]), and);
  }

#if 0

  Init(tp, nx);
  DOMAINS = malloc(sizeof(Domain)*nDoms);
  for (i = 0; i < nDoms; i++) {
    int col = i%tp;
    int row = (i/tp)%tp;
    int plane = i/(tp*tp);
    //SetDomain(i, col, row, plane, nx, tp, nDoms, maxcycles);
  }

  free(DOMAINS);
#endif

  // wait for receivers to finish.
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  hpx_lco_chan_array_delete(channels, HPX_NULL);

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-n, number of domains,nDoms\n"
          "\t-x, nx\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}


int main(int argc, char **argv)
{

  int nDoms, nx, maxcycles,cores;
  // default
  nDoms = 8;
  nx = 15;
  maxcycles = 10;
  cores = 8;

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "n:x:ih")) != -1) {
    switch (opt) {
      case 'n':
        nDoms = atoi(optarg);
        break;
      case 'x':
        nx = atoi(optarg);
        break;
      case 'i':
        maxcycles = atoi(optarg);
        break;
      case 'h':
        usage(stdout);
        return 0;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_advanceDomain, _advanceDomain_action);

  int input[4];
  input[0] = nDoms;
  input[1] = nx;
  input[2] = maxcycles;
  input[3] = cores;
  printf(" Number of domains: %d nx: %d maxcycles: %d cores: %d\n",nDoms,nx,maxcycles,cores);

  return hpx_run(&_main, input, 4*sizeof(int));

  return 0;
}

