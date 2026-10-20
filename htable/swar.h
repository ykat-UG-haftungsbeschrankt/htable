/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SWAR_H
#define _SWAR_H

#include <linux/types.h>
#include <linux/string.h>

#define SWAR_HASH_EMPTY_SIG 0x00
#define SWAR_SLOT_COUNT 7

#ifndef SWAR_HAS_U128
#if defined(__SIZEOF_INT128__)
#define SWAR_HAS_U128 1
#else
#define SWAR_HAS_U128 0
#endif
#endif

typedef u64 swar_t;
typedef u64 swar_slots_t;
typedef u32 swar_hash_t ;
typedef u32 swar_bucket_t;
typedef u8 swar_slot_offset_t;
typedef u8 swar_fingerprint_t;

#if SWAR_HAS_U128
typedef unsigned __int128 swar_u8x16_t;
#else
typedef struct {
	swar_t low;
	swar_t high;
} swar_u8x16_t;
#endif

/**
 * swar_get_hash32 - Calculate a CRC32C hash from a 32-bit key
 * @key: Key used as both the CRC32C seed and input value
 * @returns: CRC32C result produced by the 32-bit CRC instruction
 */
static __always_inline swar_hash_t swar_get_hash32(u32 key)
{
	return _mm_crc32_u32(key,key);
}

/**
 * swar_get_hash64 - Calculate a CRC32C hash from a 64-bit key
 * @key: Key used as both the CRC32C seed and input value
 * @returns: CRC32C result produced by the 64-bit CRC instruction
 */
static __always_inline swar_hash_t swar_get_hash64(u64 key)
{
	return _mm_crc32_u64(key,key);
}


/**
 * swar_get_bucket - Remove the seven fingerprint bits from a hash
 * @key: Hash whose low seven bits contain the fingerprint
 * @returns: Hash shifted right by seven bits
 */
static __always_inline swar_bucket_t swar_get_bucket(swar_hash_t key)
{
	return key >> 0x07;
}

/**
 * swar_get_key - Extract the fingerprint bits from a hash
 * @key: Hash containing the fingerprint in its low seven bits
 * @returns: Low seven bits of @key as a fingerprint; zero is preserved
 */
static __always_inline swar_fingerprint_t swar_get_key(swar_hash_t key){
	return key & 0x7F;
}

/**
 * swar_match_u8x7 - Find matching bytes in seven packed slots
 * @values: Packed slot bytes; only the seven low-order bytes are slots
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching slot, or zero if
 *           there are no matches; any eighth byte is ignored
 */
static __always_inline swar_slots_t swar_match_u8x7(swar_t values, swar_fingerprint_t target)
{
	u64 repeated_target = target * 0x0101010101010101ULL;
	u64 xor_result = values ^ repeated_target;
	u64 candidates = (xor_result - 0x0101010101010101ULL) &
			 ~xor_result & 0x8080808080808080ULL;
	u64 matches = 0;

	candidates &= 0x0080808080808080ULL;
	while (candidates) {
		unsigned int index = __builtin_ctzll(candidates) >> 3;

		if (((values >> (index * 8)) & 0xFF) == target)
			matches |= 0x80ULL << (index * 8);
		candidates &= candidates - 1;
	}

	return matches;
}

/**
 * swar_match_u8x8 - Find matching bytes in eight packed slots
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @returns: Mask with the high bit set for each matching slot, or zero if
 *           there are no matches
 */
static __always_inline swar_slots_t swar_match_u8x8(swar_t values, swar_fingerprint_t target)
{
	u64 repeated_target = target * 0x0101010101010101ULL;
	u64 xor_result = values ^ repeated_target;
	u64 candidates = (xor_result - 0x0101010101010101ULL) &
			 ~xor_result & 0x8080808080808080ULL;
	u64 matches = 0;

	while (candidates) {
		unsigned int index = __builtin_ctzll(candidates) >> 3;

		if (((values >> (index * 8)) & 0xFF) == target)
			matches |= 0x80ULL << (index * 8);
		candidates &= candidates - 1;
	}

	return matches;
}

/**
 * swar_match_u8x16 - Find matching bytes in sixteen packed slots
 * @values: Packed slot bytes; use two u64 fields when SWAR_HAS_U128 is zero
 * @target: Byte value to match
 * @returns: Packed mask with the high bit set for each matching slot; without
 *           128-bit support, low and high contain the masks for slots 0-7
 *           and 8-15 respectively
 */
static __always_inline swar_u8x16_t swar_match_u8x16(swar_u8x16_t values,
						      swar_fingerprint_t target)
{
	u64 low_matches;
	u64 high_matches;

#if SWAR_HAS_U128
	low_matches = swar_match_u8x8((u64)values, target);
	high_matches = swar_match_u8x8((u64)(values >> 64), target);
	return ((swar_u8x16_t)high_matches << 64) | low_matches;
#else
	low_matches = swar_match_u8x8(values.low, target);
	high_matches = swar_match_u8x8(values.high, target);
	{
		swar_u8x16_t matches = {
			.low = low_matches,
			.high = high_matches,
		};

		return matches;
	}
#endif
}

/**
 * swar_get_count - Count occupied slots in seven packed slots
 * @values: Packed slot bytes; only the seven low-order bytes are slots
 * @returns: Number of non-empty slots, from zero through SWAR_SLOT_COUNT
 */
static __always_inline swar_slot_offset_t swar_get_count_u8x8(swar_slots_t values)
{
	swar_slots_t empty_mask =
		swar_match_u8x7(values, SWAR_HASH_EMPTY_SIG);

	return SWAR_SLOT_COUNT - __builtin_popcountll(empty_mask);
}

/**
 * swar_find_empty_index - Find the first empty slot in seven packed slots
 * @values: Packed slot bytes; only the seven low-order bytes are slots
 * @returns: First empty slot index, or SWAR_SLOT_COUNT if all slots are
 *           occupied
 */
static __always_inline swar_slot_offset_t swar_find_empty_index(swar_slots_t values)
{
	u64 empty_mask = swar_match_u8x7(values, SWAR_HASH_EMPTY_SIG);

	return empty_mask ? __builtin_ctzll(empty_mask) >> 3 :
			    SWAR_SLOT_COUNT;
}

/**
 * swar_find_u8 - Find the first matching byte in an arbitrary-length array
 * @values: Byte array to search
 * @count: Number of bytes in @values
 * @target: Byte value to find
 * @returns: Index of the first match, or @count if no match is found
 */
static __always_inline size_t swar_find_u8(const u8 *values, size_t count,
					   u8 target)
{
	size_t offset = 0;

	while (count - offset >= sizeof(swar_t)) {
		swar_t packed;
		swar_slots_t matches;

		memcpy(&packed, values + offset, sizeof(packed));
		matches = swar_match_u8x8(packed, target);
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
 * swar_find_empty_u8 - Find the first empty byte in an arbitrary-length array
 * @values: Byte array to scan
 * @count: Number of bytes in @values
 * @returns: Index of the first empty byte, or @count if none is found
 */
static __always_inline size_t swar_find_empty_u8(const u8 *values, size_t count)
{
	return swar_find_u8(values, count, SWAR_HASH_EMPTY_SIG);
}

/**
 * swar_get_count_u8 - Count matching bytes in an arbitrary-length array
 * @values: Byte array to scan
 * @count: Number of bytes in @values
 * @target: Byte value to count
 * @returns: Number of elements equal to @target
 */
static __always_inline size_t swar_get_count_u8(const u8 *values, size_t count,
						u8 target)
{
	size_t offset = 0;
	size_t matches_count = 0;

	while (count - offset >= sizeof(swar_t)) {
		swar_t packed;

		memcpy(&packed, values + offset, sizeof(packed));
		matches_count += __builtin_popcountll(
			swar_match_u8x8(packed, target));
		offset += sizeof(packed);
	}

	while (offset < count) {
		if (values[offset] == target)
			matches_count++;
		offset++;
	}

	return matches_count;
}

#endif