/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef htable_table_bucket_BUCKET_H
#define htable_table_bucket_BUCKET_H

#include "htable_types.h"
#include "htable_list.h"
#include "htable_list_iter.h"
#include "swar.h"

struct htable_table_index {
	swar_slots_t index;
	u8       last_insert_bucket;
	swar_slots_t list_index[8];
};

struct htable_table_bucket{
	struct htable_list list[8];
	struct htable_list overflow;
};

struct htable_table_node{
	struct htable_list_node node;
};

struct htable_table{
	struct htable_table_index  *index;
	struct htable_table_bucket *buckets;
	usize                      total_entries;
	usize                      bucket_count;
};

static inline bool htable_table_get_bucket_count(
	usize entries_per_bucket,
	usize total_entries,
	usize *bucket_count
){
	if (entries_per_bucket == 0 || total_entries == 0)
		return false;

	*bucket_count = total_entries / entries_per_bucket;
	if (total_entries % entries_per_bucket != 0)
		(*bucket_count)++;

	return true;
}

static inline bool htable_table_get_buffer_size(
	usize entries_per_bucket,
	usize total_entries,
	usize *buffer_size
){
	usize bucket_count;
	const usize bytes_per_bucket =
		sizeof(struct htable_table_index) +
		sizeof(struct htable_table_bucket);

	if (!htable_table_get_bucket_count(
		    entries_per_bucket, total_entries, &bucket_count) ||
	    bucket_count > (usize)-1 / bytes_per_bucket)
		return false;

	*buffer_size = bucket_count * bytes_per_bucket;
	return true;
}

/**
 * htable_table_init - Initialize a table in caller-owned storage
 * @buffer: Storage aligned for both index and bucket structures
 */
static inline bool htable_table_init(
	 struct htable_table *table
	,void   *buffer
	,usize buffer_size
	,usize entries_per_bucket
	,usize total_entries
){
	usize bucket_count;
	usize required_size;
	usize index_bytes;

	if (!htable_table_get_bucket_count(
		    entries_per_bucket, total_entries, &bucket_count) ||
	    !htable_table_get_buffer_size(
		    entries_per_bucket, total_entries, &required_size))
		return false;

	if (!buffer || buffer_size < required_size) {
		return false;
	}

	index_bytes = sizeof(struct htable_table_index) * bucket_count;
	if ((uintptr_t)buffer % __alignof__(struct htable_table_index) != 0 ||
	    ((uintptr_t)buffer + index_bytes) %
		    __alignof__(struct htable_table_bucket) != 0)
		return false;

	table->index = buffer;
	table->buckets = (void *)(table->index + bucket_count);
	table->total_entries = total_entries;
	table->bucket_count = bucket_count;

	for (usize i = 0; i < bucket_count; i++) {
		table->index[i].index = 0;
		table->index[i].last_insert_bucket = 0;
		for (usize j = 0; j < 8; j++)
			table->index[i].list_index[j] = 0;

		for (usize j = 0; j < 8; j++)
			htable_list_init(&table->buckets[i].list[j]);
		htable_list_init(&table->buckets[i].overflow);
	}

	return true;
}

/**
 * htable_table_new - Allocate a table and its backing storage
 * @entries_per_bucket: Entry capacity parameter passed to the table initializer
 * @total_entries: Requested total table capacity
 *
 * In kernel builds this allocation may sleep and must be called from a
 * sleepable context.
 */
static inline struct htable_table *htable_table_new(
	usize entries_per_bucket,
	usize total_entries
){
	usize buffer_size;
	struct htable_table *table;
	void *buffer;

	if (!htable_table_get_buffer_size(
		    entries_per_bucket, total_entries, &buffer_size))
		return NULL;

	table = htable_malloc(sizeof(*table));
	if (!table)
		return NULL;

	buffer = htable_malloc(buffer_size);
	if (!buffer) {
		htable_free(table);
		return NULL;
	}

	if (!htable_table_init(
		    table,
		    buffer,
		    buffer_size,
		    entries_per_bucket,
		    total_entries)) {
		htable_free(buffer);
		htable_free(table);
		return NULL;
	}

	return table;
}

/**
 * htable_table_free - Free a table created by htable_table_new
 * @table: Table returned by htable_table_new
 */
static inline void htable_table_free(struct htable_table *table)
{
	if (!table)
		return;

	htable_free(table->index);
	htable_free(table);
}

static inline void htable_table_deinit(struct htable_table *table)
{
}

static inline usize htable_table_get_bucket_index(
	const struct htable_table *table,
	swar_hash_t hash
){
	return (usize)hash % table->bucket_count;
}

static inline usize htable_table_get_bucket_hash(
	const struct htable_table *table,
	swar_hash_t hash
){
	return htable_table_get_bucket_index(table, hash);
}

static inline bool
htable_table_add(
	 struct htable_table *table
	,struct htable_table_node *node
){
	struct htable_list *list = NULL;
	swar_hash_t hash = node->node.hash;
	usize sbucket = htable_table_get_bucket_index(table, hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	swar_fingerprint_t slot_key = skey | SWAR_SLOT_SET_SIG;
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t list_index_offset = -1;
	swar_offset_t key = swar_find_first_set64(index->index, slot_key);
	if(key != -1){
		list_index_offset = swar_find_first_set64(
			 index->list_index[key]
			,slot_key
		);
		if(list_index_offset != -1){
			list = &bucket->list[key];
			goto insert;
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,slot_key);
		if(key != -1){
			list_index_offset = key & 0x7;
			key = (key >> 3);
			index->index = swar_set_slot64(index->index, key, slot_key);
			list = &bucket->list[key];
			goto insert;
		}
	}
	key = swar_find_first_empty_slot((const u8 *)index->list_index, 8 * 8);
	if(key != -1){
		list_index_offset = key & 0x7;
		key = (key >> 3);
		index->index = swar_set_slot64(index->index, key, slot_key);
		index->list_index[key] = swar_set_slot64(
				index->list_index[key], list_index_offset, slot_key
		);
		list = &bucket->list[key];
	}else{
		list = &bucket->overflow;
	}

insert:
	htable_list_add(
		 list
		,&node->node
	);
	return true;
}

static inline struct htable_table *htable_table_get_by_node(
	struct htable_table_node *node
){
	while (likely(node->pprev))
		node = node->pprev;

	return container_of(node, struct htable_table, sentinel);
}

static inline void
__htable_table_del(
	struct htable_table *table,
	struct htable_table_node *node
){
	struct htable_list *list = NULL;
	swar_hash_t hash = node->node.hash;
	usize sbucket = htable_table_get_bucket_index(table, hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	swar_fingerprint_t slot_key = skey | SWAR_SLOT_SET_SIG;
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t list_index_offset = -1;
	swar_offset_t key = swar_find_first_set64(index->index, slot_key);
	if(key != -1){
		list_index_offset = swar_find_first_set64(
			 index->list_index[key]
			,slot_key
		);
		if(list_index_offset != -1){
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node == &node->node){
					list = &bucket->list[key];
					goto delete;
				}
			}
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,slot_key);
		if(key != -1){
			list_index_offset = key & 0x7;
			key = (key >> 3);
			index->index = swar_set_slot64(index->index, key, slot_key);
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node == &node->node){
					list = &bucket->list[key];
					goto delete;
				}
			}
		}
	}
	htable_list_for_each(&bucket->overflow,hash,it){
		if(it.node == &node->node){
			list = &bucket->overflow;
			goto delete;
		}
	}
	return;
delete:
	__htable_list_del(list, &node->node);
	if(list_index_offset != -1){
		htable_list_for_each(list,hash,it){
			if(swar_get_key(it.node->hash) == skey){
				return;
			}
		}
		index->list_index[key] = swar_set_slot64(
			index->list_index[key],
			list_index_offset,
			SWAR_SLOT_EMPTY_SIG
		);
	}
}


static inline void
htable_table_del(
	struct htable_table_node *node
){
	struct htable_table *table = htable_table_get_by_node(node);
	__htable_table_del(table, node);
}

static inline struct htable_list_node *
htable_table_get_first_match(
	 struct htable_table *table
	,unsigned int hash
){
	usize sbucket = htable_table_get_bucket_index(table, hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	swar_fingerprint_t slot_key = skey | SWAR_SLOT_SET_SIG;
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t key = swar_find_first_set64(index->index, slot_key);
	if(key != -1){
		if(swar_find_first_set64(
			 index->list_index[key]
			,slot_key
		) != -1){
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node->hash == hash){
					return it.node;
				}
			}
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,slot_key);
		if(key != -1){
			key = (key >> 3);
			index->index = swar_set_slot64(index->index, key, slot_key);
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node->hash == hash)
					return it.node;
			}
		}
	}
	htable_list_for_each(&bucket->overflow,hash,it){
		if(it.node->hash == hash)
			return it.node;
	}
	return NULL;
}

static inline bool
htable_table_has(
	struct htable_table *table,
	swar_hash_t hash
){
	return NULL != htable_table_get_first_match(
		table,
		hash
	);
}

static inline struct htable_list_node *
htable_table_get_next_match(
	struct htable_list_node *node
	,swar_hash_t hash
){
	return htable_list_get_next_match(node, hash);
}

#endif /* htable_table_bucket_BUCKET_H */
