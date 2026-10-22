/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR128_H
#define HTABLE_SWAR128_H

#include "htable_types.h"
#include "swar64.h"

/**
 * swar_get_hash128 - Calculate a CRC32C hash from a 128-bit key
 * @key: Key used as the CRC32C seed and input value
 * @returns: CRC32C result produced by two 64-bit CRC instructions
 */
static __always_inline swar_hash_t swar_get_hash128(swar_u128_t key)
{
	u64 low;
	u64 high;

#if SWAR_HAS_U128
	low = (u64)key;
	high = (u64)(key >> 64);
#else
	low = key.low;
	high = key.high;
#endif
	return _mm_crc32_u64(swar_get_hash64(low), high);
}

/**
 * swar_match128 - Find matching bytes in a 128-bit value
 * @values: Sixteen packed bytes
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching byte
 */
static __always_inline swar_u128_t swar_match128(swar_u128_t values, u8 target)
{
#if SWAR_HAS_U128
	const swar_u128_t byte_ones =
		((swar_u128_t)0x0101010101010101ULL << 64) |
		0x0101010101010101ULL;
	const swar_u128_t byte_low_bits =
		((swar_u128_t)0x7F7F7F7F7F7F7F7FULL << 64) |
		0x7F7F7F7F7F7F7F7FULL;
	const swar_u128_t byte_high_bits =
		((swar_u128_t)0x8080808080808080ULL << 64) |
		0x8080808080808080ULL;
	swar_u128_t xor_result = values ^ (byte_ones * target);

	return ~(((xor_result & byte_low_bits) + byte_low_bits) | xor_result) &
	       byte_high_bits;
#else
	swar_u128_t matches = {
		.low = swar_match64(values.low, target),
		.high = swar_match64(values.high, target),
	};

	return matches;
#endif
}

/**
 * swar_find_first_set128 - Find the first byte equal to a pattern
 * @values: Sixteen packed bytes
 * @pattern: Byte pattern to match
 * @returns: Byte index, or sixteen if no byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_set128(
	swar_u128_t values, u8 pattern)
{
	u64 low;
	u64 high;
	u64 matches;

#if SWAR_HAS_U128
	low = (u64)values;
	high = (u64)(values >> 64);
#else
	low = values.low;
	high = values.high;
#endif
	matches = swar_match64(low, pattern);
	if (matches)
		return __builtin_ctzll(matches) >> 3;
	matches = swar_match64(high, pattern);
	return matches ? 8 + (__builtin_ctzll(matches) >> 3) : 16;
}

/**
 * swar_find_first_zero128 - Find the first byte not equal to a pattern
 * @values: Sixteen packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Byte index, or sixteen if every byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_zero128(
	swar_u128_t values, u8 pattern)
{
	u64 low;
	u64 high;
	u64 nonmatches;

#if SWAR_HAS_U128
	low = (u64)values;
	high = (u64)(values >> 64);
#else
	low = values.low;
	high = values.high;
#endif
	nonmatches = 0x8080808080808080ULL & ~swar_match64(low, pattern);
	if (nonmatches)
		return __builtin_ctzll(nonmatches) >> 3;
	nonmatches = 0x8080808080808080ULL & ~swar_match64(high, pattern);
	return nonmatches ? 8 + (__builtin_ctzll(nonmatches) >> 3) : 16;
}

/**
 * swar_get_count_set128 - Count bytes equal to a pattern
 * @values: Sixteen packed bytes
 * @pattern: Byte pattern to match
 * @returns: Number of matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_set128(
	swar_u128_t values, u8 pattern)
{
	u64 low;
	u64 high;

#if SWAR_HAS_U128
	low = (u64)values;
	high = (u64)(values >> 64);
#else
	low = values.low;
	high = values.high;
#endif
	return swar_get_count_set64(low, pattern) +
	       swar_get_count_set64(high, pattern);
}

/**
 * swar_get_count_zero128 - Count bytes not equal to a pattern
 * @values: Sixteen packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Number of non-matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_zero128(
	swar_u128_t values, u8 pattern)
{
	return 16 - swar_get_count_set128(values, pattern);
}

#endif
