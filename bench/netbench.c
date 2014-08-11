#include <hpx/hpx.h>

typedef struct {
  hpx_addr_t lco;
  int src;
  size_t size;
  char data[];
} echo_args_t;

void echo_send(hpx_addr_t lco, int src, int dst, size_t size) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  echo_args_t *pong_args = hpx_parcel_get_data(p);
  pong_args->lco = lco;
  pong_args->src = src;
  pong_args->size = size;
  hpx_parcel_set_action(p, echo_pong);
  hpx_parcel_set_target(p, HPX_THERE(dst));
  // here we use an asynchronous parcel send since we don't care about local completion;
  // the runtime gave us the buffer with the acquire, and we gave it back;
  // and it won't affect our timing because the local send completion isn't relevant
  hpx_parcel_send(p, NULL);
}

int echo_pong_action(echo_args_t *args) {
  echo_send(hpx_get_my_rank(), args->src, args->size);
  return HPX_SUCCESS;
}

int hpx_main_action(void *args) {
  size_t sizes[] = {1, 128, 1024, 4096, 8192, 64*1024, 256*1024, 1024*1024, 4*1024*1024, 16*1024*1024};
  hpx_addr_t lco = hpx_lco_gencount_new(sizeof(sizes));
  for (int i = 0; i < sizeof(sizes); i++)
    echo_send(lco, HPX_THERE(0), HPX_THERE(1), sizes[i]);

  hpx_lco_wait(lco);
}



int main(int argc, char *argv[]) {








}
