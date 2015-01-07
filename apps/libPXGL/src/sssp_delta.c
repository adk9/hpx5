#include "hpx/hpx.h"
#include "libpxgl/sssp_common.h"
#include "libpxgl/termination.h"
#include "libpxgl/sssp_delta.h"
#include "libpxgl/sssp_dc.h"
#include <stdlib.h>
#include <stdio.h>

static const size_t size_t_max = (size_t) -1;

typedef struct {
  hpx_addr_t vertex;
  distance_t distance;
} buffer_node_t;

typedef struct {
  size_t current_size;
  size_t current_position;
  double factor;
  buffer_node_t* buffer;
} buffer_t;

typedef struct {
  buffer_t ***buckets;
  size_t current_level;
  size_t *num_buckets;
  size_t *num_vertices;
  size_t delta;
} buckets_t;

static buckets_t *buckets;

static distance_t _get_level(const distance_t distance) {
  return distance / buckets->delta;
}

static buffer_t* init_buffer(const size_t init_size, const double factor) {
  // printf("Allocating a buffer.\n");
  buffer_t *buffer = malloc(sizeof(buffer_t));
  buffer->buffer = malloc(sizeof(buffer_node_t) * init_size);
  buffer->current_size = init_size;
  buffer->current_position = 0;
  buffer->factor = factor;
  return buffer;
}

static void append(buffer_t * const buffer, const hpx_addr_t vertex, const distance_t distance) {
  // printf("Appending %zu to buffer at %zu\n", vertex, buffer);
  assert(buffer->current_position <= buffer->current_size);
  if(buffer->current_position == buffer->current_size) {
    buffer->current_size = buffer->current_size * buffer->factor;
    buffer->buffer = realloc(buffer->buffer, buffer->current_size * sizeof(buffer_node_t));
  }
  assert(buffer->current_position < buffer->current_size);
  const buffer_node_t node = { .vertex = vertex, .distance = distance };
  // printf("Appending at postion %zu\n", buffer->current_position);
  buffer->buffer[buffer->current_position++] = node;
}

static void _insert_buckets(const int thread_no, const size_t level, const hpx_addr_t vertex, const distance_t distance) {
  assert(thread_no == HPX_THREAD_ID);
  //  assert(buckets->num_buckets[thread_no] > 0);
  // printf("_insert_buckets, thread_no: %d, level: %zu, distance: %zu\n", thread_no, level, distance);
  if(buckets->num_buckets[thread_no] <= level) {
    // printf("insert_buckets reallocating at thread %d from %zu to %zu, level %zu\n", thread_no, buckets->num_buckets[thread_no], 2 * level, level);
    buckets->buckets[thread_no] = realloc(buckets->buckets[thread_no], 2 * level * sizeof(buffer_t*));
    assert(buckets->buckets[thread_no]);
    for(size_t i = buckets->num_buckets[thread_no]; i < 2 * level; ++i) {
      buckets->buckets[thread_no][i] = NULL;
    }
    buckets->num_buckets[thread_no] = 2 * level;
  }
  if(buckets->buckets[thread_no][level] == NULL) buckets->buckets[thread_no][level] = init_buffer(1024, 2);
  // printf("insert_buckets [%d][%zu] with %zu\n", thread_no, level, vertex);
  append(buckets->buckets[thread_no][level], vertex, distance);
  ++buckets->num_vertices[thread_no];
}

int _delta_sssp_send_vertex(const hpx_addr_t vertex, const hpx_action_t action, const _sssp_visit_vertex_args_t * const args, const size_t len, const hpx_addr_t result) {
  // printf("delta_send_vertex, vertex: %zu, distance: %" SSSP_UINT_PRI "\n", vertex, args->distance);
  if(_get_level(args->distance) > buckets->current_level) {
    _insert_buckets(HPX_THREAD_ID, _get_level(args->distance), vertex, args->distance);
    if (_get_termination() == COUNT_TERMINATION) _increment_finished_count();
    if (_get_termination() == AND_LCO_TERMINATION) hpx_lco_set(result, 0, NULL, HPX_NULL, HPX_NULL);
  } else {
    hpx_call(vertex, _sssp_visit_vertex, args, sizeof(*args), HPX_NULL);
    // printf("after delta_sssp_send vertex: %zu, distance: %" SSSP_UINT_PRI "\n", vertex, args->distance);
  }
  return HPX_SUCCESS;
}

static buffer_node_t pop(buffer_t * const buffer) {
  assert(buffer->current_position > 0);
  // printf("Popping at position %zu\n", buffer->current_position);
  return buffer->buffer[--buffer->current_position];
}

static bool empty(const buffer_t * const buffer) {
  return buffer->current_position == 0;
}

static size_t size(const buffer_t * const buffer) {
  return buffer->current_position;
}

static void delete_buffer(buffer_t * const buffer) {
  free(buffer->buffer);
  free(buffer);
  return;
}

hpx_action_t _init_buckets = 0;
static int _init_buckets_action(const size_t * const delta) {
  buckets = malloc(sizeof(buckets_t));
  buckets->delta = *delta;
  // buckets->current_level = malloc(sizeof(size_t) * HPX_THREADS);
  buckets->num_buckets = malloc(sizeof(size_t) * HPX_THREADS);
  buckets->num_vertices = malloc(sizeof(size_t) * HPX_THREADS);
  buckets->buckets = malloc(HPX_THREADS * sizeof(buffer_t**));
  for(size_t i = 0; i < HPX_THREADS; ++i) {
    buckets->buckets[i] = malloc(sizeof(buffer_t*));
    buckets->buckets[i][0] = NULL;
    buckets->current_level = 0;
    buckets->num_buckets[i] = 1;
    buckets->num_vertices[i] = 0;
  }
  // printf("Initialized buckets.\n");
  return HPX_SUCCESS;
}

hpx_action_t _delete_buckets = 0;
static int _delete_buckets_action(const hpx_addr_t const * termination_lco) {
  buckets_t *temp_buckets = buckets;
  buckets = NULL;
  hpx_lco_set(*termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
  for(size_t i = 0; i < HPX_THREADS; ++i) {
    free(temp_buckets->buckets[i]);
  }
  //  free(temp_buckets->current_level);
  free(temp_buckets->num_buckets);
  free(temp_buckets->buckets);
  free(temp_buckets->num_vertices);
  free(temp_buckets);
  // printf("Buckets initialized.\n");
  return HPX_SUCCESS;
}

typedef struct {
  buffer_t *buffer;
  hpx_addr_t graph;
} _visit_all_in_buffer_args_t;
hpx_action_t _visit_all_in_buffer = 0;
static int _visit_all_in_buffer_action(const _visit_all_in_buffer_args_t * const args) {
  // We rely on a guaranteed affinity rather than just a suggestion
  //  hpx_thread_set_affinity(args->thread_id);
  //  assert(args->thread_id == HPX_THREAD_ID);
  // buffer_t * const buffer = buckets->buckets[args->thread_id][buckets->current_level];
  while(!empty(args->buffer)) {
    // printf("popping in visit_all. buffer: %zu, bucket [%d][%zu]\n.", args->buffer, HPX_THREAD_ID, buckets->current_level);
    buffer_node_t node = pop(args->buffer);
    const _sssp_visit_vertex_args_t sssp_args = { .graph = args->graph, .distance = node.distance };
    // printf("Calling visit_vertex with vertex %zu and distance %" SSSP_UINT_PRI " (node.distance is: %zu in zu, and %" SSSP_UINT_PRI " in sssp_uint_pri\n", node.vertex, sssp_args.distance, node.distance, node.distance);
    hpx_call(node.vertex, _sssp_visit_vertex, &sssp_args, sizeof(sssp_args), HPX_NULL); 
  }
  delete_buffer(args->buffer);
  //  buckets->buckets[args->thread_id][buckets->current_level] = NULL;
  return HPX_SUCCESS;
}

hpx_action_t _visit_all_in_current_level = 0;
static int _visit_all_in_current_level_action(const hpx_addr_t * const graph) {
  for(size_t i = 0; i < HPX_THREADS; ++i) {
    if(buckets->num_buckets[i] > buckets->current_level && buckets->buckets[i][buckets->current_level] != NULL) {
      const _visit_all_in_buffer_args_t visit_args = { .buffer = buckets->buckets[i][buckets->current_level], .graph = *graph };
      hpx_call(HPX_HERE, _visit_all_in_buffer, &visit_args, sizeof(visit_args), HPX_NULL);
    }
  }
  return HPX_SUCCESS;
}

static void _size_t_minimum_op(size_t * const output, const size_t * const input, const size_t bytes) {
  // printf("minimum_op(%zu, %zu)\n", *output, *input);
  if(*output > *input) *output = *input;
  return;
}

static void _size_t_minimum_init(size_t * const output, const size_t bytes) {
  *output = size_t_max;
  return;
}

hpx_action_t _delta_sssp_increase_active_counts = 0;
static int _delta_sssp_increase_active_counts_action(const void * args) {
  size_t no_tasks = 0;
  for (size_t i = 0; i < HPX_THREADS; ++i) {
    if (buckets->num_buckets[i] > buckets->current_level && buckets->buckets[i][buckets->current_level] != NULL)
      no_tasks += size(buckets->buckets[i][buckets->current_level]);
  }
  // printf("Increasing termination counts by %zu.\n", no_tasks);
  _increment_active_count(no_tasks);
  // printf("Active counts increased.\n");
  return HPX_SUCCESS;
}

hpx_action_t _send_next_level = 0;
static int _send_next_level_action(const hpx_addr_t * const reduce_lco) {
  size_t max_no_buckets = 0;
  for (size_t i = 0; i < HPX_THREADS; ++i) {
    if (buckets->num_buckets[i] > max_no_buckets) max_no_buckets = buckets->num_buckets[i];
  }
  size_t new_level = size_t_max;
  for (size_t i = ++buckets->current_level; i < max_no_buckets; ++i) {
    for (size_t j = 0; j < HPX_THREADS; ++j) {
      if (i < buckets->num_buckets[j]) {
	if (buckets->buckets[j][i] != NULL) {
	  new_level = i;
	  goto found_bucket_l;
	}
      }
    }
  }
 found_bucket_l:
  hpx_lco_set(*reduce_lco, sizeof(new_level), &new_level, HPX_NULL, HPX_NULL);
  hpx_lco_get(*reduce_lco, sizeof(new_level), &new_level);
  buckets->current_level = new_level;
  // printf("Current level is %zu\n", new_level);
  return HPX_SUCCESS;
}

static void _find_next_level() {
  const hpx_addr_t level_reduce_lco = 
    hpx_lco_allreduce_new(
      HPX_LOCALITIES, 
      HPX_LOCALITIES, 
      sizeof(size_t), 
      (hpx_commutative_associative_op_t)_size_t_minimum_op, 
      (void (*)(void *, const size_t))_size_t_minimum_init);
  const hpx_addr_t bcast_lco = hpx_lco_future_new(0);
  hpx_bcast(_send_next_level, &level_reduce_lco, sizeof(level_reduce_lco), bcast_lco);
  hpx_lco_wait(bcast_lco);
  hpx_lco_delete(bcast_lco, HPX_NULL);
  hpx_lco_delete(level_reduce_lco, HPX_NULL);
}

hpx_action_t call_delta_sssp = 0;
int call_delta_sssp_action(const call_sssp_args_t *const args) {
  printf("Delta-stepping called.\n");
  const hpx_addr_t bcast_lco = hpx_lco_future_new(0);  
  hpx_bcast(_init_buckets, &args->delta, sizeof(args->delta), bcast_lco);
  hpx_lco_wait(bcast_lco);
  hpx_lco_delete(bcast_lco, HPX_NULL);
  const hpx_addr_t index
    = hpx_addr_add(args->graph, args->source * sizeof(hpx_addr_t), _index_array_block_size);
  // printf("Inserting index %zu into buckets. Source is %zu.\n", index, args->source);
  _insert_buckets(HPX_THREAD_ID, 0, index, 0);
  while(true) {
    // The following code is similar to sssp_common.  The 2 should be unified somehow.

    // DC is only supported with count termination now.
    assert(_sssp_kind != _DC_SSSP_KIND || _get_termination() == COUNT_TERMINATION);

    // Initialize counts and increment the active count for the first vertex.
    // We expect that the termination global variable is set correctly
    // on this locality.  This is true because call_sssp_action is called
    // with HPX_HERE and termination is set before the call by program flags.

    const hpx_addr_t termination_lco = hpx_lco_future_new(0);

    if (_get_termination() == COUNT_TERMINATION) {
      hpx_addr_t internal_termination_lco = hpx_lco_future_new(0);

      // Initialize DC if necessary
      hpx_addr_t init_queues_lco = HPX_NULL;
      if(_sssp_kind == _DC_SSSP_KIND) {
	init_queues_lco = hpx_lco_future_new(0);
	hpx_bcast(_sssp_init_queues, &internal_termination_lco, sizeof(internal_termination_lco), init_queues_lco);
      }
      hpx_addr_t init_termination_count_lco = hpx_lco_future_new(0);
      // printf("Starting initialization bcast.\n");
      hpx_bcast(_initialize_termination_detection, NULL, 0, init_termination_count_lco);
      // printf("Waiting on bcast.\n");
      hpx_lco_wait(init_termination_count_lco);
      hpx_lco_delete(init_termination_count_lco, HPX_NULL);
      const hpx_addr_t active_counts_termination_lco = hpx_lco_future_new(0);
      hpx_bcast(_delta_sssp_increase_active_counts, NULL, 0, active_counts_termination_lco);
      hpx_lco_wait(active_counts_termination_lco);
      hpx_lco_delete(active_counts_termination_lco, HPX_NULL);
      if(_sssp_kind == _DC_SSSP_KIND) {
	hpx_lco_wait(init_queues_lco);
	hpx_lco_delete(init_queues_lco, HPX_NULL);
      }
      // printf("Visiting all in level %zu.\n", buckets->current_level);
      hpx_bcast(_visit_all_in_current_level, &args->graph, sizeof(args->graph), HPX_NULL);
      // printf("starting termination detection\n");
      _detect_termination(termination_lco, internal_termination_lco);
    } else {
      // process termination should be easy to add
      fprintf(stderr, "sssp: invalid termination mode.\n");
      hpx_abort();
    }
    // Finish level
    hpx_lco_wait(termination_lco);
    hpx_lco_delete(termination_lco, HPX_NULL);

    // printf("Finding next level.\n");
    _find_next_level();
    if (buckets->current_level == size_t_max) {
      hpx_addr_t delete_termination_lco = hpx_lco_and_new(HPX_LOCALITIES);
      hpx_bcast(_delete_buckets, &delete_termination_lco, sizeof(delete_termination_lco), HPX_NULL);
      hpx_lco_wait(delete_termination_lco);
      hpx_lco_set(args->termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
      hpx_lco_delete(delete_termination_lco, HPX_NULL);
      break;
    }
  }
  hpx_lco_set(args->termination_lco, 0, NULL, HPX_NULL, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_CONSTRUCTOR void _sssp_register_actions() {
  HPX_REGISTER_ACTION(& _visit_all_in_buffer,
                      _visit_all_in_buffer_action);
  HPX_REGISTER_ACTION(& _send_next_level,
                      _send_next_level_action);
  HPX_REGISTER_ACTION(& _init_buckets,
                      _init_buckets_action);
  HPX_REGISTER_ACTION(& _delta_sssp_increase_active_counts,
                      _delta_sssp_increase_active_counts_action);
  HPX_REGISTER_ACTION(& _visit_all_in_current_level,
                      _visit_all_in_current_level_action);
  HPX_REGISTER_ACTION(& _delete_buckets,
                      _delete_buckets_action);
  HPX_REGISTER_ACTION(& call_delta_sssp,
                      call_delta_sssp_action);
}
