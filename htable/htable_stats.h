struct htable_stats {
	const char           *name;
	usize                entries;
	usize                size;
	cycles_t             cycles;
	usize                average;
	usize                add;
	usize                remove;
	usize                resizes;
	usize                lookups;
	usize                collisions;
};

#define HTABLE_STATS_INIT {						\
	.name = __stringify_1(__FILE__)##":"##__stringify_1(__LINE__)##":"#name, \
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

//////////////////////////////////////////////////////////////////////////
// Htable statistics functions
//////////////////////////////////////////////////////////////////////////

void htable_stats_on_add(struct htable *table){
	usize entries = ++table->stats.entries;
	table->stats.average += entries;
	table->stats.add++;
}

void htable_stats_on_del(struct htable *table){
	usize entries = --table->stats.entries;
	table->stats.average += entries;
	table->stats.remove++;
}

usize htable_stats_get_average_size(struct htable *table){
	return table->stats.average / (table->stats.add + table->stats.remove);
}

void htable_stats_on_collision(struct htable *table){
	table->stats.collisions++;
}

void htable_stats_on_resize(struct htable *table){
	table->stats.resizes++;
}

void htable_stats_on_lookup(struct htable *table){
	table->stats.lookups++;
}