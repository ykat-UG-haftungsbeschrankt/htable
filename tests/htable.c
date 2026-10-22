/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "htable/htable_list.h"

typedef uint32_t u32;

#define _KMOD_htable_table_H
struct htable_table_node {
	u32 hash;
};

struct htable_table {
	struct htable_table_node *node;
};

static inline struct htable_table_node *
htable_table_lookup_rcu(struct htable_table *table, u32 hash)
{
	return table->node && table->node->hash == hash ? table->node : NULL;
}

struct htable {
	struct htable_list list;
	struct htable_table table;
	bool list_backend;
};

bool __htable_is_type_list(struct htable *table)
{
	return table->list_backend;
}

#include "htable/htable_iter.h"

void test_htable_list(void);
void test_htable_table(void);
void test_swar(void);

static void test_list_for_each_macros(void)
{
	struct htable table = { .list_backend = true };
	struct htable_list_node nodes[4] = { 0 };
	struct htable_list_node *node;
	const size_t expected[] = { 3, 2, 0 };
	size_t count = 0;

	htable_list_init(&table.list);
	for (size_t i = 0; i < 4; i++) {
		htable_list_node_init(&nodes[i]);
		nodes[i].hash = i == 1 ? 9 : 7;
		htable_list_add(&table.list, &nodes[i]);
	}

	htable_list_for_each(&table.list, 7, node) {
		assert(node == &nodes[expected[count]]);
		count++;
	}
	assert(count == 3);

	count = 0;
	htable_for_each(&table, 7, node) {
		assert(node == &nodes[expected[count]]);
		count++;
	}
	assert(count == 3);

	count = 0;
	htable_for_each(&table, 11, node)
		count++;
	assert(count == 0);

	count = 0;
	htable_list_for_each(&table.list, 11, node)
		count++;
	assert(count == 0);
}

static void test_table_for_each_macros(void)
{
	struct htable_table_node match = { .hash = 42 };
	struct htable table = {
		.table = { .node = &match },
		.list_backend = false,
	};
	struct htable_table_node *node;
	size_t count = 0;

	htable_for_each(&table, 42, node) {
		assert(node == &match);
		count++;
	}
	assert(count == 1);

	count = 0;
	htable_for_each(&table, 24, node)
		count++;
	assert(count == 0);

	count = 0;
	htable_multitable_for_each(&table.table, 42, node) {
		assert(node == &match);
		count++;
	}
	assert(count == 1);

	count = 0;
	htable_multitable_for_each(&table.table, 24, node)
		count++;
	assert(count == 0);
}

int main(void)
{
	test_list_for_each_macros();
	test_table_for_each_macros();
	test_htable_list();
	test_htable_table();
	test_swar();
	puts("htable tests passed");
	return 0;
}