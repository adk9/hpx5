#ifndef PHOTON_FORWARDER_H
#define PHOTON_FORWARDER_H

struct photon_forwarder_interface_t {
  int (*init)(photonConfig cfg, ProcessInfo *photon_processes);
  int (*io_finalize)(ProcessInfo *photon_processes);
  int (*io_init)(ProcessInfo *photon_processes, char *file, int amode, void *view, int niter);
  void (*io_print)(void *io);
  int (*exchange)(ProcessInfo *pi);
  int (*connect)(ProcessInfo *pi);
} photon_forwarder_interface;

typedef struct photon_forwarder_interface_t * photonForwarder;

#ifdef HAVE_XSP
extern struct photon_forwarder_interface_t xsp_forwarder;
#endif

#endif
