#include "photon.h"
#include <arpa/inet.h>

#define NUM_NODES 8

#define B001      0
#define B002      1
#define B003      2
#define B004      3
#define B005      4
#define B006      5
#define B007      6
#define B008      7

typedef struct bravo_node_t {
  int           index;
  uint32_t      s_addr;
  photon_addr   mgid;
} bravo_node;

static bravo_node *node;

inline bravo_node *init_bravo_ids() {
  int i;
  node = malloc(NUM_NODES * sizeof(bravo_node));

  for (i=0; i<NUM_NODES; i++) {
    node[i].index = i;
  }

  inet_pton(AF_INET, "10.1.20.10", &node[B001].s_addr);
  inet_pton(AF_INET, "10.1.20.20", &node[B002].s_addr);
  inet_pton(AF_INET, "10.1.20.30", &node[B003].s_addr);
  inet_pton(AF_INET, "10.1.20.40", &node[B004].s_addr);
  inet_pton(AF_INET, "10.1.20.50", &node[B005].s_addr);
  inet_pton(AF_INET, "10.1.20.60", &node[B006].s_addr);
  inet_pton(AF_INET, "10.1.20.70", &node[B007].s_addr);
  inet_pton(AF_INET, "10.1.20.80", &node[B008].s_addr);

  // 224.0.2.X
  inet_pton(AF_INET6, "ff0e::ffff:e000:0201", node[B001].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0202", node[B002].mgid.raw);    
  inet_pton(AF_INET6, "ff0e::ffff:e000:0203", node[B003].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0204", node[B004].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0205", node[B005].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0206", node[B006].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0207", node[B007].mgid.raw);
  inet_pton(AF_INET6, "ff0e::ffff:e000:0208", node[B008].mgid.raw);
  
  // node   OF_port   MAC (for rewrite)
  // B001   12        c9ff:fe4b:1c9c
  // B002   13        c9ff:fe35:0350
  // B003   14        c9ff:fe17:c5d0
  // B004   15        c9ff:fe18:0700
  // B005   16        c9ff:fe18:1d50
  // B006   17        c9ff:fe18:1040
  // B007   18        14ff:fe01:7170
  // B008   19        14ff:fe01:6ff0
  
  return node;
}

inline bravo_node *find_bravo_node(photonAddr naddr) {
  int i;
  for (i=0; i< NUM_NODES; i++) {
    if (naddr->s_addr == node[i].s_addr)
      return &node[i];
  }
  return NULL;
}

inline bravo_node *get_bravo_node(int id) {
  return &node[id];
}
