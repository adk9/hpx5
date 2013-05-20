#ifndef VERBS_REMOTE_BUFFER_H
#define VERBS_REMOTE_BUFFER_H

#include <stdint.h>
#include <infiniband/verbs.h>

typedef struct verbs_remote_buffer {
    uint32_t request;
//    DAT_RMR_CONTEXT context;
//    DAT_VADDR address;
    uintptr_t addr;
    uint32_t  lkey;
    uint32_t  rkey;
    uint32_t size;
    int tag;
} verbs_remote_buffer_t;

verbs_remote_buffer_t *verbs_remote_buffer_create();
void verbs_remote_buffer_free(verbs_remote_buffer_t *drb);

#endif
