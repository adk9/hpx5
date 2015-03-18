#include "photon.h"
#include <mpi.h>

struct photon_config_t cfg = {
  .nproc = 0,
  .address = 0,
  .forwarder = {
    .use_forwarder = 0, 
  },
  .ibv = {
    .use_cma = 0,
    .use_ud = 1,
    .eth_dev = "roce0",
    .ib_dev = "mlx4_0+qib0",
    .ud_gid_prefix = "ff0e::ffff:0000:0000",  // mcast
  },
  .ugni = {
    .eth_dev = NULL,
    .bte_thresh = -1,
  },
  .cap = {
    .small_msg_size = -1,
    .small_pwc_size =  1024,
    .eager_buf_size = -1,
    .ledger_entries = -1,
    .max_rd         = -1,
    .default_rd     = -1,
    .num_cq         =  2
  },
  .exch = {
    .allgather = NULL,
    .barrier = NULL
  },
  .meta_exch = PHOTON_EXCH_MPI,
  .comm = NULL,
  .backend = "default"
};
