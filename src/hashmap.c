#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================
 * 1. Hash Function Implementation
 * ================================================== */

/**
 * Polynomial rolling hash function.
 * 
 * Algorithm:
 *   hash_0 = 0
 *   hash_{i+1} = (hash_i * 31 + char_i) % TABLE_SIZE
 * 
 * Educational Notes:
 *  - Multiplier 31: A small prime number that provides a good distribution
 *    of ASCII characters and maps well to bit shifts: (hash << 5) - hash.
 *  - Modulo TABLE_SIZE: Applied at each step to prevent integer overflow while
 *    ensuring the final hash maps directly to a valid bucket index [0, TABLE_SIZE - 1].
 */
size_t hashmap_hash(const char *shortCode) {
    if (shortCode == NULL) {
        return 0;
    }

    size_t hash = 0;
    while (*shortCode != '\0') {
        hash = (hash * 31 + (unsigned char)*shortCode) % TABLE_SIZE;
        shortCode++;
    }

    return hash;
}

/* ==================================================
 * 2. Lifecycle & Initialization
 * ================================================== */

void hashmap_init(HashMap *map) {
    if (map == NULL) {
        return;
    }

    for (size_t i = 0; i < TABLE_SIZE; i++) {
        map->buckets[i] = NULL;
    }
    map->count = 0;
}

HashMap* hashmap_create(void) {
    HashMap *map = (HashMap *)malloc(sizeof(HashMap));
    if (map == NULL) {
        return NULL;
    }

    hashmap_init(map);
    return map;
}

void hashmap_clear(HashMap *map) {
    if (map == NULL) {
        return;
    }

    /* Free all linked list nodes across every bucket */
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        url_list_clear(&map->buckets[i]);
    }

    map->count = 0;
}

void hashmap_destroy(HashMap *map) {
    if (map == NULL) {
        return;
    }

    hashmap_clear(map);
    free(map);
}

/* ==================================================
 * 3. Core Operations: Insert, Get, Remove
 * ================================================== */

int hashmap_insert(HashMap *map, URLRecord *record) {
    if (map == NULL || record == NULL) {
        return 0;
    }

    /* Validate the key format */
    if (!url_is_valid_short_code(record->shortCode)) {
        return 0;
    }

    /* 1. Calculate target bucket index using the hash function */
    size_t index = hashmap_hash(record->shortCode);

    /* 2. Check for duplicate short code in the target bucket chain */
    if (url_list_find(map->buckets[index], record->shortCode) != NULL) {
        return 0; /* Duplicate key rejected */
    }

    /* 3. Insert record into bucket's singly linked list */
    if (!url_list_append(&map->buckets[index], record)) {
        return 0;
    }

    /* 4. Increment stored record count only upon successful insertion */
    map->count++;
    return 1;
}

URLRecord* hashmap_get(HashMap *map, const char *shortCode) {
    if (map == NULL || shortCode == NULL) {
        return NULL;
    }

    /* 1. Compute bucket index via hash */
    size_t index = hashmap_hash(shortCode);

    /* 2. Search ONLY the target bucket's linked list chain: O(1 + alpha) */
    return url_list_find(map->buckets[index], shortCode);
}

URLRecord* hashmap_search(const HashMap *map, const char *shortCode) {
    if (map == NULL || shortCode == NULL) {
        return NULL;
    }

    size_t index = hashmap_hash(shortCode);
    return url_list_find(map->buckets[index], shortCode);
}

int hashmap_remove(HashMap *map, const char *shortCode) {
    if (map == NULL || shortCode == NULL) {
        return 0;
    }

    /* 1. Compute bucket index */
    size_t index = hashmap_hash(shortCode);

    /* 2. Remove node from bucket chain and free memory */
    if (url_list_remove(&map->buckets[index], shortCode)) {
        map->count--;
        return 1;
    }

    return 0; /* Not found */
}

/* ==================================================
 * 4. Traversal & Statistics Support
 * ================================================== */

void hashmap_foreach(const HashMap *map, void (*callback)(const URLRecord *record)) {
    if (map == NULL || callback == NULL) {
        return;
    }

    for (size_t i = 0; i < TABLE_SIZE; i++) {
        const URLRecord *curr = map->buckets[i];
        while (curr != NULL) {
            callback(curr);
            curr = curr->next;
        }
    }
}

HashMapStats hashmap_get_stats(const HashMap *map) {
    HashMapStats stats;
    stats.totalRecords = 0;
    stats.tableSize = TABLE_SIZE;
    stats.usedBuckets = 0;
    stats.collidedBuckets = 0;
    stats.collisions = 0;
    stats.maxChainLength = 0;
    stats.loadFactor = 0.0;

    if (map == NULL) {
        return stats;
    }

    stats.totalRecords = map->count;

    for (size_t i = 0; i < TABLE_SIZE; i++) {
        size_t chainLen = url_list_length(map->buckets[i]);
        if (chainLen > 0) {
            stats.usedBuckets++;
            if (chainLen > 1) {
                stats.collidedBuckets++;
                /* A collision occurs when a record is inserted into a bucket
                 * that already contains at least one record. Thus, for chainLen k,
                 * (k - 1) records experienced collisions upon insertion. */
                stats.collisions += (chainLen - 1);
            }
            if (chainLen > stats.maxChainLength) {
                stats.maxChainLength = chainLen;
            }
        }
    }

    stats.loadFactor = (double)stats.totalRecords / (double)TABLE_SIZE;
    return stats;
}

void hashmap_print_stats(const HashMap *map) {
    if (map == NULL) {
        printf("HashMap: (null)\n");
        return;
    }

    HashMapStats stats = hashmap_get_stats(map);
    printf("==================================================================\n");
    printf("                   Hash Table Distribution Stats                  \n");
    printf("==================================================================\n");
    printf(" Total Stored Records : %zu\n", stats.totalRecords);
    printf(" Hash Table Size      : %zu buckets\n", stats.tableSize);
    printf(" Occupied Buckets     : %zu\n", stats.usedBuckets);
    printf(" Empty Buckets        : %zu\n", stats.tableSize - stats.usedBuckets);
    printf(" Collided Buckets     : %zu\n", stats.collidedBuckets);
    printf(" Collision Count      : %zu\n", stats.collisions);
    printf(" Maximum Chain Length : %zu\n", stats.maxChainLength);
    printf(" Load Factor (alpha)  : %.4f\n", stats.loadFactor);
    printf("==================================================================\n");
}
