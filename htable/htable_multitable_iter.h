/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_MULTITABLE_ITER_H
#define HTABLE_MULTITABLE_ITER_H

#include <linux/container_of.h>

#include "htable_iter.h"
#include "htable_multitable.h"

struct htable_multitable_iter {
	const FilteredMultiTable *table;
	size_t level;
	size_t bucket;
	size_t slot;
};

static inline bool htable_multitable_iter_next_impl(struct htable_iter *base,
						    const void **item)
{
	struct htable_multitable_iter *iter =
		container_of(base, struct htable_multitable_iter, base);

	while (iter->level < NUM_LEVELS) {
		const TableLevel *level = &iter->table->levels[iter->level];

		while (iter->bucket < level->num_buckets) {
			while (iter->slot < BUCKET_CAPACITY) {
				size_t slot = iter->slot++;
				size_t index = iter->bucket * BUCKET_CAPACITY + slot;

				if (level->filter_tags[index] == EMPTY_METADATA)
					continue;

				*item = level->buckets[iter->bucket]
						.slots[slot].key_ptr;
				return true;
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
	htable_iter_init(&iter->base, &htable_multitable_iter_ops);
}

static inline bool htable_multitable_iter_next(
	struct htable_multitable_iter *iter, const void **item)
{
	return htable_iter_next(&iter->base, item);
}

#endif