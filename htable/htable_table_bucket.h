/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef htable_table_bucket_BUCKET_H
#define htable_table_bucket_BUCKET_H

#include "htable_types.h"
#include "htable_table.h"
#include "htable_list.h"
#include "htable_list_iter.h"
#include "swar.h"

struct htable_table_index {
	swar_slots_t index;
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
};

static inline bool htable_table_init(
	 struct htable_table *table
	,void   *buffer
	,usize buffer_size
	,usize entries_per_bucket
	,usize total_entries
){
	const usize bucket_count = total_entries/8;

	if (buffer_size < 
		  ((sizeof(struct htable_table_index) * bucket_count)
		+ (sizeof(struct htable_table_bucket) * bucket_count))
	){
		return false;
	}

	table->index = buffer;
	table->buckets = (void *)(table->index + bucket_count);
	table->total_entries = total_entries;

	return true;
}

static inline void htable_table_deinit(struct htable_table *table)
{
}

static inline u8 htable_table_get_bucket_index(
	struct htable_table *table,
	unsigned int hash
){
	return hash % (table->total_entries / 8);
}

static inline u8 htable_table_get_bucket_hash(
	struct htable_table *table,
	unsigned int hash
){
	return hash % (table->total_entries / 8);
}

static inline void
htable_table_add(
	 struct htable_table *table
	,struct htable_table_node *node
){
	struct htable_list *list = NULL;
	swar_hash_t hash = node->node.hash;
	swar_bucket_t sbucket = swar_get_bucket(hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t list_index_offset = -1;
	swar_offset_t key = swar_find_first_set64(index->index,skey);
	if(key != -1){
		list_index_offset = swar_find_first_set64(
			 index->list_index[key]
			,skey
		);
		if(list_index_offset != -1){
			list = &bucket->list[key];
			goto insert;
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,skey);
		if(key != -1){
			list_index_offset = key & 0x7;
			key = (key >> 3);
			swar_set_slot(index->index, key, skey);
			list = &bucket->list[key];
			goto insert;
		}
	}
	key = swar_find_first_zero(index->list_index,8*8,skey);
	if(key != -1){
		list_index_offset = key & 0x7;
		key = (key >> 3);
		swar_set_slot(index->index, key, skey);
		swar_set_slot(&index->list_index[key], list_index_offset, skey);
		list = &bucket->list[key];
	}else{
		list = &bucket->overflow;
	}

insert:
	htable_list_add(
		 list
		,&node->node
	);
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
	swar_bucket_t sbucket = swar_get_bucket(hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t list_index_offset = -1;
	swar_offset_t key = swar_find_first_set64(index->index,skey);
	if(key != -1){
		list_index_offset = swar_find_first_set64(
			 index->list_index[key]
			,skey
		);
		if(list_index_offset != -1){
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node == node){
					list = &bucket->list[key];
					goto delete;
				}
			}
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,skey);
		if(key != -1){
			list_index_offset = key & 0x7;
			key = (key >> 3);
			swar_set_slot(index->index, key, skey);
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node == node){
					list = &bucket->list[key];
					goto delete;
				}
			}
		}
	}
	htable_list_for_each(&bucket->overflow,hash,it){
		if(it.node == node){
			list = &bucket->overflow;
			goto delete;
		}
	}
	return;
delete:
	__htable_list_del(list, node);
	if(list_index_offset != -1){
		htable_list_for_each(list,hash,it){
			if(swar_get_key(it.node->hash) == skey){
				return;
			}
		}
		swar_set_slot(
			 &index->list_index[key]
			,list_index_offset
			,SWAR_SLOT_EMPTY_SIG
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
	swar_bucket_t sbucket = swar_get_bucket(hash);
	swar_fingerprint_t skey = swar_get_key(hash);
	struct htable_table_index *index = &table->index[sbucket];
	struct htable_table_bucket *bucket = &table->buckets[sbucket];
	swar_offset_t key = swar_find_first_set64(index->index,skey);
	if(key != -1){
		if(swar_find_first_set64(
			 index->list_index[key]
			,skey
		) != -1){
			htable_list_for_each(&bucket->list[key],hash,it){
				if(it.node->hash == hash){
					return it.node;
				}
			}
		}
	}else{
		key = swar_find_first_set(index->list_index,8*8,skey);
		if(key != -1){
			key = (key >> 3);
			swar_set_slot(index->index, key, skey);
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
