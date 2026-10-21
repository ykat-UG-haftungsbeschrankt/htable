/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR_H
#define HTABLE_SWAR_H

#include "swar32.h"
#include "swar64.h"
#include "swar128.h"
#include "swar_u8.h"

/**
 * swar_get_hash32 - Calculate a CRC32C hash from a 32-bit key
 * @key: Key used as both the CRC32C seed and input value
 * @returns: CRC32C result produced by the 32-bit CRC instruction
 */
static __always_inline swar_hash_t swar_get_hash(swar_hash_t key)
{
	return swar_get_hash32(key);
}

/**
 * swar_get_bucket - 32-bit compatibility wrapper for swar_get_bucket32
 * @key: Hash whose low seven bits contain the fingerprint
 * @returns: Hash shifted right by seven bits
 */
static __always_inline swar_bucket_t swar_get_bucket(swar_hash_t key)
{
	return swar_get_bucket32(key);
}

/**
 * swar_get_key - 32-bit compatibility wrapper for swar_get_key32
 * @key: Hash containing the fingerprint in its low seven bits
 * @returns: Low seven bits of @key
 */
static __always_inline swar_fingerprint_t swar_get_key(swar_hash_t key)
{
	return swar_get_key32(key);
}

/**
 * swar_match_u8 - Find matching bytes in the selected low-order slots
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @slot_count: Number of low-order bytes to examine, from zero through eight
 * @returns: Mask with the high bit set for each matching slot, or zero if
 *           there are no matches or @slot_count exceeds eight
 */
static __always_inline swar_slots_t swar_match(swar_t values,
						   swar_fingerprint_t target,
						   swar_slot_offset_t slot_count)
{
	u64 repeated_target = target * 0x0101010101010101ULL;
	u64 xor_result;
	u64 valid_lanes;
	u64 candidates;
	u64 matches = 0;

	if (slot_count > sizeof(values))
		return 0;
	if (slot_count == 0)
		return 0;

	xor_result = values ^ repeated_target;
	valid_lanes = 0x8080808080808080ULL >>
		      ((sizeof(values) - slot_count) * 8);
	candidates = (xor_result - 0x0101010101010101ULL) &
		     ~xor_result & valid_lanes;

	while (candidates) {
		unsigned int index = __builtin_ctzll(candidates) >> 3;

		if (((values >> (index * 8)) & 0xFF) == target)
			matches |= 0x80ULL << (index * 8);
		candidates &= candidates - 1;
	}

	return matches;
}

/**
 * swar_find_first_set - Find the first byte equal to a pattern
 * @values: Byte array to search
 * @count: Number of bytes in @values
 * @pattern: Byte pattern to match
 * @returns: Index of the first match, or @count if no byte matches
 */
static __always_inline size_t swar_find_first_set(const u8 *values,
						  size_t count, u8 pattern)
{
	return swar_find_u8(values, count, pattern);
}

/**
 * swar_find_first_zero - Find the first byte not equal to a pattern
 * @values: Byte array to search
 * @count: Number of bytes in @values
 * @pattern: Byte pattern to compare
 * @returns: Index of the first non-match, or @count if every byte matches
 */
static __always_inline size_t swar_find_first_zero(const u8 *values,
						   size_t count, u8 pattern)
{
	size_t offset = 0;

	while (count - offset >= sizeof(u64)) {
		u64 packed;
		u64 nonmatches;

		memcpy(&packed, values + offset, sizeof(packed));
		nonmatches = 0x8080808080808080ULL &
			     ~swar_match64(packed, pattern);
		if (nonmatches)
			return offset + (__builtin_ctzll(nonmatches) >> 3);
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] != pattern)
			return offset;
		offset++;
	}

	return count;
}

/**
 * swar_get_count_set - Count bytes equal to a pattern
 * @values: Byte array to scan
 * @count: Number of bytes in @values
 * @pattern: Byte pattern to match
 * @returns: Number of matching bytes
 */
static __always_inline size_t swar_get_count_set(const u8 *values,
						  size_t count, u8 pattern)
{
	size_t offset = 0;
	size_t matches_count = 0;

	while (count - offset >= sizeof(u64)) {
		u64 packed;

		memcpy(&packed, values + offset, sizeof(packed));
		matches_count += swar_get_count_set64(packed, target);
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] == target)
			matches_count++;
		offset++;
	}

	return matches_count;
}

/**
 * swar_get_count_zero - Count bytes not equal to a pattern
 * @values: Byte array to scan
 * @count: Number of bytes in @values
 * @pattern: Byte pattern to compare
 * @returns: Number of non-matching bytes
 */
static __always_inline size_t swar_get_count_zero(const u8 *values,
						   size_t count, u8 pattern)
{
	return count - swar_get_count_set(values, count, pattern);
}

#endif
