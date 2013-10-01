#pragma once
#ifndef LIBPHOTON_H
#define LIBPHOTON_H

#include <stdint.h>
#include <pthread.h>
#include "config.h"
#include "photon.h"
#include "photon_backend.h"
#include "squeue.h"

#define NULL_COOKIE			    0x0

/* Global config for the library */
photonConfig __photon_config;
photonBackend __photon_backend;

/* photon transfer requests */
typedef struct photon_req_t {
	LIST_ENTRY(photon_req_t) list;
	uint32_t id;
	int state;
	int type;
	int proc;
	int tag;
	pthread_mutex_t mtx;
	pthread_cond_t completed;
} photon_req;

typedef struct photon_req_t * photonRequest;

/* photon memory registration requests */
struct photon_mem_register_req {
	SLIST_ENTRY(photon_mem_register_req) list;
	char *buffer;
	int buffer_size;
};

#ifdef DEBUG
/* defined in util.c */
time_t _tictoc(time_t stime, int proc);
#endif

#endif
