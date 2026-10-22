/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_TABLE_H
#define HTABLE_TABLE_H

struct htable_table;

struct htable_table_node {
	u32 hash;
	struct rhash_head hash_node;
	struct htable_table *table;
};

struct htable_table {
	struct rhashtable hash;
};

#define HTABLE_TABLE_INIT { .hash = {} }

static const struct rhashtable_params htable_table_params = {
	.key_len = sizeof(u32),
	.key_offset = offsetof(struct htable_table_node, hash),
	.head_offset = offsetof(struct htable_table_node, hash_node),
	.automatic_shrinking = true,
};

static inline void htable_table_node_init(struct htable_table_node *node,
					  u32 hash)
{
	node->hash = hash;
	WRITE_ONCE(node->table, NULL);
}

static inline int htable_table_init(struct htable_table *table)
{
	return rhashtable_init(&table->hash, &htable_table_params);
}

/*
 * Call only after users have stopped accessing the table and all entries
 * have been removed. Use synchronize_rcu() before freeing removed nodes.
 */
static inline void htable_table_deinit(struct htable_table *table)
{
	rhashtable_destroy(&table->hash);
}

/*
 * The hash is the key, so duplicate hashes return -EEXIST. The table grows
 * automatically; callers must serialize operations on the same node.
 */
static inline int htable_table_add(struct htable_table *table,
				   struct htable_table_node *node)
{
	int ret;

	if (READ_ONCE(node->table))
		return -EALREADY;

	ret = rhashtable_lookup_insert_fast(&table->hash, &node->hash_node,
					    htable_table_params);
	if (!ret)
		WRITE_ONCE(node->table, table);

	return ret;
}

/* The caller must hold rcu_read_lock() and keep it while using the result. */
static inline struct htable_table_node *
htable_table_lookup_rcu(struct htable_table *table, u32 hash)
{
	return rhashtable_lookup(&table->hash, &hash, htable_table_params);
}

static inline struct htable_table *
htable_table_get_by_node(struct htable_table_node *node)
{
	return READ_ONCE(node->table);
}

static inline int
__htable_table_delete(struct htable_table *table,
		      struct htable_table_node *node)
{
	int ret;

	if (READ_ONCE(node->table) != table)
		return -ENOENT;

	ret = rhashtable_remove_fast(&table->hash, &node->hash_node,
				     htable_table_params);
	if (!ret)
		WRITE_ONCE(node->table, NULL);

	return ret;
}

static inline int htable_table_delete(struct htable_table_node *node)
{
	struct htable_table *table = htable_table_get_by_node(node);
	return __htable_table_delete(table, node);
}

static inline bool htable_table_resize(struct htable_table *table,size_t size)
{
	return !rhashtable_resize(&table->hash, size, htable_table_params);
}

#endif /* HTABLE_TABLE_H */
