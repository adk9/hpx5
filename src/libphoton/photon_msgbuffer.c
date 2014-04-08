#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "photon.h"
#include "logging.h"
#include "photon_msgbuffer.h"

photonMsgBuf photon_msgbuffer_new(uint64_t size, uint64_t p_size, int p_offset, int p_hsize) {

  photonMsgBuf mbuf;
  void *bptr;
  int page_size;
  int l1d_cacheline;
  int ret, i;
  
  page_size = sysconf(_SC_PAGESIZE);
  l1d_cacheline = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
  
  mbuf = malloc(sizeof(struct photon_msgbuffer_t));
  if (!mbuf)
    goto error_exit;
  
  memset(mbuf, 0, sizeof(mbuf));

  // TODO: probably want to align each entry (p_size) on a cacheline  
  ret = posix_memalign(&bptr, page_size, size);
  if (ret) {
    dbg_err("could not allocate buffer space");
    goto error_exit_buf;
  }

  mbuf->db = photon_buffer_create(bptr, size);
  if (!mbuf->db) {
    dbg_err("could not create photon buffer");
    goto error_exit_buf;
  }

  mbuf->p_size = p_size;
  mbuf->m_size = p_size - p_offset - p_hsize;
  mbuf->p_offset = p_offset;
  mbuf->p_hsize = p_hsize;
  mbuf->p_count = (int)(size / mbuf->p_size);

  // create metadata to track buffer offsets
  mbuf->entries = (photon_mbe*)malloc(mbuf->p_count * sizeof(photon_mbe));
  if (!mbuf->entries) {
    dbg_err("could not allocate buf entries");
    goto error_exit_db;
  }

  for (i = 0; i < mbuf->p_count; i++) {
    mbuf->entries[i].base = (void*)(mbuf->db)->buf.addr + (i * p_size);
    mbuf->entries[i].hptr = (void*)(mbuf->db)->buf.addr + (i * p_size) + p_offset;
    mbuf->entries[i].mptr = (void*)(mbuf->db)->buf.addr + (i * p_size) + p_offset + p_hsize;
    mbuf->entries[i].empty = false;
  }

  mbuf->s_index = 0;
  mbuf->status = 0;

  return mbuf;

 error_exit_db:
  photon_buffer_free(mbuf->db);
 error_exit_buf:
  free(bptr);
 error_exit:
  return NULL;
}

int photon_msgbuffer_free(photonMsgBuf mbuf) {
  if (mbuf) {
    if (mbuf->entries)
      free(mbuf->entries);
    if (mbuf->db)
      photon_buffer_free(mbuf->db);
    free(mbuf);
    return PHOTON_OK;
  }
  return PHOTON_ERROR;
}
