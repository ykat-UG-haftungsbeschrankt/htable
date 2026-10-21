/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>

#ifndef HTABLE_MULTITABLE_H
#define HTABLE_MULTITABLE_H

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

#include "swar_common.h"
#include "htable_list.h"

#define EMPTY_METADATA 0x00
#define MULTITABLE_MAX_BUCKET_CAPACITY 16

/* Derives an 8-bit non-zero signature tag for the Filtered layer */
static inline uint8_t generate_filter_tag(uint32_t hash) {
    uint8_t tag = (uint8_t)(hash & 0xFF);
    return (tag == EMPTY_METADATA) ? 0xAA : tag; 
}

typedef struct {
    struct htable_list list;
    uint32_t hash;
} PointerHashSlot;

typedef struct {
    PointerHashSlot *slots;
    uint8_t *filter_tags;
    size_t num_buckets;
    size_t num_slots;
} TableLevel;

typedef struct {
    TableLevel *levels;
    size_t num_levels;
    size_t bucket_capacity;
    size_t expected_values;
    double target_load;
    double failure_budget;
} FilteredMultiTable;

static inline bool multitable_find_tag_slot(const TableLevel *level,
                                            size_t bucket_idx,
                                            size_t bucket_capacity,
                                            uint8_t tag,
                                            size_t start_slot,
                                            size_t *slot_idx) {
    if (start_slot >= bucket_capacity) {
        return false;
    }

    size_t filter_offset = bucket_idx * bucket_capacity + start_slot;
    while (start_slot < bucket_capacity) {
        uint64_t packed_tags = 0;
        size_t chunk_size = bucket_capacity - start_slot;
        uint64_t matches;

        if (chunk_size > sizeof(packed_tags)) {
            chunk_size = sizeof(packed_tags);
        }
        memcpy(&packed_tags, level->filter_tags + filter_offset, chunk_size);
        matches = swar_match_u8(packed_tags, tag, (uint8_t)chunk_size);
        if (matches != 0) {
            *slot_idx = start_slot + (__builtin_ctzll(matches) >> 3);
            return true;
        }
        start_slot += chunk_size;
        filter_offset += chunk_size;
    }
    return false;
}

static inline bool multitable_list_contains(
    const struct htable_list *list, const struct htable_list_node *node) {
    const struct htable_list_node *current = list->tail;

    while (current != NULL && current != &list->sentinel) {
        if (current == node) {
            return true;
        }
        current = current->pprev;
    }
    return false;
}

static inline uint32_t multitable_level_hash(uint32_t hash,
                                             size_t level_idx) {
    /* Use a level-specific mix to decorrelate bucket choices across levels. */
    uint32_t value = hash + (uint32_t)level_idx * 0x9e3779b9U;

    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static inline size_t multitable_bucket_index(uint32_t hash,
                                            size_t num_buckets,
                                            size_t level_idx) {
    uint32_t level_hash = multitable_level_hash(hash, level_idx);

    return (size_t)(((uint64_t)level_hash * num_buckets) >> 32);
}

/* Upper-bound E[O] by summing binomial excess probabilities per bucket. */
static inline long double multitable_expected_overflow_upper(
    uint64_t keys, size_t num_buckets, size_t bucket_capacity) {
    long double mean = (long double)keys / num_buckets;
    long double probability;
    long double probability_of_empty;
    long double overflow_per_bucket = 0.0L;
    long double one_minus_p;
    uint64_t s = (uint64_t)bucket_capacity;
    uint64_t tail_limit = s > UINT64_MAX - 64 ? UINT64_MAX : s + 64;
    uint64_t last = keys < tail_limit ? keys : tail_limit;

    if (mean >= (long double)s) {
        return (long double)(keys - bucket_capacity);
    }

    long double p = 1.0L / num_buckets;
    one_minus_p = 1.0L - p;
    probability_of_empty = expl((long double)keys * log1pl(-p));
    if (probability_of_empty == 0.0L) {
        return (long double)(keys - bucket_capacity);
    }

    probability = probability_of_empty;
    for (uint64_t k = 1; k <= last; k++) {
        probability *= ((long double)(keys - k + 1) * p) /
                       ((long double)k * one_minus_p);
        if (k > s) {
            overflow_per_bucket += (long double)(k - s) * probability;
        }
    }

    if (last < keys) {
        uint64_t next = last + 1;
        long double ratio =
            ((long double)(keys - last) * p) /
            ((long double)next * one_minus_p);

        if (ratio >= 1.0L) {
            return (long double)(keys - bucket_capacity);
        }
        probability *= ratio;
        overflow_per_bucket += probability *
            ((long double)(next - s) / (1.0L - ratio) +
             ratio / ((1.0L - ratio) * (1.0L - ratio)));
    }

    long double expected = (long double)num_buckets * overflow_per_bucket;
    long double rounding_margin =
        expected * LDBL_EPSILON * ((long double)bucket_capacity + 128.0L);

    expected = nextafterl(expected + rounding_margin, INFINITY);
    if (expected > (long double)(keys - bucket_capacity)) {
        expected = (long double)(keys - bucket_capacity);
    }
    return expected;
}

static inline long double multitable_log_add_exp(long double left,
                                                 long double right) {
    long double high;
    long double low;

    if (left == -INFINITY) {
        return right;
    }
    if (right == -INFINITY) {
        return left;
    }
    high = fmaxl(left, right);
    low = fminl(left, right);
    return high + log1pl(expl(low - high));
}

static inline void multitable_binomial_log_cdf_bounds(
    uint64_t trials, long double p, uint64_t max_k, size_t bucket_capacity,
    long double *log_cdf, long double *log_tail) {
    long double cdf;
    long double tail;
    long double probability_error =
        128.0L * LDBL_EPSILON * ((long double)bucket_capacity + 2.0L);

    if (max_k >= trials || p <= 0.0L) {
        cdf = 1.0L;
        tail = 0.0L;
    } else if (p >= 1.0L) {
        cdf = 0.0L;
        tail = 1.0L;
    } else {
        long double log_one_minus_p = log1pl(-p);
        long double log_probability =
            (long double)trials * log_one_minus_p;
        long double log_sum = log_probability;

        for (uint64_t k = 1; k <= max_k; k++) {
            log_probability +=
                logl((long double)(trials - k + 1)) -
                logl((long double)k) + logl(p) - log_one_minus_p;
            log_sum = multitable_log_add_exp(log_sum, log_probability);
        }
        if (log_sum > 0.0L) {
            log_sum = 0.0L;
        }
        cdf = expl(log_sum);
        tail = -expm1l(log_sum);
    }

    cdf = fminl(1.0L, cdf + probability_error);
    tail = fminl(1.0L, tail + probability_error);
    if (log_cdf != NULL) {
        *log_cdf = logl(cdf);
    }
    if (log_tail != NULL) {
        *log_tail = logl(tail);
    }
}

static inline void multitable_tilted_overflow_bounds(
    uint64_t keys, size_t num_buckets, size_t bucket_capacity, double delta,
    long double *overflow_bound, long double *unused_bound) {
    long double p = 1.0L / num_buckets;
    long double log_cdf_s;
    long double log_tail_s_minus_1;
    long double log_failure = -logl((long double)delta);
    long double best_overflow = INFINITY;
    long double best_unused = INFINITY;
    long double s = (long double)bucket_capacity;

    multitable_binomial_log_cdf_bounds(keys, p, bucket_capacity,
                                       bucket_capacity, &log_cdf_s,
                                       NULL);
    multitable_binomial_log_cdf_bounds(keys, p, bucket_capacity - 1,
                                       bucket_capacity, NULL,
                                       &log_tail_s_minus_1);

    /* Every sampled theta gives a valid quantile bound; take the tightest. */
    for (int grid = 0; grid <= 64; grid++) {
        long double theta =
            exp2l(((long double)grid - 40.0L) / 4.0L) / s;
        long double exp_theta = expl(theta);
        long double denominator_plus =
            1.0L + p * expm1l(theta);
        long double log_base_plus =
            (long double)keys * logl(denominator_plus);
        long double p_plus = p * exp_theta / denominator_plus;
        long double log_cdf_plus;
        long double log_tail_plus;
        long double log_mgf_overflow;
        long double exp_minus_theta = expl(-theta);
        long double denominator_minus =
            1.0L + p * expm1l(-theta);
        long double log_base_minus =
            (long double)keys * logl(denominator_minus);
        long double p_minus = p * exp_minus_theta / denominator_minus;
        long double log_cdf_minus;
        long double log_mgf_unused;
        long double numeric_margin;
        long double candidate;

        if (p_plus < 1.0L) {
            p_plus = nextafterl(p_plus, 1.0L);
        }
        if (p_minus > 0.0L) {
            p_minus = nextafterl(p_minus, 0.0L);
        }
        multitable_binomial_log_cdf_bounds(
            keys, p_plus, bucket_capacity, bucket_capacity,
            &log_cdf_plus, &log_tail_plus);
        multitable_binomial_log_cdf_bounds(
            keys, p_minus, bucket_capacity - 1, bucket_capacity,
            &log_cdf_minus, NULL);

        log_mgf_overflow = multitable_log_add_exp(
            log_cdf_s, log_base_plus - theta * s + log_tail_plus);
        log_mgf_unused = multitable_log_add_exp(
            log_tail_s_minus_1,
            theta * s + log_base_minus + log_cdf_minus);
        numeric_margin =
            128.0L * LDBL_EPSILON *
            (fabsl(log_base_plus) + fabsl(log_base_minus) + theta * s + 1.0L);
        log_mgf_overflow = fmaxl(0.0L, log_mgf_overflow + numeric_margin);
        log_mgf_unused = fmaxl(0.0L, log_mgf_unused + numeric_margin);

        candidate =
            ((long double)num_buckets * log_mgf_overflow + log_failure) /
            theta;
        if (candidate < best_overflow) {
            best_overflow = candidate;
        }

        candidate =
            ((long double)num_buckets * log_mgf_unused + log_failure) /
            theta;
        if (candidate < best_unused) {
            best_unused = candidate;
        }
    }

    *overflow_bound = best_overflow;
    *unused_bound = best_unused;
}

/* Minimum of the paper-style McDiarmid, Markov, tilted, and trivial bounds. */
static inline uint64_t multitable_overflow_bound(uint64_t keys,
                                                 size_t num_buckets,
                                                 size_t bucket_capacity,
                                                 double delta) {
    uint64_t deterministic_cap = keys - bucket_capacity;
    long double expected_upper =
        multitable_expected_overflow_upper(keys, num_buckets,
                                           bucket_capacity);
    long double deviation =
        sqrtl((long double)keys * -logl((long double)delta) / 2.0L);
    long double bound1 = ceill(expected_upper + deviation);
    long double bound2 = ceill(expected_upper / (long double)delta);
    long double tilted_overflow;
    long double tilted_unused;
    long double bound;

    if (num_buckets == 1) {
        return deterministic_cap;
    }
    multitable_tilted_overflow_bounds(
        keys, num_buckets, bucket_capacity, delta, &tilted_overflow,
        &tilted_unused);
    tilted_unused +=
        (long double)keys -
        (long double)bucket_capacity * num_buckets;
    if (tilted_unused < 0.0L) {
        tilted_unused = 0.0L;
    }
    bound = fminl((long double)deterministic_cap,
                  fminl(fminl(bound1, bound2),
                        fminl(ceill(tilted_overflow),
                              ceill(tilted_unused))));

    if (bound <= 0.0L) {
        return 0;
    }
    if (bound >= (long double)deterministic_cap) {
        return deterministic_cap;
    }
    return (uint64_t)bound;
}

static inline size_t multitable_buckets_for_load(uint64_t keys,
                                                size_t bucket_capacity,
                                                double alpha,
                                                double delta) {
    long double max_buckets_value =
        floorl((long double)keys /
               ((long double)alpha * bucket_capacity));
    size_t buckets;

    if (max_buckets_value > (long double)SIZE_MAX) {
        return 0;
    }
    buckets = (size_t)max_buckets_value;
    if (buckets == 0) {
        buckets = 1;
    }

    /* Descend from the largest possible n to the largest n meeting the bound. */
    for (;;) {
        uint64_t overflow =
            multitable_overflow_bound(keys, buckets,
                                      bucket_capacity, delta);
        long double stored = (long double)(keys - overflow);
        long double target =
            (long double)alpha * bucket_capacity * buckets;

        if (stored >= target || buckets == 1) {
            return buckets;
        }

        /*
         * The overflow bound is nonincreasing with bucket count, so this
         * fixed-point descent cannot skip a larger feasible bucket count.
         */
        size_t next =
            (size_t)floorl(stored /
                           ((long double)alpha * bucket_capacity));
        if (next == 0) {
            next = 1;
        }
        buckets = next < buckets ? next : buckets - 1;
    }
}

static inline bool multitable_append_bucket_count(size_t **counts,
                                                  size_t *count,
                                                  size_t *capacity,
                                                  size_t value) {
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 4 : *capacity * 2;
        size_t *new_counts;

        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / sizeof(**counts)) {
            return false;
        }
        new_counts = realloc(*counts, new_capacity * sizeof(**counts));
        if (new_counts == NULL) {
            return false;
        }
        *counts = new_counts;
        *capacity = new_capacity;
    }
    (*counts)[(*count)++] = value;
    return true;
}

static inline bool multitable_compute_bucket_counts(size_t expected_values,
                                                    size_t bucket_capacity,
                                                    double alpha,
                                                    double delta,
                                                    size_t **bucket_counts,
                                                    size_t *num_levels) {
    uint64_t remaining = expected_values;
    size_t allocated = 0;

    *bucket_counts = NULL;
    *num_levels = 0;
    if (expected_values == 0) {
        return true;
    }

    while (remaining > bucket_capacity) {
        size_t buckets = multitable_buckets_for_load(
            remaining, bucket_capacity, alpha, delta);
        uint64_t overflow;

        if (buckets == 0 ||
            !multitable_append_bucket_count(bucket_counts, num_levels,
                                            &allocated, buckets)) {
            free(*bucket_counts);
            *bucket_counts = NULL;
            *num_levels = 0;
            return false;
        }

        overflow = multitable_overflow_bound(
            remaining, buckets, bucket_capacity, delta);
        if (overflow >= remaining) {
            free(*bucket_counts);
            *bucket_counts = NULL;
            *num_levels = 0;
            return false;
        }
        remaining = overflow;
    }

    if (!multitable_append_bucket_count(bucket_counts, num_levels,
                                        &allocated, 1)) {
        free(*bucket_counts);
        *bucket_counts = NULL;
        *num_levels = 0;
        return false;
    }
    return true;
}

static inline void multitable_free(FilteredMultiTable *mt);

/*
 * q counts stored values. Each level uses bucket_capacity slots per bucket;
 * alpha and the per-level failure budget delta determine each level's size.
 */
static inline FilteredMultiTable *multitable_create(size_t q,
                                                    size_t bucket_capacity,
                                                    double alpha,
                                                    double delta) {
    size_t *bucket_counts;
    size_t num_levels;
    FilteredMultiTable *mt;

    if (bucket_capacity == 0 ||
        bucket_capacity > MULTITABLE_MAX_BUCKET_CAPACITY ||
        !(alpha > 0.0 && alpha < 1.0) ||
        !(delta > 0.0 && delta < 1.0) ||
        !multitable_compute_bucket_counts(q, bucket_capacity, alpha, delta,
                                          &bucket_counts, &num_levels)) {
        return NULL;
    }

    mt = calloc(1, sizeof(*mt));
    if (mt == NULL) {
        free(bucket_counts);
        return NULL;
    }
    mt->num_levels = num_levels;
    mt->bucket_capacity = bucket_capacity;
    mt->expected_values = q;
    mt->target_load = alpha;
    mt->failure_budget = delta;

    if (num_levels != 0) {
        if (num_levels > SIZE_MAX / sizeof(*mt->levels)) {
            free(bucket_counts);
            free(mt);
            return NULL;
        }
        mt->levels = calloc(num_levels, sizeof(*mt->levels));
        if (mt->levels == NULL) {
            free(bucket_counts);
            free(mt);
            return NULL;
        }
    }

    for (size_t i = 0; i < num_levels; i++) {
        TableLevel *level = &mt->levels[i];

        level->num_buckets = bucket_counts[i];
        if (level->num_buckets > UINT32_MAX ||
            level->num_buckets > SIZE_MAX / bucket_capacity) {
            free(bucket_counts);
            multitable_free(mt);
            return NULL;
        }
        level->num_slots = level->num_buckets * bucket_capacity;
        if (level->num_slots > SIZE_MAX / sizeof(*level->slots)) {
            free(bucket_counts);
            multitable_free(mt);
            return NULL;
        }

        level->slots = calloc(level->num_slots, sizeof(*level->slots));
        level->filter_tags =
            calloc(level->num_slots, sizeof(*level->filter_tags));
        if (level->slots == NULL || level->filter_tags == NULL) {
            free(bucket_counts);
            multitable_free(mt);
            return NULL;
        }
        for (size_t slot = 0; slot < level->num_slots; slot++) {
            htable_list_init(&level->slots[slot].list);
        }
    }

    free(bucket_counts);
    return mt;
}

/* Borrowed nodes with equal full hashes share one collision list. */
static inline int multitable_insert(FilteredMultiTable *mt,
                                    struct htable_list_node *node,
                                    uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);

    if (mt == NULL || node == NULL) {
        return 0;
    }

    /* Reuse the existing list for this full hash. */
    for (size_t lvl = 0; lvl < mt->num_levels; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx =
            multitable_bucket_index(hash, level->num_buckets, lvl);
        size_t next_slot = 0;
        size_t slot_idx;

        while (multitable_find_tag_slot(level, bucket_idx,
                                        mt->bucket_capacity, tag,
                                        next_slot, &slot_idx)) {
            PointerHashSlot *slot =
                &level->slots[bucket_idx * mt->bucket_capacity + slot_idx];

            next_slot = slot_idx + 1;

            if (slot->hash == hash) {
                if (multitable_list_contains(&slot->list, node)) {
                    return 1;
                }
                if (node->pprev != NULL) {
                    return 0;
                }
                node->hash = hash;
                htable_list_add(&slot->list, node);
                return 1;
            }
        }
    }

    if (node->pprev != NULL) {
        return 0;
    }

    for (size_t lvl = 0; lvl < mt->num_levels; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx =
            multitable_bucket_index(hash, level->num_buckets, lvl);
        size_t slot_idx;

        if (multitable_find_tag_slot(level, bucket_idx, mt->bucket_capacity,
                                     EMPTY_METADATA, 0, &slot_idx)) {
            size_t slot_offset =
                bucket_idx * mt->bucket_capacity + slot_idx;
            PointerHashSlot *slot = &level->slots[slot_offset];

            slot->hash = hash;
            node->hash = hash;
            htable_list_add(&slot->list, node);
            level->filter_tags[slot_offset] = tag;
            return 1;
        }
        /* Bucket is saturated at this level; the key falls through to the next level array */
    }
    return 0; /* All levels exhausted (Capacity Exception / Resize Trigger) */
}

static inline int multitable_delete(FilteredMultiTable *mt,
                                    struct htable_list_node *node,
                                    uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);

    if (mt == NULL || node == NULL) {
        return 0;
    }

    for (size_t lvl = 0; lvl < mt->num_levels; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx =
            multitable_bucket_index(hash, level->num_buckets, lvl);
        size_t next_slot = 0;
        size_t slot_idx;

        while (multitable_find_tag_slot(level, bucket_idx,
                                        mt->bucket_capacity, tag,
                                        next_slot, &slot_idx)) {
            size_t slot_offset =
                bucket_idx * mt->bucket_capacity + slot_idx;
            PointerHashSlot *slot = &level->slots[slot_offset];

            next_slot = slot_idx + 1;

            if (slot->hash == hash &&
                multitable_list_contains(&slot->list, node)) {
                __htable_list_delete(&slot->list, node);
                if (slot->list.tail == NULL) {
                    slot->hash = 0;
                    level->filter_tags[slot_offset] = EMPTY_METADATA;
                }
                return 1;
            }
        }
    }
    return 0;
}

static inline bool multitable_has(FilteredMultiTable *mt,
                                  const struct htable_list_node *node,
                                  uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);

    if (mt == NULL || node == NULL) {
        return false;
    }

    for (size_t lvl = 0; lvl < mt->num_levels; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx =
            multitable_bucket_index(hash, level->num_buckets, lvl);
        size_t next_slot = 0;
        size_t slot_idx;

        while (multitable_find_tag_slot(level, bucket_idx,
                                        mt->bucket_capacity, tag,
                                        next_slot, &slot_idx)) {
            size_t slot_offset =
                bucket_idx * mt->bucket_capacity + slot_idx;
            PointerHashSlot *slot = &level->slots[slot_offset];

            next_slot = slot_idx + 1;
            if (slot->hash == hash &&
                multitable_list_contains(&slot->list, node)) {
                return 1;
            }
        }
    }
    return 0;
}

/* The table owns its allocation and unlinks borrowed nodes before freeing it. */
static inline void multitable_free(FilteredMultiTable *mt) {
    if (mt == NULL) {
        return;
    }

    if (mt->levels != NULL) {
        for (size_t lvl = 0; lvl < mt->num_levels; lvl++) {
            TableLevel *level = &mt->levels[lvl];

            if (level->slots != NULL) {
                for (size_t slot = 0; slot < level->num_slots; slot++) {
                    struct htable_list *list = &level->slots[slot].list;

                    while (list->tail != NULL) {
                        __htable_list_delete(list, list->tail);
                    }
                }
            }
            free(level->slots);
            free(level->filter_tags);
        }
    }
    free(mt->levels);
    free(mt);
}

#ifdef HTABLE_MULTITABLE_TEST_MAIN
/* Verification Runner */
int main(void) {
    FilteredMultiTable *mt =
        multitable_create(8, 16, 0.75, 1.0 / 1024.0);
    struct htable_list_node key_objects[5] = { 0 };
    const uint32_t hashes[5] = {
        0x12345611, 0x12345611, 0x12345711, 0x76543222, 0xabcdef33
    };

    if (mt == NULL) {
        fprintf(stderr, "Failed to allocate multitable.\n");
        return 1;
    }

    for (size_t i = 0; i < 5; i++) {
        htable_list_node_init(&key_objects[i]);
    }

    printf("Inserting list-node/hash pairs...\n");
    multitable_insert(mt, &key_objects[0], hashes[0]);
    multitable_insert(mt, &key_objects[1], hashes[1]);
    multitable_insert(mt, &key_objects[2], hashes[2]);

    if (multitable_has(mt, &key_objects[1], hashes[1])) {
        printf("Hit Success! Node and hash matched.\n");
    } else {
        printf("Hit Failed!\n");
    }

    if (multitable_has(mt, &key_objects[1], hashes[2])) {
        fprintf(stderr, "A different hash unexpectedly matched.\n");
        multitable_free(mt);
        return 1;
    }
    if (multitable_has(mt, &key_objects[3], hashes[3])) {
        fprintf(stderr, "An uninserted pointer unexpectedly matched.\n");
        multitable_free(mt);
        return 1;
    }

    if (!multitable_delete(mt, &key_objects[1], hashes[1]) ||
        multitable_has(mt, &key_objects[1], hashes[1])) {
        fprintf(stderr, "Delete verification failed.\n");
        multitable_free(mt);
        return 1;
    }

    if (multitable_delete(mt, &key_objects[1], hashes[1])) {
        fprintf(stderr, "Deleting a missing pointer/hash pair unexpectedly succeeded.\n");
        multitable_free(mt);
        return 1;
    }

    multitable_free(mt);

    mt = multitable_create(5, 16, 0.75, 1.0 / 1024.0);
    if (mt == NULL ||
        !multitable_insert(mt, &key_objects[0], 0x11) ||
        !multitable_insert(mt, &key_objects[1], 0x11) ||
        !multitable_insert(mt, &key_objects[2], 0x111) ||
        !multitable_insert(mt, &key_objects[3], 0x211) ||
        !multitable_insert(mt, &key_objects[4], 0x311) ||
        !multitable_delete(mt, &key_objects[0], 0x11) ||
        !multitable_insert(mt, &key_objects[4], 0x311) ||
        !multitable_has(mt, &key_objects[4], 0x311) ||
        !multitable_delete(mt, &key_objects[4], 0x311) ||
        multitable_has(mt, &key_objects[4], 0x311)) {
        fprintf(stderr, "List/hash verification failed.\n");
        multitable_free(mt);
        return 1;
    }
    multitable_free(mt);

    {
        struct htable_list_node sized_nodes[17] = { 0 };

        mt = multitable_create(17, 16, 0.75, 1.0 / 1024.0);
        if (mt == NULL || mt->num_levels != 2 ||
            mt->levels[0].num_buckets != 1 ||
            mt->levels[1].num_buckets != 1) {
            fprintf(stderr, "Paper-style level sizing verification failed.\n");
            multitable_free(mt);
            return 1;
        }

        for (size_t i = 0; i < 17; i++) {
            htable_list_node_init(&sized_nodes[i]);
            if (!multitable_insert(mt, &sized_nodes[i],
                                   0x2000u + (uint32_t)i)) {
                fprintf(stderr, "Multilevel insertion verification failed.\n");
                multitable_free(mt);
                return 1;
            }
        }
        for (size_t i = 0; i < 17; i++) {
            if (!multitable_has(mt, &sized_nodes[i],
                                0x2000u + (uint32_t)i)) {
                fprintf(stderr, "Multilevel lookup verification failed.\n");
                multitable_free(mt);
                return 1;
            }
        }
        multitable_free(mt);
        for (size_t i = 0; i < 17; i++) {
            if (sized_nodes[i].pprev != NULL) {
                fprintf(stderr, "Multilevel cleanup verification failed.\n");
                return 1;
            }
        }
    }

    mt = multitable_create(100, 16, 0.75, 1.0 / 1024.0);
    if (mt == NULL || mt->num_levels < 2) {
        fprintf(stderr, "Overflow-bound sizing verification failed.\n");
        multitable_free(mt);
        return 1;
    }
    multitable_free(mt);

    return 0;
}
#endif

#endif /* HTABLE_MULTITABLE_H */
