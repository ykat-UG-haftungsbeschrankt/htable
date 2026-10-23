/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR_H
#define HTABLE_SWAR_H

#include "htable_types.h"

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
typedef u64 swar64_t;
typedef u32 swar32_t;
typedef __int128 swar128_t;
typedef u8 swar_slot_t;
typedef u64 swar_slots_t;
typedef u32 swar_hash_t;
typedef u32 swar_bucket_t;
typedef ssize_t swar_slot_offset_t;
typedef u8 swar_fingerprint_t;
typedef ssize_t swar_offset_t;

#if SWAR_HAS_U128
typedef unsigned __int128 swar_u128_t;
#else
typedef struct {
	u64 low;
	u64 high;
} swar_u128_t;
#endif

#include "swar32.h"
#include "swar64.h"
#include "swar128.h"

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
 * swar_match - Find matching bytes in the selected low-order slots
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

	if (slot_count > sizeof(values))
		return 0;
	if (slot_count == 0)
		return 0;

	xor_result = values ^ repeated_target;
	valid_lanes = 0x8080808080808080ULL >>
		      ((sizeof(values) - slot_count) * 8);
	return ~(((xor_result & 0x7F7F7F7F7F7F7F7FULL) +
		  0x7F7F7F7F7F7F7F7FULL) |
		 xor_result) & valid_lanes;
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
	size_t offset = 0;

	while (count - offset >= sizeof(u64)) {
		u64 packed;
		swar_slot_offset_t match;

		memcpy(&packed, values + offset, sizeof(packed));
		match = swar_find_first_set64(packed, pattern);
		if (match < sizeof(packed))
			return offset + match;
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] == pattern)
			return offset;
		offset++;
	}

	return count;
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
		matches_count += swar_get_count_set64(packed, pattern);
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] == pattern)
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
