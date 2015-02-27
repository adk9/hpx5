// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// OpenFlow Controller API.
///
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

#include "hpx/hpx.h"
#include "libsync/locks.h"
#include "libsync/hashtables.h"
#include "libhpx/debug.h"
#include "libhpx/scheduler.h"
#include "libhpx/routing.h"
#include "managers.h"

#include <jansson.h>
#include <curl/curl.h>

// Hack. FIXME.
#if HAVE_PHOTON
  #include "cutter_ids.h"
#endif

typedef struct floodlight floodlight_t;
struct floodlight {
  routing_t          vtable;
  switch_t          *myswitch;
  switch_t          *switches;     // List of available switches
  CURL              *curl;         // CURL context
  struct curl_slist *useragent;    // HTTP user-agent
};

#define CTRL_ADDR  "http://192.168.1.100:8080"

#define SWITCH_URL "/wm/core/controller/switches/json"
#define   PUSH_URL "/wm/staticflowentrypusher/json"
#define   LIST_URL "/wm/staticflowentrypusher/list/%s/json"
#define  CLEAR_URL "/wm/staticflowentrypusher/clear/%s/json"

static char *caddr = CTRL_ADDR;

struct _json_buf_t {
  char     *data;
  uint64_t  pos;
  uint64_t  size;
};

static size_t _write_buf(void *ptr, size_t size, size_t nmemb, void *userp) {
  struct _json_buf_t *buf = (struct _json_buf_t *)userp;
  long bufsz = size * nmemb;
  if(buf->pos + bufsz >= buf->size - 1)
    return dbg_error("floodlight: buffer too small for json write request.\n");
  memcpy(buf->data + buf->pos, ptr, bufsz);
  buf->pos += bufsz;
  return bufsz;
}

static size_t _read_buf(void *ptr, size_t size, size_t nmemb, void *userp) {
  struct _json_buf_t *buf = (struct _json_buf_t *)userp;
  long bufsz = size * nmemb;
  if(bufsz < 1)
    return 0;

  if (buf->pos < buf->size) {
    *(char *)ptr = buf->data[0];
    buf->data++;
    buf->pos++;
    return 1;
  }
  return 0;
}


/// Send a GET request to the Floodlight controller.
static int _get_request(const floodlight_t *fl, const char *url, uint64_t size, json_t **json) {

  char *data = malloc(sizeof(char) * size);
  struct _json_buf_t buf = { .data = data, .pos = 0, .size = size };

  curl_easy_setopt(fl->curl, CURLOPT_URL, url);
  curl_easy_setopt(fl->curl, CURLOPT_HTTPHEADER, fl->useragent);
  curl_easy_setopt(fl->curl, CURLOPT_WRITEFUNCTION, _write_buf);
  curl_easy_setopt(fl->curl, CURLOPT_WRITEDATA, &buf);

  CURLcode status = curl_easy_perform(fl->curl);
  if (status != 0) {
    free(data);
    return dbg_error("floodlight: failed GET request to %s: %s.\n", url, curl_easy_strerror(status));
  }

  long code;
  curl_easy_getinfo(fl->curl, CURLINFO_RESPONSE_CODE, &code);
  if (code != 200) {
    free(data);
    return dbg_error("floodlight: invalid response %ld.\n", code);
  }

  data[buf.pos] = '\0';

  json_error_t error;
  *json = json_loads(data, 0, &error);
  free(data);

  if (!*json) {
    return dbg_error("floodlight: could not load json data: %s\n", error.text);
  }

  return HPX_SUCCESS;
}

/// Send a POST request and get a response from the Floodlight controller.
static int _post_request(const floodlight_t *fl, const char *url, uint64_t size, json_t *post,
                         json_t **response, int delete) {

  char *data = malloc(sizeof(char) * size);
  struct _json_buf_t wbuf = { .data = data, .pos = 0, .size = size };

  char *postdata = json_dumps(post, JSON_COMPACT);
  int postsize = strlen(postdata)+1;
  struct _json_buf_t rbuf = { .data = postdata, .pos = 0, .size = postsize };

  if (delete)
    curl_easy_setopt(fl->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  else
    curl_easy_setopt(fl->curl, CURLOPT_POST, 1L);

  curl_easy_setopt(fl->curl, CURLOPT_URL, url);
  curl_easy_setopt(fl->curl, CURLOPT_HTTPHEADER, fl->useragent);
  curl_easy_setopt(fl->curl, CURLOPT_READFUNCTION, _read_buf);
  curl_easy_setopt(fl->curl, CURLOPT_READDATA, &rbuf);
  curl_easy_setopt(fl->curl, CURLOPT_POSTFIELDSIZE, postsize);
  curl_easy_setopt(fl->curl, CURLOPT_WRITEFUNCTION, _write_buf);
  curl_easy_setopt(fl->curl, CURLOPT_WRITEDATA, &wbuf);

  CURLcode status = curl_easy_perform(fl->curl);
  if (status != 0) {
    free(data);
    return dbg_error("floodlight: failed POST request to %s: %s.\n", url, curl_easy_strerror(status));
  }

  long code;
  curl_easy_getinfo(fl->curl, CURLINFO_RESPONSE_CODE, &code);
  if (code != 200) {
    free(data);
    return dbg_error("floodlight: invalid response %ld.\n", code);
  }
  data[wbuf.pos] = '\0';

  if (wbuf.pos > 0) {
    json_error_t error;
    *response = json_loads(data, 0, &error);
    if (*response == NULL)
      dbg_error("floodlight: unable to parse json response.\n");
  }
  free(data);
  return HPX_SUCCESS;
}

static int _get_json_int(json_t *object, const char *key) {
  json_t *obj = json_object_get(object, key);
  if (!json_is_number(obj))
    return dbg_error("floodlight: failed to get json integer value.\n");
  return json_integer_value(obj);
}

static const char *_get_json_string(json_t *object, const char *key) {
  json_t *obj = json_object_get(object, key);
  if (!json_is_string(obj)) {
    dbg_error("floodlight: failed to get json string value.\n");
    return NULL;
  }
  return json_string_value(obj);
}

static int _hwaddr_to_str(uint64_t addr, char *str, size_t size) {
  snprintf(str, size, "%02x:%02x:%02x:%02x:%02x:%02x",
           (uint8_t)(addr >> 40), (uint8_t)(addr >> 32), (uint8_t)(addr >> 24),
           (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr);
  return HPX_SUCCESS;
}

static json_t *_create_flow_mod(switch_t *s, uint64_t src, uint64_t dst, uint16_t port) {
  json_t *obj = json_object();

  if (!obj) {
    dbg_error("floodlight: could not create new json object");
    return NULL;
  }

  json_object_set_new(obj, "switch", json_string((char*)s->dpid));
  char *name;
  asprintf(&name, "hpx-%d-%lu-%lu-%u", hpx_get_my_rank(), src, dst, port);
  json_object_set_new(obj, "name", json_string(name));

  if (src != HPX_SWADDR_WILDCARD) {
    char srcmac[18];
    _hwaddr_to_str(src, srcmac, 18);
    json_object_set_new(obj, "src-mac", json_string(srcmac));
  }

  if (dst != HPX_SWADDR_WILDCARD) {
    char dstmac[18];
    _hwaddr_to_str(dst, dstmac, 18);
    json_object_set_new(obj, "dst-mac", json_string(dstmac));
  }

  //int myport = 0;
  //json_object_set_new(obj, "ingress-port", json_integer(myport));
  int vlanid = get_bravo_vlanid();
  json_object_set_new(obj, "vlan-id", json_integer(vlanid));
  json_object_set_new(obj, "active", json_string("true"));

  char *actions;
  asprintf(&actions, "output=%d", port); // this should be an array of
                                         // key/value pairs.
  json_object_set_new(obj, "actions", json_string(actions));

  free(name);
  free(actions);
  return obj;
}


// Add switch information
static switch_t *_add_switch(json_t *json) {

  switch_t *s     = malloc(sizeof(*s));
  s->dpid         = (void*)strdup(_get_json_string(json, "dpid"));
  s->nbuffers     = _get_json_int(json, "buffers");
  s->ntables      = _get_json_int(json, "tables");
  s->capabilities = _get_json_int(json, "capabilities");
  s->actions      = _get_json_int(json, "actions");
  s->active       = true;

  // todo.
  s->ports = NULL;
  return s;
}

static int _get_switches(const floodlight_t *fl) {
  char url[512];
  json_t *response;
  switch_t *sw;

  snprintf(url, 512, "%s" SWITCH_URL, caddr);
  dbg_error("floodlight: url=%s\n", url);
  _get_request(fl, url, 4096, &response);

  if (!json_is_array(response)) {
    dbg_error("floodlight: invalid json response.\n");
    json_decref(response);
  }

  json_t *s;
  for(int i = 0; i < json_array_size(response); i++) {
    s = json_array_get(response, i);
    if(!json_is_object(s)) {
      dbg_error("floodlight: invalid json response.\n");
      json_decref(response);
    }
    sw = _add_switch(s);
    if (sw) {
      // cast because fl is const
      // LD: why is it const if it's not actually const?
      sw->next = ((floodlight_t*)fl)->switches;
      ((floodlight_t*)fl)->switches = sw;
      log_net("floodlight: new switch %s added.\n", (char*)sw->dpid);
    }
  }
  json_decref(response);
  return HPX_SUCCESS;
}

static void _disable_switch(uint64_t dpid) {
  // todo.
}

static void _init(floodlight_t *fl) {

  curl_global_init(CURL_GLOBAL_ALL);
  fl->curl = curl_easy_init();
  if (!fl->curl) {
    dbg_error("floodlight: failed to initialize curl for rest requests.\n");
    return;
  }

  fl->useragent = curl_slist_append(NULL, "User-Agent: HPX5");

  _get_switches(fl);
  // FIXME:
  fl->myswitch = fl->switches;

  init_bravo_ids(0);
  // The default rules for the switch would be added here.
  return;
}

static void _delete(routing_t *routing) {
  floodlight_t *fl = (floodlight_t*)routing;
  switch_t *s = NULL, *tmp = NULL;
  // delete all switches
  while ((s = fl->switches) != NULL) {
    fl->switches = fl->switches->next;
    free(s->dpid);
    free(s);
  }

  curl_slist_free_all(fl->useragent);
  curl_easy_cleanup(fl->curl);
  curl_global_cleanup();
  free(routing);
}

static int _add_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  const floodlight_t *fl = (const floodlight_t*)r;
  switch_t *s = fl->myswitch;
  assert(s);

  json_t *flow = _create_flow_mod(s, src, dst, port);
  assert(flow);

  dbg_error("floodlight: flow entry: %s.\n", json_dumps(flow, JSON_INDENT(2)));

  char url[512];
  snprintf(url, 512, "%s" PUSH_URL, caddr);
  json_t *response;
  _post_request(fl, url, 2048, flow, &response, 0);
  // do something with the response here.
  // should we retry if the _post_request fails.
  json_decref(response);
  return HPX_SUCCESS;
}

static int _delete_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  const floodlight_t *fl = (const floodlight_t*)r;
  switch_t *s = fl->myswitch;
  assert(s);

  json_t *flow = _create_flow_mod(s, src, dst, port);
  assert(flow);

  dbg_error("floodlight: flow entry: %s.\n", json_dumps(flow, JSON_INDENT(2)));

  char url[256];
  snprintf(url, 512, "%s" PUSH_URL, caddr);
  json_t *response;
  _post_request(fl, url, 2048, flow, &response, 1);
  // do something with the response here.
  // should we retry if the _post_request fails.
  json_decref(response);
  return HPX_SUCCESS;
}

static int _update_flow(const routing_t *r, uint64_t src, uint64_t dst, uint16_t port) {
  return _add_flow(r, src, dst, port);
}

static int _my_port(const routing_t *r) {
  photon_addr addr;
  photon_get_dev_addr(AF_INET, &addr);
  bravo_node *b = find_bravo_node(&addr);
  assert(b);
  return b->of_port;
}

static int _register_addr(const routing_t *r, uint64_t addr) {
  photon_addr daddr = {.blkaddr.blk3 = addr};
  photon_register_addr(&daddr, AF_INET6);
  return HPX_SUCCESS;
}

static int _unregister_addr(const routing_t *r, uint64_t addr) {
  photon_addr daddr = {.blkaddr.blk3 = addr};
  photon_unregister_addr(&daddr, AF_INET6);
  return HPX_SUCCESS;
}

routing_t *routing_new_floodlight(void) {
  floodlight_t *floodlight = malloc(sizeof(*floodlight));
  floodlight->vtable.delete          = _delete;
  floodlight->vtable.add_flow        = _add_flow;
  floodlight->vtable.delete_flow     = _delete_flow;
  floodlight->vtable.update_flow     = _update_flow;
  floodlight->vtable.my_port         = _my_port;
  floodlight->vtable.register_addr   = _register_addr;
  floodlight->vtable.unregister_addr = _unregister_addr;

  floodlight->myswitch = NULL;
  floodlight->switches = NULL;

  _init(floodlight);

  return &floodlight->vtable;
}
