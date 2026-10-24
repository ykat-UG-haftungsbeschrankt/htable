/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_TABLE_ITER_H
#define HTABLE_TABLE_ITER_H

#include "htable_types.h"
#include "htable_table.h"

struct htable_table_iter {
	struct htable_table *table;
	swar_hash_t hash;
	struct htable_list_node *node;
};

/*
 * Readers and writers must be externally synchronized; this iterator is not
 * RCU-safe.
 */
static inline struct htable_list_node *
htable_table_iter_get_first_match(struct htable_table *table, swar_hash_t hash)
{
	return htable_table_get_first_match(
		 table
		,hash
	);
}

/**
 * htable_table_iter_init - Initialize a hash-table match lookup
 * @table: Hash table to search
 * @hash: Hash value to find
 *
 * A hash may match multiple nodes; iteration returns each matching node.
 */
static inline struct htable_table_iter
htable_table_iter_init(struct htable_table *table, swar_hash_t hash)
{
	struct htable_table_iter iter = {
		.table = table,
		.hash = hash,
		.node = htable_table_iter_get_first_match(table,hash)
	};

	return iter;
}

static inline struct htable_list_node *
htable_table_iter_get_current_match(const struct htable_table_iter *iter)
{
	return iter->node;
}

static inline struct htable_list_node *
htable_table_iter_get_next_match(struct htable_table_iter *iter)
{
	iter->node = htable_list_get_next_match(iter->node, iter->hash);
	return iter->node;
}

#define htable_table_for_each(table,hash,ptr) \
	for(\
		struct htable_table_iter it = htable_table_iter_init(table, hash)\
		;NULL != (ptr = htable_table_iter_get_current_match(&it))\
		;htable_table_iter_get_next_match(&it)\
	)

#endif
