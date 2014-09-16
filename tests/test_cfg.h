#include "photon.h"

struct photon_config_t cfg = {
  .meta_exch = PHOTON_EXCH_MPI,
  .nproc = 0,
  .address = 0,
  .comm = MPI_COMM_WORLD,
  .use_forwarder = 0,
  .use_cma = 0,
  .use_ud = 1,
  .ud_gid_prefix = "ff0e::ffff:0000:0000",  // mcast
  .eth_dev = "roce0",
  .ib_dev = "mlx4_0",
  .ib_port = 2,
  .backend = "verbs"
};
