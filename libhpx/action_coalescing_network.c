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

#define COALESCING_SIZE 1000


static int _demux_handler(char* buffer, int n) {
  // In a for loop (normal for or par for) do the follwoing
  // 1. memcpy/extract the data and address (for graph it will be vertex address and distance data)
  // 2. Call the corresponding *function* which will take an address and distance
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _demux, _demux_handler,
                     HPX_POINTER, HPX_INT);


static int _coalesce_data(hpx_addr_t addr, action_t *action,  uint32_t dest,
				      void *args, int n) {
   // Coalesce on demand as long as we have enough pseudo-parcels available.
  int count = sync_load(&action->coalesced_dest_buffer[dest].current_data_count,
			SYNC_RELAXED);
  size_t coalescing_size = COALESCING_SIZE;
  while (count >= coalescing_size) {
    //TODO:flush
    // Notify flush operations that we might be coalescing. This prevents a race
    // where a flusher thinks everything is gone, but we have partially
    // coalesced buffers to send.
    //_atomic_inc(&network->syncflush);

    // The cas updates the count for the next loop iteration if it fails,
    // otherwise we manually update it.
    // TODO:what will happen if one thread tries to put more data in the current fat parcel that exceed the coalescing size. For this I am assuming the following line would never happen.
    //int n = count - coalescing_size;
    // modified the sync_cas to store 0 rather than the diff of parcel_count
    if (sync_cas(&action->coalesced_dest_buffer[dest].current_data_count,
		 &count, 0, SYNC_RELAXED, SYNC_RELAXED)) {
      network_send(here->network, action->coalesced_dest_buffer[dest]->fatp);
      //_send_n(network, network->coalescing_size);
      //count = n;
    }

    // TODO:fix flush
    // Notify flush operations that we're not in their way anymore.
    //_atomic_dec(&network->syncflush);
  }

  // TODO: put the buffer in the fat parcel
  size_t idx = sync_addf(&action->coalesced_dest_buffer[dest].current_data_count,
			 1, SYNC_ACQ_REL);
  //TODO: assuming we only send distance and vertex address
  size_t current_idx = (idx - 1) * (sizeof(hpx_addr_t) + sizeof(uint64_t));

  uint64_t data = (uint64_t) *args;

  memcpy( (char *)&(action->coalesced_dest_buffer[dest]->fatp) + current_idx, addr,
	  sizeof(hpx_addr_t));
  memcpy( (char *)&(action->coalesced_dest_buffer[dest]->fatp) + current_idx +
	  sizeof(hpx_addr_t), data, sizeof(uint64_t));
  return LIBHPX_OK;
}

static int _coalesce_marshalled_data(hpx_addr_t addr, action_t *action,  uint64_t dest,
				      va_list *args, int n) {
  // the following 3 lines are copied from marshalled.c _new_marshalled function
  hpx_pid_t pid = hpx_thread_current_pid(); //TODO:pid needed?
  void *data = (n) ? va_arg(*args, void*) : NULL; //TODO: should we assume a certain datatype instead of void?
  int bytes = (n) ? va_arg(*args, int) : 0;

  return _coalesce_data(addr, action, dest, data, bytes); //TODO: bytes or n?
}

static int _coalesced_ffi_data(hpx_addr_t addr, action_t *action, uint64_t  dest,
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

  return _coalesce_data(addr, action, dest, buffer, n);
}


int coalesce_data(hpx_addr_t addr, hpx_action_t id,  uint64_t dest,
		  va_list *args, int n) {
  // 1)check in which action entry we should put in the current data-parcel
  CHECK_ACTION(id);
  const action_t *action = &actions[id];

  // 2) check which type of action we are dealing with, is it marshalled or ffi

  // if marshalled-type
  if (action->attr & HPX_MARSHALLED) {
    _coalesce_marshalled_data(addr, action, dest, args, n);
    return HPX_SUCCESS;
  } else { // assume ffi-type
    _coalesce_ffi_data(addr, action, dest, args, n);
    // TODO: what to put in the buffer
    return HPX_SUCCESS;
  }

}
