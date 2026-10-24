/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_INDEX_H
#define HTABLE_INDEX_H

#include "htable_types.h"

#define HTABLE_INDEX_NODE_INIT { .node = { .next = NULL, .pprev = NULL } }

struct htable_index_node {
	struct hlist_node node;
};

struct hlist_head *__htable_index_get_instance(){
	static struct hlist_head instance = HLIST_HEAD_INIT;
	return &instance;
}

void __htable_index_node_init(struct htable_index_node *index){
	INIT_HLIST_NODE(&index->node);
}

void __htable_index_node_deinit(struct htable_index_node *index){
	hlist_del_init(&index->node);
}

bool __htable_index_node_is_added(struct htable_index_node *index){
	return !hlist_unhashed(&index->node);
}

void htable_index_add(struct htable_index_node *index){
	if(unlikely(!__htable_index_node_is_added(index))){
		hlist_add_head(&index->node, __htable_index_get_instance());
	}
}
EXPORT_SYMBOL(htable_index_add);

void htable_index_del(struct htable_index_node *index){
	if(likely(__htable_index_node_is_added(index))){
		hlist_del_init(&index->node);
	}
}
EXPORT_SYMBOL(htable_index_del);

void htable_index_each(void (*callback)(struct htable_index_node *, void *),void *arg){
	struct hlist_node *pos;
	hlist_for_each(pos, __htable_index_get_instance()){
		struct htable_index_node *entry = container_of(pos, struct htable_index_node, node);
		callback(entry, arg);
	}
}
EXPORT_SYMBOL(htable_index_each);

#endif /* HTABLE_INDEX_H */