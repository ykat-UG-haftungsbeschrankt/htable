/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#ifndef HTABLE_SWAR_U8_H
#define HTABLE_SWAR_U8_H

#include <linux/string.h>

#include "swar64.h"

/**
 * swar_find_u8 - Find the first byte equal to a target in an array
 * @values: Byte array to search
 * @count: Number of bytes in @values
 * @target: Byte value to find
 * @returns: Index of the first match, or @count if no match is found
 */
static __always_inline size_t swar_find_u8(const u8 *values, size_t count,
					   u8 target)
{
	size_t offset = 0;

	while (count - offset >= sizeof(u64)) {
		u64 packed;
		u64 matches;

		memcpy(&packed, values + offset, sizeof(packed));
		matches = swar_match64(packed, target);
		if (matches)
			return offset + (__builtin_ctzll(matches) >> 3);
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] == target)
			return offset;
		offset++;
	}

	return count;
}

/**
 * swar_find_empty_u8 - Find the first zero-valued byte in an array
 * @values: Byte array to search
 * @count: Number of bytes in @values
 * @returns: Index of the first zero byte, or @count if none is found
 */
static __always_inline size_t swar_find_empty_u8(const u8 *values, size_t count)
{
	return swar_find_u8(values, count, SWAR_HASH_EMPTY_SIG);
}

/**
 * swar_get_count_u8 - Count bytes equal to a target in an array
 * @values: Byte array to scan
 * @count: Number of bytes in @values
 * @target: Byte value to count
 * @returns: Number of matching bytes
 */
static __always_inline size_t swar_get_count_u8(const u8 *values, size_t count,
						u8 target)
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
	return swar_get_count_u8(values, count, pattern);
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

static __always_inline size_t swar_find_first_set_u8(const u8 *values,
						      size_t count, u8 pattern)
{
	return swar_find_first_set(values, count, pattern);
}

static __always_inline size_t swar_find_first_zero_u8(const u8 *values,
						       size_t count, u8 pattern)
{
	return swar_find_first_zero(values, count, pattern);
}

static __always_inline size_t swar_get_count_set_u8(const u8 *values,
						     size_t count, u8 pattern)
{
	return swar_get_count_set(values, count, pattern);
}

static __always_inline size_t swar_get_count_zero_u8(const u8 *values,
						      size_t count, u8 pattern)
{
	return swar_get_count_zero(values, count, pattern);
}

#endif
