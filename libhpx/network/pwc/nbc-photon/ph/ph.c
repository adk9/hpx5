#include <ph.h>
#include <stdbool.h>

struct photon_config_t cfg = {
	.nproc = 0,
	.address = 0,
	.forwarder = {
		.use_forwarder = 0
	},
	.ibv = {
		.use_cma = 0,
		.use_ud  = 0,
		.num_srq = 0,
		.eth_dev = "roce0",
		/*.ib_dev  = "mlx4_0+qib0",*/
		.ib_dev  = "qib0",
	},
	.ugni = {
		.bte_thresh = -1,
	},
	.cap = {
		.small_msg_size = -1,
		.small_pwc_size = -1,
		.eager_buf_size = -1,
		.ledger_entries = -1,
		.max_rd         = -1,
		.default_rd     = -1,
		.num_cq         = -1,
		.use_rcq        =  0
	},
	.exch = {
		.allgather = NULL,
		.barrier = NULL
	},
	.meta_exch = PHOTON_EXCH_MPI,
	.comm = NULL,
	.backend = PHOTON_BACKEND_VERBS
};


int init_photon_backend(int my_address, int total_nodes){

	if(!__initialized && __sync_bool_compare_and_swap(&__initialized, false, true)){	
	  if(!photon_initialized()){
		cfg.address = my_address;	
		cfg.nproc = total_nodes;	
		photon_init(&cfg);
		printf("===[%d] rank, init complete \n", my_address);
	  }else {
		printf("===[%d] rank, photon already initialized \n", my_address);
	  }
	}
	return 0;
}


bool PHOTON_Testall(int req_count, photon_req* req_array, int* done){
	int i, type;
	struct photon_status_t stat;
	for (i = 0; i < req_count; ++i) {
		/*printf("[==PHOTON TEST] req count: [%d] req_id : [%ld] dest : [%d]  is a send : [%d] completed ? : [%d] \n",*/
				       /*req_count, req_array[i].req_id, req_array[i].sink, req_array[i].send_req, req_array[i].completed );*/
		if(!req_array[i].completed){
		  int flag;	
		  int tst = photon_test(req_array[i].req_id, &flag, &type, &stat);
		  /*printf("[PHOTON TEST] req count: [%d] req_id : [%ld]  test flag: [%d]  completed flag :[%d] \n",*/
				  /*req_count, req_array[i].req_id ,tst, flag);*/
		  if(tst == 0 && flag){
			if(req_array[i].send_req){
				photon_send_FIN(req_array[i].req_id, req_array[i].sink, 0);
			}
			req_array[i].completed = 1;
		        /*printf("[PHOTON TEST SUCCESS] req count: [%d] req_id : [%ld]  test flag: [%d]  completed flag :[%d] req_buf : [%p] req_buf_val [%d] buf_sz : %d  a send ? : %s \n",
				  req_count, req_array[i].req_id ,tst, flag, req_array[i].req_buf, *((int*) req_array[i].req_buf), 
				  req_array[i].buf_size, req_array[i].send_req? "yes":"no" );
			*/	  
			photon_unregister_buffer(req_array[i].req_buf, req_array[i].buf_size);
			continue;
		  }else if(tst > 0 || tst < 0){
		        printf("[ERROR! HOTON TEST] req count: [%d] req_id : [%ld]  test flag: [%d]  completed flag :[%d] \n",
				       	req_count, req_array[i].req_id ,tst, flag);
			return !PH_OK;
		  }
		}
	}
	int all_done = 1;
	for (i = 0; i < req_count; ++i) {
	  all_done = all_done && req_array[i].completed ;			
	}
	/*printf("######TEST_ALL : %d \n", all_done);*/
	*done= all_done;
	return PH_OK;
}


