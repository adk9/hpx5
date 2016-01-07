#ifndef __PH_H__
#define __PH_H__
#include <photon.h>
#include <mpi.h>
#include <stdbool.h>

extern struct photon_config_t cfg ;

static bool __initialized = false;

#define PH_OK true

typedef struct photon_req{
	bool send_req ;
	photon_rid req_id;
	int sink;
        char* req_buf;
        int buf_size;	
	int completed;
} photon_req;

bool PHOTON_Testall(int req_count, photon_req* req_array, int* flag);
int init_photon_backend(int my_address, int total_nodes);
#endif
