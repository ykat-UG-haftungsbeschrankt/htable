/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <assert.h>

#include "htable/htable_multitable.h"

struct test_entry {
	int value;
	struct htable_list_node node;
};

static void init_entry(struct test_entry *entry, int value)
{
	entry->value = value;
	htable_list_node_init(&entry->node);
}

static void test_multitable_hash_lists(void)
{
	FilteredMultiTable *table = multitable_create(5, 16, 0.75, 1.0 / 1024.0);
	struct test_entry entries[5];

	assert(table != NULL);
	for (size_t i = 0; i < 5; i++) {
		init_entry(&entries[i], i);
		assert(multitable_insert(table, &entries[i].node, 0x11));
	}
	assert(multitable_insert(table, &entries[0].node, 0x11));

	assert(multitable_delete(table, &entries[2].node, 0x11));
	assert(!multitable_has(table, &entries[2].node, 0x11));
	assert(entries[2].node.pprev == NULL);
	assert(multitable_has(table, &entries[1].node, 0x11));
	assert(multitable_insert(table, &entries[2].node, 0x11));

	multitable_free(table);
	for (size_t i = 0; i < 5; i++)
		assert(entries[i].node.pprev == NULL);
}

static void test_multitable_bucket_collisions(void)
{
	enum { VALUE_COUNT = 17 };
	FilteredMultiTable *table =
		multitable_create(VALUE_COUNT, 16, 0.75, 1.0 / 1024.0);
	struct test_entry entries[VALUE_COUNT];

	assert(table != NULL);
	assert(table->num_levels == 2);
	assert(table->levels[0].num_buckets == 1);
	assert(table->levels[1].num_buckets == 1);
	for (size_t i = 0; i < VALUE_COUNT; i++) {
		init_entry(&entries[i], i);
		assert(multitable_insert(table, &entries[i].node,
					 0x1000u + (uint32_t)i));
	}

	for (size_t i = 0; i < VALUE_COUNT; i++)
		assert(multitable_has(table, &entries[i].node,
				      0x1000u + (uint32_t)i));
	assert(!multitable_has(table, &entries[0].node, 0x11000u));

	assert(multitable_delete(table, &entries[3].node, 0x1003u));
	assert(!multitable_has(table, &entries[3].node, 0x1003u));
	assert(multitable_insert(table, &entries[3].node, 0x1003u));

	multitable_free(table);
	for (size_t i = 0; i < VALUE_COUNT; i++)
		assert(entries[i].node.pprev == NULL);
}

void test_htable_table(void)
{
	test_multitable_hash_lists();
	test_multitable_bucket_collisions();
}