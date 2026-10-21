/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_ITER_H
#define HTABLE_ITER_H

#include <stdbool.h>

struct htable_iter;

struct htable_iter_ops {
	bool (*next)(struct htable_iter *iter, const void **item);
};

struct htable_iter {
	const struct htable_iter_ops *ops;
};

static inline void htable_iter_init(struct htable_iter *iter,
				    const struct htable_iter_ops *ops)
{
	iter->ops = ops;
}

static inline bool htable_iter_next(struct htable_iter *iter,
				    const void **item)
{
	return iter->ops->next(iter, item);
}

#endif
