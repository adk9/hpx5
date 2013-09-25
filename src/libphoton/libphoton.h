#pragma once
#ifndef LIBPHOTON_H
#define LIBPHOTON_H

#include <stdint.h>
#include "config.h"
#include "photon.h"
#include "photon_backend.h"

#define NULL_COOKIE			    0x0


/* Global config for the library */
photonConfig __photon_config;
photonBackend __photon_backend;

/* pointer to the context used for each backend */
typedef void * photonContext;

#endif
