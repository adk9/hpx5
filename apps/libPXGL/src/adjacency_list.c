
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "adjacency_list.h"
#include "libpxgl.h"
#include "hpx/hpx.h"
#include "libsync/sync.h"


typedef uint32_t count_t;

hpx_addr_t count_array;
hpx_addr_t index_array;

uint32_t _count_array_block_size = 0;
uint32_t _index_array_block_size = 0;

static hpx_action_t _set_count_array_bsize;
static int _set_count_array_bsize_action(const uint32_t *arg) {
  _count_array_block_size = *arg;
  return HPX_SUCCESS;
}

static hpx_action_t _set_index_array_bsize;
static int _set_index_array_bsize_action(const uint32_t *arg) {
  _index_array_block_size = *arg;
  return HPX_SUCCESS;
}

// Action to increment count in the count array
static hpx_action_t _increment_count;
static int _increment_count_action(const void * restrict const arg) {
  const hpx_addr_t target = hpx_thread_current_target();

  volatile count_t * const count;
  if (!hpx_gas_try_pin(target, (void**)&count))
    return HPX_RESEND;

  sync_fadd(count, 1, SYNC_RELAXED);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


// Action to count an edge
static hpx_action_t _count_edge;
static int _count_edge_action(const hpx_addr_t * const count_array) {
  const hpx_addr_t target = hpx_thread_current_target();

  const edge_list_edge_t * const edge;
  if (!hpx_gas_try_pin(target, (void**)&edge))
    return HPX_RESEND;

  const hpx_addr_t count = hpx_addr_add(*count_array, edge->source * sizeof(count_t),
                                        _count_array_block_size);
  hpx_gas_unpin(target);

  return hpx_call_sync(count, _increment_count, NULL, 0, NULL, 0);
}


// Initialize vertex adjacency list size and distance = inf
static hpx_action_t _init_vertex;
static int _init_vertex_action(void *arg) {
   const hpx_addr_t target = hpx_thread_current_target();

   adj_list_vertex_t *vertex;
   if (!hpx_gas_try_pin(target, (void**)&vertex))
     return HPX_RESEND;

   vertex->num_edges = 0;
   vertex->distance = UINT64_MAX;

   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}


static hpx_action_t _alloc_vertex;
static int _alloc_vertex_action(const count_t *count) {
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *index;
  if (!hpx_gas_try_pin(target, (void**)&index))
    return HPX_RESEND;

  // We first pin the index and then allocate to get the blocks on the
  // same locality (at least initially)
  hpx_addr_t vertex = hpx_gas_alloc(sizeof(adj_list_vertex_t) + (*count * sizeof(adj_list_edge_t)));

  memcpy(index, &vertex, sizeof(vertex));
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, _init_vertex, NULL, 0, NULL, 0);
}


// Action to allocate an adjacency list entry: a vertex structure that
// holds the list of edges.
static hpx_action_t _alloc_adj_list_entry;
static int _alloc_adj_list_entry_action(const hpx_addr_t * const index)
{
  const hpx_addr_t target = hpx_thread_current_target();

  volatile count_t * const local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  count_t count = sync_load(local, SYNC_ACQUIRE);
  hpx_gas_unpin(target);

  return hpx_call_sync(*index, _alloc_vertex, &count, sizeof(count), NULL, 0);
}


static hpx_action_t _put_edge;
static int _put_edge_action(adj_list_edge_t *edge)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  // Increment the size of the vertex
  uint64_t num_edges = sync_fadd(&vertex->num_edges, 1, SYNC_RELAXED);

  memcpy(&vertex->edge_list[num_edges], edge, sizeof(*edge));

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static hpx_action_t _put_edge_index;
static int _put_edge_index_action(adj_list_edge_t *edge)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t vertex;
  hpx_addr_t *v;
  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, _put_edge, edge, sizeof(*edge), NULL, 0);
}


static hpx_action_t _insert_edge;
static int _insert_edge_action(const hpx_addr_t * const index_array)
{
  const hpx_addr_t target = hpx_thread_current_target();

  edge_list_edge_t *edge;
  if (!hpx_gas_try_pin(target, (void**)&edge))
    return HPX_RESEND;

  const uint64_t source = edge->source;

  adj_list_edge_t e;
  e.dest = edge->dest;
  e.weight = edge->weight;

  hpx_gas_unpin(target);

 // Then get the appropriate index array position to retrieve the
 // address of the vertex in the adjacency list
  const hpx_addr_t index
    = hpx_addr_add(*index_array, source * sizeof(hpx_addr_t), _index_array_block_size);

  // Insert the edge into the index array at the right index. Since we
  // call this action synchronously, we can safely send the stack
  // pointer out.
  return hpx_call_sync(index, _put_edge_index, &e, sizeof(e), NULL, 0);
}



hpx_action_t adj_list_from_edge_list = 0;
int adj_list_from_edge_list_action(const edge_list_t * const el) {

  // Allocate the count array for creating an edge histogram
  _count_array_block_size = ((el->num_vertices + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(count_t);
  hpx_addr_t sync = hpx_lco_future_new(0);
  hpx_bcast(_set_count_array_bsize, &_count_array_block_size, sizeof(uint32_t), sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  count_array = hpx_gas_global_calloc(HPX_LOCALITIES, _count_array_block_size);

  // Count the number of edges per source vertex
  hpx_addr_t edges = hpx_lco_and_new(el->num_edges);
  for (int i = 0; i < el->num_edges; ++i) {
    hpx_addr_t edge = hpx_addr_add(el->edge_list, i * sizeof(edge_list_edge_t), el->edge_list_bsize);
    hpx_call(edge, _count_edge, &count_array, sizeof(count_array), edges);
  }
  hpx_lco_wait(edges);
  hpx_lco_delete(edges, HPX_NULL);

  // Counting is finished here. Now allocate the index array.
  _index_array_block_size = ((el->num_vertices + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(hpx_addr_t);
  sync = hpx_lco_future_new(0);
  hpx_bcast(_set_index_array_bsize, &_index_array_block_size, sizeof(uint32_t), sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);

  index_array = hpx_gas_global_alloc(HPX_LOCALITIES, _index_array_block_size);

  // Allocate and populate the adjacency list.
  hpx_addr_t vertices = hpx_lco_and_new(el->num_vertices);
  for (int i = 0; i < el->num_vertices; ++i) {
    hpx_addr_t count = hpx_addr_add(count_array, i * sizeof(count_t), _count_array_block_size);
    hpx_addr_t index = hpx_addr_add(index_array, i * sizeof(hpx_addr_t), _index_array_block_size);
    hpx_call(count, _alloc_adj_list_entry, &index, sizeof(index), vertices);
  }
  hpx_lco_wait(vertices);
  hpx_lco_delete(vertices, HPX_NULL);

  // For each edge in the edge list, we add it as an adjacency to a
  // vertex
  edges = hpx_lco_and_new(el->num_edges);
  for (int i = 0; i < el->num_edges; ++i) {
    hpx_addr_t edge = hpx_addr_add(el->edge_list, i * sizeof(edge_list_edge_t), el->edge_list_bsize);
    hpx_call(edge, _insert_edge, &index_array, sizeof(index_array), edges);
  }
  hpx_lco_wait(edges);
  hpx_lco_delete(edges, HPX_NULL);

  HPX_THREAD_CONTINUE(index_array);
  return HPX_SUCCESS;
}


hpx_action_t free_adj_list;
int free_adj_list_action(void *arg) {
  hpx_gas_free(count_array, HPX_NULL);
  hpx_gas_free(index_array, HPX_NULL);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void _adj_list_register_actions() {
  adj_list_from_edge_list  = HPX_REGISTER_ACTION(adj_list_from_edge_list_action);
  free_adj_list            = HPX_REGISTER_ACTION(free_adj_list_action);
  _increment_count         = HPX_REGISTER_ACTION(_increment_count_action);
  _set_count_array_bsize   = HPX_REGISTER_ACTION(_set_count_array_bsize_action);
  _set_index_array_bsize   = HPX_REGISTER_ACTION(_set_index_array_bsize_action);
  _count_edge              = HPX_REGISTER_ACTION(_count_edge_action);
  _init_vertex             = HPX_REGISTER_ACTION(_init_vertex_action);
  _alloc_vertex            = HPX_REGISTER_ACTION(_alloc_vertex_action);
  _alloc_adj_list_entry    = HPX_REGISTER_ACTION(_alloc_adj_list_entry_action);
  _insert_edge             = HPX_REGISTER_ACTION(_insert_edge_action);
  _put_edge_index          = HPX_REGISTER_ACTION(_put_edge_index_action);
  _put_edge                = HPX_REGISTER_ACTION(_put_edge_action);
}
