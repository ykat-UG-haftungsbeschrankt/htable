/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "htable/htable_types.h"

HLIST_HEAD(__htable_index);

#include "htable/htable.h"
#include "htable/htable_iter.h"
#include "htable_bench_common.h"

struct htable_bench_entry {
	u32 key;
	struct htable_node node;
};

static bool htable_bench_contains(
	struct htable *table,
	u32 key,
	unsigned int hash_bits
){
	u32 hash = htable_bench_hash_32(key, hash_bits);
	struct htable_list_node *match;

	htable_for_each(table, hash, match) {
		struct htable_node *node =
			htable_node_from_htable_list_node(match);
		struct htable_bench_entry *entry =
			container_of(node, struct htable_bench_entry, node);

		if (entry->key == key)
			return true;
	}

	return false;
}

static bool run_htable_size(size_t entry_count)
{
	const size_t entries_per_bucket = 8;
	size_t bucket_count;
	size_t total_entries;
	const unsigned int hash_bits = HTABLE_BENCH_HASH_BITS;
	usize buffer_size;
	void *buffer = NULL;
	struct htable_bench_entry *entries = NULL;
	struct htable table = HTABLE_INIT(benchmark_htable);
	bool initialized = false;
	bool valid = true;
	uint64_t start_ns;
	uint64_t insert_ns;
	uint64_t hit_lookup_ns;
	uint64_t miss_lookup_ns;
	uint64_t delete_ns;
	size_t hits = 0;
	size_t false_hits = 0;

	if (!htable_bench_bucket_config(
		    entry_count, entries_per_bucket, &bucket_count) ||
	    bucket_count > SIZE_MAX / entries_per_bucket) {
		fprintf(stderr, "htable benchmark: invalid size %zu\n", entry_count);
		return false;
	}
	total_entries = bucket_count * entries_per_bucket;
	if (!htable_table_get_buffer_size(
		    entries_per_bucket, total_entries, &buffer_size)) {
		fprintf(stderr, "htable benchmark: buffer size overflow\n");
		return false;
	}

	entries = calloc(entry_count, sizeof(*entries));
	buffer = malloc(buffer_size);
	if (!entries || !buffer) {
		fprintf(stderr, "htable benchmark: allocation failed for %zu entries\n",
			entry_count);
		valid = false;
		goto cleanup;
	}

	for (size_t i = 0; i < entry_count; i++)
		entries[i].key = (u32)i;

	if (!hash_init(&table, buffer, buffer_size,
		       entries_per_bucket, total_entries)) {
		fprintf(stderr, "htable benchmark: table initialization failed\n");
		htable_deinit(&table);
		valid = false;
		goto cleanup;
	}
	initialized = true;
	__htable_set_type_table(&table);

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++) {
		u32 hash = htable_bench_hash_32(entries[i].key, hash_bits);

		hash_add(&table, &entries[i].node.hnode, hash);
	}
	insert_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++) {
		if (htable_bench_contains(&table, entries[i].key, hash_bits))
			hits++;
	}
	hit_lookup_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++) {
		if (htable_bench_contains(
			    &table, (u32)(entry_count + i), hash_bits))
			false_hits++;
	}
	miss_lookup_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++)
		hash_del(&table, &entries[i].node.hnode);
	delete_ns = htable_bench_now_ns() - start_ns;

	if (hits != entry_count || false_hits != 0 || !hash_empty(&table)) {
		fprintf(stderr,
			"htable benchmark: correctness failure at size %zu "
			"(hits=%zu false_hits=%zu empty=%d)\n",
			entry_count, hits, false_hits, hash_empty(&table));
		valid = false;
	}

	htable_deinit(&table);
	initialized = false;
	htable_bench_report("htable-table", entry_count, bucket_count,
			    "insert", insert_ns);
	htable_bench_report("htable-table", entry_count, bucket_count,
			    "lookup-hit", hit_lookup_ns);
	htable_bench_report("htable-table", entry_count, bucket_count,
			    "lookup-miss", miss_lookup_ns);
	htable_bench_report("htable-table", entry_count, bucket_count,
			    "delete", delete_ns);

cleanup:
	if (initialized)
		htable_deinit(&table);
	free(buffer);
	free(entries);
	return valid;
}

int main(void)
{
	bool valid = true;

	htable_bench_print_header();
	for (size_t i = 0;
	     i < sizeof(htable_bench_entry_counts) /
		     sizeof(htable_bench_entry_counts[0]);
	     i++) {
		if (!run_htable_size(htable_bench_entry_counts[i]))
			valid = false;
	}

	return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
