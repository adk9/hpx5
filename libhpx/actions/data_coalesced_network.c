#include <stdlib.h>
#include <string.h>
#include <libsync/queues.h>
#include <libsync/sync.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include <libhpx/worker.h>
#include <inttypes.h>

#define COALESCING_SIZE 1000
// #define RATE_BASED_FLUSH_HEURISTIC 1
#define ACCELERATION_BASED_FLUSH_HEURISTIC 2
#define RATE_THRESHOLD 1



//TODO: Fix types
//TODO: check whether ampp max_count is equal to coalescing size
unsigned int max_count = COALESCING_SIZE;
double elapsed_time;


static inline void _atomic_inc(volatile int *addr) {
  sync_fadd(addr, 1, SYNC_ACQ_REL);
}

static inline void _atomic_dec(volatile int *addr) {
  sync_fadd(addr, -1, SYNC_ACQ_REL);
}

hpx_action_t _demux;
int _demux_handler(char* buffer, int n) {
  // First store the current address, so that once we are done with the following loop, we can restore the target address.
  assert(n);
  worker_t *w = self;
  hpx_addr_t previous_target = w->current->target;

  // printf("Inside the demux\n");
  //For data coalesced action, we send the action id for the invoked action on data in the continuation id field of the parcel
  hpx_action_t action_id = hpx_thread_current_cont_action();
  assert(action_id == 103);
  hpx_thread_void_current_cont_action();
  const action_t *action = &actions[action_id];
  hpx_action_handler_t handler = (hpx_action_handler_t)action->handler;
  while (n) {
    hpx_addr_t addr;
    memcpy(&addr, buffer, sizeof(hpx_addr_t));
    // printf("Received address %zu\n", addr);
    buffer += sizeof(hpx_addr_t);
    n -= sizeof(hpx_addr_t);
    uint64_t value = 0;
    memcpy(&value, buffer, sizeof(uint64_t));
    buffer += sizeof(uint64_t);
    n -= sizeof(uint64_t);
    w->current->target = addr; //TODO: Do we really need this line? what will happen in case of stealing?
    // printf("Calling the handler in the reciever side\n");
    handler(&value, sizeof(value));
    // hpx_call(addr, action_id, HPX_NULL, &value, sizeof(value));
    // process_data(addr, value);
  }
  //TODO: clean up for the thread by setting up the address
  w->current->target = previous_target;
  // printf("Done executing demultiplexer\n");
  return HPX_SUCCESS;
}

HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demux, _demux_handler,
                     HPX_POINTER, HPX_INT);

static void _clear_all(action_t *action, uint32_t dest) {
  //TODO: Free the fat parcel first.
  hpx_action_t *data_action_id = action->id;
  size_t bytes = COALESCING_SIZE * (sizeof(hpx_addr_t) + sizeof(uint64_t));
  action->coalesced_dest_buffer[dest].fatp = action_new_parcel(_demux,
				HPX_THERE(dest), 0, *data_action_id, 2, NULL, bytes);
  action->coalesced_dest_buffer[dest].next = hpx_parcel_get_data(action->coalesced_dest_buffer[dest].fatp);

  sync_store(&((action->coalesced_dest_buffer[dest].fatp)->size), 0, SYNC_RELEASE);
  // sync_store(&action->coalesced_dest_buffer[dest].next, (char*) hpx_parcel_get_data(action->coalesced_dest_buffer[dest].fatp),  SYNC_RELAXED);

  unsigned int _clear_counts = 0;
  sync_store(&action->coalesced_dest_buffer[dest].count_written, _clear_counts, SYNC_RELEASE);
  sync_store(&action->coalesced_dest_buffer[dest].count_allocated, _clear_counts, SYNC_RELEASE);

  // printf("Clearing the fat parcel done.\n");
}

static bool send_buffer(action_t *action, uint32_t dest, unsigned int my_id) {
  assert (sync_load(&action->coalesced_dest_buffer[dest].count_allocated, SYNC_RELAXED) & action->coalesced_dest_buffer[dest].sender_active);
  const unsigned int count = my_id & action->coalesced_dest_buffer[dest].count_mask;
  // assert (sync_load(&action->coalesced_dest_buffer[dest].count_mask, SYNC_RELAXED) & count);
  if ((my_id & action->coalesced_dest_buffer[dest].sender_active) != 0) // TODO: is it redundant?
    return false;
  assert (count <= max_count);
  if (count != 0) {
    // assert((action->coalesced_dest_buffer[dest].fatp)->size ==16);
    while (sync_load(&action->coalesced_dest_buffer[dest].count_written, SYNC_ACQUIRE) != count) {
      ;
    }
    unsigned int count_written = sync_load(&(action->coalesced_dest_buffer[dest].count_written), SYNC_ACQUIRE) * (sizeof(hpx_addr_t) + sizeof(uint64_t));
    sync_store(&(action->coalesced_dest_buffer[dest].fatp)->size, count_written, SYNC_RELEASE);
    assert((action->coalesced_dest_buffer[dest].fatp));
    assert(action->coalesced_dest_buffer[dest].fatp->action);
    hpx_addr_t send = hpx_lco_future_new(0);
    hpx_parcel_t *temp = action->coalesced_dest_buffer[dest].fatp;
    assert(temp->action);
    _clear_all(action, dest);
    hpx_parcel_send(temp, send);
    hpx_lco_wait(send);
    hpx_lco_delete(send, HPX_NULL);
    // printf("Have sent the fat parcel with size %"PRIu32" \n", temp->size);
    return true;
  }
  return false;
}


void flush (action_t *action) {
  // printf("In flush function\n");
  for (int l = 0, e = HPX_LOCALITIES; l < e; ++l) {
    if (l == hpx_get_my_rank())
      continue;
    assert(action->coalesced_dest_buffer[l].fatp->action);
    unsigned int my_id = sync_load(&action->coalesced_dest_buffer[l].count_allocated, SYNC_RELAXED);

    if (my_id == action->coalesced_dest_buffer[l].sender_active) {
      continue;
    }
    unsigned int temp = my_id;
    if (my_id != sync_load(&action->last_active[l], SYNC_RELAXED)) {
      sync_store(&action->last_active[l], my_id, SYNC_RELAXED);
      continue;
    }
    while (true) {
      if (my_id > 0 && my_id < max_count) {
	//TODO:my_id changed as a side effect of sync_cas?
	temp = my_id;
	if (sync_load(&action->coalesced_dest_buffer[l].count_allocated, SYNC_ACQUIRE) == 0) {
	  my_id = 0;
	  break;
	}
	if(sync_cas(&action->coalesced_dest_buffer[l].count_allocated,
		    &temp, action->coalesced_dest_buffer[l].sender_active,
		    SYNC_ACQ_REL, SYNC_RELAXED)) {
	  break;
	}
	;// 	  amplusplus::detail::do_pause();

      } else {
	break;
      }
    }
    // TODO: this is a flush operation, so the data parcel count can be less than coalescing size, so do we need to adjust any size?
    /* if (sync_load(&action->coalesced_dest_buffer[l].count_allocated, SYNC_ACQUIRE) == 0) { */
    /*   continue; */
    /* } */
    /* if (sync_load(&action->coalesced_dest_buffer[l].count_allocated, SYNC_RELAXED) */
    /* 	== action->coalesced_dest_buffer[l].sender_active) */
      if (my_id > 0 && my_id < max_count) {
	uint64_t flushed_size = my_id;
	// printf("Have flushed total no of fat parcel  %"PRIu32" \n", flushed_size);
	send_buffer(action, l, my_id);
      }
  }
}

void flush_all(void) {

#ifdef RATE_BASED_FLUSH_HEURISTIC
  elapsed_time = hpx_time_elapsed_ms(time_now)/1e3;
#endif

  for (int i = 0, e = coalesced_action_table_size(); i < e; i++) {
    action_t *action = &(actions[coalesced_action_id[i]]);
    /* while (sync_load(&action->syncflush, SYNC_ACQUIRE)) { */
    /*   /\* spin *\/; */
    /* } */

    assert(coalesced_action_id[i] == 103);
    // printf("Calling flush on action with id %d\n",coalesced_action_id[i]);
#ifdef RATE_BASED_FLUSH_HEURISTIC
    uint64_t current_msg_count = sync_load(&(action->current_count), SYNC_RELAXED);
    uint64_t previous_msg_count = sync_load(&(action->previous_count), SYNC_RELAXED);
    if (((current_msg_count - previous_msg_count) / elapsed_time) > RATE_THRESHOLD) {
      flush(action);
      sync_store(&action->previous_count, 0, SYNC_RELAXED);
      sync_store(&action->current_count, 0, SYNC_RELAXED);
      elapsed_time = 0;
      time_now = hpx_time_now();
    } //TODO: add another condition to support flushing at the very end of the program
    else {
      sync_store(&action->previous_count, current_msg_count, SYNC_RELAXED);
      elapsed_time = 0;
      time_now = hpx_time_now();
    }
#else
    // printf("In flush all\n");
    flush(action);
#endif
  }
}

static int _coalesce_data(hpx_addr_t addr, action_t *action,  uint32_t dest,
				      void *args, int bytes) {
  // Coalesce on demand as long as we have enough space available.
  assert(bytes);
  while (true) {
    while (true) {
      assert (max_count > 0);
      unsigned int x =  sync_load(&action->coalesced_dest_buffer[dest].count_allocated, SYNC_RELAXED);
      // fprintf(stderr, "send_spin %08x of %x\n", x, max_count);
      if ((x & action->coalesced_dest_buffer[dest].count_mask) < max_count && (x & action->coalesced_dest_buffer[dest].sender_active) == 0) {
	break;
      }
      ;//TODO: equivalent of pausing with special purpose instruction?->//amplusplus::detail::do_pause();
    }

    unsigned int my_id = sync_fadd(&action->coalesced_dest_buffer[dest].count_allocated, 1, SYNC_ACQ_REL);
    if (my_id & action->coalesced_dest_buffer[dest].sender_active)
      continue;
    if ((my_id & action->coalesced_dest_buffer[dest].count_mask) >= max_count)
      continue;
    assert((my_id & action->coalesced_dest_buffer[dest].count_mask) < max_count);
    assert(action->coalesced_dest_buffer[dest].fatp);

    size_t current_idx = my_id * (sizeof(hpx_addr_t) + bytes);

    memcpy((action->coalesced_dest_buffer[dest].next) + current_idx, &addr, sizeof(hpx_addr_t));
    // sync_fadd(&(action->coalesced_dest_buffer[dest].fatp)->size, sizeof(hpx_addr_t), SYNC_RELAXED);
    memcpy((action->coalesced_dest_buffer[dest].next) + current_idx + sizeof(hpx_addr_t), args, bytes);
    // sync_fadd(&(action->coalesced_dest_buffer[dest].fatp)->size, bytes, SYNC_RELAXED);
    // assert((action->coalesced_dest_buffer[dest].fatp)->size);

#ifdef RATE_BASED_FLUSH_HEURISTIC
    sync_fadd(&(action->current_count), 1, SYNC_RELAXED);
#endif

    if ((my_id & action->coalesced_dest_buffer[dest].count_mask) == max_count - 1) {
      // _atomic_inc(&action->syncflush);
      sync_store(&action->coalesced_dest_buffer[dest].count_allocated, action->coalesced_dest_buffer[dest].sender_active, SYNC_RELAXED);
      sync_fadd(&(action->coalesced_dest_buffer[dest].count_written), 1, SYNC_ACQ_REL); //TODO: count_written with ACQ_REL
      // printf("Sending coalesced data\n");
      // printf("trying to send with id: %zu\n", my_id);
      send_buffer(action, dest, max_count);
      // _atomic_dec(&action->syncflush);
      // printf("Have sent the coalesced parcel with size %"PRIu32" \n", (action->coalesced_dest_buffer[dest].fatp)->size);
    } else {
      sync_fadd(&(action->coalesced_dest_buffer[dest].count_written), 1, SYNC_ACQ_REL);
    }
    break;
  }
  return LIBHPX_OK;
}

static int _coalesce_marshalled_data(hpx_addr_t addr, action_t *action,  uint64_t dest,
				      va_list *args, int n) {
  // the following 3 lines are copied from marshalled.c _new_marshalled function
  hpx_pid_t pid = hpx_thread_current_pid(); //TODO:pid needed?
  void *data = (n) ? va_arg(*args, void*) : NULL; //TODO: should we assume a certain datatype instead of void?
  int bytes = (n) ? va_arg(*args, int) : 0;

  // printf("In _coalesce_marshalled_data calling _coalesced_data\n");
  return _coalesce_data(addr, action, dest, data, bytes); //TODO: bytes or n?
}

static int _coalesce_ffi_data(hpx_addr_t addr, action_t *action, uint64_t  dest,
			       va_list *args, int n) {
  hpx_pid_t pid = hpx_thread_current_pid();
  size_t bytes = ffi_raw_size(action->cif);
  //pack
  void *buffer;
  const ffi_cif *cif = action->cif;
  DEBUG_IF (n != cif->nargs) {
    const char *key = action->key;
    dbg_error("%s requires %d arguments (%d given).\n", key, cif->nargs, n);
  }
  // copy the vaargs (pointers) to my stack array, which is what ffi
  // wants---this seems wasteful since va_args are likely *already* on my stack,
  // but that's not public information
  void *argps[n];
  for (int i = 0, e = n; i < e; ++i) {
    argps[i] = va_arg(*args, void*);
  }

  // use ffi to copy them to the buffer
  ffi_ptrarray_to_raw((void*)cif, argps, buffer);

  return _coalesce_data(addr, action, dest, buffer, bytes);
}


int coalesce_data(hpx_addr_t addr, hpx_action_t id,  uint64_t dest,
		  va_list *args, int n) {
  // 1)check in which action entry we should put in the current data-parcel
  CHECK_ACTION(id);
  action_t *action = &actions[id];

  // 2) check which type of action we are dealing with, is it marshalled or ffi

  // if marshalled-type
  if (action->attr & HPX_MARSHALLED) {
    // printf("Calling coalesed marshalled data function\n");
    _coalesce_marshalled_data(addr, action, dest, args, n);
    return HPX_SUCCESS;
  } else { // assume ffi-type
    _coalesce_ffi_data(addr, action, dest, args, n);
    // TODO: what to put in the buffer
    return HPX_SUCCESS;
  }

}


/*
oid send(const arg_type& arg, transport::rank_type dest) {
    BOOST_ASSERT (trans.is_valid_rank(dest));
    message_buffer& buf = outgoing_buffers[dest];
    const unsigned int max_count = buf.max_count;
    while (true) {
      while (true) {
        assert (max_count > 0);
        unsigned int x = buf.count_allocated.load();
        // fprintf(stderr, "send_spin %08x of %x\n", x, max_count);
        if ((x & message_buffer::count_mask) < max_count && (x & message_buffer::sender_active) == 0) {
          break;
        }
        amplusplus::detail::do_pause();
      }
      unsigned int my_id = buf.count_allocated.fetch_add(1);
      // fprintf(stderr, "send %08x\n", my_id);
      if (my_id & message_buffer::sender_active) continue;
      if ((my_id & message_buffer::count_mask) >= max_count) continue;
      BOOST_ASSERT ((my_id & message_buffer::count_mask) < max_count);
      BOOST_ASSERT (buf.data_owner);
      BOOST_ASSERT (buf.data);
      buf.data[my_id & message_buffer::count_mask] = arg;
      if ((my_id & message_buffer::count_mask) == max_count - 1) {
        buf.count_allocated.store(message_buffer::sender_active);
        if (!buf.registered_with_td.exchange(true)) {
          this->mt.message_being_built(dest);
        }
        ++buf.count_written;
        amplusplus::performance_counters::hook_full_buffer_send(dest, max_count, sizeof(Arg));
        send_buffer(buf, max_count, dest);
      } else if ((my_id & message_buffer::count_mask) == 0) {
        if (!buf.registered_with_td.exchange(true)) {
          this->mt.message_being_built(dest);
        }
        ++buf.count_written;
      } else {
        ++buf.count_written;
      }
      break;
    }
  }

 bool send_buffer(message_buffer& buf, unsigned int my_id, transport::rank_type dest) {
    // fprintf(stderr, "%zu: send_buffer %08x to %zu, current state is %08x\n", trans.rank(), my_id, dest, buf.count_allocated.load());
    BOOST_ASSERT (buf.count_allocated.load() & message_buffer::sender_active);
    const unsigned int count = my_id & message_buffer::count_mask;
    if ((my_id & message_buffer::sender_active) != 0) return false;
    BOOST_ASSERT (!!buf.data_owner);
    BOOST_ASSERT (count <= buf.max_count);
    if (count != 0) {
      while (buf.count_written.load() != count) {amplusplus::detail::do_pause();} // Make sure all elements are written, must be an acquire so we can read data in the buffer
      BOOST_ASSERT (buf.registered_with_td.load() == true); // This may not be true before the previous line's while loop is done
      Arg* send_data = buf.data;
      boost::shared_ptr<void> send_data_owner = buf.data_owner;
      buf.clear(buf_cache->allocate());
      // fprintf(stderr, "%zu: actual send %u to %zu, current state is %08x\n", trans.rank(), count, dest, buf.count_allocated.load());
      this->mt.send(send_data, count, dest, sp_deleter(send_data_owner));
      return true;
    }
    return false;
  }
    data_owner = new_data_owner;
      data = static_cast<Arg*>(data_owner.get());
      registered_with_td.store(false);
      count_written.store(0u);
      count_allocated.store(0u); /

 */

/*
 bool flush(boost::shared_ptr<bool> alive) {
    BOOST_ASSERT (alive.get());
    if(!*alive) return false;
    // fprintf(stderr, "%zu: counter_coalesced_message_type::flush(%zu)\n", trans.rank(), dest);
    for (size_t i = 0; i < this->mt.get_possible_dests()->count(); ++i) {
      rank_type r = this->mt.get_possible_dests()->rank_from_index(i);
      BOOST_ASSERT (trans.is_valid_rank(r));
      message_buffer& buf = this->outgoing_buffers[r];
      const unsigned int max_count = buf.max_count;
      unsigned int my_id = buf.count_allocated.load();
      if(my_id != last_active[r].load()) {
      last_active[r].store(my_id);
      continue;
      }
      while (true) {
      if (my_id > 0 && my_id < max_count) {
        if (buf.count_allocated.compare_exchange_weak(my_id, message_buffer::sender_active)) break;
	  amplusplus::detail::do_pause();
	  } else {
	    break;
	    }
      }
      if (my_id > 0 && my_id < max_count) {
      amplusplus::performance_counters::hook_flushed_message_size(r, my_id, sizeof(Arg));
      send_buffer(buf, my_id, r);
      }
    }
    return true;
  }
*/
