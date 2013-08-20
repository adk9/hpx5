#ifndef VERBS_CONNECT_H
#define VERBS_CONNECT_H

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

struct message {
	enum {
	    MSG_READY,
	    MSG_DONE,
	    MSG_STOP
	} type;

	struct ibv_mr mr;
	uint64_t size;
	uint64_t buffer_id;
};

struct xfer_context {
	struct ibv_context *context;
	struct ibv_pd      *pd;
	struct ibv_mr      *recv_mr;
	struct ibv_mr      *send_mr;
	struct message     *recv_msg;
	struct message     *send_msg;
	struct ibv_cq      *cq;
	struct ibv_qp      *qp;
	struct rdma_cm_id  *cm_id;
	struct ibv_comp_channel *ch;
	int                 tx_depth;
	struct ibv_sge      list;
	struct ibv_send_wr  wr;
};

// might need this again in the future
// for non CMA
struct xfer_dest {
	int lid;
	int qpn;
	int psn;
	unsigned rkey;
	unsigned long long vaddr;
	unsigned size;
};

struct xfer_data {
	int                             port;
	int                             ib_port;
	int                             tx_depth;
	int                             use_cma;
	int                             sockfd;
	char                            *servername;
	struct ibv_device               *ib_dev;
	struct rdma_event_channel       *cm_channel;
	struct rdma_cm_id               *cm_id;
	void                            *local_priv;
	void                            *remote_priv;
	uint64_t                        local_priv_size;
	uint64_t                        remote_priv_size;
};

typedef struct verbs_cma_buf_handle_t {
	int                             opcode;
	int                             got_done;
	void                            *buf;
	uint64_t                        local_size;
	uint64_t                        remote_size;
	uint64_t                        id;
	struct ibv_mr                   *local_mr;
	struct ibv_mr                   *remote_mr;
	struct xfer_context             *ctx;
} verbs_cma_buf_handle;

typedef struct verbs_cma_poll_info_t {
	int                            opcode;
	int                            status;
	uint64_t                       id;
} verbs_cma_poll_info;


int verbs_cma_init(struct xfer_data *data);
int verbs_cma_finalize(struct xfer_data *data);
struct xfer_context *verbs_cma_init_ctx(void *, struct xfer_data *);
void verbs_cma_destroy_ctx(struct xfer_context *ctx);
struct verbs_cma_buf_handle_t *verbs_cma_alloc_handle();

int verbs_cma_post_os_put(struct verbs_cma_buf_handle_t **handles, int hcount);
int verbs_cma_post_os_get(struct verbs_cma_buf_handle_t **handles, int hcount);
int verbs_cma_wait_os(struct verbs_cma_buf_handle_t *handle);
int verbs_cma_wait_os_event(struct xfer_context *ctx, struct verbs_cma_poll_info_t *info);

int verbs_cma_wait_buffer(struct verbs_cma_buf_handle_t *handle);
int verbs_cma_post_buffer(struct verbs_cma_buf_handle_t *handle);

int verbs_cma_send_stop(struct verbs_cma_buf_handle_t *handle);
int verbs_cma_send_done(struct verbs_cma_buf_handle_t *handle);
int verbs_cma_wait_done(struct verbs_cma_buf_handle_t *handle);

struct xfer_context *verbs_cma_client_connect(struct xfer_data *data);
struct xfer_context *verbs_cma_server_connect(struct xfer_data *data);

int verbs_cma_register_buffer(struct xfer_context *ctx, struct verbs_cma_buf_handle_t *handle);
int verbs_cma_unregister_buffer(struct verbs_cma_buf_handle_t *handle);

#endif
