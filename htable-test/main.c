/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <stdio.h>

#include "htable/htable_list.h"
#include "htable/htable_list_iter.h"
#include "htable/htable_multitable_iter.h"

struct test_entry {
	int value;
	struct htable_list_node node;
};

static void init_entry(struct test_entry *entry, int value)
{
	entry->value = value;
	htable_list_node_init(&entry->node);
}

static void test_empty_and_single_node(void)
{
	struct htable_list list;
	struct test_entry entry;

	htable_list_init(&list);
	assert(list.tail == NULL);
	assert(htable_list_last(&list) == NULL);

	init_entry(&entry, 1);
	htable_list_add(&list, &entry.node);
	assert(htable_list_last(&list) == &entry.node);
	assert(entry.node.pprev == &list.sentinel);
	assert(htable_list_get_by_node(&entry.node) == &list);

	htable_list_delete(&entry.node);
	assert(list.tail == NULL);
	assert(entry.node.pprev == NULL);
}

static void test_middle_head_and_tail_deletion(void)
{
	struct htable_list list;
	struct test_entry entries[4];

	htable_list_init(&list);
	for (int i = 0; i < 4; i++) {
		init_entry(&entries[i], i);
		htable_list_add(&list, &entries[i].node);
	}

	assert(htable_list_last(&list) == &entries[3].node);
	for (int i = 0; i < 4; i++) {
		assert(htable_list_get_by_node(&entries[i].node) == &list);
		assert(entries[i].node.pprev ==
		       (i == 0 ? &list.sentinel : &entries[i - 1].node));
	}

	htable_list_delete(&entries[1].node);
	assert(entries[2].node.pprev == &entries[0].node);
	assert(entries[1].node.pprev == NULL);
	assert(htable_list_last(&list) == &entries[3].node);

	htable_list_delete(&entries[0].node);
	assert(entries[2].node.pprev == &list.sentinel);
	assert(entries[0].node.pprev == NULL);

	htable_list_delete(&entries[3].node);
	assert(htable_list_last(&list) == &entries[2].node);
	assert(entries[2].node.pprev == &list.sentinel);

	htable_list_delete(&entries[2].node);
	assert(htable_list_last(&list) == NULL);
}

static void test_independent_lists(void)
{
	struct htable_list lists[2];
	struct test_entry entries[2];

	for (int i = 0; i < 2; i++) {
		htable_list_init(&lists[i]);
		init_entry(&entries[i], i);
		htable_list_add(&lists[i], &entries[i].node);
		assert(htable_list_get_by_node(&entries[i].node) == &lists[i]);
	}

	htable_list_delete(&entries[0].node);
	assert(lists[0].tail == NULL);
	assert(lists[1].tail == &entries[1].node);
	assert(htable_list_get_by_node(&entries[1].node) == &lists[1]);
}

static void test_list_iterator(void)
{
	struct htable_list list;
	struct htable_list_iter iter;
	struct test_entry entries[3];
	const void *item;

	htable_list_init(&list);
	for (int i = 0; i < 3; i++) {
		init_entry(&entries[i], i);
		htable_list_add(&list, &entries[i].node);
	}

	htable_list_iter_init(&iter, &list);
	assert(htable_iter_next(&iter.base, &item));
	assert(item == &entries[2].node);
	assert(htable_iter_next(&iter.base, &item));
	assert(item == &entries[1].node);
	assert(htable_iter_next(&iter.base, &item));
	assert(item == &entries[0].node);
	assert(!htable_iter_next(&iter.base, &item));
}

static void test_multitable_iterator(void)
{
	FilteredMultiTable *table = multitable_create(1);
	struct htable_multitable_iter iter;
	int objects[5];
	const void *item;
	size_t count = 0;
	int found;

	assert(table != NULL);
	for (size_t i = 0; i < 5; i++)
		assert(multitable_insert(table, &objects[i], 0x11 + i));

	htable_multitable_iter_init(&iter, table);
	while (htable_iter_next(&iter.base, &item)) {
		found = 0;
		for (size_t i = 0; i < 5; i++)
			if (item == &objects[i])
				found = 1;
		assert(found);
		count++;
	}
	assert(count == 5);
	assert(!htable_iter_next(&iter.base, &item));
	multitable_free(table);
}

int main(void)
{
	test_empty_and_single_node();
	test_middle_head_and_tail_deletion();
	test_independent_lists();
	test_list_iterator();
	test_multitable_iterator();
	puts("htable iterator tests passed");
	return 0;
}
