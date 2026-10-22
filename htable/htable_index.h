/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_INDEX_H
#define HTABLE_INDEX_H

#include "htable_types.h"

struct hlist_head __htable_index = { 0 };

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

void htable_index_add(struct htable *table){
	if(unlikely(!table->is_added_to_index)){
		table->is_added_to_index = true;
		hlist_add(&__htable_index, &table->index_node);
	}
}
EXPORT_SYMBOL(htable_index_add);

void htable_index_del(struct htable *table){
	if(likely(table->is_added_to_index)){
		table->is_added_to_index = false;
		hlist_del(&table->index_node);
	}
}
EXPORT_SYMBOL(htable_index_del);

void htable_index_each(void (*callback)(struct htable *, void *),void *arg){
	struct hlist_node *pos;
	hlist_for_each(pos, &__htable_index){
		struct htable *entry = container_of(pos, struct htable, index_node);
		callback(entry, arg);
	}
}
EXPORT_SYMBOL(htable_index_each);

#endif /* HTABLE_INDEX_H */