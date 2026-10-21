/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_LIST_ITER_H
#define HTABLE_LIST_ITER_H

#include "htable_iter.h"
#include "htable_list.h"

struct htable_list_iter {
	struct htable_iter base;
	struct htable_list *list;
	struct htable_list_node *next;
};

static inline bool htable_list_iter_next_impl(struct htable_iter *base,
					      const void **item)
{
	struct htable_list_iter *iter =
		container_of(base, struct htable_list_iter, base);
	struct htable_list_node *node = iter->next;

	if (!node)
		return false;

	iter->next = node->pprev == &iter->list->sentinel ?
		NULL : node->pprev;
	*item = node;
	return true;
}

static const struct htable_iter_ops htable_list_iter_ops = {
	.next = htable_list_iter_next_impl,
};

/**
 * htable_list_iter_init - Initialize iteration over a list
 * @iter: Iterator storage
 * @list: List to traverse
 *
 * Nodes are returned from tail to head because the list stores predecessor
 * links only. The list must not be modified during iteration.
 */
static inline void htable_list_iter_init(struct htable_list_iter *iter,
					 struct htable_list *list)
{
	iter->list = list;
	iter->next = list->tail;
	htable_iter_init(&iter->base, &htable_list_iter_ops);
}

static inline bool htable_list_iter_next(struct htable_list_iter *iter,
					 const void **item)
{
	return htable_iter_next(&iter->base, item);
}

#endif