/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HTABLE_SWAR_COMMON_H
#define HTABLE_SWAR_COMMON_H

#include <linux/types.h>

#ifndef __always_inline
#define __always_inline inline __attribute__((always_inline))
#endif

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

typedef swar_u128_t swar_u8x16_t;

/**
 * swar_match_u8 - Find matching bytes in the selected low-order slots
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @slot_count: Number of low-order bytes to examine, from zero through eight
 * @returns: Mask with the high bit set for each matching slot, or zero if
 *           there are no matches or @slot_count exceeds eight
 */
static __always_inline swar_slots_t swar_match_u8(swar_t values,
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
 * swar_match_u8x7 - Match target in the seven low-order bytes
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @returns: Per-byte match mask from swar_match_u8
 */
static __always_inline swar_slots_t swar_match_u8x7(swar_t values,
						     swar_fingerprint_t target)
{
	return swar_match_u8(values, target, SWAR_SLOT_COUNT);
}

/**
 * swar_match_u8x8 - Match target in all eight packed bytes
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @returns: Per-byte match mask from swar_match_u8
 */
static __always_inline swar_slots_t swar_match_u8x8(swar_t values,
						     swar_fingerprint_t target)
{
	return swar_match_u8(values, target, SWAR_SLOT_COUNT + 1);
}

/**
 * swar_match_u8x16 - Find matching bytes in sixteen packed slots
 * @values: Packed slot bytes
 * @target: Byte value to match
 * @returns: Packed mask with the high bit set for each matching slot
 */
static __always_inline swar_u8x16_t swar_match_u8x16(swar_u8x16_t values,
						      swar_fingerprint_t target)
{
	u64 low;
	u64 high;
	u64 low_matches;
	u64 high_matches;

#if SWAR_HAS_U128
	low = (u64)values;
	high = (u64)(values >> 64);
#else
	low = values.low;
	high = values.high;
#endif
	low_matches = swar_match_u8x8(low, target);
	high_matches = swar_match_u8x8(high, target);

#if SWAR_HAS_U128
	return ((swar_u8x16_t)high_matches << 64) | low_matches;
#else
	{
		swar_u8x16_t matches = {
			.low = low_matches,
			.high = high_matches,
		};

		return matches;
	}
#endif
}

#endif
