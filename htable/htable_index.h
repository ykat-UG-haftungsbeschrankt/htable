/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_INDEX_H
#define HTABLE_INDEX_H

#include "htable_types.h"

HLIST_HEAD(__htable_index);

#define HTABLE_INDEX_INIT HLIST_NODE_INIT

struct htable_index {
	struct hlist_node node;
};

void htable_index_init(struct htable_index *index){
	INIT_HLIST_NODE(&index->node);
}

void htable_index_deinit(struct htable_index *index){
	hlist_del(&index->node);
}

bool htable_index_is_added(struct htable_index *index){
	return index->node.pprev != NULL;
}

void htable_index_add(struct htable_index *index){
	if(unlikely(!htable_index_is_added(index))){
		hlist_add(&__htable_index, &index->node);
	}
}
EXPORT_SYMBOL(htable_index_add);

void htable_index_del(struct htable_index *index){
	if(likely(htable_index_is_added(index))){
		hlist_del(&index->node);
	}
}
EXPORT_SYMBOL(htable_index_del);

void htable_index_each(void (*callback)(struct htable_index *, void *),void *arg){
	struct hlist_node *pos;
	hlist_for_each(pos, &__htable_index){
		struct htable_index *entry = container_of(pos, struct htable_index, node);
		callback(entry, arg);
	}
}
EXPORT_SYMBOL(htable_index_each);

#endif /* HTABLE_INDEX_H */