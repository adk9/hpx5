#ifndef LOGTABLE_H
#define LOGTABLE_H

#include "libhpx/logging.h"

unsigned int get_logging_record_size(unsigned int user_data_size);

int logtable_init(logtable_t *logtable, char* filename, 
                  size_t total_size);

void logtable_fini(logtable_t *logtable);

hpx_logging_event_t* logtable_next_and_increment(logtable_t *lt);
#endif
