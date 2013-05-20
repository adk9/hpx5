#include "mpi.h"

#include <dat2/udat.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "photon.h"
#include "dapl.h"
#include "dapl_buffer.h"
#include "dapl_remote_buffer.h"
#include "htable.h"
#include "buffertable.h"
#include "logging.h"

int _photon_nproc;
int _photon_myrank;
int _photon_forwarder;
MPI_Comm _photon_comm;

#ifdef WITH_XSP

static int dapl_xsp_init();
static int dapl_xsp_setup_session(libxspSess **sess, char *xsp_hop);
static int dapl_xsp_setup_listeners();
static int dapl_xsp_exchange_ri_ledgers();
static int dapl_xsp_exchange_FIN_ledger();

int _photon_fp;
int sess_count;

#endif

static int dapl_ep2proc(DAT_EP_HANDLE ep, int *type);
static int dapl_connqual2proc(DAT_CONN_QUAL cq, int *type);
static int dapl_setup_listeners(int myrank);
static int dapl_exchange_ri_ledgers();
int dapl_register_buffer(char *buffer, int buffer_size);
int dapl_setup_ri_ledgers(char *buf, int num_entries);
static int dapl_exchange_FIN_ledger();
int dapl_setup_FIN_ledger(char *buf, int num_entries);
const char *str_ep_state(const DAT_EP_STATE status);
static int __dapl_wait_one();
static int __dapl_wait_dto(DAT_TIMEOUT timeout, int *ret_proc, uint32_t *ret_cookie, int *is_error, int *timed_out);
static int __dapl_wait_evd(dapl_req_t *req);
static int __dapl_wait_ledger(dapl_req_t *req);
static int __dapl_nbpop_ledger(dapl_req_t *req);
static int __dapl_nbpop_dto(int *ret_proc, uint32_t *ret_cookie, int *is_error);
static int __dapl_nbpop_evd(dapl_req_t *req);
static inline dapl_req_t *dapl_get_request();
dapl_buffer_t *shared_storage;

static ProcessInfo *dapl_processes;
static int dapl_processes_count;
static int handshake_rdma_write_count=0;
static DAT_EVD_HANDLE cevd = DAT_HANDLE_NULL;
static DAT_EVD_HANDLE dto_evd = DAT_HANDLE_NULL;

static struct ifaddrs *self_addr;

static DAT_IA_HANDLE ia = DAT_HANDLE_NULL;
static DAT_PZ_HANDLE pz = DAT_HANDLE_NULL;
static char *dapl_device;
static htable_t *reqtable, *ledger_reqtable;
static struct dapl_req *requests;
static int num_requests;
static LIST_HEAD(freereqs, dapl_req) free_reqs_list;
static LIST_HEAD(unreapedevdreqs, dapl_req) unreaped_evd_reqs_list;
static LIST_HEAD(unreapedledgerreqs, dapl_req) unreaped_ledger_reqs_list;
static LIST_HEAD(pendingreqs, dapl_req) pending_reqs_list;
static SLIST_HEAD(pendingmemregs, mem_register_req) pending_mem_register_list;

// We only want to spawn a dedicated thread for ledgers on
// multithreaded instantiations of the library (e.g. in xspd).
// FIXME: All of the pthreads stuff below should also depend on this.
#ifdef PHOTON_MULTITHREADED
static pthread_t ledger_watcher;

static int __dapl_wait_ledger_mt(dapl_req_t *req);
static void *dapl_watch_ledgers(void *arg);
#endif

///////////////////////////////////////////////////////////////////////////////
static uint32_t _curr_cookie_count = 0;
static pthread_mutex_t cookie_mtx;

static inline uint32_t get_curr_cookie_count() {
    uint32_t cookie;
    pthread_mutex_lock(&cookie_mtx);
    {
        cookie = _curr_cookie_count;
    }
    pthread_mutex_unlock(&cookie_mtx);
    return cookie;
}

static inline uint32_t get_inc_curr_cookie_count() {
    uint32_t cookie;
    pthread_mutex_lock(&cookie_mtx);
    {
        cookie = _curr_cookie_count;
        ++_curr_cookie_count;
        if( _curr_cookie_count == 0 ) ++_curr_cookie_count;
    }
    pthread_mutex_unlock(&cookie_mtx);
    return cookie;
}

int dapl_init_common(int nproc, int myrank, MPI_Comm comm, int forwarder) {
    DAT_RETURN retval;
    DAT_EVD_HANDLE async_evd;
    char *device;
    struct ifaddrs *if_addrs;
    struct ifaddrs *curr_if;
    int i;
    char *buf;
    int bufsize, offset;

    _photon_nproc = nproc;
    _photon_myrank = myrank;
    _photon_forwarder = forwarder;
    _photon_comm = comm;

    if (_curr_cookie_count != 0) {
        log_err("dapl_init(): Error: already initialized");
        return -1;
    }

    ctr_info(" > dapl_init(%d, %d)", nproc, myrank);

    pthread_mutex_init(&cookie_mtx, NULL);
    _curr_cookie_count = 1;

    device = getenv("DAPL_PROVIDER");

    if (device == NULL) {
        log_err("dapl_init(): Error: no dapl provider specified: set the environmental variable DAPL_PROVIDER");
        goto error_exit;
    }

    dbg_info("dapl_init(): %s", device);
    dapl_device = strdup(device);
    if (!dapl_device) {
        log_err("dapl_init(): Failed to allocate device string");
        goto error_exit;
    }

    requests = malloc(sizeof(dapl_req_t) * DEF_NUM_REQUESTS);
    if (!requests) {
        log_err("dapl_init(): Failed to allocate request list");
        goto error_exit_dev;
    }
    // XXX: This assumes PTHREAD_MUTEX_INITIALIZER and PTHREAD_COND_INITIALIZER
    //   just set everything to 0/NULL (might not be portable/forward compatible).
    memset(requests, 0, sizeof(dapl_req_t) * DEF_NUM_REQUESTS);

    num_requests = DEF_NUM_REQUESTS;
    LIST_INIT(&free_reqs_list);
    LIST_INIT(&unreaped_evd_reqs_list);
    LIST_INIT(&unreaped_ledger_reqs_list);
    LIST_INIT(&pending_reqs_list);

    for(i = 0; i < num_requests; i++) {
        LIST_INSERT_HEAD(&free_reqs_list, &(requests[i]), list);
    }

    dbg_info("create_buffertable()");
    fflush(stderr);
    if (buffertable_init(193)) {
        log_err("dapl_init(); Failed to allocate buffer table");
        goto error_exit_dev;
    }

    dbg_info("create_reqtable()");
    reqtable = htable_create(193);
    if (!reqtable) {
        log_err("dapl_init(): Failed to allocate request table");
        goto error_exit_bt;
    }

    dbg_info("create_ledger_reqtable()");

    ledger_reqtable = htable_create(193);
    if (!ledger_reqtable) {
        log_err("dapl_init(): Failed to allocate request table");
        goto error_exit_rt;
    }


    dbg_info("Device: %s", dapl_device);

    if (getifaddrs(&if_addrs) != 0)
        goto error_exit_lrt;

    dbg_info("dapl_init(): got interfaces");

    // this block gets the dapl interface without having to hardcode it
    int family, s;
    char host[NI_MAXHOST];
    char delims[] = "-";
    char *res = NULL;
    char dev[12];

    res = strtok( device, delims );
    while( res != NULL ) {
        strncpy(dev, res, 9);
        dev[strlen(dev)] = '\0';
        res = strtok( NULL, delims );
    }

    for(curr_if = if_addrs; curr_if != NULL; curr_if = curr_if->ifa_next) {
        if (curr_if->ifa_addr == NULL)
            continue;

        family = curr_if->ifa_addr->sa_family;

        /* Display interface name and family (including symbolic
		   form of the latter for the common families) */

        dbg_info("%s  address family: %d%s",
                curr_if->ifa_name, family,
                (family == AF_PACKET) ? " (AF_PACKET)" :
                        (family == AF_INET) ?   " (AF_INET)" :
                                (family == AF_INET6) ?  " (AF_INET6)" : "");

        /* For an AF_INET* interface address, display the address */

        if (family == AF_INET || family == AF_INET6) {
            s = getnameinfo(curr_if->ifa_addr,
                    (family == AF_INET) ? sizeof(struct sockaddr_in) :
                            sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            dbg_info("\taddress: <%s>", host);
        }

        if ((strcmp(curr_if->ifa_name, dev) == 0) &&
                (curr_if->ifa_addr->sa_family == AF_INET))
            break;
    }

    if (!curr_if) {
        goto error_exit_rt;
    }

    dbg_info("dapl_init(): found %s", dev);

    self_addr = curr_if;

    async_evd = DAT_HANDLE_NULL;

    retval = dat_ia_open(dapl_device, DEF_QUEUE_LENGTH, &async_evd, &ia);

    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_init(): Couldn't open Infiniband Interface");
        goto error_exit_ib;
    }

    dbg_info("dapl_init(): opened ia");

    retval = dat_pz_create(ia, &pz);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_init(): Couldn't create protection zone");
        goto error_exit_ia;
    }

    dbg_info("dapl_init(): opened pz");

    dapl_processes = (ProcessInfo *) malloc(sizeof(ProcessInfo) * (nproc+forwarder));
    if (!dapl_processes) {
        log_err("dapl_init(): Couldn't allocate process information");
        goto error_exit_pz;
    }

    // Set it to zero, so that we know if it ever got initialized
    memset(dapl_processes, 0, sizeof(ProcessInfo) * nproc);

    dapl_processes_count = nproc;

    for(i = 0; i < dapl_processes_count+forwarder; i++) {
        dapl_processes[i].curr_remote_buffer = dapl_remote_buffer_create();
        if(!dapl_processes[i].curr_remote_buffer) {
            log_err("Couldn't allocate process remote buffer information");
            goto error_exit_dp;
        }
    }

    dbg_info("dapl_init(): alloc'd process info");

    retval = dat_evd_create(ia, DEF_QUEUE_LENGTH, DAT_HANDLE_NULL, DAT_EVD_CONNECTION_FLAG | DAT_EVD_CR_FLAG, &cevd);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_init(): Couldn't create event queue");
        goto error_exit_crb;
    }

    dbg_info("dapl_init(): created connection evd");

    retval = dat_evd_create(ia, DEF_QUEUE_LENGTH, DAT_HANDLE_NULL, DAT_EVD_DTO_FLAG, &dto_evd);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_init(): Couldn't create event queue");
        goto error_exit_cevd;
    }

    dbg_info("dapl_init(): created dto evd");

    // Everything is x2 cause we need a local and a remote copy of each ledger.
    // Remote Info (_ri_) ledger has an additional x2 cause we need "send-info" and "receive-info" ledgers.
    bufsize = 2 * 2 * sizeof(dapl_ri_ledger_entry_t) * LEDGER_SIZE * (dapl_processes_count+forwarder) + 2 * sizeof(dapl_rdma_FIN_ledger_entry_t) * LEDGER_SIZE * (dapl_processes_count+forwarder);
    buf = malloc(bufsize);
    if (!buf) {
        log_err("dapl_init(): Couldn't allocate ledgers");
        goto error_exit_dto_evd;
    }

    dbg_info("Bufsize: %d", bufsize);

    shared_storage = dapl_buffer_create(buf, bufsize);
    if (!shared_storage) {
        log_err("dapl_init(): Couldn't register shared storage");
        goto error_exit_buf;
    }

    if (dapl_buffer_register(shared_storage, ia, pz) != 0) {
        log_err("dapl_init(): couldn't register local buffer for the ledger entries");
        goto error_exit_ss;
    }


    if (dapl_setup_ri_ledgers(buf, LEDGER_SIZE) != 0) {
        log_err("dapl_init(); couldn't setup snd/rcv info ledgers");
        goto error_exit_ri_ledger;
    }

    // skip 4 ledgers (rcv info local, rcv info remote, snd info local, snd info remote)
    offset = 4 * sizeof(dapl_ri_ledger_entry_t) * LEDGER_SIZE * (nproc+forwarder);
    if (dapl_setup_FIN_ledger(buf + offset, LEDGER_SIZE) != 0) {
        log_err("dapl_init(); couldn't setup send ledgers");
        goto error_exit_ri_ledger;
    }

#ifdef PHOTON_MULTITHREADED
    if (pthread_create(&ledger_watcher, NULL, dapl_watch_ledgers, NULL)) {
        log_err("dapl_init(): pthread_create() failed.\n");
        goto error_exit_ledger_watcher;
    }
#endif

    return 0;

#ifdef PHOTON_MULTITHREADED
    error_exit_ledger_watcher:
#endif
    error_exit_ri_ledger:
    error_exit_ss:
    dapl_buffer_free(shared_storage);
    error_exit_buf:
    if (buf)
        free(buf);
    error_exit_dto_evd:
    error_exit_cevd:
    error_exit_crb:
    for(i = 0; i < dapl_processes_count+forwarder; i++) {
        if (dapl_processes[i].curr_remote_buffer != NULL) {
            dapl_remote_buffer_free(dapl_processes[i].curr_remote_buffer);
        }
    }
    error_exit_dp:
    free(dapl_processes);
    error_exit_pz:
    error_exit_ia:
    //	dat_ia_close(ia, DAT_CLOSE_ABRUPT_FLAG);
    dat_ia_close(ia, DAT_CLOSE_GRACEFUL_FLAG);
    error_exit_ib:
    freeifaddrs(if_addrs);
    error_exit_lrt:
    htable_free(ledger_reqtable);
    error_exit_rt:
    htable_free(reqtable);
    error_exit_bt:
    buffertable_finalize();
    error_exit_dev:
    free(dapl_device);
    error_exit:
    _curr_cookie_count = 0;
    return -1;
}

int dapl_init(int nproc, int myrank, MPI_Comm comm, int forwarder) {

    if (dapl_init_common(nproc, myrank, comm, forwarder) != 0)
        goto error_exit;

    if (dapl_setup_listeners(myrank) != 0) {
        log_err("dapl_init(); couldn't setup listeners");
        goto error_exit_ss;
    }

    if (dapl_exchange_ri_ledgers() != 0) {
        log_err("dapl_init(); couldn't exchange rdma ledgers");
        goto error_exit_listeners;
    }

    if (dapl_exchange_FIN_ledger() != 0) {
        log_err("dapl_init(); couldn't exchange send ledgers");
        goto error_exit_FIN_ledger;
    }

    while( !SLIST_EMPTY(&pending_mem_register_list) ){
        struct mem_register_req *mem_reg_req;
        dbg_info("dapl_init(): registering buffer in queue");
        mem_reg_req = SLIST_FIRST(&pending_mem_register_list);
        SLIST_REMOVE_HEAD(&pending_mem_register_list, list);
        dapl_register_buffer(mem_reg_req->buffer, mem_reg_req->buffer_size);
    }

#ifdef WITH_XSP
    if (forwarder) {

        _photon_fp = nproc;
        sess_count = 1;

        if (dapl_xsp_init() != 0) {
            log_err("dapl_init(); couldn't initialize phorwarder connection");
            goto error_exit_FIN_ledger;
        }
    }
#endif	

    dbg_info("dapl_init(): ended successfully =============");

    return 0;

    error_exit_FIN_ledger:
    error_exit_listeners:
    error_exit_ss:
    dapl_buffer_free(shared_storage);
    error_exit:
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_finalize() {
    DAT_EVENT event;
    DAT_COUNT count;
    DAT_RETURN ret;
    int i;
    int num_disconnected=0;

    ctr_info(" > dapl_finalize()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_finalize(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    MPI_Barrier(_photon_comm);
    _curr_cookie_count = 0;

    // clear out any events before we disconnect the eps
    do {
        ret = dat_evd_dequeue(cevd, &event);
    } while (ret == DAT_SUCCESS);

    do {
        ret = dat_evd_dequeue(dto_evd, &event);
    } while (ret == DAT_SUCCESS);

    for(i = 0; i < dapl_processes_count; i++) {
        ret = dat_ep_disconnect(dapl_processes[i].clnt_ep, DAT_CLOSE_ABRUPT_FLAG);
        if( ret != DAT_SUCCESS )
            log_err("dapl_finalize(): Failed to disconnect dapl_processes[%d].clnt_ep",i);

        ret = dat_ep_disconnect(dapl_processes[i].srvr_ep, DAT_CLOSE_ABRUPT_FLAG);
        if( ret != DAT_SUCCESS )
            log_err("dapl_finalize(): Failed to disconnect dapl_processes[%d].srvr_ep",i);
    }

    // Wait for the disconnection events
    while( num_disconnected < 2*dapl_processes_count ) {
        if ( (dat_evd_wait (cevd, DAT_TIMEOUT_INFINITE, 1, &event, &count) == DAT_SUCCESS ) &&
                (event.event_number == DAT_CONNECTION_EVENT_DISCONNECTED)){
            num_disconnected++;
        }
    }

    for(i = 0; i < dapl_processes_count; i++) {
        //		ret = dat_ep_free(dapl_processes[i].clnt_ep);
        //		if( ret != DAT_SUCCESS ) dbg_err("Failed to free dapl_processes[%d].clnt_ep",i);

        //		ret = dat_ep_free(dapl_processes[i].srvr_ep);
        //		if( ret != DAT_SUCCESS ) dbg_err("Failed to free dapl_processes[%d].srvr_ep",i);

        dapl_ri_ledger_free(dapl_processes[i].local_rcv_info_ledger);
        dapl_ri_ledger_free(dapl_processes[i].remote_rcv_info_ledger);
        dapl_ri_ledger_free(dapl_processes[i].local_snd_info_ledger);
        dapl_ri_ledger_free(dapl_processes[i].remote_snd_info_ledger);
        dapl_rdma_FIN_ledger_free(dapl_processes[i].local_FIN_ledger);
        dapl_rdma_FIN_ledger_free(dapl_processes[i].remote_FIN_ledger);
    }

    // clear out any new events before we start freeing the eps
    do {
        ret = dat_evd_dequeue(cevd, &event);
    } while (ret == DAT_SUCCESS);

    do {
        ret = dat_evd_dequeue(dto_evd, &event);
    } while (ret == DAT_SUCCESS);


    for(i = 0; i < dapl_processes_count; i++) {
        DAT_BOOLEAN in_idle, out_idle;
        DAT_EP_STATE ep_state;

        if (dapl_processes[i].curr_remote_buffer != NULL) {
            dapl_remote_buffer_free(dapl_processes[i].curr_remote_buffer);
        }

        dat_ep_get_status(dapl_processes[i].clnt_ep, &ep_state, &in_idle, &out_idle);
        dbg_info("Clnt EP Status: %s %d %d", str_ep_state(ep_state), in_idle, out_idle);

        dat_ep_get_status(dapl_processes[i].srvr_ep, &ep_state, &in_idle, &out_idle);
        dbg_info("Srvr EP Status: %s %d %d", str_ep_state(ep_state), in_idle, out_idle);


        ret = dat_ep_free(dapl_processes[i].clnt_ep);
        if( ret != DAT_SUCCESS )
            log_dat_err(ret, "dapl_finalize(): Failed to free dapl_processes[%d].clnt_ep",i);

        ret = dat_psp_free(dapl_processes[i].srvr_psp);
        if( ret != DAT_SUCCESS )
            log_dat_err(ret, "dapl_finalize(): Failed to free dapl_processes[%d].psp",i);

        ret = dat_ep_free(dapl_processes[i].srvr_ep);
        if( ret != DAT_SUCCESS )
            log_dat_err(ret, "dapl_finalize(): Failed to free dapl_processes[%d].srvr_ep",i);
    }

    //	for(i = 0; i < dapl_processes_count; i++) {
    //		ret = dat_psp_free( dapl_processes[i].srvr_psp );
    //		if( ret != DAT_SUCCESS ) log_err("Failed to free dapl_processes[%d].srvr_psp",i);
    //	}

    ret = dat_evd_free(dto_evd);
    if( ret != DAT_SUCCESS ) dbg_err("dapl_finalize(): Failed to free dto_evd");

    ret = dat_evd_free(cevd);
    if( ret != DAT_SUCCESS ) dbg_err("dapl_finalize(): Failed to free cevd");

    free(dapl_processes);
    ret=dat_ia_close(ia, DAT_CLOSE_GRACEFUL_FLAG);
    if (ret != DAT_SUCCESS){
#ifdef DEBUG
        log_dat_err(ret,"dapl_finalize(): Failed to GRACEFULLY close ia");
#endif
        //		dbg_err("Failed to GRACEFULLY close ia");
        //		ret = dat_ia_close (ia, DAT_CLOSE_ABRUPT_FLAG);
        //		if (ret != DAT_SUCCESS)
        //			log_err("Could not close IA");
    }

    htable_free(reqtable);
    htable_free(ledger_reqtable);
    buffertable_finalize();
    free(dapl_device);
    // FIXME: free requests mtx and cond variables; also mtxs on list entries.

    //exit:
    ret = -1 * !!(ret^DAT_SUCCESS);
    return ret;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_wait_connect(int nw, int nc) {
    int num_connecting, num_waiting;
    DAT_RETURN retval;

    ctr_info(" > dapl_wait_connect()");

    num_waiting = nw;
    num_connecting = nc;

    while(num_waiting > 0 || num_connecting > 0) {
        DAT_EVENT event;
        DAT_COUNT count;

        retval = dat_evd_wait(cevd, 3*60*1000*1000, 1, &event, &count);

        if( (retval == DAT_TIMEOUT_EXPIRED) || (retval == DAT_ABORT) ){
            log_dat_err(retval, "dapl_setup_listeners(): Failed to setup listeners.  Exiting");
            goto error_exit;
        }

        if (retval != DAT_SUCCESS) {
            log_dat_err(retval, "dapl_setup_listeners(): Failed to to get event");
            goto error_exit;
        }

        if (event.event_number == DAT_CONNECTION_REQUEST_EVENT) {
            int i, type;

            i = dapl_connqual2proc(event.event_data.cr_arrival_event_data.conn_qual, &type);

            dbg_info("Got connection request event from %d/%llu", i, (unsigned long long int) event.event_data.cr_arrival_event_data.conn_qual);

            retval = dat_cr_accept(event.event_data.cr_arrival_event_data.cr_handle, dapl_processes[i].srvr_ep, 0, NULL);
            if (retval != DAT_SUCCESS) {
                log_dat_err(retval, "dapl_setup_listeners(): Accept failed for %d", i);
                goto error_exit;
            }
        } else if (event.event_number == DAT_CONNECTION_EVENT_ESTABLISHED) {
            // here we either got a connection established from someone we're connecting to or from someone connecting to us
            int i, is_connecting;

            i = dapl_ep2proc(event.event_data.connect_event_data.ep_handle, &is_connecting);

            if (i >= 0) {
                if (is_connecting) {
                    dbg_info("Got connection established event for server: %d", i);
                    num_connecting--;
                } else {
                    dbg_info("Got connection established event for client: %d", i);
                    num_waiting--;
                }
            }
        }

        dbg_info("Remaining: %d/%d", num_connecting, num_waiting);
    }

    return 0;

    error_exit:
    return -1;
}

int dapl_setup_listeners(int myrank) {
    int i, rnd, n;
    DAT_RETURN retval;
    DAT_CONN_QUAL conn_qual;
    MPI_Request *rreq;

    ctr_info(" > dapl_setup_listeners()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_setup_listeners(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // get ready to receive all the connect info
    rreq = (MPI_Request *)malloc( 2*dapl_processes_count * sizeof(MPI_Request) );
    if( !rreq )
        goto error_exit;
    memset(rreq, 0, 2*dapl_processes_count*sizeof(MPI_Request));

    for(i = 0; i < dapl_processes_count; i++) {

        dbg_info("dapl_setup_listeners(): going to receive information from %d", i);

        n = MPI_Irecv(&(dapl_processes[i].sa), sizeof(dapl_processes[i].sa), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_setup_listeners(): Failed to receive interface address from process %d", i);
            goto error_exit;
        }

        n = MPI_Irecv(&(dapl_processes[i].clnt_conn_qual), sizeof(dapl_processes[i].clnt_conn_qual), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i+1]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_setup_listeners(): Failed to receive connection qualifier from process %d", i);
            goto error_exit;
        }
    }

    srand((unsigned int)time(NULL));
    rnd = 1+(int)rand();
    // This creates a limit of 2^29 for the number of tasks.
    conn_qual = ((uint64_t)myrank+1) << 35 | ((uint64_t)rnd << 3);

    for(i = 0; i < dapl_processes_count; i++) {

        conn_qual++;

        dbg_info("dapl_setup_listeners(): creating listener: %d/%llu", i, (unsigned long long) conn_qual);

        if (dapl_create_listener(ia, pz, &conn_qual, cevd, dto_evd, &dapl_processes[i].srvr_ep, &dapl_processes[i].srvr_psp) != 0) {
            log_err("dapl_setup_listeners(): Couldn't create listener for process %d", i);
            goto error_exit;
        }

        dapl_processes[i].srvr_conn_qual = conn_qual;

        dbg_info("dapl_setup_listeners(): sending data to %d", i);

        //dbg_info("%s", inet_ntoa(((struct sockaddr_in *)self_addr->ifa_addr)->sin_addr));

        // send across the address
        n = MPI_Send(self_addr->ifa_addr, sizeof(*(self_addr->ifa_addr)), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_setup_listeners(): Couldn't send interface address to process %d", i);
            goto error_exit;
        }

        n = MPI_Send(&conn_qual, sizeof(conn_qual), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_setup_listeners(): Couldn't send connection qualifer to process %d", i);
            goto error_exit;
        }

        dbg_info("dapl_setup_listeners(): transmitted data");

    }

    // now we wait until Irecvs complete
    if (MPI_Waitall(2*dapl_processes_count, rreq, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        log_err("dapl_setup_listeners(): Couldn't wait() for control info from task %d", i);
        goto error_exit;
    }

    dbg_info("Waiting for initial barrier");

    // wait for everyone to setup their listeners
    MPI_Barrier(_photon_comm);

    for(i = 0; i < dapl_processes_count; i++) {
        dbg_info("dapl_setup_listeners(): connecting to %d/%llu", i, (unsigned long long int) dapl_processes[i].clnt_conn_qual);

        retval = dat_client_connect(ia, pz, (struct sockaddr *) &(dapl_processes[i].sa), dapl_processes[i].clnt_conn_qual, cevd, dto_evd, &dapl_processes[i].clnt_ep);
        if (retval != DAT_SUCCESS) {
            log_dat_err(retval, "dapl_setup_listeners(): Failed to connect to process %d", i);
            goto error_exit;
        }
    }

    if (dapl_wait_connect(dapl_processes_count, dapl_processes_count) != 0) {
        log_err("dapl_setup_listeners(): Couldn't complete all connections");
        goto error_exit;
    }

    // wait for everyone to make all their requisite connections
    dbg_info("Waiting on final barrier");
    MPI_Barrier(_photon_comm);

    free(rreq);

    return 0;

    error_exit:

    return -1;

}


///////////////////////////////////////////////////////////////////////////////
int dapl_exchange_ri_ledgers() {
    int i;
    MPI_Request *rreq;
    DAT_VADDR *va;

    ctr_info(" > dapl_exchange_ri_ledgers()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_exchange_ri_ledgers(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    va = (DAT_VADDR *)malloc( dapl_processes_count*sizeof(DAT_VADDR) );
    rreq = (MPI_Request *)malloc( 2*dapl_processes_count * sizeof(MPI_Request) );
    if( !va || !rreq ){
        log_err("dapl_exchange_ri_ledgers(): Cannot malloc temporary message buffers\n");
        return -1;
    }
    memset(va, 0, dapl_processes_count*sizeof(DAT_VADDR));
    memset(rreq, 0, 2*dapl_processes_count*sizeof(MPI_Request));

    // Recv the receive-info ledger context and pointers.  The context is also used for the send-info ledgers.
    for(i = 0; i < dapl_processes_count; i++) {
        int n;

        n = MPI_Irecv(&dapl_processes[i].remote_rcv_info_ledger->remote.context, sizeof(DAT_RMR_CONTEXT), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't send receive info ledger to process %d", i);
            continue;
        }

        n = MPI_Irecv(&(va[i]), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i+1]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't receive receive info ledger to process %d", i);
            continue;
        }
    }

    // wait for everyone to do their Irecvs
    MPI_Barrier(_photon_comm);

    // Send the receive-info ledger context and pointers
    for(i = 0; i < dapl_processes_count; i++) {
        int n;
        DAT_VADDR va;

        n = MPI_Send(&shared_storage->rmr_context, sizeof(DAT_RMR_CONTEXT), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't send receive info ledger to process %d", i);
            continue;
        }

        va = (DAT_VADDR) dapl_processes[i].local_rcv_info_ledger->entries;

        dbg_info("Transmitting rcv_info ledger info to %d: %p", i, (void *)va);

        n = MPI_Send(&(va), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't send receive info ledger to process %d", i);
            continue;
        }
    }

    // now we wait until Irecvs complete
    if (MPI_Waitall(2*dapl_processes_count, rreq, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        log_err("dapl_exchange_ri_ledgers(): Couldn't wait() for control info from task %d", i);
        return -1;
    }

    for(i = 0; i < dapl_processes_count; i++) {
        dbg_info("Receiving rcv_info ledger info from %d: %p", i, (void *)va[i]);
        dapl_processes[i].remote_rcv_info_ledger->remote.address = va[i];
        // snd_info and rcv_info ledgers are all stored in the same
        // contiguous memory region and share a common "context"
        dapl_processes[i].remote_snd_info_ledger->remote.context = dapl_processes[i].remote_rcv_info_ledger->remote.context;
    }

    memset(va, 0, dapl_processes_count*sizeof(DAT_VADDR));
    memset(rreq, 0, dapl_processes_count*sizeof(MPI_Request));

    // Receive the send-info ledger pointers
    for(i = 0; i < dapl_processes_count; i++) {
        int n;

        n = MPI_Irecv(&(va[i]), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm, &rreq[i]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't receive send info ledger from process %d", i);
            continue;
        }
    }

    // Send the send-info ledger pointers
    for(i = 0; i < dapl_processes_count; i++) {
        int n;
        DAT_VADDR va;

        va = (DAT_VADDR) dapl_processes[i].local_snd_info_ledger->entries;

        dbg_info("Transmitting snd_info ledger info to %d: %p", i, (void *)va);

        n = MPI_Send(&(va), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_ri_ledgers(): Couldn't send send info ledger to process %d", i);
            continue;
        }
    }

    // now we wait until Irecvs complete
    if (MPI_Waitall(dapl_processes_count, rreq, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        log_err("dapl_exchange_ri_ledgers(): Couldn't wait() for control info from task %d", i);
        return -1;
    }

    for(i = 0; i < dapl_processes_count; i++) {
        dbg_info("Received snd_info ledger info from %d: %p", i, (void *)va[i]);
        dapl_processes[i].remote_snd_info_ledger->remote.address = va[i];
    }

    free(va);
    free(rreq);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_setup_ri_ledgers(char *buf, int num_entries) {
    int i;
    int ledger_size, offset;

    ctr_info(" > dapl_setup_ri_ledgers()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_setup_ri_ledgers(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    ledger_size = sizeof(dapl_ri_ledger_entry_t) * num_entries;

    // Allocate the receive info ledgers
    for(i = 0; i < (dapl_processes_count + _photon_forwarder); i++) {
        dbg_info("allocating rcv info ledger for %d: %p", i, (buf + ledger_size * i));
        dbg_info("Offset: %d", ledger_size * i);

        // allocate the ledger
        dapl_processes[i].local_rcv_info_ledger = dapl_ri_ledger_create_reuse((dapl_ri_ledger_entry_t * ) (buf + ledger_size * i), num_entries);
        if (!dapl_processes[i].local_rcv_info_ledger) {
            log_err("dapl_setup_ri_ledgers(): couldn't create local rcv info ledger for process %d", i);
            return -1;
        }

        dbg_info("allocating remote ri ledger for %d: %p", i, buf + ledger_size * dapl_processes_count + ledger_size * i);
        dbg_info("Offset: %d", ledger_size * dapl_processes_count + ledger_size * i);

        dapl_processes[i].remote_rcv_info_ledger = dapl_ri_ledger_create_reuse((dapl_ri_ledger_entry_t * ) (buf + ledger_size * dapl_processes_count + ledger_size * i), num_entries);
        if (!dapl_processes[i].remote_rcv_info_ledger) {
            log_err("dapl_setup_ri_ledgers(): couldn't create remote rcv info ledger for process %d", i);
            return -1;
        }
    }

    // Allocate the send info ledgers
    offset = 2 * ledger_size * dapl_processes_count;
    for(i = 0; i < (dapl_processes_count + _photon_forwarder); i++) {
        dbg_info("allocating snd info ledger for %d: %p", i, (buf + offset + ledger_size * i));
        dbg_info("Offset: %d", offset + ledger_size * i);

        // allocate the ledger
        dapl_processes[i].local_snd_info_ledger = dapl_ri_ledger_create_reuse((dapl_ri_ledger_entry_t * ) (buf + offset + ledger_size * i), num_entries);
        if (!dapl_processes[i].local_snd_info_ledger) {
            log_err("dapl_setup_ri_ledgers(): couldn't create local snd info ledger for process %d", i);
            return -1;
        }

        dbg_info("allocating remote ri ledger for %d: %p", i, buf + offset + ledger_size * dapl_processes_count + ledger_size * i);
        dbg_info("Offset: %d", offset + ledger_size * dapl_processes_count + ledger_size * i);

        dapl_processes[i].remote_snd_info_ledger = dapl_ri_ledger_create_reuse((dapl_ri_ledger_entry_t * ) (buf + offset + ledger_size * dapl_processes_count + ledger_size * i), num_entries);
        if (!dapl_processes[i].remote_snd_info_ledger) {
            log_err("dapl_setup_ri_ledgers(): couldn't create remote snd info ledger for process %d", i);
            return -1;
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_exchange_FIN_ledger() {
    int i;
    MPI_Request *rreq;
    DAT_VADDR *va;

    ctr_info(" > dapl_exchange_FIN_ledger()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_exchange_FIN_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    va = (DAT_VADDR *)malloc( dapl_processes_count*sizeof(DAT_VADDR) );
    rreq = (MPI_Request *)malloc( 2*dapl_processes_count * sizeof(MPI_Request) );
    if( !va || !rreq ){
        log_err("dapl_exchange_FIN_ledger(): Cannot malloc temporary message buffers\n");
        return -1;
    }
    memset(va, 0, dapl_processes_count*sizeof(DAT_VADDR));
    memset(rreq, 0, 2*dapl_processes_count*sizeof(MPI_Request));

    for(i = 0; i < dapl_processes_count; i++) {
        int n;

        n = MPI_Irecv(&dapl_processes[i].remote_FIN_ledger->remote.context, sizeof(DAT_RMR_CONTEXT), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
            continue;
        }

        n = MPI_Irecv(&(va[i]), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm, &rreq[2*i+1]);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
            continue;
        }
    }

    // wait for everyone to do their Irecvs
    MPI_Barrier(_photon_comm);

    for(i = 0; i < dapl_processes_count; i++) {
        int n;
        DAT_VADDR va;

        n = MPI_Send(&shared_storage->rmr_context, sizeof(DAT_RMR_CONTEXT), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_FIN_ledger(): Couldn't send rdma send ledger to process %d", i);
            continue;
        }

        va = (DAT_VADDR) dapl_processes[i].local_FIN_ledger->entries;

        n = MPI_Send(&(va), sizeof(DAT_VADDR), MPI_BYTE, i, 0, _photon_comm);
        if (n != MPI_SUCCESS) {
            log_err("dapl_exchange_FIN_ledger(): Couldn't send rdma info ledger to process %d", i);
            continue;
        }
    }
    // now we wait until Irecvs complete
    if (MPI_Waitall(2*dapl_processes_count, rreq, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        log_err("dapl_exchange_FIN_ledger(): Couldn't wait() for control info from task %d", i);
        return -1;
    }

    for(i = 0; i < dapl_processes_count; i++) {
        dapl_processes[i].remote_FIN_ledger->remote.address = va[i];
    }

    free(rreq);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_setup_FIN_ledger(char *buf, int num_entries) {
    int i;
    int ledger_size;

    ctr_info(" > dapl_setup_FIN_ledger()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_setup_FIN_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    ledger_size = sizeof(dapl_rdma_FIN_ledger_entry_t) * num_entries;

    for(i = 0; i < (dapl_processes_count + _photon_forwarder); i++) {
        // allocate the ledger
        dbg_info("allocating local FIN ledger for %d", i);

        dapl_processes[i].local_FIN_ledger = dapl_rdma_FIN_ledger_create_reuse((dapl_rdma_FIN_ledger_entry_t *) (buf + ledger_size * i), num_entries);
        if (!dapl_processes[i].local_FIN_ledger) {
            log_err("dapl_setup_FIN_ledger(): couldn't create local FIN ledger for process %d", i);
            return -1;
        }

        dbg_info("allocating remote FIN ledger for %d", i);

        dapl_processes[i].remote_FIN_ledger = dapl_rdma_FIN_ledger_create_reuse((dapl_rdma_FIN_ledger_entry_t *) (buf + ledger_size * dapl_processes_count + ledger_size * i), num_entries);
        if (!dapl_processes[i].remote_FIN_ledger) {
            log_err("dapl_setup_FIN_ledger(): couldn't create remote FIN ledger for process %d", i);
            return -1;
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_register_buffer(char *buffer, int buffer_size) {
    static int first_time = 1;
    dapl_buffer_t *db;

    ctr_info(" > dapl_register_buffer(%p, %d)",buffer, buffer_size);

    if( _curr_cookie_count == 0 ){
        struct mem_register_req *mem_reg_req;
        if( first_time ){
            SLIST_INIT(&pending_mem_register_list);
            first_time = 0;
        }
        mem_reg_req = malloc( sizeof(struct mem_register_req) );
        mem_reg_req->buffer = buffer;
        mem_reg_req->buffer_size = buffer_size;

        SLIST_INSERT_HEAD(&pending_mem_register_list, mem_reg_req, list);
        dbg_info("dapl_register_buffer(): called before init, queueing buffer info");
        goto normal_exit;
    }

    if (buffertable_find_exact((void *)buffer, buffer_size, &db) == 0) {
        dbg_info("dapl_register_buffer(): we had an existing buffer, reusing it");
        db->ref_count++;
        goto normal_exit;
    }

    db = dapl_buffer_create(buffer, buffer_size);
    if (!db) {
        log_err("Couldn't register shared storage");
        goto error_exit;
    }

    dbg_info("dapl_register_buffer(): created buffer: %p", db);

    if (dapl_buffer_register(db, ia, pz) != 0) {
        log_err("Couldn't register buffer");
        goto error_exit_db;
    }

    dbg_info("dapl_register_buffer(): registered buffer");

    if (buffertable_insert(db) != 0) {
        goto error_exit_db;
    }

    dbg_info("dapl_register_buffer(): added buffer to hash table");

    normal_exit:
    return 0;
    error_exit_db:
    dapl_buffer_free(db);
    error_exit:
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_unregister_buffer(char *buffer, int size) {
    dapl_buffer_t *db;

    ctr_info(" > dapl_unregister_buffer()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_unregister_buffer(): Library not initialized.  Call photon_init() first");
        goto error_exit;
    }

    if (buffertable_find_exact((void *)buffer, size, &db) != 0) {
        dbg_info("dapl_unregister_buffer(): no such buffer is registered");
        return 0;
    }

    if (--(db->ref_count) == 0) {
        if (dapl_buffer_unregister(db) != 0) {
            goto error_exit;
        }
        buffertable_remove( db );
        dapl_buffer_free(db);
    }

    return 0;

    error_exit:
    return -1;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//           Some Utility Functions to wait for specific events              //
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
static int __dapl_wait_dto(DAT_TIMEOUT timeout, int *ret_proc, uint32_t *ret_cookie, int *is_error, int *timed_out) {
    DAT_RETURN retval;
    DAT_COUNT count;
    DAT_EVENT event;

    if (ret_proc == NULL || ret_cookie == NULL) {
        return -1;
    }

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_wait_dto(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    while(1) {
        // try to grab an event from the sr queue. if we have one, fix the requests
        retval = dat_evd_wait(dto_evd, timeout, 1, &event, &count);
        if (retval == DAT_SUCCESS) {
            uint32_t proc, cookie_arrived;

            // we should probably check for close events or similar
            if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
                log_dat_err(retval, "Event is not a completion");
                continue;
            }

            cookie_arrived = (uint32_t) (event.event_data.dto_completion_event_data.user_cookie.as_64<<32>>32);
            proc = (uint32_t) (event.event_data.dto_completion_event_data.user_cookie.as_64>>32);

            dbg_info("Received cookie:%d from:%u as_64:%llu", cookie_arrived, proc, (unsigned long long)event.event_data.dto_completion_event_data.user_cookie.as_64);

            if (event.event_data.dto_completion_event_data.status != DAT_DTO_SUCCESS) {
                log_dto_err(event.event_data.dto_completion_event_data.status, "__dapl_wait_dto(): Request %lu did not complete successfully", cookie_arrived);
                *is_error = 1;
            } else {
                *is_error = 0;
            }

            *ret_cookie = cookie_arrived;
            *ret_proc = proc;
            return 0;
        } else if (retval == DAT_TIMEOUT_EXPIRED) {
            *timed_out = 1;
            return -1;
        } else {
            log_dat_err(retval, "__dapl_wait_dto(): Error: received something bad from the event queue");
            return -1;
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
// __dapl_nbpop_dto() calls dat_evd_wait() with timeout=0 and returns:
// -1 if an error occured
//  0 if an event was in the queue and was successfully poped
//  1 if no completion even was in event queue
static int __dapl_nbpop_dto(int *ret_proc, uint32_t *ret_cookie, int *is_error) {
    DAT_RETURN retval;
    DAT_COUNT count;
    DAT_EVENT event;

    ctr_info(" > __dapl_nbpop_dto()");

    if (ret_proc == NULL || ret_cookie == NULL) {
        return -1;
    }

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_nbpop_dto(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // See if there is an event in the sr queue. if there is one, grab it and fix the requests
    retval = dat_evd_wait(dto_evd, 0, 1, &event, &count);
    if (retval == DAT_SUCCESS) {
        uint32_t proc, cookie_arrived;

        // we should probably check for close events or similar
        if (event.event_number != DAT_DTO_COMPLETION_EVENT) {
            return 1;
        }

        cookie_arrived = (uint32_t) (event.event_data.dto_completion_event_data.user_cookie.as_64<<32>>32);
        proc = (uint32_t) (event.event_data.dto_completion_event_data.user_cookie.as_64>>32);

        dbg_info("__dapl_nbpop_dto(): Received Cookie: proc:%u cookie:%u as_64:%llu", proc, cookie_arrived, (unsigned long long)event.event_data.dto_completion_event_data.user_cookie.as_64);

        if (event.event_data.dto_completion_event_data.status != DAT_DTO_SUCCESS) {
            log_dto_err(event.event_data.dto_completion_event_data.status, "__dapl_nbpop_dto(): Request %lu did not complete successfully", cookie_arrived);
            *is_error = 1;
        } else {
            *is_error = 0;
        }

        *ret_cookie = cookie_arrived;
        *ret_proc = proc;
        dbg_info("__dapl_nbpop_dto(): returning 0");
        return 0;
    } else if (retval == DAT_TIMEOUT_EXPIRED) {
        dbg_info("__dapl_nbpop_dto(): returning 1");
        return 1;
    } else {
        log_dat_err(retval, "__dapl_nbpop_dto(): Error: received something bad from the event queue");
        dbg_info("__dapl_nbpop_dto(): returning -1");
        return -1;
    }

    dbg_info("__dapl_nbpop_dto(): returning -1");
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
// this function can be used to wait for a DTO event to occur
static int __dapl_wait_one() {
    dbg_info("__dapl_wait_one(): remaining: %d+%d", htable_count(reqtable),handshake_rdma_write_count);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_wait_one(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if ( (htable_count(reqtable) == 0) && (handshake_rdma_write_count == 0) ) {
        dbg_info("No events on queue, or handshake writes pending to wait on");
        return -1;
    }

    while(1) {
        int is_error;
        int timed_out;
        int proc;
        uint32_t cookie;

        if (__dapl_wait_dto(DAT_TIMEOUT_INFINITE, &proc, &cookie, &is_error, &timed_out) == 0) {
            dapl_req_t *tmp_req;
            void *test;

            dbg_info("__dapl_wait_one(): received sr: %u/%u", proc, cookie);

            if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
                tmp_req = test;

                tmp_req->state = (is_error)?REQUEST_FAILED:REQUEST_COMPLETED;
                SAFE_LIST_REMOVE(tmp_req, list);
                SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
            }else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
                if( --handshake_rdma_write_count < 0 ){
                    log_err("__dapl_wait_one(): handshake_rdma_write_count is negative");
                }
            }

            break;
        } else {
            log_err("__dapl_wait_one(): Something bad happened to the event queue");
        }
    }

    return 0;
}

#ifdef WITH_XSP
int dapl_xsp_test(uint32_t request, int *flag, int *type, void *status) {

    return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// dapl_test() is a nonblocking operation that checks the event queue to see if
// the event associated with the "request" parameter has completed.  It returns:
//  0 if the event associated with "request" was in the queue and was successfully poped.
//  1 if "request" was not in the request tables.  This is not an error if dapl_test()
//    is called in a loop and is called multiple times for each request.
// -1 if an error occured.
//
// When dapl_test() returns zero (success) the "flag" parameter has the value:
//  0 if the event that was poped does not correspond to "request", or if none of the operations completed.
//  1 if the event that was poped does correspond to "request".
//
//  When dapl_test() returns 0 and flag==0 the "status" structure is also filled
//  unless the constant "MPI_STATUS_IGNORE" was passed as the "status" argument.
//
// Regardless of the return value and the value of "flag", the parameter "type"
// will be set to 0 (zero) when the request is of type event and 1 (one) when the
// request is of type ledger.
int dapl_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
    dapl_req_t *req;
    void *test;
    int ret_val;

    ctr_info(" > dapl_test(%d)",request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_test(): Library not initialized.  Call photon_init() first");
        dbg_info("dapl_test(): returning -1");
        return -1;
    }

    if (htable_lookup(reqtable, (uint64_t)request, &test) != 0){
        if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0){
            dbg_info("Request is not in either request-table");
            // Unlike dapl_wait(), we might call dapl_test() multiple times on a request,
            // e.g., in an unguarded loop.  flag==-1 will signify that the operation is
            // not pending.  This means, it might be completed, it might have never been
            // issued.  It's up to the application to guarantee correctness, by keeping
            // track, of  what's going on.  Unless you know what you are doing, consider
            // (flag==-1 && return_value==1) to be an error case.
            dbg_info("dapl_test(): returning 1, flag:-1");
            *flag = -1;
            return 1;
        }
    }

    req = test;

    *flag = 0;
    if (req->type == LEDGER){
        if( type != NULL ) *type = 1;
        ret_val = __dapl_nbpop_ledger(req);
    }else{
        if( type != NULL ) *type = 0;
        ret_val = __dapl_nbpop_evd(req);
    }

    if( !ret_val ){
        *flag = 1;
        if( status != MPI_STATUS_IGNORE ){
            status->MPI_SOURCE = req->proc;
            status->MPI_TAG = req->tag;
            status->MPI_ERROR = 0; // FIXME: Make sure that "0" means success?
        }
        dbg_info("dapl_test(): returning 0, flag:1");
        return 0;
    }else if( ret_val > 0 ){
        dbg_info("dapl_test(): returning 0, flag:0");
        *flag = 0;
        return 0;
    }else{
        dbg_info("dapl_test(): returning -1, flag:0");
        *flag = 0;
        return -1;
    }
}


///////////////////////////////////////////////////////////////////////////////
// returns
// -1 if an error occured.
//  0 if the FIN associated with "req" was found and poped, or
//    the "req" is not pending.  This is not an error, if a previous
//    call to __dapl_nbpop_ledger() poped the FIN that corresponds to "req".
//  1 if the request is pending and the FIN has not arrived yet
static int __dapl_nbpop_ledger(dapl_req_t *req){
    void *test;
    int curr, i=-1;

    ctr_info(" > __dapl_nbpop_ledger(%d)",req->id);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_nbpop_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    //#ifdef DEBUG
    //	for(i = 0; i < dapl_processes_count; i++) {
    //		dapl_rdma_FIN_ledger_entry_t *curr_entry;
    //		curr = dapl_processes[i].local_FIN_ledger->curr;
    //		curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[curr]);
    //		dbg_info("__dapl_nbpop_ledger() curr_entry(proc==%d)=%p",i,curr_entry);
    //	}
    //#endif

    if(req->state == REQUEST_PENDING) {

        // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
        for(i = 0; i < dapl_processes_count; i++) {
            dapl_rdma_FIN_ledger_entry_t *curr_entry;
            curr = dapl_processes[i].local_FIN_ledger->curr;
            curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[curr]);
            if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
                dbg_info("__dapl_nbpop_ledger(): Found curr:%d req:%u while looking for req:%u", curr, curr_entry->request, req->id);
                curr_entry->header = 0;
                curr_entry->footer = 0;

                if (curr_entry->request == req->id){
                    req->state = REQUEST_COMPLETED;
                    dbg_info("__dapl_nbpop_ledger(): removing RDMA i:%u req:%u", i, req->id);
                    htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
                    SAFE_LIST_REMOVE(req, list);
                    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
                    int num = dapl_processes[i].local_FIN_ledger->num_entries;
                    int new_curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % num;
                    dapl_processes[i].local_FIN_ledger->curr = new_curr;
                    dbg_info("__dapl_nbpop_ledger(): returning 0");
                    return 0;
                } else {
                    dapl_req_t *tmp_req;

                    if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
                        tmp_req = test;

                        tmp_req->state = REQUEST_COMPLETED;
                        SAFE_LIST_REMOVE(tmp_req, list);
                        SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
                    }
                }

                int num = dapl_processes[i].local_FIN_ledger->num_entries;
                int new_curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % num;
                dapl_processes[i].local_FIN_ledger->curr = new_curr;
            }
        }
    }else{
        dbg_info("__dapl_nbpop_ledger(): req->state != PENDING, returning 0");
        return 0;
    }

    dbg_info("__dapl_nbpop_ledger(): at end, returning %d",(req->state == REQUEST_COMPLETED)?0:1);
    return (req->state == REQUEST_COMPLETED)?0:1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_wait(uint32_t request) {
    dapl_req_t *req;
    void *test;

    ctr_info(" > dapl_wait(%d)",request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (htable_lookup(reqtable, (uint64_t)request, &test) != 0){
        if (htable_lookup(ledger_reqtable, (uint64_t)request, &test) != 0){
            log_err("dapl_wait(): Wrong request value, operation not in table");
            return -1;
        }
    }

    req = test;

    if (req->type == LEDGER) {

#ifdef PHOTON_MULTITHREADED
        return __dapl_wait_ledger_mt(req);
#else
        return __dapl_wait_ledger(req);
#endif

    } else
        return __dapl_wait_evd(req);
}


///////////////////////////////////////////////////////////////////////////////
static int __dapl_wait_evd(dapl_req_t *req) {

    ctr_info(" > __dapl_wait_evd(%d)",req->id);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_wait_evd(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // I think here we should check if the request is in the unreaped_evd_reqs_list
    // (i.e. already completed) and if so, move it to the free_reqs_list:
    //	if(req->state == REQUEST_COMPLETED){
    //		LIST_REMOVE(req, list);
    //		LIST_INSERT_HEAD(&free_reqs_list, req, list);
    //    }

    while(req->state == REQUEST_PENDING) {
        int proc;
        uint32_t cookie;
        int is_error;
        int timed_out;

        if (__dapl_wait_dto(DAT_TIMEOUT_INFINITE, &proc, &cookie, &is_error, &timed_out) == 0) {
            if (cookie == req->id) {
                req->state = (is_error)?REQUEST_FAILED:REQUEST_COMPLETED;

                dbg_info("dapl_wait_evd(): removing sr: %u/%u", proc, cookie);
                htable_remove(reqtable, (uint64_t)req->id, NULL);
                SAFE_LIST_REMOVE(req, list);
                SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
                dbg_info("dapl_wait_evd(): %d requests left in reqtable", htable_count(reqtable));
            } else if (cookie != NULL_COOKIE) {
                void *test;

                if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
                    dapl_req_t *tmp_req = test;

                    tmp_req->state = (is_error)?REQUEST_FAILED:REQUEST_COMPLETED;
                    SAFE_LIST_REMOVE(tmp_req, list);
                    SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
                }else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
                    if( --handshake_rdma_write_count < 0 ){
                        log_err("__dapl_wait_evd(): handshake_rdma_write_count is negative");
                    }
                }
            }
        } else {
            log_err("__dapl_wait_evd(): Something bad happened to the event queue");
            goto error_exit;
        }
    }

    return (req->state == REQUEST_COMPLETED)?0:-1;

    error_exit:
    exit( dapl_finalize() );
}


///////////////////////////////////////////////////////////////////////////////
// __dapl_nbpop_evd() is non blocking and returns:
// -1 if an error occured.
//  0 if the request (req) specified in the argument has completed.
//  1 if either no event was in the queue, or there was an event but not for the specified request (req).
static int __dapl_nbpop_evd(dapl_req_t *req) {

    ctr_info(" > __dapl_nbpop_evd(%d)",req->id);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_nbpop_evd(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if(req->state == REQUEST_PENDING) {
        int proc;
        uint32_t cookie;
        int is_error;

        int error_occured = __dapl_nbpop_dto(&proc, &cookie, &is_error);
        if ( !error_occured ) {
            if (cookie == req->id) {
                req->state = (is_error)?REQUEST_FAILED:REQUEST_COMPLETED;

                dbg_info("dapl_nbpop_evd(): removing req from table proc:%u cookie:%u", proc, cookie);
                htable_remove(reqtable, (uint64_t)req->id, NULL);
                SAFE_LIST_REMOVE(req, list);
                SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
                dbg_info("dapl_nbpop_evd(): %d requests left in reqtable", htable_count(reqtable));
            } else if (cookie != NULL_COOKIE) {
                void *test;

                if (htable_lookup(reqtable, (uint64_t)cookie, &test) == 0) {
                    dapl_req_t *tmp_req = test;

                    tmp_req->state = (is_error)?REQUEST_FAILED:REQUEST_COMPLETED;
                    SAFE_LIST_REMOVE(tmp_req, list);
                    SAFE_LIST_INSERT_HEAD(&unreaped_evd_reqs_list, tmp_req, list);
                }else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
                    if( --handshake_rdma_write_count < 0 ){
                        log_err("__dapl_nbpop_evd(): handshake_rdma_write_count is negative");
                    }
                }
            }
        } else if(error_occured > 0) {
            dbg_info("dapl_nbpop_evd(): error occured, returning %d", error_occured);
            return error_occured;
        } else {
            log_err("__dapl_nbpop_evd(): Something bad happened to the event queue");
            goto error_exit;
        }
    }

    dbg_info("__dapl_nbpop_evd(): returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
    return (req->state == REQUEST_COMPLETED)?0:1;

    error_exit:
    exit( dapl_finalize() );
}


///////////////////////////////////////////////////////////////////////////////
static int __dapl_wait_ledger(dapl_req_t *req) {
    void *test;
    int curr, i=-1;

    ctr_info(" > __dapl_wait_ledger(%d)",req->id);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_wait_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

#ifdef DEBUG
    for(i = 0; i < dapl_processes_count; i++) {
        dapl_rdma_FIN_ledger_entry_t *curr_entry;
        curr = dapl_processes[i].local_FIN_ledger->curr;
        curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[curr]);
        dbg_info("__dapl_wait_ledger() curr_entry(proc==%d)=%p",i,curr_entry);
    }
#endif
    while(req->state == REQUEST_PENDING) {

        // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
        for(i = 0; i < dapl_processes_count; i++) {
            dapl_rdma_FIN_ledger_entry_t *curr_entry;
            curr = dapl_processes[i].local_FIN_ledger->curr;
            curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[curr]);
            if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0
                    // FIXME: short-term fix to make this "thread-safer"
                    && curr_entry->request == req->id) {
                dbg_info("__dapl_wait_ledger() Found: %d/%u/%u", curr, curr_entry->request, req->id);
                curr_entry->header = 0;
                curr_entry->footer = 0;

                if (curr_entry->request == req->id){
                    req->state = REQUEST_COMPLETED;
                } else {
                    dapl_req_t *tmp_req;

                    if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
                        tmp_req = test;

                        tmp_req->state = REQUEST_COMPLETED;
                        SAFE_LIST_REMOVE(tmp_req, list);
                        SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
                    }
                }

                dapl_processes[i].local_FIN_ledger->curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % dapl_processes[i].local_FIN_ledger->num_entries;
            }
        }
    }
    dbg_info("dapl_wait_ledger(): removing RDMA: %u/%u", i, req->id);
    htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
    SAFE_LIST_REMOVE(req, list);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_info("dapl_wait_ledger(): %d requests left in reqtable", htable_count(ledger_reqtable));

    return (req->state == REQUEST_COMPLETED)?0:-1;
}

///////////////////////////////////////////////////////////////////////////////
int dapl_wait_remaining() {
    ctr_info(" > dapl_wait_remaining(): remaining: %d", htable_count(reqtable));

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_remaining(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // clear out the unrepead nodes
    LIST_LOCK(&unreaped_evd_reqs_list);
    while(LIST_FIRST(&unreaped_evd_reqs_list) != NULL) {
        dapl_req_t *req = LIST_FIRST(&unreaped_evd_reqs_list);

        htable_remove(reqtable, (uint64_t)req->id, NULL);

        dbg_info("dapl_wait_remaining(): removing unreaped request %u", req->id);

        LIST_REMOVE(req, list);
        LIST_INSERT_HEAD(&free_reqs_list, req, list);
    }
    LIST_UNLOCK(&unreaped_evd_reqs_list);

    while(htable_count(reqtable) != 0) {
        int proc;
        uint32_t cookie;
        int is_error;
        int timed_out;

        if (__dapl_wait_dto(DAT_TIMEOUT_INFINITE, &proc, &cookie, &is_error, &timed_out) == 0) {
            void *test;

            dbg_info("dapl_wait_remaining(): removing sr: %u/%u", proc, cookie);

            if (htable_remove(reqtable, (uint64_t)cookie, &test) == 0) {
                dapl_req_t *req = test;

                dbg_info("dapl_wait_remaining(): returned value: %p/%p", req, test);
                dbg_info("dapl_wait_remaining(): %d was an actual request, moving it to the free list", req->id);

                SAFE_LIST_REMOVE(req, list);
                SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
            }else if (htable_lookup(ledger_reqtable, (uint64_t)cookie, &test) == 0) {
                if( --handshake_rdma_write_count < 0 ){
                    log_err("__dapl_wait_one(): handshake_rdma_write_count is negative");
                }
            }

            dbg_info("dapl_wait_remaining(): %d requests left in reqtable", htable_count(reqtable));

        } else {
            log_err("dapl_wait_remaining(): Something bad happened to the event queue");
            goto error_exit;
        }
    }

    dbg_info("dapl_wait_remaining(): remaining: %d", htable_count(reqtable));

    return 0;

    error_exit:

    exit( dapl_finalize() );
}


///////////////////////////////////////////////////////////////////////////////
int dapl_wait_any(int *ret_proc, uint32_t *ret_req) {
    dbg_info("dapl_wait_any(): remaining: %d", htable_count(reqtable));

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_any(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (ret_req == NULL) {
        return -1;
    }

    if (htable_count(reqtable) == 0) {
        log_err("dapl_wait_any(): No events on queue to wait on");
        return -1;
    }

    while(1) {
        int is_error;
        int timed_out;
        int proc;
        uint32_t cookie;
        int existed;

        if (__dapl_wait_dto(DAT_TIMEOUT_INFINITE, &proc, &cookie, &is_error, &timed_out) == 0) {
            dapl_req_t *req;

            if (cookie != NULL_COOKIE) {
                void *test;
                dbg_info("dapl_wait_any(): removing sr: %u/%u", proc, cookie);
                existed = htable_remove(ledger_reqtable, (uint64_t)cookie, &test);
                req = test;
                SAFE_LIST_REMOVE(req, list);
                SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
            } else {
                existed = -1;
            }

            if (existed == -1) {
                continue;
            } else {
                *ret_req = cookie;
                *ret_proc = proc;
                return 0;
            }
        } else {
            log_err("dapl_wait_any(): Something bad happened to the event queue");
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
    static int i = -1; // this is static so we don't starve events in later processes

    dbg_info("dapl_wait_any(): remaining: %d", htable_count(reqtable));

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_any_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (ret_req == NULL || ret_proc == NULL) {
        return -1;
    }

    if (htable_count(ledger_reqtable) == 0) {
        log_err("dapl_wait_any_ledger(): No events on queue to wait_one()");
        return -1;
    }

    while(1) {
        int exists;

        //		i++;
        i=(i+1)%_photon_nproc;

        // check if an event occurred on the RDMA end of things
        dapl_rdma_FIN_ledger_entry_t *curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[dapl_processes[i].local_FIN_ledger->curr]);

        if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
            void *test;
            dbg_info("Wait All In: %d/%u", dapl_processes[i].local_FIN_ledger->curr, curr_entry->request);
            curr_entry->header = 0;
            curr_entry->footer = 0;

            exists = htable_remove(ledger_reqtable, (uint64_t)curr_entry->request, &test);
            if (exists != -1) {
                dapl_req_t *req;
                req = test;
                *ret_req = curr_entry->request;
                *ret_proc = i;
                SAFE_LIST_REMOVE(req, list);
                SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
                break;
            }

            dapl_processes[i].local_FIN_ledger->curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % dapl_processes[i].local_FIN_ledger->num_entries;
            dbg_info("Wait All Out: %d", dapl_processes[i].local_FIN_ledger->curr);
        }
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_wait_remaining_ledger() {
    time_t stime, etime;

    ctr_info(" > dapl_wait_remaining_ledger(): remaining: %d", htable_count(ledger_reqtable));

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_any_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // clear out the unrepead nodes
    LIST_LOCK(&unreaped_ledger_reqs_list);
    while(LIST_FIRST(&unreaped_ledger_reqs_list) != NULL) {
        dapl_req_t *req = LIST_FIRST(&unreaped_ledger_reqs_list);

        htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);

        dbg_info("dapl_wait_remaining(): removing unreaped rdma request %u", req->id);

        LIST_REMOVE(req, list);
        LIST_INSERT_HEAD(&free_reqs_list, req, list);
    }
    LIST_UNLOCK(&unreaped_ledger_reqs_list);

    stime = time(NULL);
    while(htable_count(ledger_reqtable) != 0) {
        int i;

        // check if an event occurred on the RDMA end of things
        for(i = 0; i < dapl_processes_count; i++) {
            dapl_rdma_FIN_ledger_entry_t *curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[dapl_processes[i].local_FIN_ledger->curr]);

            if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
                curr_entry->header = 0;
                curr_entry->footer = 0;

                dapl_processes[i].local_FIN_ledger->curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % dapl_processes[i].local_FIN_ledger->num_entries;

                dbg_info("dapl_wait_remaining(): removing rdma: %d/%u", i, curr_entry->request);

                htable_remove(ledger_reqtable, (uint64_t)curr_entry->request, NULL);
                if (htable_count(ledger_reqtable) == 0)
                    break;
            }
        }

        etime = time(NULL);
        if ((etime - stime) > 10) {
            dbg_info("Hash table count: %d", htable_count(ledger_reqtable));
            stime = etime;
        }
    }

    dbg_info("dapl_wait_remaining_ledger(): remaining: %d", htable_count(ledger_reqtable));

    return 0;
}

/////////////////////////////////////////
//  DAPL Two-Sided Sends/Recvs         //
/////////////////////////////////////////
int dapl_post_recv(int proc, char *ptr, uint32_t size, uint32_t *request) {
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    dapl_buffer_t *db;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_recv(%d)", proc);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_recv(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
        log_err("dapl_post_recv(): Requested send from ptr not in table");
        goto error_exit;
    }

    curr_cookie = get_inc_curr_cookie_count();
    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));
    dbg_info("Incremented current cookie to %d", curr_cookie+1);

    lmr.lmr_context = db->lmr_context;
    lmr.virtual_address = (uint64_t) ptr;
    lmr.segment_length = size;

    dbg_info("Posting recv for %d", proc);

    retval = dat_ep_post_recv(dapl_processes[proc].srvr_ep, 1, &lmr, cookie, DAT_COMPLETION_DEFAULT_FLAG);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_post_recv(): Failed to post recv");
        goto error_exit;
    }

    if( request != NULL ){
        dapl_req_t *req;

        *request = curr_cookie;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_recv(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        req->type = EVQUEUE;
        req->proc = proc;
        req->tag = -1;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("Inserting the SR recv request into the request table: %d/%p", curr_cookie, req);

        if (htable_insert(reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("dapl_post_recv(): Couldn't save request in hashtable");
        }
    }

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_post_send(int proc, char *ptr, uint32_t size, uint32_t *request) {
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    dapl_buffer_t *db;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_send(%d)", proc);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_send(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
        log_err("dapl_post_send(): Requested send from ptr not in table");
        goto error_exit;
    }

    curr_cookie = get_inc_curr_cookie_count();
    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));
    dbg_info("Incremented current cookie to %d", curr_cookie+1);

    lmr.lmr_context = db->lmr_context;
    lmr.virtual_address = (uint64_t) ptr;
    lmr.segment_length = size;

    dbg_info("Posting send for %d", proc);

    retval = dat_ep_post_send(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, DAT_COMPLETION_DEFAULT_FLAG);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_post_send(): Failed to post rdma_write()");
        goto error_exit;
    }

    if( request != NULL ){
        dapl_req_t *req;

        *request = curr_cookie;
        dbg_info("Inserting the SR send request into the request table: %d", curr_cookie);

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_recv(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        req->type = EVQUEUE;
        req->proc = proc;
        req->tag = -1;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        if (htable_insert(reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("dapl_post_send(): Couldn't save request in hashtable");
        }
    }

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
static time_t _tictoc(time_t stime, int proc){
    time_t etime;
    etime = time(NULL);
    if ((etime - stime) > 10) {
        if( proc >= 0 )
            dbg_info("Still waiting for a recv buffer from %d", proc);
        else
            dbg_info("Still waiting for a recv buffer from any peer");
        stime = etime;
    }
    return stime;
}
#endif


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//                DAPL One-Sided send()s/recv()s and handshake functions                //
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
int dapl_wait_recv_buffer_rdma(int proc, int tag) {
    dapl_remote_buffer_t *curr_remote_buffer;
    dapl_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
    int count;
#ifdef DEBUG
    time_t stime;
#endif
    int curr, still_searching;

    ctr_info(" > dapl_wait_recv_buffer_rdma(%d, %d)", proc, tag);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_recv_buffer_rdma(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // If we've received a Rendezvous-Start from processor "proc" that is still pending
    curr_remote_buffer = dapl_processes[proc].curr_remote_buffer;
    if ( curr_remote_buffer->request != NULL_COOKIE ) {
        // If it is for the same tag, return without looking
        if ( curr_remote_buffer->tag == tag ) {
            goto normal_exit;
        }else{ // Otherwise it's an error.  We should never process a Rendezvous-Start before
            // fully serving the previous ones.
            goto error_exit;
        }
    }

    dbg_info("dapl_wait_recv_buffer_rdma(): Spinning on info ledger looking for receive request");
    dbg_info("dapl_wait_recv_buffer_rdma(): curr == %d", dapl_processes[proc].local_rcv_info_ledger->curr);


    curr = dapl_processes[proc].local_rcv_info_ledger->curr;
    curr_entry = &(dapl_processes[proc].local_rcv_info_ledger->entries[curr]);

    dbg_info("dapl_wait_recv_buffer_rdma(): looking in position %d/%p", dapl_processes[proc].local_rcv_info_ledger->curr, curr_entry);

#ifdef DEBUG
    stime = time(NULL);
#endif
    count = 1;
    still_searching = 1;
    entry_iterator = curr_entry;
    do{
        while(entry_iterator->header == 0 || entry_iterator->footer == 0) {
#ifdef DEBUG
            stime = _tictoc(stime, proc);
#else
            ;
#endif
        }
        if( (tag < 0) || (entry_iterator->tag == tag ) ){
            still_searching = 0;
        }else{
            curr = (dapl_processes[proc].local_rcv_info_ledger->curr + count++) % dapl_processes[proc].local_rcv_info_ledger->num_entries;
            entry_iterator = &(dapl_processes[proc].local_rcv_info_ledger->entries[curr]);
        }
    }while(still_searching);

    // If it wasn't the first pending receive request, swap the one we will serve (entry_iterator) with
    // the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
    // (dapl_processes[proc].local_rcv_info_ledger->curr) and skip the request we will serve without losing any
    // pending requests.
    if( entry_iterator != curr_entry ){
        tmp_entry = *entry_iterator;
        *entry_iterator = *curr_entry;
        *curr_entry = tmp_entry;
    }

    dapl_processes[proc].curr_remote_buffer->request = curr_entry->request;
    dapl_processes[proc].curr_remote_buffer->context = curr_entry->context;
    dapl_processes[proc].curr_remote_buffer->address = curr_entry->address;
    dapl_processes[proc].curr_remote_buffer->size = curr_entry->size;
    dapl_processes[proc].curr_remote_buffer->tag = curr_entry->tag;

    dbg_info("dapl_wait_recv_buffer_rdma(): Request: %u", curr_entry->request);
    dbg_info("dapl_wait_recv_buffer_rdma(): Context: %u", curr_entry->context);
    dbg_info("dapl_wait_recv_buffer_rdma(): Address: %p", (void *)curr_entry->address);
    dbg_info("dapl_wait_recv_buffer_rdma(): Size: %u", curr_entry->size);
    dbg_info("dapl_wait_recv_buffer_rdma(): Tag: %d", curr_entry->tag);

    curr_entry->header = 0;
    curr_entry->footer = 0;

    dapl_processes[proc].local_rcv_info_ledger->curr = (dapl_processes[proc].local_rcv_info_ledger->curr + 1) % dapl_processes[proc].local_rcv_info_ledger->num_entries;

    dbg_info("dapl_wait_recv_buffer_rdma(): new curr == %d", dapl_processes[proc].local_rcv_info_ledger->curr);

    normal_exit:
    return 0;
    error_exit:
    return 1;

}


// dapl_wait_send_buffer_rdma() should never be called between a dapl_wait_recv_buffer_rdma()
// and the corresponding dapl_post_os_put(), or between an other dapl_wait_send_buffer_rdma()
// and the corresponding dapl_post_os_get() for the same proc.
// In other words if dapl_processes[proc].curr_remote_buffer is full, dapl_wait_send_buffer_rdma()
// should not be called.
int dapl_wait_send_buffer_rdma(int proc, int tag) {
    dapl_remote_buffer_t *curr_remote_buffer;
    dapl_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
    int count;
#ifdef DEBUG
    time_t stime;
#endif
    int curr, still_searching;

    ctr_info(" > dapl_wait_send_buffer_rdma(%d, %d)", proc, tag);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_send_buffer_rdma(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    // If we've received a Rendezvous-Start from processor "proc" that is still pending
    curr_remote_buffer = dapl_processes[proc].curr_remote_buffer;
    if ( curr_remote_buffer->request != NULL_COOKIE ) {
        // If it is for the same tag, return without looking
        if ( curr_remote_buffer->tag == tag ) {
            goto normal_exit;
        }else{ // Otherwise it's an error.  We should never process a Rendezvous-Start before
            // fully serving the previous ones.
            goto error_exit;
        }
    }

    curr = dapl_processes[proc].local_snd_info_ledger->curr;
    curr_entry = &(dapl_processes[proc].local_snd_info_ledger->entries[curr]);

    dbg_info("dapl_wait_send_buffer_rdma(): Spinning on info ledger looking for receive request");
    dbg_info("dapl_wait_send_buffer_rdma(): looking in position %d/%p", curr, curr_entry);

#ifdef DEBUG
    stime = time(NULL);
#endif
    count = 1;
    still_searching = 1;
    entry_iterator = curr_entry;
    do{
        while(entry_iterator->header == 0 || entry_iterator->footer == 0) {
#ifdef DEBUG
            stime = _tictoc(stime, proc);
#else
            ;
#endif
        }
        if( (tag < 0) || (entry_iterator->tag == tag ) ){
            still_searching = 0;
        }else{
            curr = (dapl_processes[proc].local_snd_info_ledger->curr + count++) % dapl_processes[proc].local_snd_info_ledger->num_entries;
            entry_iterator = &(dapl_processes[proc].local_snd_info_ledger->entries[curr]);
        }
    }while(still_searching);

    // If it wasn't the first pending receive request, swap the one we will serve (entry_iterator) with
    // the first pending (curr_entry) in the info ledger, so that we can increment the current pointer
    // (dapl_processes[proc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
    // pending requests.
    if( entry_iterator != curr_entry ){
        tmp_entry = *entry_iterator;
        *entry_iterator = *curr_entry;
        *curr_entry = tmp_entry;
    }

    dapl_processes[proc].curr_remote_buffer->request = curr_entry->request;
    dapl_processes[proc].curr_remote_buffer->context = curr_entry->context;
    dapl_processes[proc].curr_remote_buffer->address = curr_entry->address;
    dapl_processes[proc].curr_remote_buffer->size = curr_entry->size;
    dapl_processes[proc].curr_remote_buffer->tag = curr_entry->tag;

    dbg_info("dapl_wait_send_buffer_rdma(): Request: %u", curr_entry->request);
    dbg_info("dapl_wait_send_buffer_rdma(): Context: %u", curr_entry->context);
    dbg_info("dapl_wait_send_buffer_rdma(): Address: %p", (void *)curr_entry->address);
    dbg_info("dapl_wait_send_buffer_rdma(): Size: %u", curr_entry->size);
    dbg_info("dapl_wait_send_buffer_rdma(): Tag: %d", curr_entry->tag);

    curr_entry->header = 0;
    curr_entry->footer = 0;

    dapl_processes[proc].local_snd_info_ledger->curr = (dapl_processes[proc].local_snd_info_ledger->curr + 1) % dapl_processes[proc].local_snd_info_ledger->num_entries;

    dbg_info("dapl_wait_send_buffer_rdma(): new curr == %d", dapl_processes[proc].local_snd_info_ledger->curr);

    normal_exit:
    return 0;
    error_exit:
    return 1;

}


////////////////////////////////////////////////////////////////////////////////
//// dapl_wait_send_request_rdma() treats "tag == -1" as ANY_TAG
int dapl_wait_send_request_rdma(int tag) {
    dapl_ri_ledger_entry_t *curr_entry, *entry_iterator, tmp_entry;
    int count, iproc;
#ifdef DEBUG
    time_t stime;
#endif
    int curr, still_searching;

    ctr_info(" > dapl_wait_send_request_rdma(%d)", tag);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_wait_send_request_rdma(): Library not initialized.  Call photon_init() first");
        goto error_exit;
    }

    dbg_info("dapl_wait_send_request_rdma(): Spinning on send info ledger looking for send request");

    still_searching = 1;
    iproc = -1;
#ifdef DEBUG
    stime = time(NULL);
#endif
    do{
        iproc = (iproc+1)%dapl_processes_count;
        curr = dapl_processes[iproc].local_snd_info_ledger->curr;
        curr_entry = &(dapl_processes[iproc].local_snd_info_ledger->entries[curr]);
        dbg_info("dapl_wait_send_request_rdma(): looking in position %d/%p for proc %d", curr, curr_entry,iproc);

        count = 1;
        entry_iterator = curr_entry;
        // Some peers (procs) might have sent more than one send requests using different tags, so check them all.
        while(entry_iterator->header == 1 && entry_iterator->footer == 1) {
            if( (entry_iterator->address == (DAT_VADDR) NULL) && (entry_iterator->context == (DAT_RMR_CONTEXT) 0) &&
                    ((tag < 0) || (entry_iterator->tag == tag )) ){
                still_searching = 0;
                dbg_info("dapl_wait_send_request_rdma(): Found matching send request with tag %d from proc %d", tag, iproc);
                break;
            }else{
                dbg_info("dapl_wait_send_request_rdma(): Found non-matching send request with tag %d from proc %d", tag, iproc);
                curr = (dapl_processes[iproc].local_snd_info_ledger->curr + count) % dapl_processes[iproc].local_snd_info_ledger->num_entries;
                ++count;
                entry_iterator = &(dapl_processes[iproc].local_snd_info_ledger->entries[curr]);
            }
        }
#ifdef DEBUG
        stime = _tictoc(stime, -1);
#endif
    }while(still_searching);

    // If it wasn't the first pending send request, swap the one we will serve (entry_iterator) with
    // the first pending (curr_entry) in the send info ledger, so that we can increment the current pointer
    // (dapl_processes[iproc].local_snd_info_ledger->curr) and skip the request we will serve without losing any
    // pending requests.
    if( entry_iterator != curr_entry ){
        tmp_entry = *entry_iterator;
        *entry_iterator = *curr_entry;
        *curr_entry = tmp_entry;
    }

    curr_entry->header = 0;
    curr_entry->footer = 0;
    // NOTE:
    // curr_entry->request contains the curr_cookie_count om the sender size.  In the current implementation we
    // are not doing anything with it.  Maybe we should keep it somehow and pass it back to the sender with
    // through post_recv_buffer().

    dapl_processes[iproc].local_snd_info_ledger->curr = (dapl_processes[iproc].local_snd_info_ledger->curr + 1) % dapl_processes[iproc].local_snd_info_ledger->num_entries;

    dbg_info("dapl_wait_send_request_rdma(): new curr == %d", dapl_processes[iproc].local_snd_info_ledger->curr);

    return iproc;

    error_exit:
    return -1;

}


///////////////////////////////////////////////////////////////////////////////
int dapl_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
    dapl_buffer_t *db;
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_RMR_TRIPLET rmr;
    DAT_LMR_TRIPLET lmr;
    dapl_ri_ledger_entry_t *entry;
    uint32_t curr_cookie;
    int failed=0;
    int curr;

    ctr_info(" > dapl_post_recv_buffer_rdma(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_recv_buffer_rdma(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
        log_err("dapl_post_recv_buffer_rdma(): Requested recv from ptr not in table");
        goto error_exit;
    }

    /*
     * GFR: Seems like dapl_get_request() was changed to allocate a new request
     *   when the free reqs list is empty, so I see no point in this check.
     */
    //if (LIST_EMPTY(&free_reqs_list) && request != NULL) {
    //    log_err("dapl_post_recv_buffer_rdma(): No free request to use");
    //    goto error_exit;
    //}


    // proc == -1 means ANY_SOURCE.  In this case all potential senders must post a send request
    // which will write into our snd_info ledger entries such that:
    // context == (DAT_RMR_CONTEXT) 0
    // address == (DAT_VADDR) NULL
    if( proc == -1 ){
        proc = dapl_wait_send_request_rdma(tag);
    }

    curr = dapl_processes[proc].remote_rcv_info_ledger->curr;
    entry = &dapl_processes[proc].remote_rcv_info_ledger->entries[curr];

    curr_cookie = get_inc_curr_cookie_count();
    dbg_info("Incremented current cookie to %d", curr_cookie+1);

    // fill in what we're going to transfer
    entry->header = 1;
    entry->request = curr_cookie;
    entry->context = db->rmr_context;
    entry->address = (DAT_VADDR) ptr;
    entry->size = size;
    entry->tag = tag;
    entry->footer = 1;

    dbg_info("Post recv");
    dbg_info("Request: %u", entry->request);
    dbg_info("Context: %u", entry->context);
    dbg_info("Address: %p", (void *)entry->address);
    dbg_info("Size: %u", entry->size);
    dbg_info("Tag: %d", entry->tag);

    // setup the local memory region to point to the thing we just filled in
    lmr.lmr_context = shared_storage->lmr_context;
    lmr.virtual_address = (DAT_VADDR) entry;
    lmr.segment_length = sizeof(*entry);

    // setup the remote memory region to point to the next entry in the remote clients buffer
    rmr.rmr_context = dapl_processes[proc].remote_rcv_info_ledger->remote.context;
    rmr.virtual_address = dapl_processes[proc].remote_rcv_info_ledger->remote.address + dapl_processes[proc].remote_rcv_info_ledger->curr * sizeof(*entry);
    rmr.segment_length = sizeof(*entry);

    dbg_info("Target Context: %u", rmr.rmr_context);
    dbg_info("Target Address: %p", (void *)rmr.virtual_address);

    dbg_info("Posting recv buffer for one-sided send from %d/%d", proc, entry->request);

    cookie.as_64 = (( (uint64_t)proc)<<32) | NULL_COOKIE;
    dbg_info("Posted NULL Cookie: %u/%u/%llu", proc, NULL_COOKIE, (unsigned long long)(cookie.as_64));

    do {
        retval = dat_ep_post_rdma_write(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_write(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            failed = 1;
            break;
        }
    } while ( retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_post_recv_buffer_rdma(): Failed to post rdma_write(), exiting");
        goto error_exit;
    }

    // Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
    // that __dapl_wait_one() is able to wait on this event's completion
    ++handshake_rdma_write_count;

    if (request != NULL) {
        dapl_req_t *req;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_recv(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        // dapl_post_recv_buffer_rdma() initiates a receiver initiated handshake.  For this reason,
        // we don't care when the function is completed, but rather when the transfer associated with
        // this handshake is completed.  This will be reflected in the LEDGER by the corresponding
        // dapl_send_FIN() posted by the sender.
        req->type = LEDGER;
        req->proc = proc;
        req->tag = tag;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("Inserting the RDMA request into the request table: %d/%p", curr_cookie, req);

        if (htable_insert(ledger_reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("Couldn't save request in hashtable");
        }
        *request = curr_cookie;
    }
    dapl_processes[proc].remote_rcv_info_ledger->curr = (dapl_processes[proc].remote_rcv_info_ledger->curr + 1) % dapl_processes[proc].remote_rcv_info_ledger->num_entries;

    dbg_info("dapl_post_recv_buffer_rdma(): New curr (proc=%d): %u", proc, dapl_processes[proc].remote_rcv_info_ledger->curr);

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_RMR_TRIPLET rmr;
    DAT_LMR_TRIPLET lmr;
    dapl_ri_ledger_entry_t *entry;
    int curr, num_entries, offset, failed=0;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_send_request_rdma(%d, %u, %d, %p)", proc, size, tag, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_send_request_rdma(): Library not initialized.  Call photon_init() first");
        goto error_exit;
    }

    /*
     * GFR: Seems like dapl_get_request() was changed to allocate a new request
     *   when the free reqs list is empty, so I see no point in this check.
     */
    //if (LIST_EMPTY(&free_reqs_list) && request != NULL) {
    //    log_err("dapl_post_send_request_rdma(): No free request to use");
    //    goto error_exit;
    //}

    curr = dapl_processes[proc].remote_snd_info_ledger->curr;
    entry = &dapl_processes[proc].remote_snd_info_ledger->entries[curr];

    curr_cookie = get_inc_curr_cookie_count();
    dbg_info("dapl_post_send_request_rmda(): Incremented curr_cookie_count to: %d", curr_cookie+1);

    // fill in what we're going to transfer
    entry->header = 1;
    entry->request = curr_cookie;
    entry->context = (DAT_RMR_CONTEXT) 0;
    entry->address = (DAT_VADDR) NULL;
    entry->size = size;
    entry->tag = tag;
    entry->footer = 1;

    dbg_info("Post send request");
    dbg_info("Request: %u", entry->request);
    dbg_info("Context: %u", entry->context);
    dbg_info("Address: %p", (void *)entry->address);
    dbg_info("Size: %u", entry->size);
    dbg_info("Tag: %d", entry->tag);

    // setup the local memory region to point to the thing we just filled in
    lmr.lmr_context = shared_storage->lmr_context;
    lmr.virtual_address = (DAT_VADDR) entry;
    lmr.segment_length = sizeof(*entry);

    // setup the remote memory region to point to the next entry in the remote clients buffer
    rmr.rmr_context = dapl_processes[proc].remote_snd_info_ledger->remote.context;
    offset = dapl_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
    rmr.virtual_address = dapl_processes[proc].remote_snd_info_ledger->remote.address + offset;
    rmr.segment_length = sizeof(*entry);

    dbg_info("Target Context: %u", rmr.rmr_context);
    dbg_info("Target Address: %p", (void *)rmr.virtual_address);
    dbg_info("Posting send request to proc=%d, req=%d", proc, entry->request);

    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));

    do {
        retval = dat_ep_post_rdma_write(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_write(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            failed = 1;
            break;
        }
    } while ( retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_post_send_request_rdma(): Failed to post rdma_write(), exiting");
        goto error_exit;
    }

    // Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
    // that __dapl_wait_one() is able to wait on this event's completion
    ++handshake_rdma_write_count;

    if (request != NULL) {
        dapl_req_t *req;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_send_request_rdma(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        // dapl_post_send_request_rdma() causes an RDMA transfer, but its own completion is
        // communicated to the task that posts it through a DTO completion event.  This
        // function informs the receiver about an upcoming send, it does NOT initiate
        // a data transfer handshake and that's why it's not a LEDGER event.
        req->type = EVQUEUE;
        req->proc = proc;
        req->tag = tag;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("dapl_post_send_request_rmda(): Inserting the RDMA request into the request table: %d/%p", curr_cookie, req);

        // GFR: Maybe FIXME? The other post operations insert into the ledger table.
        //   So either this is wrong or the ++handshake_count above is unnecessary.
        if (htable_insert(reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("Couldn't save request in hashtable");
        }
        *request = curr_cookie;
    }

    num_entries = dapl_processes[proc].remote_snd_info_ledger->num_entries;
    dapl_processes[proc].remote_snd_info_ledger->curr = (dapl_processes[proc].remote_snd_info_ledger->curr + 1) % num_entries;
    dbg_info("dapl_post_send_request_rmda(): New curr: %u", dapl_processes[proc].remote_snd_info_ledger->curr);

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
    dapl_buffer_t *db;
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_RMR_TRIPLET rmr;
    DAT_LMR_TRIPLET lmr;
    dapl_ri_ledger_entry_t *entry;
    int curr, num_entries, offset, failed=0;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_send_buffer_rdma(%d, %p, %u, %d, %p)", proc, ptr, size, tag, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_send_buffer_rdma(): Library not initialized.  Call photon_init() first");
        goto error_exit;
    }

    if (buffertable_find_containing( (void *) ptr, (int)size, &db) != 0) {
        log_err("dapl_post_recv_buffer_rdma(): Requested post of send buffer for ptr not in table");
        goto error_exit;
    }

    /*
     * GFR: Seems like dapl_get_request() was changed to allocate a new request
     *   when the free reqs list is empty, so I see no point in this check.
     */
    //if (LIST_EMPTY(&free_reqs_list) && request != NULL) {
    //    log_err("dapl_post_send_buffer_rdma(): No free request to use");
    //    goto error_exit;
    //}

    curr = dapl_processes[proc].remote_snd_info_ledger->curr;
    entry = &dapl_processes[proc].remote_snd_info_ledger->entries[curr];

    curr_cookie = get_inc_curr_cookie_count();
    dbg_info("dapl_post_send_buffer_rdma(): Incremented curr_cookie_count to: %d", curr_cookie+1);

    // fill in what we're going to transfer
    entry->header = 1;
    entry->request = curr_cookie;
    entry->context = db->rmr_context;
    entry->address = (DAT_VADDR) ptr;
    entry->size = size;
    entry->tag = tag;
    entry->footer = 1;

    dbg_info("Post send request");
    dbg_info("Request: %u", entry->request);
    dbg_info("Context: %u", entry->context);
    dbg_info("Address: %p", (void *)entry->address);
    dbg_info("Size: %u", entry->size);
    dbg_info("Tag: %d", entry->tag);

    // setup the local memory region to point to the thing we just filled in
    lmr.lmr_context = shared_storage->lmr_context;
    lmr.virtual_address = (DAT_VADDR) entry;
    lmr.segment_length = sizeof(*entry);

    // setup the remote memory region to point to the next entry in the remote clients buffer
    rmr.rmr_context = dapl_processes[proc].remote_snd_info_ledger->remote.context;
    offset = dapl_processes[proc].remote_snd_info_ledger->curr * sizeof(*entry);
    rmr.virtual_address = dapl_processes[proc].remote_snd_info_ledger->remote.address + offset;
    rmr.segment_length = sizeof(*entry);

    dbg_info("Target Context: %u", rmr.rmr_context);
    dbg_info("Target Address: %p", (void *)rmr.virtual_address);
    dbg_info("Posting send buffer to proc=%d, req=%d", proc, entry->request);

    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));

    do {
        retval = dat_ep_post_rdma_write(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_write(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            failed = 1;
            break;
        }
    } while ( retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_post_send_buffer_rdma(): Failed to post rdma_write(), exiting");
        goto error_exit;
    }

    // Keep track of the dat_ep_post_rdma_write()s that do not insert an entry into "reqtable", so
    // that __dapl_wait_one() is able to wait on this event's completion
    ++handshake_rdma_write_count;

    if (request != NULL) {
        dapl_req_t *req;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_send_buffer_rdma(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        // dapl_post_send_buffer_rdma() initiates a sender initiated handshake.  For this reason,
        // we don't care when the function is completed, but rather when the transfer associated with
        // this handshake is completed.  This will be reflected in the LEDGER by the corresponding
        // dapl_send_FIN() posted by the receiver.
        req->type = LEDGER;
        req->proc = proc;
        req->tag = tag;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("dapl_post_send_buffer_rmda(): Inserting the RDMA request into the request table: %d/%p", curr_cookie, req);

        if (htable_insert(ledger_reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("Couldn't save request in hashtable");
        }
        *request = curr_cookie;
    }

    num_entries = dapl_processes[proc].remote_snd_info_ledger->num_entries;
    dapl_processes[proc].remote_snd_info_ledger->curr = (dapl_processes[proc].remote_snd_info_ledger->curr + 1) % num_entries;
    dbg_info("dapl_post_send_buffer_rmda(): New curr: %u", dapl_processes[proc].remote_snd_info_ledger->curr);

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
static inline dapl_req_t *dapl_get_request() {
    dapl_req_t *req;

    LIST_LOCK(&free_reqs_list);
    req = LIST_FIRST(&free_reqs_list);
    if (req)
        LIST_REMOVE(req, list);
    LIST_UNLOCK(&free_reqs_list);

    if(!req) {
        req = malloc(sizeof(dapl_req_t));
        pthread_mutex_init(&req->mtx, NULL);
        pthread_cond_init (&req->completed, NULL);
    }

    return req;
}


#if 0
// Needs to be brought up to data (maybe) and compared with the RDMA+ledger based version of the
// post_recv_buffer()
int dapl_post_recv_buffer_sr(int proc, char *base, uint32_t offset, uint32_t size, uint32_t *request) {
    dapl_buffer_t *db;
    void *test;
    DAT_RETURN retval;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    dapl_ri_ledger_entry_t *entry;

    ctr_info(" > dapl_post_recv_buffer_rdma(%d, %p, %u, %u, %p)", proc, base, offset, size, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_recv_buffer_sr(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (htable_lookup(buffertable, (uint64_t) base, &test) != 0) {
        log_err("dapl_post_recv_buffer_sr(): Requested recv from base not in table");
        goto error_exit;
    }

    db = test;

    entry = &dapl_processes[proc].remote_rcv_info_ledger->entries[dapl_processes[proc].remote_rcv_info_ledger->curr];

    // fill in what we're going to transfer
    entry->header = 1;
    entry->request = curr_cookie_count;
    entry->context = db->rmr_context;
    entry->address = (DAT_VADDR) base + offset;
    entry->size = size;
    entry->footer = 1;

    dbg_info("Post recv");
    dbg_info("Request: %u", entry->request);
    dbg_info("Context: %u", entry->context);
    dbg_info("Address: %p", entry->address);
    dbg_info("Size: %u", entry->size);

    // setup the local memory region to point to the thing we just filled in
    if (dapl_processes[proc].remote_rcv_info_ledger->local != NULL)
        lmr.lmr_context = dapl_processes[proc].remote_rcv_info_ledger->local->lmr_context;
    else
        lmr.lmr_context = shared_storage->lmr_context;
#ifdef USING_DAPL1
    lmr.pad = 0;
#endif
    lmr.virtual_address = (DAT_VADDR) entry;
    lmr.segment_length = sizeof(*entry);

    dbg_info("Posting recv buffer for one-sided send from %d/%d using SR", proc, entry->request);

    cookie.as_64 = (( (uint64_t)proc)<<32) | NULL_COOKIE;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, NULL_COOKIE, cookie.as_64);

    retval = dat_ep_post_send(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, DAT_COMPLETION_DEFAULT_FLAG);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_post_recv_buffer_sr(): Failed to post send");
        goto error_exit;
    }

#if 0
    evd_counter++;
    while(evd_counter >= EVD_MAX) {
        __dapl_wait_one();
        evd_counter--;
    }
#endif

    if (request != NULL) {
        dapl_req_t *req;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_recv(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie_count;
        req->state = REQUEST_PENDING;
        req->type = LEDGER;
        LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("Inserting the RDMA request into the request table: %d/%p", curr_cookie_count, req);

        if (htable_insert(ledger_reqtable, (uint64_t)curr_cookie_count, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("dapl_post_recv_buffer_sr(): Couldn't save request in hashtable");
        }
        *request = curr_cookie_count;
    }
    curr_cookie_count++;
    if( curr_cookie_count == 0 ) curr_cookie_count++;
    dbg_info("Incrementing curr_cookie_count to: %d", curr_cookie_count);

    dbg_info("New curr: %u", dapl_processes[proc].remote_rcv_info_ledger->num_entries);

    dapl_processes[proc].remote_rcv_info_ledger->curr = (dapl_processes[proc].remote_rcv_info_ledger->curr + 1) % dapl_processes[proc].remote_rcv_info_ledger->num_entries;

    return 0;

    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}
#endif


///////////////////////////////////////////////////////////////////////////////
int dapl_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
    dapl_remote_buffer_t *drb;
    dapl_buffer_t *db;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    DAT_RMR_TRIPLET rmr;
    DAT_RETURN retval;
    int failed;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_os_put(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_os_put(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    drb = dapl_processes[proc].curr_remote_buffer;

    if (drb->request == NULL_COOKIE) {
        log_err("dapl_post_os_put(): Tried posting a send with no recv buffer. Have you called dapl_wait_recv_buffer_rdma() first?");
        return -1;
    }

    if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
        log_err("dapl_post_os_put(): Tried posting a send for a buffer not registered");
        return -1;
    }

    if (drb->size > 0 && size + remote_offset > drb->size) {
        log_err("dapl_post_os_put(): Requested to send %u bytes to a %u buffer size at offset %u", size, drb->size, remote_offset);
        return -2;
    }

    curr_cookie = get_inc_curr_cookie_count();
    dbg_info("dapl_post_os_put(): Incremented curr_cookie_count to: %d", curr_cookie+1);

    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));

    lmr.lmr_context = db->lmr_context;
    lmr.virtual_address = (uint64_t) ptr;
    lmr.segment_length = size;

    rmr.rmr_context = drb->context;
    rmr.virtual_address = drb->address + remote_offset;
    rmr.segment_length = size;

    dbg_info("Post_os_send");
    dbg_info("Request: %u", drb->request);
    dbg_info("Context: %u", drb->context);
    dbg_info("Address: %p", (void *)drb->address);
    dbg_info("Size: %u", drb->size);

    failed = 0;
    do {
        retval = dat_ep_post_rdma_write(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_write(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            log_dat_err(retval, "dapl_post_os_put(): Failed to post rdma_write()");
            break;
        }
    } while (retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_post_os_put(): Failed to post rdma_write(), exiting");
        goto error_exit;
    }

    if (request != NULL) {
        dapl_req_t *req;

        *request = curr_cookie;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_os_put(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        // dapl_post_os_put() causes an RDMA transfer, but its own completion is
        // communicated to the task that posts it through a DTO completion event.
        req->type = EVQUEUE;
        req->proc = proc;
        req->tag = tag;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("Inserting the OS send request into the request table: %d/%d/%p", proc, curr_cookie, req);

        if (htable_insert(reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("dapl_post_os_put(): Couldn't save request in hashtable");
        }
    }

    return 0;
    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}



///////////////////////////////////////////////////////////////////////////////
int dapl_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
    dapl_remote_buffer_t *drb;
    dapl_buffer_t *db;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    DAT_RMR_TRIPLET rmr;
    DAT_RETURN retval;
    int failed;
    uint32_t curr_cookie;

    ctr_info(" > dapl_post_os_get(%d, %p, %u, %u, %p)", proc, ptr, size, remote_offset, request);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_post_os_get(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    drb = dapl_processes[proc].curr_remote_buffer;

    if (drb->request == NULL_COOKIE) {
        log_err("dapl_post_os_get(): Tried posting an os_get() with no send buffer");
        return -1;
    }

    if (buffertable_find_containing( (void *)ptr, (int)size, &db) != 0) {
        log_err("dapl_post_os_get(): Tried posting a og_get() into a buffer that's not registered");
        return -1;
    }

    if ( (drb->size > 0) && ((size+remote_offset) > drb->size) ) {
        log_err("dapl_post_os_get(): Requested to get %u bytes from a %u buffer size at offset %u", size, drb->size, remote_offset);
        return -2;
    }

    curr_cookie = get_inc_curr_cookie_count();
    dbg_info("dapl_post_os_get(): Incremented curr_cookie_count to: %d", curr_cookie+1);

    cookie.as_64 = (( (uint64_t)proc)<<32) | curr_cookie;
    dbg_info("Posted Cookie: %u/%u/%llu", proc, curr_cookie, (unsigned long long)(cookie.as_64));

    lmr.lmr_context = db->lmr_context;
    lmr.virtual_address = (uint64_t) ptr;
    lmr.segment_length = size;

    rmr.rmr_context = drb->context;
    rmr.virtual_address = drb->address + remote_offset;
    rmr.segment_length = size;

    dbg_info("Post_os_get() Request: %u", drb->request);
    dbg_info("Post_os_get() Context: %u", drb->context);
    dbg_info("Post_os_get() Address: %p", (void *)drb->address);
    dbg_info("Post_os_get() Size: %u", drb->size);

    failed = 0;
    do {
        retval = dat_ep_post_rdma_read(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_read(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            log_dat_err(retval, "dapl_post_os_get(): Failed to post rdma_read()");
            break;
        }
    } while (retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_post_os_get(): Failed to post rdma_read(), exiting");
        goto error_exit;
    }

    if (request != NULL) {
        dapl_req_t *req;

        *request = curr_cookie;

        req = dapl_get_request();
        if (!req) {
            log_err("dapl_post_os_get(): Couldn't allocate request\n");
            goto error_exit;
        }
        req->id = curr_cookie;
        req->state = REQUEST_PENDING;
        // dapl_post_os_get() causes an RDMA transfer, but its own completion is
        // communicated to the task that posts it through a DTO completion event.
        req->type = EVQUEUE;
        req->proc = proc;
        req->tag = tag;
        SAFE_LIST_INSERT_HEAD(&pending_reqs_list, req, list);

        dbg_info("Inserting the OS get request into the request table: %d/%d/%p", proc, curr_cookie, req);

        if (htable_insert(reqtable, (uint64_t)curr_cookie, req) != 0) {
            // this is bad, we've submitted the request, but we can't track it
            log_err("dapl_post_os_get(): Couldn't save request in hashtable");
        }
    }

    return 0;
    error_exit:
    if (request != NULL) {
        *request = NULL_COOKIE;
    }
    return -1;
}



///////////////////////////////////////////////////////////////////////////////
int dapl_send_FIN(int proc) {
    dapl_remote_buffer_t *drb;
    DAT_DTO_COOKIE cookie;
    DAT_LMR_TRIPLET lmr;
    DAT_RMR_TRIPLET rmr;
    DAT_RETURN retval;
    dapl_rdma_FIN_ledger_entry_t *entry;
    int curr, failed;

    ctr_info(" > dapl_send_FIN(%d)", proc);

    if( _curr_cookie_count == 0 ){
        log_err("dapl_send_FIN(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    if (dapl_processes[proc].curr_remote_buffer->request == NULL_COOKIE) {
        log_err("dapl_send_FIN(): Cannot send FIN, curr_remote_buffer->request is NULL_COOKIE");
        return -1;
    }

    drb = dapl_processes[proc].curr_remote_buffer;
    curr = dapl_processes[proc].remote_FIN_ledger->curr;
    entry = &dapl_processes[proc].remote_FIN_ledger->entries[curr];
    dbg_info("dapl_send_FIN() dapl_processes[%d].remote_FIN_ledger->curr==%d\n",proc, curr);

    if( entry == NULL ){
        log_err("dapl_send_FIN() entry is NULL for proc=%d\n",proc);
        return 1;
    }

    entry->header = 1;
    entry->request = drb->request;
    entry->footer = 1;

    // setup the local memory region to point to the thing we just filled in
    lmr.lmr_context = shared_storage->lmr_context;
    lmr.virtual_address = (DAT_VADDR) entry;
    lmr.segment_length = sizeof(*entry);

    // setup the remote memory region to point to the next entry in the remote clients buffer
    rmr.rmr_context = dapl_processes[proc].remote_FIN_ledger->remote.context;
    rmr.virtual_address = dapl_processes[proc].remote_FIN_ledger->remote.address + dapl_processes[proc].remote_FIN_ledger->curr * sizeof(*entry);
    rmr.segment_length = sizeof(*entry);
    dbg_info("dapl_send_FIN() rmr.target_address(proc==%d)=%p", proc, (void *)rmr.virtual_address);

    cookie.as_64 = (( (uint64_t)proc)<<32) | NULL_COOKIE;
    dbg_info("dapl_send_FIN() Posted NULL Cookie: %u/%u/%llu", proc, NULL_COOKIE, (unsigned long long)(cookie.as_64));

    failed = 0;
    do {
        retval = dat_ep_post_rdma_write(dapl_processes[proc].clnt_ep, 1, &lmr, cookie, &rmr, DAT_COMPLETION_DEFAULT_FLAG);
        if (DAT_GET_TYPE(retval) == DAT_INSUFFICIENT_RESOURCES) {
            dbg_info("Failed to post rdma_write(), trying to free resources");
            if( !!__dapl_wait_one() ){
                failed = 1;
                break;
            }
        } else if (retval != DAT_SUCCESS) {
            log_dat_err(retval, "dapl_send_FIN(): Failed to post rdma_write()");
            break;
        }
    } while (retval != DAT_SUCCESS);
    if( failed ){
        log_dat_err(retval, "dapl_send_FIN(): Failed to post rdma_write(), exiting");
        goto error_exit;
    }

    dapl_processes[proc].remote_FIN_ledger->curr = (dapl_processes[proc].remote_FIN_ledger->curr + 1) % dapl_processes[proc].remote_FIN_ledger->num_entries;
    drb->request = NULL_COOKIE;

    return 0;
    error_exit:
    return -1;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// the actual photon API

inline int photon_init(int nproc, int myrank, MPI_Comm comm) {
    return dapl_init(nproc, myrank, comm, 0);
}

inline int photon_register_buffer(char *buffer, int buffer_size) {
    return dapl_register_buffer(buffer, buffer_size);
}

inline int photon_unregister_buffer(char *buffer, int size) {
    return dapl_unregister_buffer(buffer, size);
}

inline int photon_post_recv(int proc, char *ptr, uint32_t size, uint32_t *request) {
    return dapl_post_recv(proc, ptr, size, request);
}

inline int photon_post_send(int proc, char *ptr, uint32_t size, uint32_t *request) {
    return dapl_post_send(proc, ptr, size, request);
}

#ifdef WITH_XSP
inline int photon_test(uint32_t request, int *flag, int *type, void *status) {
    return dapl_xsp_test(request, flag, type, status);
}
#else
inline int photon_test(uint32_t request, int *flag, int *type, MPI_Status *status) {
    return dapl_test(request, flag, type, status);
}
#endif
inline int photon_wait(uint32_t request) {
    return dapl_wait(request);
}

inline int photon_wait_ledger(uint32_t request) {
    return dapl_wait(request);
}

inline int photon_wait_remaining() {
    return dapl_wait_remaining();
}

inline int photon_wait_remaining_ledger() {
    return dapl_wait_remaining_ledger();
}

inline int photon_post_recv_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
    return dapl_post_recv_buffer_rdma(proc, ptr, size, tag, request);
}

inline int photon_post_send_buffer_rdma(int proc, char *ptr, uint32_t size, int tag, uint32_t *request) {
    return dapl_post_send_buffer_rdma(proc, ptr, size, tag, request);
}

inline int photon_post_send_request_rdma(int proc, uint32_t size, int tag, uint32_t *request) {
    return dapl_post_send_request_rdma(proc, size, tag, request);
}

inline int photon_wait_recv_buffer_rdma(int proc, int tag) {
    return dapl_wait_recv_buffer_rdma(proc, tag);
}

inline int photon_wait_send_buffer_rdma(int proc, int tag) {
    return dapl_wait_send_buffer_rdma(proc, tag);
}

inline int photon_wait_send_request_rdma(int tag) {
    return dapl_wait_send_request_rdma(tag);
}

inline int photon_post_os_put(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
    return dapl_post_os_put(proc, ptr, size, tag, remote_offset, request);
}

inline int photon_post_os_get(int proc, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {
    return dapl_post_os_get(proc, ptr, size, tag, remote_offset, request);
}

inline int photon_send_FIN(int proc) {
    return dapl_send_FIN(proc);
}

inline int photon_wait_any(int *ret_proc, uint32_t *ret_req) {
    return dapl_wait_any(ret_proc, ret_req);
}

inline int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req) {
    return dapl_wait_any_ledger(ret_proc, ret_req);
}

inline int photon_finalize() {
    return dapl_finalize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//                                     DAPL Utility Functions                                    //
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
int dapl_create_listener(DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz, DAT_CONN_QUAL *conn_qual, DAT_EVD_HANDLE cevd, DAT_EVD_HANDLE dto_evd, DAT_EP_HANDLE *ret_ep, DAT_PSP_HANDLE *ret_psp) {
    DAT_RETURN retval;
    DAT_EP_HANDLE ep;
    DAT_PSP_HANDLE psp;

    ctr_info(" > dapl_create_listener()");

    retval = dat_ep_create(ia, pz, dto_evd, dto_evd, cevd, (DAT_EP_ATTR*) DAT_HANDLE_NULL, &ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_create_listener(): Couldn't create endpoint");
        goto error_exit;
    }

    while(1){
        retval = dat_psp_create(ia, *conn_qual, cevd, DAT_PSP_CONSUMER_FLAG, &psp);
        if( retval != DAT_CONN_QUAL_IN_USE ){
            break;
        }else{
            *conn_qual = *conn_qual+1;
        }
    }
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_create_listener(): Couldn't create service point");
        goto error_exit_ep;
    }

    *ret_ep = ep;
    *ret_psp = psp;

    return 0;

    error_exit_ep:
    retval = dat_ep_free(ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_create_listener(): Couldn't free connect event queue");
    }
    error_exit:
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dat_client_connect(DAT_IA_HANDLE ia, DAT_PZ_HANDLE pz, DAT_IA_ADDRESS_PTR remote_addr, DAT_CONN_QUAL conn_qual, DAT_EVD_HANDLE cevd, DAT_EVD_HANDLE dto_evd, DAT_EP_HANDLE *ret_ep) {
    DAT_EP_HANDLE ep;
    //DAT_EVENT event;
    //DAT_COUNT count;
    DAT_RETURN retval;

    ctr_info(" > dapl_client_connect()");
    retval = dat_ep_create(ia, pz, dto_evd, dto_evd, cevd, (DAT_EP_ATTR *) DAT_HANDLE_NULL, &ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dat_client_connect(): Couldn't create endpoint");
        goto error_exit;
    }

    retval = dat_ep_connect(ep, remote_addr, conn_qual, DAT_TIMEOUT_INFINITE, 0, NULL, DAT_QOS_BEST_EFFORT, DAT_CONNECT_DEFAULT_FLAG);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dat_client_connect(): Couldn't connect to host");
        goto error_exit_ep;
    }

    *ret_ep = ep;

    return 0;

    error_exit_ep:
    retval = dat_ep_free(ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dat_client_connect(): Couldn't free endpoint");
    }
    error_exit:
    return -1;
}


///////////////////////////////////////////////////////////////////////////////
void log_dat_err(const DAT_RETURN status, const char *fmt, ...) {
    const char *major_msg, *minor_msg;
    char buf[255];
    va_list argp;

    if (dat_strerror(status, &major_msg, &minor_msg) == DAT_SUCCESS) {
        va_start(argp, fmt);
        vsnprintf(buf, 255, fmt, argp);
        va_end(argp);

        log_err("%s: %s %s", buf, major_msg, minor_msg);
    } else {
        log_err("%s: Unknown Error", buf);
    }
}


///////////////////////////////////////////////////////////////////////////////
void log_dto_err(const DAT_DTO_COMPLETION_STATUS status, const char *fmt, ...) {
    char buf[255];
    va_list argp;

    va_start(argp, fmt);
    vsnprintf(buf, 255, fmt, argp);
    va_end(argp);

    log_err("%s: %s", buf, str_dto_completion_status(status));
}


///////////////////////////////////////////////////////////////////////////////
const char *str_ep_state(const DAT_EP_STATE status) {
    switch(status) {
    case DAT_EP_STATE_COMPLETION_PENDING:
        return "DAT_EP_STATE_COMPLETION_PENDING";
    case DAT_EP_STATE_UNCONFIGURED_PASSIVE:
        return "DAT_EP_STATE_UNCONFIGURED_PASSIVE";
    case DAT_EP_STATE_UNCONFIGURED_RESERVED:
        return "DAT_EP_STATE_UNCONFIGURED_RESERVED";
    case DAT_EP_STATE_UNCONFIGURED_UNCONNECTED:
        return "DAT_EP_STATE_UNCONFIGURED_UNCONNECTED";
    case DAT_EP_STATE_UNCONFIGURED_TENTATIVE:
        return "DAT_EP_STATE_UNCONFIGURED_TENTATIVE";
    case DAT_EP_STATE_UNCONNECTED:
        return "DAT_EP_STATE_UNCONNECTED";
    case DAT_EP_STATE_RESERVED:
        return "DAT_EP_STATE_RESERVED";
    case DAT_EP_STATE_PASSIVE_CONNECTION_PENDING:
        return "DAT_EP_STATE_PASSIVE_CONNECTION_PENDING";
    case DAT_EP_STATE_ACTIVE_CONNECTION_PENDING:
        return "DAT_EP_STATE_ACTIVE_CONNECTION_PENDING";
    case DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING:
        return "DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING";
    case DAT_EP_STATE_CONNECTED:
        return "DAT_EP_STATE_CONNECTED";
    case DAT_EP_STATE_DISCONNECT_PENDING:
        return "DAT_EP_STATE_DISCONNECT_PENDING";
    case DAT_EP_STATE_DISCONNECTED:
        return "DAT_EP_STATE_DISCONNECTED";
    case DAT_EP_STATE_CONNECTED_SINGLE_PATH:
        return "DAT_EP_STATE_CONNECTED_SINGLE_PATH";
    case DAT_EP_STATE_CONNECTED_MULTI_PATH:
        return "DAT_EP_STATE_CONNECTED_MULTI_PATH";

    }

    return "UNKNOWN";
}


///////////////////////////////////////////////////////////////////////////////
const char *str_dto_completion_status(const DAT_DTO_COMPLETION_STATUS status) {
    switch(status) {
    case DAT_DTO_SUCCESS:
        return "DAT_DTO_SUCCESS";
    case DAT_DTO_ERR_FLUSHED:
        return "DAT_DTO_ERR_FLUSHED";
    case DAT_DTO_ERR_LOCAL_LENGTH:
        return "DAT_DTO_ERR_LOCAL_LENGTH";
    case DAT_DTO_ERR_LOCAL_EP:
        return "DAT_DTO_ERR_LOCAL_EP";
    case DAT_DTO_ERR_LOCAL_PROTECTION:
        return "DAT_DTO_ERR_LOCAL_PROTECTION";
    case DAT_DTO_ERR_BAD_RESPONSE:
        return "DAT_DTO_ERR_BAD_RESPONSE";
    case DAT_DTO_ERR_REMOTE_ACCESS:
        return "DAT_DTO_ERR_REMOTE_ACCESS";
    case DAT_DTO_ERR_REMOTE_RESPONDER:
        return "DAT_DTO_ERR_REMOTE_RESPONDER";
    case DAT_DTO_ERR_TRANSPORT:
        return "DAT_DTO_ERR_TRANSPORT";
    case DAT_DTO_ERR_RECEIVER_NOT_READY:
        return "DAT_DTO_ERR_RECEIVER_NOT_READY";
    case DAT_DTO_ERR_PARTIAL_PACKET:
        return "DAT_DTO_ERR_PARTIAL_PACKET";
    case DAT_RMR_OPERATION_FAILED:
        return "DAT_RMR_OPERATION_FAILED";
    case DAT_DTO_ERR_LOCAL_MM_ERROR:
        return "DAT_DTO_ERR_LOCAL_MM_ERROR";
    }

    return "UNKNOWN";
}


///////////////////////////////////////////////////////////////////////////////
int dapl_connqual2proc(DAT_CONN_QUAL cq, int *type) {
    int i;

    ctr_info(" > dapl_connqual2proc()");
    for(i = 0; i < (dapl_processes_count + _photon_forwarder); i++) {
        if (dapl_processes[i].srvr_conn_qual == cq) {
            *type = 0;
            return i;
        } else if (dapl_processes[i].clnt_conn_qual == cq) {
            *type = 1;
            return i;
        }
    }

    return -1;
}


///////////////////////////////////////////////////////////////////////////////
int dapl_ep2proc(DAT_EP_HANDLE ep, int *type) {
    int i;

    ctr_info(" > dapl_ep2proc()");
    for(i = 0; i < (dapl_processes_count + _photon_forwarder); i++) {
        //dbg_info("EP: %p/%p/%p", dapl_processes[i].srvr_ep, dapl_processes[i].clnt_ep, ep);
        if (dapl_processes[i].srvr_ep == ep) {
            *type = 0;
            return i;
        } else if (dapl_processes[i].clnt_ep == ep) {
            *type = 1;
            return i;
        }
    }

    return -1;
}

#ifdef WITH_XSP

int dapl_xsp_init() {
    char *forwarder_node;

    ctr_info(" > dapl_xsp_init()");

    forwarder_node = getenv("PHOTON_FORWARDER");

    if (!forwarder_node) {
        log_err("dapl_xsp_init(): Error: no photon forwarder specified: set the environmental variable PHOTON_FORWARDER");
        return -1;
    }

    if (dapl_xsp_setup_session(&(dapl_processes[_photon_fp].sess), forwarder_node) != 0) {
        log_err("dapl_xsp_init(): Error: could not setup XSP session");
        return -1;
    }

    if (dapl_xsp_setup_listeners() != 0) {
        log_err("dapl_xsp_init(); couldn't setup listeners");
        return -1;
    }

    if (dapl_xsp_exchange_ri_ledgers() != 0) {
        log_err("dapl_xsp_init(); couldn't exchange rdma ledgers");
        return -1;
    }

    if (dapl_xsp_exchange_FIN_ledger() != 0) {
        log_err("dapl_xsp_init(); couldn't exchange send ledgers");
        return -1;
    }

    return 0;
}

int dapl_xsp_setup_session(libxspSess **sess, char *xsp_hop) {

    if (libxsp_init() < 0) {
        perror("libxsp_init(): failed");
        return -1;
    }

    *sess = xsp_session();
    if (!sess) {
        perror("xsp_session() failed");
        return -1;
    }

    xsp_sess_appendchild(*sess, xsp_hop, XSP_HOP_NATIVE);

    if (xsp_connect(*sess)) {
        perror("xsp_connect(): connect failed");
        return -1;
    }

    dbg_info("XSP session established with %s", xsp_hop);

    return 0;
}

int dapl_xsp_setup_listeners() {
    int rnd, n;
    DAT_RETURN retval;
    DAT_CONN_QUAL conn_qual;

    PhotonConnectInfo ci;
    PhotonConnectInfo *ret_ci;
    int ret_len;
    int ret_type;

    ctr_info(" > dapl_xsp_setup_listeners()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_xsp_setup_listeners(): Library not initialized.  Call photon_xsp_init() first");
        return -1;
    }

    // TODO: We need a proper parallel RNG
    rnd = 1+(int)rand();
    conn_qual = ((uint64_t)_photon_myrank+1) << 35 | ((uint64_t)rnd << 3);

    dbg_info("dapl_xsp_setup_listeners(): creating listener: %d/%llu", _photon_fp, (unsigned long long) conn_qual);

    if (dapl_create_listener(ia, pz, &conn_qual, cevd, dto_evd, &dapl_processes[_photon_fp].srvr_ep, &dapl_processes[_photon_fp].srvr_psp) != 0 ) {
        log_err("dapl_xsp_setup_listeners(): Couldn't create listener for phorwarder");
        goto error_exit;
    }

    dapl_processes[_photon_fp].srvr_conn_qual = conn_qual;

    dbg_info("dapl_xsp_setup_listeners(): sending data");

    bcopy(self_addr->ifa_addr, &(ci.sa), sizeof(struct sockaddr_storage));
    ci.cq = conn_qual;

    if ((n = xsp_send_msg(dapl_processes[_photon_fp].sess, &ci, sizeof(PhotonConnectInfo), PHOTON_CI)) <= 0) {
        log_err("dapl_xsp_setup_listener(): Couldn't send connect info");
        goto error_exit;
    }

    if ((n = xsp_recv_msg(dapl_processes[_photon_fp].sess, (void**)&ret_ci, &ret_len, &ret_type)) <= 0) {
        log_err("dapl_xsp_setup_listener(): Couldn't receive connect info");
        goto error_exit;
    }

    // set remote connect info
    dapl_processes[_photon_fp].sa = ret_ci->sa;
    dapl_processes[_photon_fp].clnt_conn_qual = ret_ci->cq;

    free(ret_ci);

    // we got the remote info so that means phorwarder end should be listening
    // now connect
    dbg_info("dapl_xsp_setup_listeners(): connecting to %d/%llu", _photon_fp, (unsigned long long int) dapl_processes[_photon_fp].clnt_conn_qual);

    retval = dat_client_connect(ia, pz, (struct sockaddr *) &(dapl_processes[_photon_fp].sa), dapl_processes[_photon_fp].clnt_conn_qual, cevd, dto_evd, &dapl_processes[_photon_fp].clnt_ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_xsp_setup_listeners(): Failed to connect to process %d", _photon_fp);
        goto error_exit;
    }

    if (dapl_wait_connect(1, 1) != 0) {
        log_err("dapl_xsp_setup_listeners(): Could not complete all connections");
        goto error_exit;
    }

    return 0;

    error_exit:
    return -1;
}

int dapl_xsp_exchange_ri_ledgers() {
    PhotonRIInfo ri;
    PhotonRIInfo *ret_ri;

    int n;
    int ret_len;
    int ret_type;

    ctr_info(" > dapl_xsp_exchange_ri_ledgers()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_xsp_exchange_ri_ledgers(): Library not initialized.  Call photon_xsp_init() first");
        return -1;
    }

    ri.recv_ledger = (DAT_VADDR) dapl_processes[_photon_fp].local_rcv_info_ledger->entries;
    ri.send_ledger = (DAT_VADDR) dapl_processes[_photon_fp].local_snd_info_ledger->entries;
    ri.rmr = shared_storage->rmr_context;

    if ((n = xsp_send_msg(dapl_processes[_photon_fp].sess, &ri, sizeof(PhotonRIInfo), PHOTON_RI)) <= 0) {
        log_err("dapl_xsp_exchange_ri_ledgers(): Couldn't send ledger info");
        goto error_exit;
    }

    if ((n = xsp_recv_msg(dapl_processes[_photon_fp].sess, (void**)&ret_ri, &ret_len, &ret_type)) <= 0) {
        log_err("dapl_xsp_exchange_ri_ledgers(): Couldn't receive ledger info");
        goto error_exit;
    }

    // snd_info and rcv_info ledgers are all stored in the same
    // contiguous memory region and share a common "context"
    dapl_processes[_photon_fp].remote_rcv_info_ledger->remote.context = ret_ri->rmr;
    dapl_processes[_photon_fp].remote_snd_info_ledger->remote.context = ret_ri->rmr;
    dapl_processes[_photon_fp].remote_rcv_info_ledger->remote.address = ret_ri->recv_ledger;
    dapl_processes[_photon_fp].remote_snd_info_ledger->remote.address = ret_ri->send_ledger;

    free(ret_ri);

    return 0;

    error_exit:
    return -1;
}

int dapl_xsp_exchange_FIN_ledger() {
    PhotonFINInfo fi;
    PhotonFINInfo *ret_fi;

    int n;
    int ret_len;
    int ret_type;

    ctr_info(" > dapl_xsp_exchange_FIN_ledger()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_xsp_exchange_FIN_ledger(): Library not initialized.  Call photon_xsp_init() first");
        return -1;
    }

    fi.fin_ledger = (DAT_VADDR) dapl_processes[_photon_fp].local_FIN_ledger->entries;
    fi.rmr = shared_storage->rmr_context;

    if ((n = xsp_send_msg(dapl_processes[_photon_fp].sess, &fi, sizeof(PhotonFINInfo), PHOTON_FI)) <= 0) {
        log_err("dapl_xsp_exchange_ri_ledgers(): Couldn't send ledger info");
        goto error_exit;
    }

    if ((n = xsp_recv_msg(dapl_processes[_photon_fp].sess, (void**)&ret_fi, &ret_len, &ret_type)) <= 0) {
        log_err("dapl_xsp_exchange_ri_ledgers(): Couldn't receive ledger info");
        goto error_exit;
    }

    dapl_processes[_photon_fp].remote_FIN_ledger->remote.address = ret_fi->fin_ledger;
    dapl_processes[_photon_fp].remote_FIN_ledger->remote.context = ret_fi->rmr;

    free(ret_fi);

    return 0;

    error_exit:
    return -1;
}

// this call sets up the context for nproc photon-xsp connections
int dapl_xsp_init_server(int nproc) {

    if (dapl_init_common(nproc+1, 0, MPI_COMM_SELF, 0) != 0) {
        log_err("dapl_xsp_init_server(): Couldn't initialize libphoton");
        goto error_exit;
    }

    return 0;

    error_exit:
    return -1;
}

int dapl_xsp_lookup_proc(libxspSess *sess, int *index) {
    int i;

    for(i = 0; i < dapl_processes_count; i++) {
        if (dapl_processes[i].sess &&
                !xsp_sesscmp(dapl_processes[i].sess, sess)) {
            *index = i;
            return i;
        }
    }

    *index = -1;
    return -1;
}

int photon_decode_MPI_Datatype(MPI_Datatype type, PhotonMPIDatatype *ptype) {
    int i;
    MPI_Datatype *types;

    MPI_Type_get_envelope(type, &ptype->nints, &ptype->naddrs,
            &ptype->ndatatypes, &ptype->combiner);

    if (ptype->nints) {
        ptype->integers = malloc(sizeof(int)*ptype->nints);
        if (!ptype->integers) {
            fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
            return -1;
        }
    }

    if (ptype->naddrs) {
        ptype->addresses = malloc(sizeof(MPI_Aint)*ptype->naddrs);
        if (!ptype->addresses) {
            fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
            goto error_exit_addresses;
        }
    }

    if (ptype->ndatatypes) {
        types = malloc(sizeof(MPI_Datatype)*ptype->ndatatypes);
        ptype->datatypes = malloc(sizeof(int)*ptype->ndatatypes);
        if (!types || !ptype->datatypes) {
            fprintf(stderr, "photon_decode_MPI_Datatype(): out of memory");
            goto error_exit_datatypes;
        }
    }

    MPI_Type_get_contents(type, ptype->nints, ptype->naddrs, ptype->ndatatypes,
            ptype->integers, ptype->addresses, types);

    /* Transform MPI_Datatypes to our own mapping to send over the wire.
     * There might be a better way to do this.
     */
    for (i = 0; i < ptype->ndatatypes; i++) {
        if (types[i] == MPI_DOUBLE)
            ptype->datatypes[i] = PHOTON_MPI_DOUBLE;
        else
            ptype->datatypes[i] = -1;
    }

    return 0;

    error_exit_datatypes:
    free(ptype->addresses);
    error_exit_addresses:
    free(ptype->integers);
    return -1;
}

#define INT_ASSIGN_MOVE(ptr, i) do { \
        *((int *)ptr) = i; \
        ptr += sizeof(int); \
} while(0)

inline void photon_destroy_mpi_datatype (PhotonMPIDatatype *pd) {
    if (pd->nints)      free(pd->integers);
    if (pd->naddrs)     free(pd->addresses);
    if (pd->ndatatypes) free(pd->datatypes);
}

void print_photon_io_info(PhotonIOInfo *io) {
    fprintf(stderr, "PhotonIOInfo:\n"
            "fileURI    = %s\n"
            "amode      = %d\n"
            "niter      = %d\n"
            "v.combiner = %d\n"
            "v.nints    = %d\n"
            "v.ints[0]  = %d\n"
            "v.naddrs   = %d\n"
            "v.ndts     = %d\n"
            "v.dts[0]   = %d\n",
            io->fileURI, io->amode, io->niter, io->view.combiner,
            io->view.nints, io->view.integers[0], io->view.naddrs,
            io->view.ndatatypes, io->view.datatypes[0]
    );
}

/* See photon_xsp.h for message format */
void *photon_create_xsp_io_init_msg(PhotonIOInfo *io, int *size) {
    void *msg;
    void *msg_ptr;
    int totalsize = 0;

    totalsize += sizeof(int) + strlen(io->fileURI) + 1;
    totalsize += sizeof(int)*3;
    totalsize += sizeof(int) + io->view.nints*sizeof(int);
    totalsize += sizeof(int) + io->view.naddrs*sizeof(MPI_Aint);
    totalsize += sizeof(int) + io->view.ndatatypes*sizeof(int);

    msg_ptr = msg = malloc(totalsize);
    if (!msg) {
        log_err("photon_create_xsp_io_init_msg(): out of memory");
        return NULL;
    }

    INT_ASSIGN_MOVE(msg_ptr, strlen(io->fileURI) + 1);
    strcpy((char*)msg_ptr, io->fileURI);
    msg_ptr += strlen(io->fileURI) + 1;

    INT_ASSIGN_MOVE(msg_ptr, io->amode);
    INT_ASSIGN_MOVE(msg_ptr, io->niter);
    INT_ASSIGN_MOVE(msg_ptr, io->view.combiner);

    INT_ASSIGN_MOVE(msg_ptr, io->view.nints);
    memcpy(msg_ptr, io->view.integers, io->view.nints*sizeof(int));
    msg_ptr += io->view.nints*sizeof(int);

    INT_ASSIGN_MOVE(msg_ptr, io->view.naddrs);
    memcpy(msg_ptr, io->view.addresses, io->view.naddrs*sizeof(MPI_Aint));
    msg_ptr += io->view.naddrs*sizeof(MPI_Aint);

    INT_ASSIGN_MOVE(msg_ptr, io->view.ndatatypes);
    memcpy(msg_ptr, io->view.datatypes, io->view.ndatatypes*sizeof(int));

    *size = totalsize;
    return msg;
}

// stub function for higher-level I/O operation
int dapl_xsp_phorwarder_io_init(char *file, int amode, MPI_Datatype view, int niter) {
    PhotonIOInfo io;
    void *msg;
    int msg_size;

    ctr_info(" > dapl_xsp_phorwarder_io_init()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_xsp_phorwarder_io_init(): Library not initialized.  Call photon_xsp_init() first");
        return -1;
    }

    io.fileURI = file;
    io.amode = amode;
    io.niter = niter;

    if (photon_decode_MPI_Datatype(view, &io.view) != 0)
        return -1;

    msg = photon_create_xsp_io_init_msg(&io, &msg_size);
    if (msg == NULL) {
        photon_destroy_mpi_datatype(&io.view);
        return -1;
    }

    print_photon_io_info(&io);

    if (xsp_send_msg(dapl_processes[_photon_fp].sess, msg, msg_size, PHOTON_IO) <= 0) {
        log_err("dapl_xsp_phorwarder_io_init(): Couldn't send IO info");
        photon_destroy_mpi_datatype(&io.view);
        free(msg);
        return -1;
    }

    /* TODO: Maybe we should receive an ACK? */

    photon_destroy_mpi_datatype(&io.view);
    free(msg);

    ctr_info(" > dapl_xsp_phorwarder_io_init() completed.");
    return 0;
}

#ifdef PHOTON_MULTITHREADED
///////////////////////////////////////////////////////////////////////////////
static void *dapl_watch_ledgers(void *arg) {
    void *test;
    int curr, i=-1;

    ctr_info(" > ledger watcher started.");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_watch_ledgers(): Library not initialized.  Call photon_init() first");
        pthread_exit((void*)-1);
    }

    // FIXME: Should run only when ledger_reqtable has elements.
    while(1) {
        for(i = 0; i < dapl_processes_count; i++) {
            dapl_rdma_FIN_ledger_entry_t *curr_entry;
            curr = dapl_processes[i].local_FIN_ledger->curr;
            curr_entry = &(dapl_processes[i].local_FIN_ledger->entries[curr]);
            if (curr_entry->header != (uint8_t) 0 && curr_entry->footer != (uint8_t) 0) {
                dbg_info("ledger watcher found: %d/%u", curr, curr_entry->request);
                curr_entry->header = 0;
                curr_entry->footer = 0;

                dapl_req_t *tmp_req;

                if (htable_lookup(ledger_reqtable, (uint64_t)curr_entry->request, &test) == 0) {
                    tmp_req = test;

                    pthread_mutex_lock(&tmp_req->mtx);
                    {
                        tmp_req->state = REQUEST_COMPLETED;
                        SAFE_LIST_REMOVE(tmp_req, list);
                        SAFE_LIST_INSERT_HEAD(&unreaped_ledger_reqs_list, tmp_req, list);
                        pthread_cond_broadcast(&tmp_req->completed);
                    }
                    pthread_mutex_unlock(&tmp_req->mtx);
                }

                dapl_processes[i].local_FIN_ledger->curr = (dapl_processes[i].local_FIN_ledger->curr + 1) % dapl_processes[i].local_FIN_ledger->num_entries;
                dbg_info("ledger watcher: %d requests left in reqtable", htable_count(ledger_reqtable));
            }
        }
    }

    pthread_exit(NULL);
}

///////////////////////////////////////////////////////////////////////////////
static int __dapl_wait_ledger_mt(dapl_req_t *req) {
    ctr_info(" > __dapl_wait_ledger(%d)",req->id);

    if( _curr_cookie_count == 0 ){
        log_err("__dapl_wait_ledger(): Library not initialized.  Call photon_init() first");
        return -1;
    }

    pthread_mutex_lock(&req->mtx);
    {
        while(req->state == REQUEST_PENDING)
            pthread_cond_wait(&req->completed, &req->mtx);

        if (htable_lookup(ledger_reqtable, (uint64_t)req->id, NULL) != -1) {
            dbg_info("dapl_wait_ledger(): removing RDMA: %u", req->id);
            htable_remove(ledger_reqtable, (uint64_t)req->id, NULL);
            SAFE_LIST_REMOVE(req, list);
            SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
            dbg_info("dapl_wait_ledger(): %d requests left in reqtable", htable_count(ledger_reqtable));
        }
    }
    pthread_mutex_unlock(&req->mtx);

    return (req->state == REQUEST_COMPLETED)?0:-1;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// the actual photon XSP API (phorwarder)

int photon_xsp_init(int nproc, int myrank, MPI_Comm comm, int *phorwarder) {
    int ret;
    if((ret = dapl_init(nproc, myrank, comm, 1)) != 0)
        return ret;
    *phorwarder = _photon_fp;
    ctr_info(" > photon_xsp_init(%d, %d): %d", nproc, myrank, _photon_fp);
    return 0;
}

inline int photon_xsp_init_server(int nproc) {
    return dapl_xsp_init_server(nproc);
}

inline int photon_xsp_phorwarder_io_init(char *file, int amode, MPI_Datatype view, int niter) {
    return dapl_xsp_phorwarder_io_init(file, amode, view, niter);
}

int photon_xsp_phorwarder_io_finalize() {
    return 0;
}


////////////////////////////////////////////////////////////////
// Util methods for the XSP libphoton server implementation

int dapl_xsp_register_session(libxspSess *sess) {
    int i;

    if (sess_count >= _photon_nproc) {
        log_err("dapl_xsp_register_session(): Error: out of active DAT buffers!");
        return -1;
    }

    // find a process struct that has no session...
    // proc 0 has the phorwarder server info
    for (i = 1; i < _photon_nproc; i++) {
        if (!dapl_processes[i].sess)
            break;
    }

    dbg_info("dapl_xsp_register_session(): registering session to proc: %d", i);

    dapl_processes[i].sess = sess;

    sess_count++;

    return 0;
}

int dapl_xsp_unregister_session(libxspSess *sess) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_dergister_session(): Couldn't find proc associated with session");
        return -1;
    }

    dapl_processes[ind].sess = NULL;

    sess_count--;

    return 0;
}

int dapl_xsp_get_ci(libxspSess *sess, PhotonConnectInfo *ci) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_get_ci(): Couldn't find proc associated with session");
        return -1;
    }

    ci->sa = dapl_processes[ind].sa;
    ci->cq = dapl_processes[ind].clnt_conn_qual;

    return 0;
}

int dapl_xsp_set_ci(libxspSess *sess, PhotonConnectInfo *ci, PhotonConnectInfo **ret_ci) {
    int ind;
    int rnd;
    DAT_CONN_QUAL conn_qual;

    ctr_info(" > dapl_xsp_set_ci()");

    if( _curr_cookie_count == 0 ){
        log_err("dapl_xsp_set_ci(): Library not initialized.  Call photon_xsp_init_server() first");
        goto error_exit;
    }

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_set_ci(): Couldn't find proc associated with session");
        goto error_exit;
    }

    if (ci) {
        dapl_processes[ind].sa = ci->sa;
        dapl_processes[ind].clnt_conn_qual = ci->cq;
    }
    else
        goto error_exit;


    srand((unsigned int)time(NULL));
    rnd = 1+(int)rand();
    conn_qual = ((uint64_t)ind+1) << 35 | ((uint64_t)rnd << 3);

    dbg_info("dapl_xsp_set_ci(): creating listener: %d/%llu", ind, (unsigned long long) conn_qual);

    if (dapl_create_listener(ia, pz, &conn_qual, cevd, dto_evd, &dapl_processes[ind].srvr_ep, &dapl_processes[ind].srvr_psp) != 0 ) {
        log_err("dapl_xsp_set_ci(): Couldn't create listener for process %d", ind);
        goto error_exit;
    }
    dapl_processes[ind].srvr_conn_qual = conn_qual;

    if (*ret_ci) {
        // fill in the return connect info
        bcopy(self_addr->ifa_addr, &((*ret_ci)->sa), sizeof(struct sockaddr_storage));
        (*ret_ci)->cq = dapl_processes[ind].srvr_conn_qual;
    }

    return 0;

    error_exit:
    return -1;
}

int dapl_xsp_wait_connect(libxspSess *sess) {
    DAT_RETURN retval;
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_get_ri(): Couldn't find proc associated with session");
        return -1;
    }

    // we got the remote info so that means client end should be listening
    // now connect
    dbg_info("dapl_xsp_set_ci(): connecting to %d/%llu", ind, (unsigned long long int) dapl_processes[ind].clnt_conn_qual);
    dbg_info("dapl_xsp_set_ci(): at address %s", inet_ntoa(((struct sockaddr_in *)&dapl_processes[ind].sa)->sin_addr));

    retval = dat_client_connect(ia, pz, (struct sockaddr *) &(dapl_processes[ind].sa), dapl_processes[ind].clnt_conn_qual, cevd, dto_evd, &dapl_processes[ind].clnt_ep);
    if (retval != DAT_SUCCESS) {
        log_dat_err(retval, "dapl_xsp_set_ci(): Failed to connect to process %d", ind);
        goto error_exit;
    }

    if (dapl_wait_connect(1, 1) != 0) {
        log_err("dapl_xsp_set_ci(): Could not complete all connections");
        goto error_exit;
    }

    return 0;

    error_exit:
    return -1;
}


int dapl_xsp_get_ri(libxspSess *sess, PhotonRIInfo *ri) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_get_ri(): Couldn't find proc associated with session");
        return -1;
    }

    if (ri) {
        ri->recv_ledger = (DAT_VADDR) dapl_processes[_photon_fp].local_rcv_info_ledger->entries;
        ri->send_ledger = (DAT_VADDR) dapl_processes[_photon_fp].local_snd_info_ledger->entries;
        ri->rmr = shared_storage->rmr_context;
    }
    else
        return -1;

    return 0;
}

int dapl_xsp_set_ri(libxspSess *sess, PhotonRIInfo *ri, PhotonRIInfo **ret_ri) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_set_ri(): Couldn't find proc associated with session");
        return -1;
    }

    if (ri) {
        dapl_processes[ind].remote_rcv_info_ledger->remote.context = ri->rmr;
        dapl_processes[ind].remote_snd_info_ledger->remote.context = ri->rmr;
        dapl_processes[ind].remote_rcv_info_ledger->remote.address = ri->recv_ledger;
        dapl_processes[ind].remote_snd_info_ledger->remote.address = ri->send_ledger;
    }
    else
        return -1;

    if (*ret_ri) {
        (*ret_ri)->recv_ledger = (DAT_VADDR) dapl_processes[ind].local_rcv_info_ledger->entries;
        (*ret_ri)->send_ledger = (DAT_VADDR) dapl_processes[ind].local_snd_info_ledger->entries;
        (*ret_ri)->rmr = shared_storage->rmr_context;
    }
    else
        return -1;

    return 0;
}

int dapl_xsp_get_fi(libxspSess *sess, PhotonFINInfo *fi) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_set_ri(): Couldn't find proc associated with session");
        return -1;
    }

    if (fi) {
        fi->fin_ledger = (DAT_VADDR) dapl_processes[ind].local_FIN_ledger->entries;
        fi->rmr = shared_storage->rmr_context;
    }
    else
        return -1;

    return 0;
}

int dapl_xsp_set_fi(libxspSess *sess, PhotonFINInfo *fi, PhotonFINInfo **ret_fi) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_set_ri(): Couldn't find proc associated with session");
        return -1;
    }

    if (fi) {
        dapl_processes[_photon_fp].remote_FIN_ledger->remote.address = fi->fin_ledger;
        dapl_processes[_photon_fp].remote_FIN_ledger->remote.context = fi->rmr;
    }
    else
        return -1;

    if (*ret_fi) {
        (*ret_fi)->fin_ledger = (DAT_VADDR) dapl_processes[ind].local_FIN_ledger->entries;
        (*ret_fi)->rmr = shared_storage->rmr_context;
    }
    else
        return -1;

    return 0;
}

int dapl_xsp_set_io(libxspSess *sess, PhotonIOInfo *io) {
    int ind;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_set_io(): Couldn't find proc associated with session");
        return -1;
    }

    dapl_processes[ind].io_info = io;

    return 0;
}

int dapl_xsp_do_io(libxspSess *sess) {
    int i;
    int ind;
    int ndimensions;
    int bufsize;
    char *filename;
    FILE *file;
    void *buf[2];
    MPI_Aint dtextent;
    uint32_t request;
    ProcessInfo *p;

    if (dapl_xsp_lookup_proc(sess, &ind) < 0) {
        log_err("dapl_xsp_do_io(): Couldn't find proc associated with session");
        return -1;
    }

    p = &dapl_processes[ind];

    if (!p->io_info) {
        log_err("dapl_xsp_do_io(): Trying to do I/O without I/O Info set");
        return -1;
    }

    /* TODO: So this is how I think this will go: */
    if (p->io_info->view.combiner != MPI_COMBINER_SUBARRAY) {
        log_err("dapl_xsp_do_io(): Unsupported combiner");
        return -1;
    }

    /* We can't do this because it requires MPI_Init. We need to figure out
     * the best way to support MPI_Datatypes. Also, OpenMPI doesn't use simple
     * int constants like MPICH2, so I'm not sure how we would transfer these.
     *
     * MPI_Type_get_true_extent(p->io_info->view.datatypes[0], &dtlb, &dtextent);
     */
    if (p->io_info->view.datatypes[0] == PHOTON_MPI_DOUBLE) {
        dtextent = 8;
    } else {
        log_err("dapl_xsp_do_io(): Unsupported datatype");
        return -1;
    }

    bufsize = dtextent;
    ndimensions = p->io_info->view.integers[0];
    for(i = ndimensions+1; i <= 2*ndimensions; i++) {
        bufsize *= p->io_info->view.integers[i];
    }

    buf[0] = malloc(bufsize);
    buf[1] = malloc(bufsize);
    if (buf[0] == NULL || buf[1] == NULL) {
        log_err("dapl_xsp_do_io(): Out of memory");
        return -1;
    }

    if(dapl_register_buffer(buf[0], bufsize) != 0) {
        log_err("dapl_xsp_do_io(): Couldn't register first receive buffer");
        return -1;
    }

    if(dapl_register_buffer(buf[1], bufsize) != 0) {
        log_err("dapl_xsp_do_io(): Couldn't register second receive buffer");
        return -1;
    }

    /* For now we just write locally (fileURI is a local path on phorwarder) */
    filename = malloc(strlen(p->io_info->fileURI) + 10);
    sprintf(filename, "%s_%d", p->io_info->fileURI, ind);
    file = fopen(filename, "w");
    if (file == NULL) {
        log_err("dapl_xsp_do_io(): Couldn't open local file %s", filename);
        return -1;
    }

    dapl_post_recv_buffer_rdma(ind, buf[0], bufsize, 0, &request);
    /* XXX: is the index = rank? FIXME: not right now! */
    for (i = 1; i < p->io_info->niter; i++) {
        dapl_wait(request);
        /* Post the second buffer so we can overlap the I/O */
        dapl_post_recv_buffer_rdma(ind, buf[i%2], bufsize, i, &request);

        if(fwrite(buf[(i-1)%2], 1, bufsize, file) != bufsize) {
            log_err("dapl_xsp_do_io(): Couldn't write to local file %s: %m", filename);
            return -1;
        }
        /* For now we just write locally, AFAIK this is blocking */
        /*
         * Process the buffer:
         * This is one iteration of (I/O) data in a contiguous buffer.
         * The actual data layout is specified by p->io_info.view.
         * Basically what we do is have p->io_info.view described on the
         * eXnode as the metadata for what this particular node is writing.
         * Each node will write to its own file and we keep track of the
         * offsets using p->io_info.view and the iteration number.
         * Right now I'm assuming only new files and write only.
         *
         * TODO: add 'file offset' to PhotonIOInfo so we can keep track.
         *   This should also allow the client to start writing at any place
         *   in the file.
         *
         * So this buffer would actually need to be moved somewhere else
         * where we manage the transfer to the I/O server (or write it locally).
         */
    }
    /* wait for last write */
    dapl_wait(request);
    if(fwrite(buf[(i-1)%2], 1, bufsize, file) != bufsize) {
        log_err("dapl_xsp_do_io(): Couldn't write to local file %s: %m", filename);
        return -1;
    }
    fclose(file);
    free(filename);
    free(buf[0]);
    free(buf[1]);

    return 0;
}

int dapl_xsp_post_recv(libxspSess* sess, char *ptr, uint32_t size, uint32_t *request) {

    return 0;
}

int dapl_xsp_post_send(libxspSess* sess, char *ptr, uint32_t size, uint32_t *request) {

    return 0;
}

int dapl_xsp_post_recv_buffer_rdma(libxspSess* sess, char *ptr, uint32_t size, int tag, uint32_t *request) {

    return 0;
}

int dapl_xsp_post_send_buffer_rdma(libxspSess* sess, char *ptr, uint32_t size, int tag, uint32_t *request) {

    return 0;
}

int dapl_xsp_post_send_request_rdma(libxspSess* sess, uint32_t size, int tag, uint32_t *request) {

    return 0;
}

int dapl_xsp_wait_recv_buffer_rdma(libxspSess* sess, int tag) {

    return 0;
}

int dapl_xsp_post_os_put(libxspSess* sess, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {

    return 0;
}

int dapl_xsp_post_os_get(libxspSess *sess, char *ptr, uint32_t size, int tag, uint32_t remote_offset, uint32_t *request) {

    return 0;
}

#endif
