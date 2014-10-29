#include "photon_event.h"

int __photon_handle_cq_event(photonRequest req, photon_rid id) {
  photon_rid cookie = id;
  uint32_t prefix;

  prefix = (uint32_t)(cookie>>32);
  if (prefix == REQUEST_COOK_EAGER) {
    return 1;
  }
  
  if ((cookie == req->id) && (req->type == EVQUEUE)) {
    req->state = REQUEST_COMPLETED;
    dbg_trace("set request completed with cookie: 0x%016lx", cookie);
  }
  // handle any other request completions we might get from the backend event queue
  else if (cookie != NULL_COOKIE) {
    photonRequest tmp_req;    
    if (htable_lookup(reqtable, cookie, (void**)&tmp_req) == 0) {
      if (tmp_req->type == EVQUEUE) {
	tmp_req->state = REQUEST_COMPLETED;
	dbg_trace("set request completed with cookie: 0x%016lx", cookie);
      }
      else
	tmp_req->flags &= REQUEST_FLAG_LDONE;
    }
    else if (htable_lookup(pwc_reqtable, cookie, (void**)&tmp_req) == 0) {
      if (tmp_req->type == EVQUEUE && (--tmp_req->num_entries) == 0) {
	tmp_req->state = REQUEST_COMPLETED;
	SAFE_SLIST_INSERT_HEAD(&pending_pwc_list, tmp_req, slist);
	htable_remove(pwc_reqtable, cookie, NULL);
	dbg_trace("set pwc request 0x%016lx completed with cookie: 0x%016lx", tmp_req->id, cookie);
      } 
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
  
  dbg_trace("(0x%016lx)",req->id);

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
    dbg_trace("Removing request 0x%016lx for remote buffer request 0x%016lx", req->id, req->remote_buffer.request);
    htable_remove(reqtable, (uint64_t)req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_trace("%d requests left in reqtable", htable_count(reqtable));
  } 
  
  dbg_trace("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
  return (req->state == REQUEST_COMPLETED)?0:1;

error_exit:
  return -1;
}

int __photon_handle_send_event(photonRequest req, photon_rid id) {
  photonRequest creq = NULL;
  photon_rid cookie;
  int i;

  dbg_trace("handling send completion with id: 0x%016lx", id);

  cookie = (uint32_t)((id<<32)>>32);

  if(req && (cookie == req->id)) {
    creq = req;
  }
  else if (cookie != NULL_COOKIE) {
    photonRequest tmp_req;
    if (htable_lookup(sr_reqtable, (uint64_t)cookie, (void**)&tmp_req) == 0) {
      creq = tmp_req;
    }
  }

  // we have a request to update
  if (creq) {
    if (creq->state == REQUEST_PENDING) {
      uint16_t msn;
      msn = (uint16_t)((id<<16)>>48);
      //creq->mmask |= ((uint64_t)1<<msn);
      bit_array_set(creq->mmask, msn);
      //if (!( creq->mmask ^ ~(~(uint64_t)0<<creq->num_entries))) {
      if (bit_array_num_bits_set(creq->mmask) == creq->num_entries) {
        // additional condition would be ACK from receiver
        creq->state = REQUEST_COMPLETED;
        // mark sendbuf entries as available again
        for (i=0; i<creq->num_entries; i++) {
          photon_msgbuffer_free_entry(sendbuf, creq->bentries[i]);
        }
      }
    }
    else {
      dbg_warn("not updating request that's not pending");
    }
  }
  else {
    dbg_err("could not find matching request for id: 0x%016lx", id);
    return PHOTON_ERROR;
  }

  return PHOTON_OK;
}

int __photon_handle_recv_event(photon_rid id) {
  photonRequest req;
  photon_ud_hdr *hdr;
  uint64_t cookie;
  uint32_t bindex;

  dbg_trace("handling recv completion with id: 0x%016lx", id);

  bindex = (uint32_t)((id<<32)>>32);
  
  // TODO: get backend gid...not really needed
  //dbg_trace("got msg from: %s to: %s",
  //	   inet_ntop(AF_INET6, recvbuf->entries[bindex].base+8, gid, 40),
  //	   inet_ntop(AF_INET6, recvbuf->entries[bindex].base+24, gid2, 40));
  
  hdr = (photon_ud_hdr*)recvbuf->entries[bindex].hptr;
  dbg_trace("r_request: %u", hdr->request);
  dbg_trace("src      : %u", hdr->src_addr);
  dbg_trace("length   : %u", hdr->length);
  dbg_trace("msn      : %d", hdr->msn);
  dbg_trace("nmsg     : %d", hdr->nmsg);

  /*
  if (hdr->src_addr == _photon_myrank) {
    // we somehow sent to our own rank, just reset mbuf entry and return
    __photon_backend->rdma_recv(NULL, (uintptr_t)recvbuf->entries[bindex].base,
                                recvbuf->p_size, &recvbuf->db->buf,
                                (( (uint64_t)REQUEST_COOK_RECV) << 32) | bindex);
    photon_msgbuffer_free_entry(recvbuf, bindex);
    return PHOTON_OK;
  }
  */

  // construct a cookie to query the htable with
  // |..~remote proc...|...remote r_id...|
  cookie = ((uint64_t)(~hdr->src_addr)<<32) | hdr->request;
  if (htable_lookup(sr_reqtable, cookie, (void**)&req) == 0) {
    // update existing req
    //req->mmask |= ((uint64_t)1<<hdr->msn);
    bit_array_set(req->mmask, hdr->msn);
    req->length += hdr->length;
    req->bentries[hdr->msn] = bindex;
  }
  else {
    // create a new request for the message
    photon_addr addr = {.global.proc_id = hdr->src_addr};
    req = __photon_setup_request_recv(&addr, hdr->msn, hdr->length, bindex, hdr->nmsg, cookie);
  }
  
  // now check if we have the whole message
  // if so, add to pending recv list
  if (req) {
    //if (!( req->mmask ^ ~(~(uint64_t)0<<req->num_entries))) {
    if (bit_array_num_bits_set(req->mmask) == req->num_entries) {
      dbg_trace("adding recv request to pending recv list: 0x%016lx/0x%016lx", req->id, cookie);
      SAFE_SLIST_INSERT_HEAD(&pending_recv_list, req, slist);
      req->state = REQUEST_COMPLETED;
      // send an ACK back to sender here
    }
  }
  else {
    dbg_err("request creation failed");
  }

  return PHOTON_OK;
}

int __photon_nbpop_sr(photonRequest req) {
  int rc;

  /*
  if (req) {
    dbg_trace("(0x%016lx)", req->id);
  }
  else {
    dbg_trace("(probing)");
  }
  */

  if (!req || (req && req->state == REQUEST_PENDING)) {

    uint32_t prefix;
    photon_event_status event;
    
    rc = __photon_backend->get_event(&event);
    if (rc < 0) {
      dbg_err("Error getting event, rc=%d", rc);
      goto error_exit;
    }
    else if (rc != PHOTON_OK) {
      return 1;
    }

    prefix = (uint32_t)(event.id>>32);
    if (prefix == REQUEST_COOK_RECV) {
      __photon_handle_recv_event(event.id);
    }
    else {
      __photon_handle_send_event(req, event.id);
    }
  }

  if (req && req->state == REQUEST_COMPLETED) {
    dbg_trace("Removing request 0x%016lx for send/recv", req->id);
    htable_remove(sr_reqtable, req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
    dbg_trace("%d requests left in sr_reqtable", htable_count(sr_reqtable));
  } 

  if (req) {
    dbg_trace("returning %d", (req->state == REQUEST_COMPLETED)?0:1 );
    return (req->state == REQUEST_COMPLETED)?0:1;
  }
  else {
    return 1;
  }

 error_exit:
  return -1;
}

///////////////////////////////////////////////////////////////////////////////
// returns
// -1 if an error occured.
//	0 if the FIN associated with "req" was found and poped, or
//		the "req" is not pending.	 This is not an error, if a previous
//		call to __photon_nbpop_ledger() popped the FIN that corresponds to "req".
//	1 if the request is pending and the FIN has not arrived yet
int __photon_nbpop_ledger(photonRequest req) {
  int curr, i=-1;

  dbg_trace("(0x%016lx)", req->id);

  if(req->state == REQUEST_PENDING) {
    
    // clear any completed tasks from the event queue
    while (__photon_nbpop_event(req) != 2)
      ;

    // Check if an entry of the FIN LEDGER was written with "id" equal to to "req"
    for(i = 0; i < _photon_nproc; i++) {
      photonLedgerEntry curr_entry;
      curr = photon_processes[i].local_fin_ledger->curr;
      curr_entry = &(photon_processes[i].local_fin_ledger->entries[curr]);
      if (curr_entry->request != (uint64_t) 0) {
        dbg_trace("Found curr: %d, req: 0x%016lx while looking for req: 0x%016lx",
                 curr, curr_entry->request, req->id);

        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        // handle requests we are not looking for
        else {
          photonRequest tmp_req;
          if (htable_lookup(reqtable, curr_entry->request, (void**)&tmp_req) == 0) {
            dbg_trace("setting request completed with id: 0x%016lx", curr_entry->request);
            tmp_req->state = REQUEST_COMPLETED;
          }
        }
        // reset entry
        curr_entry->request = 0;
        NEXT_LEDGER_ENTRY(photon_processes[i].local_fin_ledger);
      }
    } 
  }
  
  if (req->state == REQUEST_COMPLETED) {
    dbg_trace("removing RDMA req: 0x%016lx", req->id);
    htable_remove(reqtable, req->id, NULL);
    SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
  }
  
  if ((req->state != REQUEST_COMPLETED) && (req->state != REQUEST_PENDING)) {
    dbg_trace("req->state != (PENDING | COMPLETE), returning 0");
    return 0;
  }
  
  dbg_trace("at end, returning %d", (req->state == REQUEST_COMPLETED)?0:1);
  return (req->state == REQUEST_COMPLETED)?0:1;
}

int __photon_wait_ledger(photonRequest req) {
  void *test;
  int curr, num_entries, i=-1;

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
      curr = photon_processes[i].local_fin_ledger->curr;
      curr_entry = &(photon_processes[i].local_fin_ledger->entries[curr]);
      if (curr_entry->request != (uint64_t) 0) {
        dbg_trace("Found: %d/0x%016lx/0x%016lx", curr, curr_entry->request, req->id);

        if (curr_entry->request == req->id) {
          req->state = REQUEST_COMPLETED;
        }
        else {
          photonRequest tmp_req;
          
          if (htable_lookup(reqtable, (uint64_t)curr_entry->request, &test) == 0) {
            tmp_req = test;
            tmp_req->state = REQUEST_COMPLETED;
          }
        }
        
        curr_entry->request = 0;
        num_entries = photon_processes[i].local_fin_ledger->num_entries;
        curr = photon_processes[i].local_fin_ledger->curr;
        curr = (curr + 1) % num_entries;
        photon_processes[i].local_fin_ledger->curr = curr;
      }
    }
  }
  dbg_trace("removing RDMA: %u/0x%016lx", i, req->id);
  htable_remove(reqtable, (uint64_t)req->id, NULL);
  SAFE_LIST_INSERT_HEAD(&free_reqs_list, req, list);
  dbg_trace("%d requests left in reqtable", htable_count(reqtable));

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
    if (cookie == req->id) {
      req->state = REQUEST_COMPLETED;
      dbg_trace("setting request complete with cookie: 0x%016lx", cookie);
    }
  }

  return (req->state == REQUEST_COMPLETED)?0:-1;

error_exit:
  return -1;
}
