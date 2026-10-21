/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_TEST_LINUX_CONTAINER_OF_H
#define HTABLE_TEST_LINUX_CONTAINER_OF_H

#include <stddef.h>

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#define likely(value) (value)

#endif /* HTABLE_TEST_LINUX_CONTAINER_OF_H */
