#ifndef util_H
#define util_H

#include <stdbool.h>
#include "photon.h"
#include "config.h"

#define ALIGN(s, b) (((s) + ((b) - 1)) & ~((b) - 1))
#define TEST_ALIGN(t, a) (((uintptr_t)t % a) == 0)

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

static inline __attribute__((const))
bool is_power_of_2(unsigned long n)
{
  return (n != 0 && ((n & (n - 1)) == 0));
}

#endif
