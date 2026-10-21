/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_MULTITABLE_ITER_H
#define HTABLE_MULTITABLE_ITER_H

#include <linux/container_of.h>

#include "htable_iter.h"
#include "htable_multitable.h"

struct htable_multitable_iter {
	struct htable_iter base;
	const FilteredMultiTable *table;
	size_t level;
	size_t bucket;
	size_t slot;
	const struct htable_list_node *next_node;
	const struct htable_list_node *sentinel;
};

static inline bool htable_multitable_iter_next_impl(struct htable_iter *base,
						    const void **item)
{
	struct htable_multitable_iter *iter =
		container_of(base, struct htable_multitable_iter, base);

	while (iter->level < NUM_LEVELS) {
		const TableLevel *level = &iter->table->levels[iter->level];

		while (iter->bucket < level->num_buckets) {
			while (iter->slot < BUCKET_CAPACITY ||
			       iter->next_node != NULL) {
				if (iter->next_node != NULL) {
					const struct htable_list_node *node =
						iter->next_node;
					const struct htable_list_node *previous =
						node->pprev;

					iter->next_node =
						previous == iter->sentinel ?
						NULL : previous;
					*item = node;
					return true;
				}

				size_t slot = iter->slot++;
				size_t index = iter->bucket * BUCKET_CAPACITY + slot;

				if (level->filter_tags[index] == EMPTY_METADATA)
					continue;

				const struct htable_list *list =
					&level->buckets[iter->bucket]
						 .slots[slot].list;

				iter->sentinel = &list->sentinel;
				iter->next_node = list->tail;
			}
			iter->bucket++;
			iter->slot = 0;
		}

		iter->level++;
		iter->bucket = 0;
		iter->slot = 0;
	}

	return false;
}

static const struct htable_iter_ops htable_multitable_iter_ops = {
	.next = htable_multitable_iter_next_impl,
};

/**
 * htable_multitable_iter_init - Initialize iteration over a multitable
 * @iter: Iterator storage
 * @table: Multitable to traverse
 *
 * Returned items are the linked htable_list_node pointers.
 * The table must not be modified during iteration.
 */
static inline void htable_multitable_iter_init(
	struct htable_multitable_iter *iter,
	const FilteredMultiTable *table)
{
	iter->table = table;
	iter->level = 0;
	iter->bucket = 0;
	iter->slot = 0;
	iter->next_node = NULL;
	iter->sentinel = NULL;
	htable_iter_init(&iter->base, &htable_multitable_iter_ops);
}

static inline bool htable_multitable_iter_next(
	struct htable_multitable_iter *iter, const void **item)
{
	return htable_iter_next(&iter->base, item);
}

#endif