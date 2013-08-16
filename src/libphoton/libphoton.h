#pragma once
#ifndef LIBPHOTON_H
#define LIBPHOTON_H

#include <stdint.h>
#include "config.h"
#include "photon.h"
#include "photon_backend.h"

/* Global config for the library */
photonConfig __photon_config;
photonBackend __photon_backend;

#endif
