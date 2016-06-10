#include "allreduce.h"

#ifndef LIBHPX_PROCESS_ALLREDUCE_TREE_H
#define LIBHPX_PROCESS_ALLREDUCE_TREE_H

void allreduce_tree_init(allreduce_t *r, size_t bytes, hpx_addr_t parent,
                    hpx_monoid_id_t id, hpx_monoid_op_t op) ;

int32_t allreduce_tree_add(allreduce_t *r, hpx_action_t op, hpx_addr_t addr) ;

void allreduce_tree_register_leaves(allreduce_t *r, hpx_addr_t leaf_addr);

void allreduce_tree_algo_nary(allreduce_t *r, hpx_addr_t *locals,
	       	int32_t num_locals, int32_t arity);

void allreduce_tree_algo_binomial(allreduce_t *r, hpx_addr_t *locals, int32_t num_locals);

int32_t allreduce_tree_setup_parent(allreduce_t *r, hpx_action_t op, hpx_addr_t child) ;

void allreduce_tree_setup_child(allreduce_t *r, hpx_addr_t parent) ;

void allreduce_tree_fini(allreduce_t *r) ;

extern HPX_ACTION_DECL(allreduce_tree_init_async);
extern HPX_ACTION_DECL(allreduce_tree_fini_async);
extern HPX_ACTION_DECL(allreduce_tree_add_async);
extern HPX_ACTION_DECL(allreduce_tree_register_leaves_async);
extern HPX_ACTION_DECL(allreduce_tree_algo_nary_async);
extern HPX_ACTION_DECL(allreduce_tree_algo_binomial_async);
extern HPX_ACTION_DECL(allreduce_tree_setup_parent_async);
extern HPX_ACTION_DECL(allreduce_tree_setup_child_async);

#endif
