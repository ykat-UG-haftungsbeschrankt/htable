/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_ITER_H
#define HTABLE_ITER_H

#include "htable_types.h"
#include "htable_list_iter.h"
#include "htable_table_iter.h"

struct htable_iter {
	union {
		struct htable_list_iter list;
		struct htable_table_iter table;
	};
	void *ptr;
};

static inline void *htable_iter_get_first_match(struct htable *table, struct htable_iter *iter)
{
	if (__htable_is_type_list(table))
		return htable_list_iter_get_first_match(&iter->list);
	else
		return htable_table_iter_get_current_match(&iter->table);
}

static inline struct htable_iter htable_iter_init(struct htable *table, size_t hash)
{
	struct htable_iter it;
	if (__htable_is_type_list(table))
		it.list = htable_list_iter_init(&table->list, hash);
	else
		it.table = htable_table_iter_init(&table->table, hash);

	it.ptr = htable_iter_get_first_match(table, &it);
	return it;
}

static inline void *htable_iter_get_current_match(struct htable *table, struct htable_iter *iter)
{
	(void)table;
	return iter->ptr;
}

static inline void htable_iter_get_next_match(struct htable *table, struct htable_iter *iter)
{
	if (__htable_is_type_list(table))
		iter->ptr = htable_list_iter_get_next_match(&iter->list);
	else
		iter->ptr = htable_table_iter_get_next_match(&iter->table);
}

#define htable_for_each(table,hash,ptr) \
	for(\
		struct htable_iter it = htable_iter_init(table, hash)\
		;NULL != (ptr = htable_iter_get_current_match(table,&it))\
		;htable_iter_get_next_match(table,&it)\
	)

#endif
