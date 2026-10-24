/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_BENCH_COMMON_H
#define HTABLE_BENCH_COMMON_H

#include <stdio.h>
#include <stdlib.h>

#ifndef __KERNEL__
#include <inttypes.h>
#include <time.h>
#else
struct htable_bench_timespec {
	long tv_sec;
	long tv_nsec;
};
extern int clock_gettime(int clock_id, void *timestamp);
#define HTABLE_BENCH_CLOCK_MONOTONIC 1
#endif

#include "htable/htable_types.h"

#define HTABLE_BENCH_HASH_BITS 7U
#define HTABLE_BENCH_BUCKET_COUNT \
	((size_t)1U << HTABLE_BENCH_HASH_BITS)

static const size_t htable_bench_entry_counts[] = {
	1000,
	10000,
	100000,
};

static inline bool htable_bench_bucket_config(
	size_t entry_count,
	size_t entries_per_bucket,
	size_t *bucket_count
){
	if (entry_count == 0 || entries_per_bucket == 0)
		return 0;
	if (entries_per_bucket >
	    (size_t)-1 / HTABLE_BENCH_BUCKET_COUNT)
		return 0;
	*bucket_count = HTABLE_BENCH_BUCKET_COUNT;
	return 1;
}

/*
 * Userspace equivalent of Linux hash_32(key, bits).
 */
static inline u32 htable_bench_hash_32(u32 key, unsigned int bits)
{
	u32 product = key * 0x61C88647U;

	if (bits == 0)
		return 0;
	if (bits >= 32)
		return product;

	return product >> (32U - bits);
}

static inline u64 htable_bench_now_ns(void)
{
#ifdef __KERNEL__
	struct htable_bench_timespec timestamp;

	if (clock_gettime(HTABLE_BENCH_CLOCK_MONOTONIC, &timestamp) != 0) {
#else
	struct timespec timestamp;

	if (clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
#endif
		perror("clock_gettime");
		exit(EXIT_FAILURE);
	}

#ifdef __KERNEL__
	return (u64)timestamp.tv_sec * 1000000000ULL +
	       (u64)timestamp.tv_nsec;
#else
	return (u64)timestamp.tv_sec * UINT64_C(1000000000) +
	       (u64)timestamp.tv_nsec;
#endif
}

static inline void htable_bench_print_header(void)
{
	puts("backend,entries,buckets,operation,total_ns,ns_per_op");
}

static inline void htable_bench_report(
	const char *backend,
	size_t entries,
	size_t buckets,
	const char *operation,
	u64 elapsed_ns
){
	double ns_per_op =
		entries != 0 ? (double)elapsed_ns / (double)entries : 0.0;

	printf("%s,%zu,%zu,%s,%llu,%.3f\n",
	       backend,
	       entries,
	       buckets,
	       operation,
	       (unsigned long long)elapsed_ns,
	       ns_per_op);
}

#endif
