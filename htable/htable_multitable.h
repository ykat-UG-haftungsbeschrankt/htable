#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define NUM_LEVELS 3
#define BUCKET_CAPACITY 4
#define EMPTY_METADATA 0x00

/* Simple MurmurHash3-inspired 32-bit mix for ANSI C string keys */
uint32_t hash_key(const char *key) {
    uint32_t h = 0x811c9dc5;
    while (*key) {
        h ^= (uint32_t)(*key++);
        h *= 0x01000193;
    }
    return h;
}

/* Derives an 8-bit non-zero signature tag for the Filtered layer */
uint8_t generate_filter_tag(uint32_t hash) {
    uint8_t tag = (uint8_t)(hash & 0xFF);
    return (tag == EMPTY_METADATA) ? 0xAA : tag; 
}

typedef struct {
    char *key;
    int value;
} KeyValueSlot;

typedef struct {
    KeyValueSlot slots[BUCKET_CAPACITY];
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

/* Memory Allocation & Initialization */
FilteredMultiTable* multitable_create(size_t base_buckets) {
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

/* Insertion Algorithm: Try to place in Level 0, cascade downward on conflict */
int multitable_insert(FilteredMultiTable *mt, const char *key, int value) {
    uint32_t hash = hash_key(key);
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    /* Update an existing key before using any newly available slot. */
    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        size_t filter_offset = bucket_idx * BUCKET_CAPACITY;

        for (s = 0; s < BUCKET_CAPACITY; s++) {
            uint8_t current_tag = level->filter_tags[filter_offset + s];

            if (current_tag == tag && b->slots[s].key != NULL && strcmp(b->slots[s].key, key) == 0) {
                b->slots[s].value = value; /* Key already exists, update value */
                return 1;
            }
        }
    }

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        size_t filter_offset = bucket_idx * BUCKET_CAPACITY;

        for (s = 0; s < BUCKET_CAPACITY; s++) {
            uint8_t current_tag = level->filter_tags[filter_offset + s];
            if (current_tag == EMPTY_METADATA) {
                /* Empty slot found: claim it */
                b->slots[s].key = malloc(strlen(key) + 1);
                strcpy(b->slots[s].key, key);
                b->slots[s].value = value;
                level->filter_tags[filter_offset + s] = tag; /* Arm the 8-bit filter */
                return 1; 
            }
        }
        /* Bucket is saturated at this level; the key falls through to the next level array */
    }
    return 0; /* All levels exhausted (Capacity Exception / Resize Trigger) */
}

/* Delete a key and release its string storage. */
int multitable_delete(FilteredMultiTable *mt, const char *key) {
    uint32_t hash = hash_key(key);
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        size_t filter_offset = bucket_idx * BUCKET_CAPACITY;

        for (s = 0; s < BUCKET_CAPACITY; s++) {
            if (level->filter_tags[filter_offset + s] == tag &&
                b->slots[s].key != NULL &&
                strcmp(b->slots[s].key, key) == 0) {
                free(b->slots[s].key);
                b->slots[s].key = NULL;
                b->slots[s].value = 0;
                level->filter_tags[filter_offset + s] = EMPTY_METADATA;
                return 1;
            }
        }
    }
    return 0;
}

/* Lookup Algorithm: Leverage the 8-bit filter tags to skip unneeded string matches */
int multitable_get(FilteredMultiTable *mt, const char *key, int *out_value) {
    uint32_t hash = hash_key(key);
    uint8_t tag = generate_filter_tag(hash);
    int lvl, s;

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        size_t bucket_idx = hash % level->num_buckets;
        Bucket *b = &level->buckets[bucket_idx];
        size_t filter_offset = bucket_idx * BUCKET_CAPACITY;

        for (s = 0; s < BUCKET_CAPACITY; s++) {
            uint8_t current_tag = level->filter_tags[filter_offset + s];

            /* FAST PATH FILTER CHECK: If metadata doesn't match, instantly pass */
            if (current_tag == EMPTY_METADATA) {
                continue; 
            }
            if (current_tag == tag) {
                /* SLOW PATH: Resolve deep comparison only if the filter tag passes */
                if (b->slots[s].key != NULL && strcmp(b->slots[s].key, key) == 0) {
                    *out_value = b->slots[s].value;
                    return 1; /* Key found */
                }
            }
        }
    }
    return 0; /* Key completely missing (Fast negative lookup) */
}

/* Memory Deallocation */
void multitable_free(FilteredMultiTable *mt) {
    int lvl, s;
    size_t b;
    if (mt == NULL) {
        return;
    }

    for (lvl = 0; lvl < NUM_LEVELS; lvl++) {
        TableLevel *level = &mt->levels[lvl];
        for (b = 0; b < level->num_buckets; b++) {
            for (s = 0; s < BUCKET_CAPACITY; s++) {
                if (level->buckets[b].slots[s].key != NULL) {
                    free(level->buckets[b].slots[s].key);
                }
            }
        }
    }
    free(mt);
}

/* Verification Runner */
int main(void) {
    FilteredMultiTable *mt = multitable_create(8); /* Init Level 0 with 8 buckets */
    int retrieved_value = 0;

    if (mt == NULL) {
        fprintf(stderr, "Failed to allocate multitable.\n");
        return 1;
    }

    printf("Inserting Key-Value Pairs...\n");
    multitable_insert(mt, "user_id_102", 9942);
    multitable_insert(mt, "session_token", 5511);
    multitable_insert(mt, "api_key", 8810);

    /* Test Hit (Positive Lookup) */
    if (multitable_get(mt, "session_token", &retrieved_value)) {
        printf("Hit Success! session_token = %d\n", retrieved_value);
    } else {
        printf("Hit Failed!\n");
    }

    /* Test Miss (Negative Lookup utilizing Fast Byte Filters) */
    if (multitable_get(mt, "invalid_key", &retrieved_value)) {
        printf("Found unexpected key!\n");
    } else {
        printf("Miss Success! 'invalid_key' was quickly dismissed by the filters.\n");
    }

    if (!multitable_delete(mt, "session_token") ||
        multitable_get(mt, "session_token", &retrieved_value)) {
        fprintf(stderr, "Delete verification failed.\n");
        multitable_free(mt);
        return 1;
    }

    if (multitable_delete(mt, "session_token")) {
        fprintf(stderr, "Deleting a missing key unexpectedly succeeded.\n");
        multitable_free(mt);
        return 1;
    }

    multitable_free(mt);

    mt = multitable_create(1);
    if (mt == NULL ||
        !multitable_insert(mt, "collision-1", 1) ||
        !multitable_insert(mt, "collision-2", 2) ||
        !multitable_insert(mt, "collision-3", 3) ||
        !multitable_insert(mt, "collision-4", 4) ||
        !multitable_insert(mt, "overflow-key", 5) ||
        !multitable_delete(mt, "collision-1") ||
        !multitable_insert(mt, "overflow-key", 6) ||
        !multitable_get(mt, "overflow-key", &retrieved_value) ||
        retrieved_value != 6 ||
        !multitable_delete(mt, "overflow-key") ||
        multitable_get(mt, "overflow-key", &retrieved_value)) {
        fprintf(stderr, "Cascaded delete/reinsert verification failed.\n");
        multitable_free(mt);
        return 1;
    }
    multitable_free(mt);
    return 0;
}
