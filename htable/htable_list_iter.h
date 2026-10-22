/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_LIST_ITER_H
#define HTABLE_LIST_ITER_H

#include "htable_types.h"
#include "htable_list.h"

struct htable_list_iter {
	struct htable_list *list;
	unsigned int hash;
	struct htable_list_node *node;
};

/**
 * htable_list_iter_init - Initialize iteration over matching list nodes
 * @list: List to search
 * @hash: Hash value to match
 *
 * Nodes are returned from tail to head. The list must not be modified during
 * iteration.
 */
static inline struct htable_list_iter
htable_list_iter_init(struct htable_list *list, unsigned int hash)
{
	struct htable_list_iter iter = {
		.list = list,
		.hash = hash,
		.node = htable_list_get_first_match(list, hash),
	};

	return iter;
}

static inline struct htable_list_node *
htable_list_iter_get_current_match(const struct htable_list_iter *iter)
{
	return iter->node;
}

static inline struct htable_list_node *
htable_list_iter_get_first_match(const struct htable_list_iter *iter)
{
	return iter->node;
}

static inline struct htable_list_node *
htable_list_iter_get_next_match(struct htable_list_iter *iter)
{
	iter->node = htable_list_get_next_match(iter->node, iter->hash);
	return iter->node;
}

#define htable_list_for_each(list,hash,it) \
	for(\
		struct htable_list_iter it = htable_list_iter_init(list, hash)\
		;NULL != htable_list_iter_get_current_match(&it)\
		;htable_list_iter_get_next_match(&it)\
	)

#endif
