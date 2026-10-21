#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef HTABLE_MULTITABLE_H
#define HTABLE_MULTITABLE_H

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

#include "swar_common.h"

#define NUM_LEVELS 3
#define BUCKET_CAPACITY 4
#define EMPTY_METADATA 0x00

/* Derives an 8-bit non-zero signature tag for the Filtered layer */
static inline uint8_t generate_filter_tag(uint32_t hash) {
    uint8_t tag = (uint8_t)(hash & 0xFF);
    return (tag == EMPTY_METADATA) ? 0xAA : tag; 
}

typedef struct {
    struct   htable_list list;
    uint32_t hash;
} PointerHashSlot;

typedef struct {
    PointerHashSlot slots[BUCKET_CAPACITY];
} Bucket;

typedef struct {
    uint8_t *filter_tags; /* Flat array of 8-bit metadata (size: num_buckets * BUCKET_CAPACITY) */
    Bucket *buckets;      /* Flat array of Buckets */
    size_t num_buckets;
} TableLevel;

typedef struct {
    TableLevel levels[NUM_LEVELS];
    max_align_t storage[];
} FilteredMultiTable;

static inline uint32_t multitable_tag_matches(const TableLevel *level,
                                              size_t bucket_idx, uint8_t tag) {
    uint32_t packed_tags;
    size_t filter_offset = bucket_idx * BUCKET_CAPACITY;

    memcpy(&packed_tags, level->filter_tags + filter_offset,
           sizeof(packed_tags));
    return (uint32_t)swar_match_u8(packed_tags, tag, BUCKET_CAPACITY);
}

/* Memory Allocation & Initialization */
static inline FilteredMultiTable* multitable_create(size_t base_buckets) {
    size_t bucket_counts[NUM_LEVELS];
    size_t bucket_bytes = 0;
    size_t tag_bytes = 0;
    size_t current_buckets = base_buckets;
    size_t allocation_bytes;
    size_t bucket_offset = 0;
    size_t tag_offset;
    int i;

    if (base_buckets == 0) {
        return NULL;
    }

    for (i = 0; i < NUM_LEVELS; i++) {
        size_t level_bucket_bytes;
        size_t level_tag_bytes;

        bucket_counts[i] = current_buckets;
        if (current_buckets > SIZE_MAX / sizeof(Bucket) ||
            current_buckets > SIZE_MAX / BUCKET_CAPACITY) {
            return NULL;
        }

        level_bucket_bytes = current_buckets * sizeof(Bucket);
        level_tag_bytes = current_buckets * BUCKET_CAPACITY;
        if (level_bucket_bytes > SIZE_MAX - bucket_bytes ||
            level_tag_bytes > SIZE_MAX - tag_bytes) {
            return NULL;
        }
        bucket_bytes += level_bucket_bytes;
        tag_bytes += level_tag_bytes;

        /* Cascade down: Each level is strictly smaller than the previous one */
        current_buckets /= 2;
        if (current_buckets == 0) current_buckets = 1;
    }

    if (bucket_bytes > SIZE_MAX - sizeof(FilteredMultiTable)) {
        return NULL;
    }
    allocation_bytes = sizeof(FilteredMultiTable) + bucket_bytes;
    if (tag_bytes > SIZE_MAX - allocation_bytes) {
        return NULL;
    }
    allocation_bytes += tag_bytes;

    FilteredMultiTable *mt = (FilteredMultiTable*)calloc(1, allocation_bytes);
    if (mt == NULL) {
        return NULL;
    }

    unsigned char *storage = (unsigned char*)mt->storage;
    tag_offset = bucket_bytes;
    for (i = 0; i < NUM_LEVELS; i++) {
        size_t level_bucket_bytes = bucket_counts[i] * sizeof(Bucket);

        mt->levels[i].num_buckets = bucket_counts[i];
        mt->levels[i].buckets = (Bucket*)(storage + bucket_offset);
        mt->levels[i].filter_tags = storage + tag_offset;

        bucket_offset += level_bucket_bytes;
        tag_offset += bucket_counts[i] * BUCKET_CAPACITY;
    }
    return mt;
}

/* The table stores borrowed pointers; their owners manage pointed-to memory. */
static inline int multitable_insert(FilteredMultiTable *mt,
                                    const void *key_ptr, uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    /* Avoid storing the same pointer/hash pair more than once. */
    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        uint32_t matching_slots =
            multitable_tag_matches(level, bucket_idx, tag);

        while (matching_slots != 0) {
            s = __builtin_ctz(matching_slots) >> 3;
            matching_slots &= matching_slots - 1;
            if (b->slots[s].hash == hash &&
                b->slots[s].key_ptr == key_ptr) {
                return 1;
            }
        }
    }

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        uint32_t empty_slots =
            multitable_tag_matches(level, bucket_idx, EMPTY_METADATA);

        if (empty_slots != 0) {
            s = __builtin_ctz(empty_slots) >> 3;
            b->slots[s].key_ptr = key_ptr;
            b->slots[s].hash = hash;
            level->filter_tags[bucket_idx * BUCKET_CAPACITY + s] = tag;
            return 1;
        }
        /* Bucket is saturated at this level; the key falls through to the next level array */
    }
    return 0; /* All levels exhausted (Capacity Exception / Resize Trigger) */
}

static inline int multitable_delete(FilteredMultiTable *mt,
                                    const void *key_ptr, uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        uint32_t matching_slots =
            multitable_tag_matches(level, bucket_idx, tag);

        while (matching_slots != 0) {
            s = __builtin_ctz(matching_slots) >> 3;
            matching_slots &= matching_slots - 1;
            if (b->slots[s].hash == hash &&
                b->slots[s].key_ptr == key_ptr) {
                b->slots[s].key_ptr = NULL;
                b->slots[s].hash = 0;
                level->filter_tags[bucket_idx * BUCKET_CAPACITY + s] =
                    EMPTY_METADATA;
                return 1;
            }
        }
    }
    return 0;
}

static inline bool multitable_has(FilteredMultiTable *mt,
                                  const void *key_ptr, uint32_t hash) {
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        uint32_t matching_slots =
            multitable_tag_matches(level, bucket_idx, tag);

        while (matching_slots != 0) {
            s = __builtin_ctz(matching_slots) >> 3;
            matching_slots &= matching_slots - 1;
            if (b->slots[s].hash == hash &&
                b->slots[s].key_ptr == key_ptr) {
                return 1;
            }
        }
    }
    return 0;
}

/* The table owns its backing allocation, not the pointed-to keys. */
static inline void multitable_free(FilteredMultiTable *mt) {
    free(mt);
}

#ifdef HTABLE_MULTITABLE_TEST_MAIN
/* Verification Runner */
int main(void) {
    FilteredMultiTable *mt = multitable_create(8); /* Init Level 0 with 8 buckets */
    int key_objects[5] = { 0 };
    const uint32_t hashes[5] = {
        0x12345611, 0x12345611, 0x12345711, 0x76543222, 0xabcdef33
    };

    if (mt == NULL) {
        fprintf(stderr, "Failed to allocate multitable.\n");
        return 1;
    }

    printf("Inserting pointer/hash pairs...\n");
    multitable_insert(mt, &key_objects[0], hashes[0]);
    multitable_insert(mt, &key_objects[1], hashes[1]);
    multitable_insert(mt, &key_objects[2], hashes[2]);

    if (multitable_has(mt, &key_objects[1], hashes[1])) {
        printf("Hit Success! Pointer and hash matched.\n");
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

    mt = multitable_create(1);
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
        fprintf(stderr, "Cascaded pointer/hash verification failed.\n");
        multitable_free(mt);
        return 1;
    }
    multitable_free(mt);
    return 0;
}
#endif

#endif /* HTABLE_MULTITABLE_H */
