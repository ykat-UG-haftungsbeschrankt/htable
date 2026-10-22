#ifndef HTABLE_TYPES_H
#define HTABLE_TYPES_H

#undef __KERNEL__

# ifdef __KERNEL__
#include <linux/types.h>
#include <linux/container_of.h>
#include <linux/build_bug.h>
#include <linux/list.h>
#include <linux/stddef.h>
#include <linux/container_of.h>
#include <linux/stringify.h>
# else

#  include <stdint.h>
#  include <stdbool.h>
typedef uint64_t usize;
typedef int64_t cycles_t;
typedef int64_t s64;
typedef uint64_t u64;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint8_t u8;
typedef int8_t s8;
typedef bool b;

#  define EXPORT_SYMBOL(x)
#  define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#  define likely(x)	__builtin_expect(!!(x), 1)
#  define unlikely(x)	__builtin_expect(!!(x), 0)
#  define offsetof(type, member) __builtin_offsetof(type, member)
#  define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#  define __stringify(x) #x
#  define __stringify_1(x) #x
#  define stringify(x) __stringify(x)
#  define NULL ((void*)0)
#  define BUILD_BUG_ON(condition) \
	_Static_assert(!(condition), "BUILD_BUG_ON failed: " #condition)
#  define htable_malloc(size) malloc(size)
#  define htable_free(ptr) free(ptr)
#  define POISON_POINTER_DELTA   0
#  define LIST_POISON1  ((void *) 0x100 + POISON_POINTER_DELTA)
#  define LIST_POISON2  ((void *) 0x122 + POISON_POINTER_DELTA)
#  define WRITE_ONCE(var, val) ((var) = (val))


struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#  define HLIST_HEAD_INIT { .first = NULL }
#  define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#  define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}
#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos ; pos = pos->next)

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}
static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;

	WRITE_ONCE(*pprev, next);
	if (next)
		WRITE_ONCE(next->pprev, pprev);
}

/**
 * hlist_del - Delete the specified hlist_node from its list
 * @n: Node to delete.
 *
 * Note that this function leaves the node in hashed state.  Use
 * hlist_del_init() or similar instead to unhash @n.
 */
static inline void hlist_del(struct hlist_node *n)
{
	__hlist_del(n);
	n->next = LIST_POISON1;
	n->pprev = LIST_POISON2;
}

# endif

# ifndef __always_inline
#  define __always_inline inline __attribute__((always_inline))
# endif

#endif /* HTABLE_TYPES_H */