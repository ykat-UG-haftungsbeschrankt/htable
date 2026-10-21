/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR64_H
#define HTABLE_SWAR64_H

#include "swar_common.h"

/**
 * swar_get_hash64 - Calculate a CRC32C hash from a 64-bit key
 * @key: Key used as both the CRC32C seed and input value
 * @returns: CRC32C result produced by the 64-bit CRC instruction
 */
static __always_inline swar_hash_t swar_get_hash64(u64 key)
{
	return _mm_crc32_u64(key, key);
}

/**
 * swar_get_bucket64 - Remove the seven fingerprint bits from a 64-bit hash
 * @key: Hash whose low seven bits contain the fingerprint
 * @returns: Hash shifted right by seven bits
 */
static __always_inline u64 swar_get_bucket64(u64 key)
{
	return key >> 7;
}

/**
 * swar_get_key64 - Extract the fingerprint from a 64-bit hash
 * @key: Hash containing the fingerprint in its low seven bits
 * @returns: Low seven bits of @key
 */
static __always_inline swar_fingerprint_t swar_get_key64(u64 key)
{
	return key & 0x7F;
}

/**
 * swar_match64 - Find matching bytes in a 64-bit value
 * @values: Eight packed bytes
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching byte
 */
static __always_inline u64 swar_match64(u64 values, u8 target)
{
	return swar_match_u8(values, target, 8);
}

/**
 * swar_find_first_set64 - Find the first byte equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to match
 * @returns: Byte index, or eight if no byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_set64(u64 values,
								 u8 pattern)
{
	u64 matches = swar_match64(values, pattern);

	return matches ? __builtin_ctzll(matches) >> 3 : 8;
}

/**
 * swar_find_first_zero64 - Find the first byte not equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Byte index, or eight if every byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_zero64(u64 values,
								  u8 pattern)
{
	u64 nonmatches = 0x8080808080808080ULL & ~swar_match64(values, pattern);

	return nonmatches ? __builtin_ctzll(nonmatches) >> 3 : 8;
}

/**
 * swar_get_count_set64 - Count bytes equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to match
 * @returns: Number of matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_set64(u64 values,
								u8 pattern)
{
	return __builtin_popcountll(swar_match64(values, pattern));
}

/**
 * swar_get_count_zero64 - Count bytes not equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Number of non-matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_zero64(u64 values,
								 u8 pattern)
{
	return 8 - swar_get_count_set64(values, pattern);
}

#endif
