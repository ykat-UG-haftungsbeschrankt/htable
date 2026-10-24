/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

#include "htable/swar.h"

static u64 pack_bytes(const u8 *bytes, size_t count)
{
	u64 packed = 0;

	for (size_t i = 0; i < count; i++)
		packed |= (u64)bytes[i] << (i * 8);
	return packed;
}

static u64 expected_mask(const u8 *bytes, size_t count, u8 pattern)
{
	u64 matches = 0;

	for (size_t i = 0; i < count; i++) {
		if (bytes[i] == pattern)
			matches |= 0x80ULL << (i * 8);
	}
	return matches;
}

static void test_byte_matching(void)
{
	const u8 bytes[8] = {
		0x00, 0x01, 0x80, 0x81, 0xFF, 0x00, 0x7F, 0x01,
	};
	u64 packed = pack_bytes(bytes, sizeof(bytes));

	assert(swar_match(packed, 0x00, 0) == 0);
	assert(swar_match(packed, 0x00, 9) == 0);
	assert(swar_match(packed, 0x00, 4) ==
	       expected_mask(bytes, 4, 0x00));
	assert(swar_match(packed, 0x00, 8) ==
	       expected_mask(bytes, 8, 0x00));
	for (unsigned int target = 0; target <= UINT8_MAX; target++) {
		u8 pattern = (u8)target;

		assert(swar_match32((u32)packed, pattern) ==
		       (u32)expected_mask(bytes, 4, pattern));
		assert(swar_match64(packed, pattern) ==
		       expected_mask(bytes, 8, pattern));
		for (u8 slot_count = 0; slot_count <= 8; slot_count++) {
			assert(swar_match(packed, pattern, slot_count) ==
			       expected_mask(bytes, slot_count, pattern));
		}
	}
	assert(swar_find_first_empty_slot32((u32)packed) == 0);
	assert(swar_find_first_empty_slot32(UINT32_MAX) == -1);
	assert(swar_find_first_empty_slot64(packed) == 0);
	assert(swar_find_first_empty_slot64(UINT64_MAX) == -1);
}

static void test_match64_iterator(void)
{
	const u64 match_mask =
		(0x80ULL << 8) | (0x80ULL << 40) | (0x80ULL << 56);
	const swar_slot_offset_t expected[] = { 1, 5, 7 };
	size_t count = 0;

	swar_match64_for_each(match_mask, iter) {
		assert(count < sizeof(expected) / sizeof(expected[0]));
		assert(iter.slot == expected[count]);
		count++;
	}
	assert(count == 3);

	count = 0;
	swar_match64_for_each(0, iter)
		count++;
	assert(count == 0);
}

static void test_byte_array_operations(void)
{
	const u8 bytes[] = {
		0x11, 0xAA, 0x00, 0xAA, 0x44, 0x55, 0x66, 0x77,
		0xAA, 0x99, 0xEE, 0xBB, 0xCC, 0xDD, 0x11, 0x22,
		0x33, 0xAA, 0x44,
	};
	const u8 repeated_bytes[8] = {
		0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	};
	size_t count = sizeof(bytes);

	assert(swar_find_first_set(bytes, count, 0xAA) == 1);
	assert(swar_find_first_set(bytes, count, 0xFE) == -1);
	assert(swar_find_first_empty_slot(bytes, count) == 2);
	assert(swar_find_first_zero(bytes, count, 0xAA) == 0);
	assert(swar_find_first_zero(bytes, count, 0x11) == 1);
	assert(swar_find_first_zero(repeated_bytes, sizeof(repeated_bytes),
				    0xAA) == -1);
	assert(swar_get_count_set(bytes, count, 0xAA) == 4);
	assert(swar_get_count_zero(bytes, count, 0xAA) == count - 4);
	assert(swar_find_first_set(bytes, 0, 0xAA) == -1);
	assert(swar_find_first_empty_slot(bytes, 0) == -1);
	assert(swar_find_first_zero(bytes, 0, 0xAA) == -1);
	assert(swar_get_count_set(bytes, 0, 0xAA) == 0);
}

static void test_wide_byte_operations(void)
{
	u8 bytes[16];
	u64 low_mask;
	u64 high_mask;
	swar_u128_t packed;
	swar_u128_t matches;
	u8 empty_bytes[16];
	swar_u128_t empty_packed;

	for (size_t i = 0; i < 16; i++)
		bytes[i] = (u8)(i + 1);
	for (size_t i = 0; i < 16; i++)
		empty_bytes[i] = bytes[i];
	bytes[2] = 0xAA;
	bytes[9] = 0xAA;
	bytes[10] = 0xEE;
	bytes[15] = 0xAA;
	empty_bytes[12] = SWAR_SLOT_EMPTY_SIG;

	low_mask = expected_mask(bytes, 8, 0xAA);
	high_mask = expected_mask(bytes + 8, 8, 0xAA);
#if SWAR_HAS_U128
	packed = ((swar_u128_t)pack_bytes(bytes + 8, 8) << 64) |
		 pack_bytes(bytes, 8);
	empty_packed = ((swar_u128_t)pack_bytes(empty_bytes + 8, 8) << 64) |
		       pack_bytes(empty_bytes, 8);
#else
	packed.low = pack_bytes(bytes, 8);
	packed.high = pack_bytes(bytes + 8, 8);
	empty_packed.low = pack_bytes(empty_bytes, 8);
	empty_packed.high = pack_bytes(empty_bytes + 8, 8);
#endif
	assert(swar_find_first_empty_slot128(packed) == -1);
	assert(swar_find_first_empty_slot128(empty_packed) == 12);
	matches = swar_match128(packed, 0xAA);
#if SWAR_HAS_U128
	assert((u64)matches == low_mask);
	assert((u64)(matches >> 64) == high_mask);
#else
	assert(matches.low == low_mask);
	assert(matches.high == high_mask);
#endif
	for (unsigned int target = 0; target <= UINT8_MAX; target++) {
		u8 pattern = (u8)target;

		matches = swar_match128(packed, pattern);
#if SWAR_HAS_U128
		assert((u64)matches == expected_mask(bytes, 8, pattern));
		assert((u64)(matches >> 64) ==
		       expected_mask(bytes + 8, 8, pattern));
#else
		assert(matches.low == expected_mask(bytes, 8, pattern));
		assert(matches.high == expected_mask(bytes + 8, 8, pattern));
#endif
	}
	assert(swar_find_first_set128(packed, 0xAA) == 2);
	assert(swar_find_first_set128(packed, 0xEE) == 10);
	assert(swar_find_first_zero128(packed, 0xAA) == 0);
	assert(swar_get_count_set128(packed, 0xAA) == 3);
	assert(swar_get_count_zero128(packed, 0xAA) == 13);
}

static void test_128_bit_slot_updates(void)
{
	swar_u128_t slots;

#if SWAR_HAS_U128
	slots = ~(swar_u128_t)0;
#else
	slots.low = UINT64_MAX;
	slots.high = UINT64_MAX;
#endif
	slots = swar_set_slot128(slots, 15, 0x5A);
	assert(swar_find_first_set128(slots, 0x5A) == 15);
	slots = swar_clear_slot128(slots, 15);
	assert(swar_find_first_set128(slots, 0x5A) == -1);
}

static void test_hash_and_fingerprint_wrappers(void)
{
	u32 hash = 0x12345678;

	assert(swar_get_hash(hash) == swar_get_hash32(hash));
	assert(swar_get_bucket(hash) == hash >> 7);
	assert(swar_get_key(hash) == (hash & 0x7F));
	assert(swar_get_hash64(0x123456789ABCDEF0ULL) ==
	       swar_get_hash64(0x123456789ABCDEF0ULL));
}

void test_swar(void)
{
	test_byte_matching();
	test_match64_iterator();
	test_byte_array_operations();
	test_wide_byte_operations();
	test_128_bit_slot_updates();
	test_hash_and_fingerprint_wrappers();
}

#ifdef SWAR_TEST_STANDALONE
int main(void)
{
	test_swar();
	puts("swar tests passed");
	return 0;
}
#endif