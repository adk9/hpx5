#ifndef PHOTON_IO_H
#define PHOTON_IO_H

typedef enum { PHOTON_CI,
	       PHOTON_RI,
	       PHOTON_SI,
	       PHOTON_FI,
	       PHOTON_IO,
	       PHOTON_MTU,
	       PHOTON_GET_ALIGN,
	       PHOTON_PUT_ALIGN 
} photon_info_t;

int photon_io_init(char *file, int amode, void *view, int niter);
int photon_io_finalize();
void photon_io_print_info(void *io);

#endif
