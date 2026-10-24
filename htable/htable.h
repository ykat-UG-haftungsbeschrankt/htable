/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_H
#define HTABLE_H

#include "swar.h"
#include "htable_types.h"
#include "htable_index.h"
#include "htable_stats.h"
#include "htable_list.h"
#include "htable_table.h"

#define HTABLE_RESIZE_THRESHOLD 1000

typedef u32 htable_hash_t;

enum htable_flags {
	HTABLE_FLAGS_INIT  = 0,
	HTABLE_FLAGS_DEINIT = 1 << 2,
	HTABLE_FLAGS_IS_TABLE = 1 << 0,
	HTABLE_FLAGS_IS_ADDED_TO_INDEX = 1 << 1,
};

struct htable {
	struct htable_index_node      index;
	struct htable_stats      stats;
	struct htable_list       list;
	struct htable_table      table;
	u8                       flags;
};

enum htable_node_flags {
	HTABLE_NODE_FLAGS_INIT  = 0,
	HTABLE_NODE_FLAGS_DEINIT = 1 << 2,
	HTABLE_NODE_FLAGS_IS_TABLE = 1 << 0,
	HTABLE_NODE_FLAGS_IS_ADDED_TO_INDEX = 1 << 1,
};

struct htable_node{
	struct hlist_node hnode;
	union{
		struct {
			u32  flags;
			swar_hash_t  hash;
			void *ptr;
		};
		union{
			struct htable_list_node  list;
			struct htable_table_node table;
		}node;
	};
};

static inline void htable_node_init(struct htable_node *node, swar_hash_t hash){
	INIT_HLIST_NODE(&node->hnode);
	node->flags = HTABLE_NODE_FLAGS_INIT;
	node->hash = hash;
	node->ptr = NULL;
}

static inline struct htable_node *htable_node_from_hlist_node(struct hlist_node *hnode)
{
	return container_of(hnode, struct htable_node, hnode);
}

static inline struct htable_table_node *htable_node_to_htable_table_node(struct htable_node *hnode)
{
	return &hnode->node.table;
}

static inline struct htable_list_node *htable_node_to_htable_list_node(struct htable_node *hnode)
{
	return &hnode->node.list;
}

static inline struct htable_node *htable_node_from_htable_list_node(struct htable_list_node *lnode)
{
	return container_of(lnode, struct htable_node, node.list);
}

static inline bool htable_node_is_type_table(struct htable_node *hnode)
{
	return hnode->flags & HTABLE_NODE_FLAGS_IS_TABLE;
}

static inline void htable_node_set_type_list(struct htable_node *hnode)
{
	hnode->flags &= ~HTABLE_NODE_FLAGS_IS_TABLE;
}

static inline void htable_node_set_type_table(struct htable_node *hnode)
{
	hnode->flags |= HTABLE_NODE_FLAGS_IS_TABLE;
}

static inline bool htable_node_is_hashed(struct htable_node *hnode){
	return hnode->ptr != NULL;
}

BUILD_BUG_ON(sizeof(struct hlist_head) < sizeof(void*));
BUILD_BUG_ON(offsetof(struct htable_node, hnode) != 0);
BUILD_BUG_ON(sizeof(struct htable_list_node) != sizeof(struct htable_table_node));


bool __htable_is_type_list(struct htable *table){
	return !(table->flags & HTABLE_FLAGS_IS_TABLE);
}

bool __htable_is_type_table(struct htable *table){
	return table->flags & HTABLE_FLAGS_IS_TABLE;
}

void __htable_set_type_list(struct htable *table){
	table->flags &= ~HTABLE_FLAGS_IS_TABLE;
}

void __htable_set_type_table(struct htable *table){
	table->flags |= HTABLE_FLAGS_IS_TABLE;
}

bool htable_init(
	struct htable *htable,
	const char *name,
	void *table_buffer,
	usize table_buffer_size,
	usize entries_per_bucket,
	usize total_entries
){
	__htable_index_node_init(&htable->index);
	htable_stats_init(&htable->stats,name);
	htable_list_init(&htable->list);
	htable->table = (struct htable_table){0};
	htable->flags = HTABLE_FLAGS_INIT;

	if (!htable_table_init(
		    &htable->table,
		    table_buffer,
		    table_buffer_size,
		    entries_per_bucket,
		    total_entries)) {
		htable->flags = HTABLE_FLAGS_DEINIT;
		return false;
	}

	return true;
}
void htable_deinit(struct htable *htable){
	__htable_index_node_deinit(&htable->index);
	htable_stats_deinit(&htable->stats);
	htable_list_deinit(&htable->list);
	htable_table_deinit(&htable->table);
	htable->flags = HTABLE_FLAGS_DEINIT;
}


//////////////////////////////////////////////////////////////////////////
// Htable index management functions
//////////////////////////////////////////////////////////////////////////
static inline struct htable_node *
htable_resize_list_remove_node(struct htable *table)
{
	struct htable_list_node *node = htable_list_last(&table->list);
	if(!node)
		return NULL;
	htable_list_del(node);
	return htable_node_from_htable_list_node(node);
}

static inline struct htable_node *
htable_resize_table_remove_node(struct htable *table)
{
	struct htable_list_node *node = htable_list_last(&table->list);
	if(!node)
		return NULL;
	htable_list_del(node);
	return htable_node_from_htable_list_node(node);
}

static inline void
htable_resize_list_add_node(struct htable *table,struct htable_node *node)
{
	htable_node_set_type_list(node);
	htable_list_add(&table->list, htable_node_to_htable_list_node(node));
}

static inline bool
htable_resize_table_add_node(struct htable *table, struct htable_node *node)
{
	if (!htable_table_add(&table->table, htable_node_to_htable_table_node(node)))
		return false;

	htable_node_set_type_table(node);
	return true;
}

void htable_resize_list2table(struct htable *table){
	struct htable_node *node = NULL;
	if(__htable_is_type_list(table)){
		__htable_set_type_table(table);
		while((node = htable_resize_list_remove_node(table))){
			if(!htable_resize_table_add_node(table,node)){
				htable_resize_list_add_node(table,node);
				while((node = htable_resize_table_remove_node(table))){
					htable_resize_list_add_node(table,node);
				}
				__htable_set_type_list(table);
				break;
			}
		}
	}	
}

void htable_resize_table2list(struct htable *table){
	struct htable_node *node = NULL;
	if(__htable_is_type_table(table)){
		__htable_set_type_list(table);
		while((node = htable_resize_table_remove_node(table))){
			htable_resize_list_add_node(table,node);
		}
	}
}

void htable_add_node(struct htable *table, struct htable_node *node, uintptr_t key){
	if(__htable_is_type_table(table)){
		node->hash = key;
		if(!htable_resize_table_add_node(table,node)){
			htable_resize_table2list(table);
			htable_resize_list_add_node(table,node);
		}
	}else{
		htable_resize_list_add_node(table,node);
	}
}

void htable_remove_node(struct htable *table, struct htable_node *node){
	if(htable_node_is_type_table(node)){
		__htable_table_del(
			&table->table,
			htable_node_to_htable_table_node(node)
		);
	}else{
		__htable_list_del(
			&table->list,
			htable_node_to_htable_list_node(node)
		);
	}
	htable_node_set_type_list(node);
	htable_stats_on_del(&table->stats);
}




//////////////////////////////////////////////////////////////////////////
// Htable hashtable.h wrapper
//////////////////////////////////////////////////////////////////////////

#define HTABLE_INIT(name) {						\
	.index = HTABLE_INDEX_NODE_INIT,						\
	.stats = {0},								\
	.list = HTABLE_LIST_INIT,							\
	.table = {0},								\
	.flags = HTABLE_FLAGS_INIT,						\
}

#define DEFINE_HASHTABLE(name, bits)						    \
	struct htable name = HTABLE_INIT(name)

#define DEFINE_READ_MOSTLY_HASHTABLE(name, bits)				\
	struct htable __read_mostly name = HTABLE_INIT(name)

#define DECLARE_HASHTABLE(name, bits)                         	\
	struct htable name

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Initialize the table with caller-provided storage and capacity.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_init(hashtable, buffer, buffer_size, entries_per_bucket, total_entries) \
	__hash_init(hashtable, #hashtable, buffer, buffer_size, \
		    entries_per_bucket, total_entries)
static inline bool __hash_init(
	struct htable *htable,
	const char *name,
	void *buffer,
	usize buffer_size,
	usize entries_per_bucket,
	usize total_entries
)
{
	return htable_init(
		htable,
		name,
		buffer,
		buffer_size,
		entries_per_bucket,
		total_entries
	);
}

#define hash_deinit(hashtable) __hash_deinit(hashtable)
static inline void __hash_deinit(struct htable *htable)
{
	htable_deinit(htable);
}

#define htable_hash(val, bits) hash_long(val, bits)

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the hnode member of an embedded &struct htable_node
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	__hash_add(hashtable,node, key)
static inline void __hash_add(struct htable *htable, struct hlist_node *hnode, htable_hash_t key)
{
	struct htable_node *node = htable_node_from_hlist_node(hnode);
	htable_index_add(&htable->index);
	htable_stats_on_add(&htable->stats);
	htable_node_init(node,key);
	if(__htable_is_type_table(htable)){
		if(!htable_resize_table_add_node(htable,node)){
			htable_resize_table2list(htable);
			htable_resize_list_add_node(htable,node);
		}
	}else{
		htable_resize_list_add_node(htable,node);
	}
}



/**
 * hash_add_rcu - compatibility alias for hash_add
 * @hashtable: hashtable to add to
 * @node: the hnode member of an embedded &struct htable_node
 * @key: the key of the object to be added
 *
 * This function does not provide RCU semantics. Callers must externally
 * synchronize readers and writers.
 */
#define hash_add_rcu(hashtable, node, key)					\
	__hash_add_rcu(hashtable, node, key)
static inline void __hash_add_rcu(struct htable *htable, struct hlist_node *node, htable_hash_t key)
{
	hash_add(htable, node, key);
}

/**
 * hash_hashed - check whether an object is in any hashtable
 * @node: the &struct hlist_node of the object to be checked
 */
static inline bool hash_hashed(struct hlist_node *hnode)
{
	struct htable_node *node = htable_node_from_hlist_node(hnode);
	return htable_node_is_hashed(node);
}



/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_empty(hashtable) __hash_empty(hashtable)
static inline bool __hash_empty(struct htable *htable)
{
	return htable->stats.entries == 0;
}

/**
 * hash_del - remove an object from a hashtable
 * @hashtable: hashtable containing the object
 * @node: the hnode member of an embedded &struct htable_node
 */
static inline void hash_del(struct htable *hashtable, struct hlist_node *hnode)
{
	struct htable_node *node = htable_node_from_hlist_node(hnode);

	htable_remove_node(hashtable, node);
}

/**
 * hash_del_rcu - compatibility alias for hash_del
 * @hashtable: hashtable containing the object
 * @node: the hnode member of an embedded &struct htable_node
 *
 * This function does not provide RCU semantics. Callers must externally
 * synchronize readers and writers.
 */
static inline void hash_del_rcu(struct htable *hashtable, struct hlist_node *node)
{
	hash_del(hashtable, node);
}

#endif /* HTABLE_H */
