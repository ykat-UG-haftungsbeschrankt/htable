/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "htable/htable_types.h"
HLIST_HEAD(__htable_index);
#include "htable/htable_index.h"
#include "htable/htable_list.h"
#include "htable/htable_list_iter.h"
#include "htable/htable_stats.h"

static void test_stats_initializer(void)
{
	struct htable_stats stats = HTABLE_STATS_INIT(test_stats);

	assert(stats.name != NULL);
	assert(stats.entries == 0);
}

static void test_stats_average(void)
{
	struct htable_stats stats;
	usize average = 99;

	htable_stats_init(&stats, "average-test");
	assert(!htable_stats_get_average_size(&stats, &average));
	assert(average == 99);

	htable_stats_on_add(&stats);
	assert(htable_stats_get_average_size(&stats, &average));
	assert(average == 1);
}

static void test_index_lifecycle(void)
{
	struct htable_index_node index = HTABLE_INDEX_NODE_INIT;

	__htable_index_node_init(&index);
	assert(!__htable_index_node_is_added(&index));
	__htable_index_node_deinit(&index);
	assert(!__htable_index_node_is_added(&index));

	htable_index_add(&index);
	assert(__htable_index_node_is_added(&index));
	htable_index_del(&index);
	assert(!__htable_index_node_is_added(&index));

	htable_index_add(&index);
	assert(__htable_index_node_is_added(&index));
	__htable_index_node_deinit(&index);
	assert(!__htable_index_node_is_added(&index));
}

void test_htable_list(void);
void test_swar(void);

static void test_list_for_each_macros(void)
{
	struct htable_list list;
	struct htable_list_node nodes[4] = { 0 };
	const size_t expected[] = { 3, 2, 0 };
	size_t count = 0;

	htable_list_init(&list);
	for (size_t i = 0; i < 4; i++) {
		htable_list_node_init(&nodes[i]);
		nodes[i].hash = i == 1 ? 9 : 7;
		htable_list_add(&list, &nodes[i]);
	}

	htable_list_for_each(&list, 7, it) {
		assert(it.node == &nodes[expected[count]]);
		count++;
	}
	assert(count == 3);

	count = 0;
	htable_list_for_each(&list, 11, it)
		count++;
	assert(count == 0);
}

int main(void)
{
	test_stats_initializer();
	test_stats_average();
	test_index_lifecycle();
	test_list_for_each_macros();
	test_htable_list();
	test_swar();
	puts("htable tests passed");
	return 0;
}