#ifndef PHOTON_EXCHANGE
#define PHOTON_EXCHANGE

#define PHOTON_TPROC (_photon_nproc + _photon_nforw)
#define PHOTON_NP_INFO_SIZE (PHOTON_TPROC * LEDGER_SIZE * sizeof(struct photon_ri_ledger_entry_t))
#define PHOTON_NP_LEDG_SIZE (PHOTON_TPROC * LEDGER_SIZE * sizeof(struct photon_rdma_ledger_entry_t))
#define PHOTON_NP_EBUF_SIZE (PHOTON_TPROC * LEDGER_SIZE * sizeof(struct photon_rdma_eager_buf_entry_t))

// The ledger buffer (shared_storage) is laid out as follows:
// | local_rcv | remote_rcv | local_snd | remote_snd | local_fin | remote_fin |
// | local_pwc | remote_pwc | local_eager | remote_eager | local_eager_bufs | remote_eager_bufs |

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
#define PHOTON_EB_PTR(a) (a + PHOTON_NP_INFO_SIZE * 4 + PHOTON_NP_LEDG_SIZE * 6)

int photon_exchange_ledgers(ProcessInfo *processes, int flags);

int photon_setup_ri_ledger(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_eager_ledger(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_fin_ledger(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_pwc_ledger(ProcessInfo *processes, char *buf, int num_entries);
int photon_setup_eager_buf(ProcessInfo *processes, char *buf, int num_entries);

#endif
