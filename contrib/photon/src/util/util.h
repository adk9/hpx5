#ifndef util_H
#define util_H

#include "photon.h"

typedef struct photon_dev_list_t {
  struct photon_dev_list_t *next;
  char                     *dev;
  int                       port;
} photon_dev_list;

int photon_scan(const char *s, int c, int len) ;
int photon_parse_devstr(const char *devstr, photon_dev_list **ret_list);
int photon_match_dev(photon_dev_list *dlist, const char *dev, int port);
void photon_free_devlist(photon_dev_list *d);
void photon_gettime_(double *s);
const char *photon_addr_str(photon_addr *addr, int af);

#endif
