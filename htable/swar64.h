/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR64_H
#define HTABLE_SWAR64_H

#include "htable_types.h"

typedef u64 swar_match64_t;

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
 * swar_match64 - Find matching bytes in a 64-bit value
 * @values: Eight packed bytes
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching byte
 */
static __always_inline swar_match64_t swar_match64(u64 values, u8 target)
{
	swar_match64_t xor_result = values ^ (target * 0x0101010101010101ULL);

	return ~(((xor_result & 0x7F7F7F7F7F7F7F7FULL) +
		  0x7F7F7F7F7F7F7F7FULL) |
		 xor_result) & 0x8080808080808080ULL;
}

#define swar_match64_for_each(matches, it)\
	for (\
		  struct{\
			swar_slots_t matches;\
			usize slot;\
		  } it = {\
			.matches = matches,\
			.slot = (matches ? __builtin_ctzll(matches) : -1)\
		  }\
		; it.slot > -1\
		; it.matches >>= (it.slot << 3)\
		, it.slot = (it.matches ? __builtin_ctzll(it.matches) >> 3 : -1)\
	)

/**
 * swar_find_first_set64 - Find the first byte equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to match
 * @returns: Byte index, or -1 if no byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_set64(u64 values,
								 u8 pattern)
{
	u64 matches = swar_match64(values, pattern);

	return matches ? __builtin_ctzll(matches) >> 3 : -1;
}

swar_slot_offset_t swar_random_slot64()
{
	static u8 x = 314159265;
	x ^= x << 13;
	return x % 8;
}

static __always_inline swar_slot_offset_t swar_find_random_set64(
	swar_match64_t match
){
	static u8 rand = 314159265;
	rand ^= rand << 13;
    rand ^= rand >> 17;
    rand ^= rand << 5;
	int max = __builtin_popcountll(match);
	return rand % max;
}

/**
 * swar_find_first_zero64 - Find the first byte not equal to a pattern
 * @values: Eight packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Byte index, or -1 if every byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_zero64(u64 values,
								  u8 pattern)
{
	u64 nonmatches = 0x8080808080808080ULL & ~swar_match64(values, pattern);

	return nonmatches ? __builtin_ctzll(nonmatches) >> 3 : -1;
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

static __always_inline swar_slot_offset_t swar_set_slot(swar64_t *swar,
							 swar_slot_offset_t slot,
							 u8 pattern)
{
	return *swar |= ((swar64_t)pattern) << (slot << 3);
}

#endif
