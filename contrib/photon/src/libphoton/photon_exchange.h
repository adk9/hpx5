#ifndef PHOTON_EXCHANGE
#define PHOTON_EXCHANGE

#include "photon_backend.h"

#define PHOTON_TPROC (_photon_nproc + _photon_nforw)

#define PHOTON_INFO_SSIZE(s) (s * sizeof(struct photon_ri_ledger_entry_t) + sizeof(struct photon_ri_ledger_t))
#define PHOTON_LEDG_SSIZE(s) (s * sizeof(struct photon_rdma_ledger_entry_t) + sizeof(struct photon_rdma_ledger_t))
#define PHOTON_EBUF_SSIZE(s) (s + sizeof(struct photon_rdma_eager_buf_t))

#define PHOTON_INFO_SIZE (PHOTON_INFO_SSIZE(_LEDGER_SIZE))
#define PHOTON_LEDG_SIZE (PHOTON_LEDG_SSIZE(_LEDGER_SIZE))
#define PHOTON_EBUF_SIZE (PHOTON_EBUF_SSIZE(_photon_ebsize))

#define PHOTON_NP_INFO_SIZE (PHOTON_TPROC * PHOTON_INFO_SIZE)
#define PHOTON_NP_LEDG_SIZE (PHOTON_TPROC * PHOTON_LEDG_SIZE)
#define PHOTON_NP_EBUF_SIZE (PHOTON_TPROC * PHOTON_EBUF_SIZE)

// The ledger buffer (shared_storage) is laid out as follows:
// | local_rcv | remote_rcv | local_snd | remote_snd | local_fin | remote_fin |
// | local_pwc | remote_pwc | local_eager | remote_eager | local_eager_bufs | remote_eager_bufs |
// | local_pwc_bufs | remote_pwc_bufs |

// accounting structs are now included at the end of each buffer

#define PHOTON_LRI_PTR(a) (a)
#define PHOTON_RRI_PTR(a) (a + PHOTON_NP_INFO_SIZE)
#define PHOTON_LSI_PTR(a) (a + PHOTON_NP_INFO_SIZE * 2)
#define PHOTON_RSI_PTR(a) (a + PHOTON_NP_INFO_SIZE * 3)
#define PHOTON_LF_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4)
#define PHOTON_RF_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE)
#define PHOTON_LP_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 2)
#define PHOTON_RP_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 3)
#define PHOTON_LE_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 4)
#define PHOTON_RE_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 5)
#define PHOTON_LEB_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 6)
#define PHOTON_REB_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 6 + PHOTON_NP_EBUF_SIZE)
#define PHOTON_LPB_PTR(a) (PHOTON_LEB_PTR(a) + PHOTON_NP_EBUF_SIZE * 2)
#define PHOTON_RPB_PTR(a) (PHOTON_LEB_PTR(a) + PHOTON_NP_EBUF_SIZE * 3)

PHOTON_INTERNAL int photon_exchange_allgather(void *s, void *d, int n);
PHOTON_INTERNAL int photon_exchange_barrier();
PHOTON_INTERNAL int photon_exchange_ledgers(ProcessInfo *processes, int flags);
PHOTON_INTERNAL int photon_setup_ri_ledger(ProcessInfo *processes, char *buf, int num_entries);
PHOTON_INTERNAL int photon_setup_eager_ledger(ProcessInfo *processes, char *buf, int num_entries);
PHOTON_INTERNAL int photon_setup_fin_ledger(ProcessInfo *processes, char *buf, int num_entries);
PHOTON_INTERNAL int photon_setup_pwc_ledger(ProcessInfo *processes, char *buf, int num_entries);
PHOTON_INTERNAL int photon_setup_eager_buf(ProcessInfo *processes, char *buf, int num_entries);
PHOTON_INTERNAL int photon_setup_pwc_buf(ProcessInfo *processes, char *buf, int num_entries);

#endif
