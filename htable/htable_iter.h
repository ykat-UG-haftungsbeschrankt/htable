/**
 * htable_iter_init - Initialize an iterator for a table or one bucket
 * @iter: Iterator to initialize
 * @table: Table being traversed
 * @possible_only: Traverse only the bucket selected by @key
 * @key: Key used when @possible_only is true
 */
static inline void htable_iter_init(struct htable_iter *iter,
				    struct htable *table, bool possible_only,
				    unsigned long key)
{
	iter->state = rcu_dereference_raw(table->state);
	iter->next_link = NULL;
	if (!iter->state) {
		iter->bucket = 0;
		iter->end_bucket = 0;
		return;
	}

	if (possible_only) {
		iter->bucket = htable_hash(key, iter->state->hash_bits);
		iter->end_bucket = iter->bucket + 1;
	} else {
		iter->bucket = 0;
		iter->end_bucket = iter->state->size;
	}
}

static inline struct hlist_node *
htable_iter_next(struct htable_iter *iter, unsigned int *bucket)
{
	while (iter->state && iter->bucket < iter->end_bucket) {
		if (!iter->next_link) {
			struct hlist_node *first = rcu_dereference_raw(
				iter->state->buckets[iter->bucket].first);

			if (!first) {
				iter->bucket++;
				continue;
			}
			iter->next_link =
				hlist_entry(first, struct htable_link, bucket_node);
		}

		{
			struct htable_link *link = iter->next_link;
			struct hlist_node *next =
				rcu_dereference_raw(link->bucket_node.next);

			iter->next_link = next ?
				hlist_entry(next, struct htable_link, bucket_node) :
				NULL;
			*bucket = iter->bucket;
			return link->entry_node;
		}
	}

	return NULL;
}

static inline struct hlist_node *
htable_iter_next_safe(struct htable_iter *iter, unsigned int *bucket,
		      struct hlist_node **tmp)
{
	struct hlist_node *node = htable_iter_next(iter, bucket);

	*tmp = iter->next_link ? iter->next_link->entry_node : NULL;
	return node;
}

/* Iteration requires an RCU read lock or external exclusion from writers. */
#define __htable_for_each(name, bucket, obj, member, possible, key)	\
	for (struct htable_iter __iter,					\
	     *__iter_once = (htable_iter_init(&__iter, &name##_htable,	\
					 possible, (unsigned long)(key)),	\
			     &__iter);					\
	     __iter_once; __iter_once = NULL)				\
		for (struct hlist_node *__node =				\
			     htable_iter_next(&__iter, &(bucket));		\
		     __node && ((obj) = container_of(__node, typeof(*(obj)),	\
						       member), 1);	\
		     __node = htable_iter_next(&__iter, &(bucket)))

#define hash_for_each(name, bucket, obj, member) \
	__htable_for_each(name, bucket, obj, member, false, 0)
#define hash_for_each_rcu(name, bucket, obj, member) \
	__htable_for_each(name, bucket, obj, member, false, 0)
#define hash_for_each_safe(name, bucket, tmp, obj, member)		\
	for (struct htable_iter __iter,					\
	     *__iter_once = (htable_iter_init(&__iter, &name##_htable,	\
					 false, 0), &__iter);		\
	     __iter_once; __iter_once = NULL)				\
		for (struct hlist_node *__node =				\
			     htable_iter_next_safe(&__iter, &(bucket),	\
						   &(tmp));		\
		     __node && ((obj) = container_of(__node,		\
						      typeof(*(obj)),	\
						      member), 1);	\
		     __node = htable_iter_next_safe(&__iter, &(bucket),	\
						    &(tmp)))
#define hash_for_each_possible(name, obj, member, key)			\
	for (struct htable_iter __iter,					\
	     *__iter_once = (htable_iter_init(&__iter, &name##_htable,	\
					 true, (unsigned long)(key)),		\
			     &__iter);					\
	     __iter_once; __iter_once = NULL)				\
		for (unsigned int __bucket = 0; __bucket == 0; __bucket++)	\
			for (struct hlist_node *__node =			\
				     htable_iter_next(&__iter, &__bucket);	\
			     __node && ((obj) = container_of(__node,		\
							      typeof(*(obj)),	\
							      member), 1);	\
			     __node = htable_iter_next(&__iter, &__bucket))

#define hash_for_each_possible_rcu(name, obj, member, key, cond...)	\
	hash_for_each_possible(name, obj, member, key)
#define hash_for_each_possible_rcu_notrace(name, obj, member, key)	\
	hash_for_each_possible(name, obj, member, key)
#define hash_for_each_possible_safe(name, obj, tmp, member, key)	\
	for (struct htable_iter __iter,					\
	     *__iter_once = (htable_iter_init(&__iter, &name##_htable,	\
					 true, (unsigned long)(key)),	\
			     &__iter);					\
	     __iter_once; __iter_once = NULL)				\
		for (unsigned int __bucket = 0; __bucket == 0; __bucket++)	\
			for (struct hlist_node *__node =			\
				     htable_iter_next_safe(&__iter,		\
							   &__bucket,	\
							   &(tmp));	\
			     __node && ((obj) = container_of(__node,		\
							      typeof(*(obj)),	\
							      member), 1);	\
			     __node = htable_iter_next_safe(&__iter,		\
							    &__bucket,	\
							    &(tmp)))

#endif /* _KMOD_HTABLE_H */
