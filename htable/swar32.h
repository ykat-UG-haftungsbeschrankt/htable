/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR32_H
#define HTABLE_SWAR32_H

/**
 * swar_get_hash32 - Calculate a CRC32C hash from a 32-bit key
 * @key: Key used as both the CRC32C seed and input value
 * @returns: CRC32C result produced by the 32-bit CRC instruction
 */
static __always_inline swar_hash_t swar_get_hash32(u32 key)
{
	return _mm_crc32_u32(key, key);
}

/**
 * swar_get_bucket32 - Remove the seven fingerprint bits from a 32-bit hash
 * @key: Hash whose low seven bits contain the fingerprint
 * @returns: Hash shifted right by seven bits
 */
static __always_inline swar_bucket_t swar_get_bucket32(swar_hash_t key)
{
	return key >> 7;
}

/**
 * swar_get_key32 - Extract the fingerprint from a 32-bit hash
 * @key: Hash containing the fingerprint in its low seven bits
 * @returns: Low seven bits of @key
 */
static __always_inline swar_fingerprint_t swar_get_key32(swar_hash_t key)
{
	return key & 0x7F;
}

/**
 * swar_match32 - Find matching bytes in a 32-bit value
 * @values: Four packed bytes
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching byte
 */
static __always_inline u32 swar_match32(u32 values, u8 target)
{
	u32 xor_result = values ^ (target * 0x01010101U);

	return ~(((xor_result & 0x7F7F7F7FU) + 0x7F7F7F7FU) |
		 xor_result) & 0x80808080U;
}

/**
 * swar_find_first_set32 - Find the first byte equal to a pattern
 * @values: Four packed bytes
 * @pattern: Byte pattern to match
 * @returns: Byte index, or four if no byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_set32(u32 values,
								 u8 pattern)
{
	u32 matches = swar_match32(values, pattern);

	return matches ? __builtin_ctz(matches) >> 3 : 4;
}

/**
 * swar_find_first_zero32 - Find the first byte not equal to a pattern
 * @values: Four packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Byte index, or four if every byte matches
 */
static __always_inline swar_slot_offset_t swar_find_first_zero32(u32 values,
								  u8 pattern)
{
	u32 nonmatches = 0x80808080U & ~swar_match32(values, pattern);

	return nonmatches ? __builtin_ctz(nonmatches) >> 3 : 4;
}

/**
 * swar_get_count_set32 - Count bytes equal to a pattern
 * @values: Four packed bytes
 * @pattern: Byte pattern to match
 * @returns: Number of matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_set32(u32 values,
								u8 pattern)
{
	return __builtin_popcount(swar_match32(values, pattern));
}

/**
 * swar_get_count_zero32 - Count bytes not equal to a pattern
 * @values: Four packed bytes
 * @pattern: Byte pattern to compare
 * @returns: Number of non-matching bytes
 */
static __always_inline swar_slot_offset_t swar_get_count_zero32(u32 values,
								 u8 pattern)
{
	return 4 - swar_get_count_set32(values, pattern);
}

#endif
