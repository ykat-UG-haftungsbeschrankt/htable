/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_TEST_LINUX_CONTAINER_OF_H
#define HTABLE_TEST_LINUX_CONTAINER_OF_H

#include <stddef.h>

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#define likely(value) (value)

#endif /* HTABLE_TEST_LINUX_CONTAINER_OF_H */
