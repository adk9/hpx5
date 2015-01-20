#include <sys/mman.h>

#include "libhpx/logging.h"

//unsigned int get_logging_record_size(unsigned int user_data_size) {
//  return sizeof(hpx_logging_event_t) + user_data_size;
//}

int logtable_init(logtable_t *logtable, char* filename, size_t total_size) {
  unsigned int record_size = sizeof(hpx_logging_event_t); // change this if user data size can vary
  int fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) { 
    // TODO hpx-specific error handling
    perror("init_action: open");
    return -1;
  }
  lseek(fd, logtable->data_length-1, SEEK_SET);
  int bytes = write(fd, "", 1);
  if (bytes != 1)
    return -1;

  void* data = mmap(NULL, logtable->data_length, 
                    PROT_READ|PROT_WRITE, MAP_SHARED | MAP_NORESERVE, 
                    fd, 0);
  //  printf("mmap at %p\n", data);
  if (data == MAP_FAILED) {
    // TODO hpx-specific error handling
    perror("init_action: mmap");
    printf("errno = %d (%s)\n", errno, strerror(errno));
    return -1;
  }

  logtable->record_size = record_size;
  snprintf(filename, 256, "%s", logtable->filename);
  logtable->data_size = total_size;
  logtable->data   = data;
  logtable->record_size = record_size;
  logtable->fd     = fd;
  logtable->index  = 0;
  logtable->inited = true;

  return HPX_SUCCESS;
}

void logtable_fini(logtable_t *logtable) {
  munmap(logtable->data, logtable->data_size);
  int error = ftruncate(logtable->fd, logtable->index * logtable->record_size);
  if (error) {
    // TODO error handling
    perror("fini: ftruncate");
  }

  error = close(logtable->fd);
  if (error) {
    // TODO error handling
    perror("shutdown: close");
  }
}

hpx_logging_event_t* logtable_next_and_increment(logtable_t *lt) {
  unsigned int index = sync_fadd(lt->index, 1, SYNC_RELAXED);
  return (hpx_logging_event_t*)((uintptr_t)lt->data + lt->record_size * index);
}
