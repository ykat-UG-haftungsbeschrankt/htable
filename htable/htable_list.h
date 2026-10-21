/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KMOD_htable_list_H
#define _KMOD_htable_list_H

#include <stdbool.h>

#include <linux/container_of.h>

/**
 * struct htable_list - Predecessor chain with a sentinel
 * @sentinel: Root node; its pprev is NULL
 * @tail: Last entry, or NULL when the list is empty
 *
 * Entries must not already belong to a list, and all access must be externally
 * synchronized. Traversal from an entry to its predecessor follows pprev.
 */
struct htable_list_node {
	unsigned int hash;
	struct htable_list_node *pprev;
};

struct htable_list {
	struct htable_list_node sentinel;
	struct htable_list_node *tail;
};

#define HTABLE_LIST_INIT {						\
	.sentinel = { .pprev = NULL },					\
	.tail = NULL,							\
}

static inline void htable_list_node_init(struct htable_list_node *node){
	node->pprev = NULL;
}

/**
 * htable_list_init - Initialize an empty sentinel list
 * @list: List to initialize
 */
static inline void htable_list_init(
	struct htable_list *list
){
	list->sentinel.pprev = NULL;
	list->tail = NULL;
}

static inline void
htable_list_add(
	struct htable_list *list
	, struct htable_list_node *node
){
	register struct htable_list_node *tail = list->tail;
	node->pprev = likely(tail) ? tail : &list->sentinel;
	list->tail = node;
}

static inline struct htable_list_node *
htable_list_last(
	struct htable_list *list
){
	return list->tail;
}

static inline struct htable_list *htable_list_get_by_node(
	struct htable_list_node *node
){
	while (likely(node->pprev))
		node = node->pprev;

	return container_of(node, struct htable_list, sentinel);
}

static inline void
__htable_list_delete(
	struct htable_list *list
	, struct htable_list_node *node
){
	struct htable_list_node *successor;

	if (likely(list->tail != node)) {
		/*
		 * Links point toward the sentinel, so find the node whose
		 * predecessor is being removed.
		 */
		successor = list->tail;
		while (likely(successor->pprev != node))
			successor = successor->pprev;
		successor->pprev = node->pprev;
	} else {
		list->tail = likely(node->pprev != &list->sentinel) ?
			node->pprev : NULL;
	}

	node->pprev = NULL;
}

static inline void
htable_list_delete(
	struct htable_list_node *node
){
	struct htable_list *list = htable_list_get_by_node(node);
	__htable_list_delete(list, node);
}

static inline bool
htable_list_has(
	struct htable_list *list,
	unsigned int hash
){
	struct htable_list_node *node = list->tail;
	while (likely(node != &list->sentinel)) {
		if (node->hash == hash)
			return true;
		node = node->pprev;
	}
	return false;
}

static inline struct htable_list_node *
htable_list_get_first_match(
	struct htable_list *list
	,unsigned int hash
){
	struct htable_list_node *current = list->tail;
	while (likely(current != &list->sentinel)) {
		if (current->hash == hash)
			return current;
		current = current->pprev;
	}
	return NULL;
}

static inline struct htable_list_node *
htable_list_get_next_match(
	struct htable_list_node *node
	,unsigned int hash
){
	if(node){
		struct htable_list_node *current = node->pprev;
		while (likely(current != &htable_list_get_by_node(node)->sentinel)) {
			if (current->hash == hash)
				return current;
			current = current->pprev;
		}
	}
	return NULL;
}

#endif /* _KMOD_htable_list_H */