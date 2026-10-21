/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR_COMMON_H
#define HTABLE_SWAR_COMMON_H


#include <linux/types.h>

#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif

#define SWAR_SLOT_SET_SIG   0x80
#define SWAR_SLOT_EMPTY_SIG 0x00

#ifndef SWAR_HAS_U128
#if defined(__SIZEOF_INT128__)
#define SWAR_HAS_U128 1
#else
#define SWAR_HAS_U128 0
#endif
#endif

typedef u64 swar_t;
typedef u64 swar_slots_t;
typedef u32 swar_hash_t;
typedef u32 swar_bucket_t;
typedef u8 swar_slot_offset_t;
typedef u8 swar_fingerprint_t;

#if SWAR_HAS_U128
typedef unsigned __int128 swar_u128_t;
#else
typedef struct {
	u64 low;
	u64 high;
} swar_u128_t;
#endif

#endif
