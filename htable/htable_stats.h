/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_STATS_H
#define HTABLE_STATS_H

#include "htable_types.h"

struct htable_stats {
	const char           *name;
	usize                entries;
	usize                size;
	s64                  cycles;
	usize                average;
	usize                add;
	usize                remove;
	usize                resizes;
	usize                lookups;
	usize                collisions;
};

#define HTABLE_STATS_INIT(table_name) {					\
	.name = __FILE__ ":" __stringify(__LINE__) ":" #table_name,		\
	.entries = 0,							\
	.size = 0,							\
	.cycles = 0,							\
	.average = 0,							\
	.add = 0,							\
	.remove = 0,							\
	.resizes = 0,							\
	.lookups = 0,							\
	.collisions = 0,						\
}

static inline void htable_stats_init(struct htable_stats *stats, const char *name){
	stats->name = name;
	stats->entries = 0;
	stats->size = 0;
	stats->cycles = 0;
	stats->average = 0;
	stats->add = 0;
	stats->remove = 0;
	stats->resizes = 0;
	stats->lookups = 0;
	stats->collisions = 0;
}

static inline void htable_stats_deinit(struct htable_stats *stats){
	stats->name = (const char *)NULL;
	stats->entries = 0;
	stats->size = 0;
	stats->cycles = 0;
	stats->average = 0;
	stats->add = 0;
	stats->remove = 0;
	stats->resizes = 0;
	stats->lookups = 0;
	stats->collisions = 0;
}

static inline void htable_stats_on_add(struct htable_stats *stats){
	usize entries = ++stats->entries;
	stats->average += entries;
	stats->add++;
}

static inline void htable_stats_on_del(struct htable_stats *stats){
	usize entries = --stats->entries;
	stats->average += entries;
	stats->remove++;
}

static inline bool htable_stats_get_average_size(
	const struct htable_stats *stats,
	usize *average
){
	usize operation_count;

	if (!stats || !average ||
	    stats->add > (usize)-1 - stats->remove)
		return false;

	operation_count = stats->add + stats->remove;
	if (operation_count == 0)
		return false;

	*average = stats->average / operation_count;
	return true;
}

static inline void htable_stats_on_collision(struct htable_stats *stats){
	stats->collisions++;
}

static inline void htable_stats_on_resize(struct htable_stats *stats){
	stats->resizes++;
}

static inline void htable_stats_on_lookup(struct htable_stats *stats){
	stats->lookups++;
}

#endif /* HTABLE_STATS_H */