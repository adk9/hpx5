#include <stdio.h>
#include <arpa/inet.h>
#include "photon.h"

#define NUM_NODES 16

typedef struct bravo_node_t {
  int           index;
  uint32_t      s_addr;
  photon_addr  *block;
  int           nblocks;
} bravo_node;

static bravo_node *node;

bravo_node *init_bravo_ids(int num_blocks, int myrank, char *addr) {
  int i, j;

  node = malloc(NUM_NODES * sizeof(bravo_node));

  for (i=0; i<NUM_NODES; i++) {
    node[i].block = calloc(num_blocks, sizeof(photon_addr));
    node[i].index = i;
    node[i].nblocks = num_blocks;
  }
  fprintf(detailed_log,"Initializing ID %d with IP addr = %s\n", myrank, addr);

  inet_pton(AF_INET, addr, &node[myrank].s_addr);
  
  for (i=0; i<NUM_NODES; i++) {
    for (j=0; j<num_blocks; j++) {
      uint32_t bid = ((uint32_t)(i+1)<<16) | (uint32_t)j;
      node[i].block[j].blkaddr.blk3 = bid;
    }
  }

  return node;
}

bravo_node *find_bravo_node(photonAddr naddr) {
  int i;
  for (i=0; i< NUM_NODES; i++) {
    if (naddr->s_addr == node[i].s_addr)
      return &node[i];
  }
  return NULL;
}

bravo_node *get_bravo_node(int id) {
  return &node[id];
}
