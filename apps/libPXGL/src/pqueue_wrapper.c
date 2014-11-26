#include "libpxgl/pqueue_wrapper.h"
#include <stdlib.h>

typedef struct {
  hpx_addr_t vertex;
  distance_t distance;
  size_t pos; //needed by the pqueue interface
} pqueue_node;

/* priority queue functions needed for priority init function */

static int cmp_pri(distance_t next, distance_t curr) {
  return (next < curr);
}

static distance_t get_pri(void *a) {
  return ((pqueue_node *) a)->distance;
}

static void set_pri(void *a, distance_t pri) {
  ((pqueue_node *) a)->distance = pri;
}

static size_t get_pos(void *a) {
  return ((pqueue_node *) a)->pos;
}

static void set_pos(void *a, size_t pos) {
  ((pqueue_node *) a)->pos = pos;
}

bool sssp_queue_empty(sssp_queue_t *q) {
  sync_tatas_acquire(&(q->mutex));
  size_t size = pqueue_size(q->pq);
  sync_tatas_release(&(q->mutex));
  return size == 0;
}

bool sssp_queue_pop(sssp_queue_t *q, hpx_addr_t *v, distance_t *d) {
  pqueue_node *head;
  // printf("Inside the pop function\n");
  sync_tatas_acquire(&(q->mutex)); // acquire the mutex
  // assert(pqueue_size(q->pq) > 0);
  head = pqueue_pop(q->pq);
  // printf("Popped one vertex\n");
  sync_tatas_release(&(q->mutex)); // release it
  if(head != NULL) { 
    *v = head->vertex;
    *d = head->distance;
    free(head);
    return true;
  } else {
    return false;
  }
}

bool sssp_queue_push(sssp_queue_t *q, hpx_addr_t v, distance_t d) {
  int success;
  pqueue_node *pq_node = (pqueue_node*) malloc(sizeof(pqueue_node));
  pq_node->vertex = v;
  pq_node->distance = d;
  sync_tatas_acquire(&(q->mutex)); // acquire the mutex
  bool empty = pqueue_size(q->pq) == 0;
  success = pqueue_insert(q->pq, pq_node);
  sync_tatas_release(&(q->mutex)); // release it
  assert(success == 0);
  // printf("Done inserting the vertex in the priority queue\n");
  return empty;
}

sssp_queue_t *sssp_queue_create(size_t num_elem) {
  sssp_queue_t *q = malloc(sizeof(locked_pq));
  q->pq = pqueue_init(num_elem, cmp_pri, get_pri, set_pri, get_pos, set_pos);
  // printf("returned from initializing the priority queue\n");
  //TODO: don't need to initialize the mutex?
  //pq_array[i]->mutex = hpx_lco_sema_new(1);
  sync_tatas_init(&q->mutex);
  return q;
}

void sssp_queue_free(sssp_queue_t *q) {
  pqueue_free(q->pq);
  free(q);
}
