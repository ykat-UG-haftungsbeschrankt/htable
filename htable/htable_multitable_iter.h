/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_MULTITABLE_ITER_H
#define HTABLE_MULTITABLE_ITER_H

#include "htable_table.h"

struct htable_multitable_iter {
	struct htable_table *table;
	u32 hash;
};

/**
 * htable_multitable_iter_init - Initialize a hash-table match lookup
 * @table: Hash table to search
 * @hash: Hash value to find
 *
 * The table backend is keyed by hash, so each query has at most one match.
 */
static inline struct htable_multitable_iter
htable_multitable_iter_init(struct htable_table *table, u32 hash)
{
	struct htable_multitable_iter iter = {
		.table = table,
		.hash = hash,
	};

	return iter;
}

/*
 * The caller must hold rcu_read_lock() while retrieving and using a match.
 */
static inline struct htable_table_node *
htable_multitable_iter_get_first_match(struct htable_multitable_iter *iter)
{
	return htable_table_lookup_rcu(iter->table, iter->hash);
}

static inline struct htable_table_node *
htable_multitable_iter_get_next_match(struct htable_multitable_iter *iter)
{
	(void)iter;
	return NULL;
}

#define htable_multitable_for_each(table,hash,ptr) \
	for(\
		struct htable_multitable_iter it = htable_multitable_iter_init(table, hash)\
		;NULL != (ptr = htable_multitable_iter_get_first_match(&it))\
		;htable_multitable_iter_get_next_match(&it)\
	)

#endif
