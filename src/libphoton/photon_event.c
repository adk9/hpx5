#include <assert.h>

#include "photon_event.h"
#include "photon_pwc.h"

int __photon_handle_cq_special(photon_rid cookie) {
  uint32_t prefix;
  prefix = (uint32_t)(cookie>>32);

  switch (prefix) {
  case REQUEST_COOK_EAGER:
    break;
  case REQUEST_COOK_EBUF:
    {
      int proc = (int)(cookie<<32>>32);
      assert(IS_VALID_PROC(proc));
      sync_store(&photon_processes[proc].remote_eager_buf->acct.rloc, 0, SYNC_RELEASE);
    }
    break;
  case REQUEST_COOK_PBUF:
    {
      int proc = (int)(cookie<<32>>32);
      assert(IS_VALID_PROC(proc));
      sync_store(&photon_processes[proc].remote_pwc_buf->acct.rloc, 0, SYNC_RELEASE);
    }
    break;
  case REQUEST_COOK_FIN:
    {
      int proc = (int)(cookie<<32>>32);
      assert(IS_VALID_PROC(proc));
      sync_store(&photon_processes[proc].remote_fin_ledger->acct.rloc, 0, SYNC_RELEASE);
    }
    break;
  case REQUEST_COOK_RINFO:
    break;
  case REQUEST_COOK_SINFO:
    break;
  case REQUEST_COOK_ELEDG:
    {
      int proc = (int)(cookie<<32>>32);
      assert(IS_VALID_PROC(proc));
      sync_store(&photon_processes[proc].remote_eager_ledger->acct.rloc, 0, SYNC_RELEASE);
    }
    break;
  case REQUEST_COOK_PLEDG:
    {
      int proc = (int)(cookie<<32>>32);
      assert(IS_VALID_PROC(proc));
      sync_store(&photon_processes[proc].remote_pwc_ledger->acct.rloc, 0, SYNC_RELEASE);
    }
    break;
  default:
    return PHOTON_ERROR;
    break;
  }

  return PHOTON_OK;
}

int __photon_handle_cq_event(photonRequest req, photon_rid cookie) {
  int rc = __photon_handle_cq_special(cookie);
  if (rc == PHOTON_OK) {
    return 1;
  }
  
  if ((cookie == req->id) && (req->type == EVQUEUE)) {
    req->state = REQUEST_COMPLETED;
    dbg_trace("Set request completed with rid: 0x%016lx", cookie);
  }
  // handle any other request completions we might get from the backend event queue
  else if (cookie != NULL_COOKIE) {
    photonRequest treq;
    treq = photon_lookup_request(cookie);
    if (treq) {
      if (treq->type == EVQUEUE && (--treq->events) == 0) {
	treq->state = REQUEST_COMPLETED;
	if (treq->op == REQUEST_OP_PWC) {
	  photon_pwc_add_req(treq);
	  dbg_trace("Enqueuing PWC local completion");
	}
	dbg_trace("Set request completed, rid: 0x%016lx", cookie);
      }
      else {
	// this was an RDMA event associated with a ledger
	// mark the request as having the local completion popped
	treq->flags |= REQUEST_FLAG_LDONE;
	dbg_trace("Set local completion done flag for ledger rid: 0x%016lx", cookie);
      }
    }
    else {
      dbg_trace("Got an event that we did not expect: 0x%016lx", cookie);
    }
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// __photon_nbpop_event() is non blocking and returns:
// -1 if an error occured.
//	0 if the request (req) specified in the argument has completed.
//	1 if either no event was in the queue, or there was an event but not for the specified request (req).
int __photon_nbpop_event(photonRequest req) {
  int rc;
  
  dbg_trace("(%d/0x%016lx)", req->proc, req->id);

  if (req->state == REQUEST_PENDING) {
    photon_rid cookie;
    photon_event_status event;

    if (req->flags & REQUEST_FLAG_EDONE)
      req->state = REQUEST_COMPLETED;

    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event");
      goto error_exit;
    }
    else if (rc != PHOTON_OK) {
      return 2; // nothing in the EVQ
    }
    
    cookie = event.id;
    
    dbg_trace("(req type=%d) got completion for: 0x%016lx", req->type, cookie);
    
    rc = __photon_handle_cq_event(req, cookie);
    if (rc) return rc;
  }

  // clean up a completed request if the FIN has already been sent
  // otherwise, we clean up in send_FIN()
  if ((req->state == REQUEST_COMPLETED) && (req->flags & REQUEST_FLAG_FIN)) {
    dbg_trace("Marking request 0x%016lx (remote=0x%016lx) as completed", req->id, req->remote_buffer.request);
    photon_free_request(req);
    dbg_trace("%d requests left in %d's reqtable", photon_count_request(req->proc), req->proc);
  } 
  
  dbg_trace("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
  return (req->state == REQUEST_COMPLETED)?0:1;

error_exit:
  return -1;
}

int __photon_nbpop_sr(photonRequest req) {

  return PHOTON_OK;
}

///////////////////////////////////////////////////////////////////////////////
// returns
// -1 if an error occured.
//	0 if the FIN associated with "req" was found and poped, or
//		the "req" is not pending.	 This is not an error, if a previous
//		call to __photon_nbpop_ledger() popped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
int __photon_nbpop_ledger(photonRequest req) {
  uint64_t curr;
  int c_ind, i=-1;

  dbg_trace("(0x%016lx)", req->id);

  if(req->state == REQUEST_PENDING) {
    
    // clear any completed tasks from the event queue
    while (__photon_nbpop_event(req) != 2)
      ;

    // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
    for(i = 0; i < _photon_nproc; i++) {
      photonLedgerEntry curr_entry;
      curr = sync_load(&photon_processes[i].local_fin_ledger->curr, SYNC_RELAXED);
      c_ind = curr & (photon_processes[i].local_fin_ledger->num_entries - 1);
      curr_entry = &(photon_processes[i].local_fin_ledger->entries[c_ind]);
      if ((curr_entry->request != (uint64_t) 0) &&
	  sync_cas(&photon_processes[i].local_fin_ledger->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED)) {
        dbg_trace("Found curr: %d, req: 0x%016lx while looking for req: 0x%016lx",
		  c_ind, curr_entry->request, req->id);
	
        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        // handle requests we are not looking for
        else {
          photonRequest treq;
	  if ((treq = photon_lookup_request(curr_entry->request)) != NULL) {
	    dbg_trace("Setting request completed, rid: 0x%016lx", curr_entry->request);
	    treq->state = REQUEST_COMPLETED;
	  }
	}
	// reset entry
        curr_entry->request = 0;
      }
    }
  }
  
  if (req->state == REQUEST_COMPLETED) {
    dbg_trace("removing RDMA req: 0x%016lx", req->id);
    photon_free_request(req);
  }
  
  if ((req->state != REQUEST_COMPLETED) && (req->state != REQUEST_PENDING)) {
    dbg_trace("req->state != (PENDING | COMPLETE), returning 0");
    return 0;
  }
  
  dbg_trace("at end, returning %d", (req->state == REQUEST_COMPLETED)?0:1);
  return (req->state == REQUEST_COMPLETED)?0:1;
}

int __photon_wait_ledger(photonRequest req) {
  uint64_t curr;
  int c_ind, i=-1;

  dbg_trace("(0x%016lx)",req->id);

#ifdef CALLTRACE
  for(i = 0; i < _photon_nproc; i++) {
    photonLedgerEntry curr_entry;
    curr = photon_processes[i].local_fin_ledger->curr;
    curr_entry = &(photon_processes[i].local_fin_ledger->entries[curr]);
    dbg_trace("curr_entry(proc==%d)=%p", i ,curr_entry);
  }
#endif
  while (req->state == REQUEST_PENDING) {

    // clear any completed tasks from the event queue
    while (__photon_nbpop_event(req) != 2)
      ;
    
    // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
    for(i = 0; i < _photon_nproc; i++) {
      photonLedgerEntry curr_entry;
      curr = sync_load(&photon_processes[i].local_fin_ledger->curr, SYNC_RELAXED);
      c_ind = curr & (photon_processes[i].local_fin_ledger->num_entries - 1);
      curr_entry = &(photon_processes[i].local_fin_ledger->entries[c_ind]);
      if ((curr_entry->request != (uint64_t) 0) &&
	  sync_cas(&photon_processes[i].local_fin_ledger->curr, curr, curr+1, SYNC_RELAXED, SYNC_RELAXED)) {
        dbg_trace("Found: %d/0x%016lx/0x%016lx", c_ind, curr_entry->request, req->id);
	
        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        else {
	  photonRequest treq;
          if ((treq = photon_lookup_request(curr_entry->request)) != NULL) {
	    treq->state = REQUEST_COMPLETED;
	  }
	}
        curr_entry->request = 0;
      }
    }
  }
  dbg_trace("Removing RDMA: %u/0x%016lx", i, req->id);
  photon_free_request(req);
  dbg_trace("%d requests left in %d's reqtable", photon_count_request(req->proc), req->proc);

  return (req->state == REQUEST_COMPLETED)?0:-1;
}

int __photon_wait_event(photonRequest req) {
  int rc;
  
  while (req->state == REQUEST_PENDING) {
    photon_rid cookie;
    photon_event_status event;

    rc = __photon_backend->get_event(&event);
    if (rc != PHOTON_OK) {
      dbg_err("Could not get event");
      goto error_exit;
    }
    
    cookie = event.id;
    rc = __photon_handle_cq_event(req, cookie);
    if (rc) return rc;
  }

  return (req->state == REQUEST_COMPLETED)?0:-1;

error_exit:
  return -1;
}
