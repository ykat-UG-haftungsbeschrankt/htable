/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_ITER_H
#define HTABLE_ITER_H

#include <stdbool.h>
#include "htable_list_iter.h"
#include "htable_multitable_iter.h"

struct htable_iter {
	union {
		struct htable_list_iter list;
		struct htable_multitable_iter table;
	};
	void *ptr;
};

void *htable_iter_get_first_match(struct htable *table, struct htable_iter *iter)
{
	if (htable_table_is_list(table))
		return htable_list_iter_get_first_match(&iter->list);
	else
		return htable_multitable_iter_get_first_match(&iter->table);
}

struct htable_iter htable_iter_init(struct htable *table, size_t hash)
{
	struct htable_iter it;
	if (htable_table_is_list(table))
		it.list = htable_list_iter_init(table, hash);
	else
		it.table = htable_multitable_iter_init(table, hash);

	it.ptr = htable_iter_get_first_match(table, &it);
	return it;
}

void *htable_iter_get_current_match(struct htable *table, struct htable_iter *iter)
{
	return iter->ptr;
}

void htable_iter_get_next_match(struct htable *table, struct htable_iter *iter)
{
	if (htable_table_is_list(table))
		iter->ptr = htable_list_iter_get_next_match(&iter->list);
	else
		iter->ptr = htable_multitable_iter_get_next_match(&iter->table);
}

#define htable_for_each(table,hash,ptr) \
	for(\
		struct htable_iter it = htable_iter_init(table, hash)\
		;NULL != (ptr = htable_iter_get_current_match(table,&it))\
		;htable_iter_get_next_match(table,&it))

#endif
