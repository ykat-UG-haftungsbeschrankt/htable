/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SWAR_H
#define _SWAR_H

#include <linux/types.h>

#define SWAR_HASH_EMPTY_SIG 0x00
#define SWAR_SLOT_COUNT 7

typedef u64 swar_t;
typedef u64 swar_slots_t;
typedef u32 swar_hash_t ;
typedef u32 swar_bucket_t;
typedef u8 swar_slot_offset_t;
typedef u8 swar_fingerprint_t;

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
 * swar_get_count - Count occupied slots in seven packed slots
 * @values: Packed slot bytes; only the seven low-order bytes are slots
 * @returns: Number of non-empty slots, from zero through SWAR_SLOT_COUNT
 */
static __always_inline swar_slot_offset_t swar_get_count(swar_slots_t values)
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

#endif