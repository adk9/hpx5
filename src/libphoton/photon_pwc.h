#ifndef PHOTON_PWC_H
#define PHOTON_PWC_H

#include "libphoton.h"

#define PWC_ALIGN      8


PHOTON_INTERNAL int _photon_put_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
						struct photon_buffer_priv_t priv,
						photon_rid local, photon_rid remote, int flags);
PHOTON_INTERNAL int _photon_get_with_completion(int proc, void *ptr, uint64_t size, void *rptr,
						struct photon_buffer_priv_t priv,
						photon_rid local, int flags);
PHOTON_INTERNAL int _photon_probe_completion(int proc, int *flag, photon_rid *request, int flags);

#endif
