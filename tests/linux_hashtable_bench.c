/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "htable_bench_common.h"
#include <linux/hashtable.h>

struct linux_bench_entry {
	u32 key;
	struct hlist_node node;
};

static DEFINE_HASHTABLE(linux_bench_table, HTABLE_BENCH_HASH_BITS);

static bool linux_hashtable_contains(u32 key)
{
	struct linux_bench_entry *entry;

	hash_for_each_possible(linux_bench_table, entry, node, key) {
		if (entry->key == key)
			return 1;
	}

	return 0;
}

static bool run_linux_hashtable_size(size_t entry_count)
{
	size_t bucket_count;
	struct linux_bench_entry *entries = NULL;
	int valid = 1;
	u64 start_ns;
	u64 insert_ns;
	u64 hit_lookup_ns;
	u64 miss_lookup_ns;
	u64 delete_ns;
	size_t hits = 0;
	size_t false_hits = 0;

	if (!htable_bench_bucket_config(entry_count, 8, &bucket_count)) {
		fprintf(stderr, "Linux hashtable benchmark: invalid size %zu\n",
			entry_count);
		return 0;
	}
	if (bucket_count != HASH_SIZE(linux_bench_table) ||
	    HASH_BITS(linux_bench_table) != HTABLE_BENCH_HASH_BITS) {
		fprintf(stderr, "Linux hashtable benchmark: bucket configuration mismatch\n");
		return 0;
	}

	hash_init(linux_bench_table);
	entries = calloc(entry_count, sizeof(*entries));
	if (!entries) {
		fprintf(stderr,
			"Linux hashtable benchmark: allocation failed for %zu entries\n",
			entry_count);
		free(entries);
		return 0;
	}

	for (size_t i = 0; i < entry_count; i++) {
		entries[i].key = (u32)i;
		INIT_HLIST_NODE(&entries[i].node);
	}

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++)
		hash_add(linux_bench_table, &entries[i].node, entries[i].key);
	insert_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++) {
		if (linux_hashtable_contains(entries[i].key))
			hits++;
	}
	hit_lookup_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++) {
		if (linux_hashtable_contains((u32)(entry_count + i)))
			false_hits++;
	}
	miss_lookup_ns = htable_bench_now_ns() - start_ns;

	start_ns = htable_bench_now_ns();
	for (size_t i = 0; i < entry_count; i++)
		hash_del(&entries[i].node);
	delete_ns = htable_bench_now_ns() - start_ns;

	if (hits != entry_count || false_hits != 0 ||
	    !hash_empty(linux_bench_table)) {
		fprintf(stderr,
			"Linux hashtable benchmark: correctness failure at size %zu "
			"(hits=%zu false_hits=%zu empty=%d)\n",
			entry_count,
			hits,
			false_hits,
			hash_empty(linux_bench_table));
		valid = 0;
	}

	htable_bench_report("linux-hashtable", entry_count, bucket_count,
			    "insert", insert_ns);
	htable_bench_report("linux-hashtable", entry_count, bucket_count,
			    "lookup-hit", hit_lookup_ns);
	htable_bench_report("linux-hashtable", entry_count, bucket_count,
			    "lookup-miss", miss_lookup_ns);
	htable_bench_report("linux-hashtable", entry_count, bucket_count,
			    "delete", delete_ns);

	free(entries);
	return valid != 0;
}

int main(void)
{
	int valid = 1;

	htable_bench_print_header();
	for (size_t i = 0;
	     i < sizeof(htable_bench_entry_counts) /
		     sizeof(htable_bench_entry_counts[0]);
	     i++) {
		if (!run_linux_hashtable_size(htable_bench_entry_counts[i]))
			valid = 0;
	}

	return valid != 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
