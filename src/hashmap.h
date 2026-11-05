#ifndef HASHMAP_H
#define HASHMAP_H

#include <stddef.h>
#include "url.h"

/**
 * Prime table capacity to minimize clustering when combined with
 * the polynomial rolling hash function.
 */
#define TABLE_SIZE 101

/**
 * Hash Table with Separate Chaining collision handling.
 * 
 * Conceptual Layout:
 *   HashMap
 *       |
 *       +-- buckets[0]   -> URLRecord -> URLRecord -> NULL
 *       +-- buckets[1]   -> NULL
 *       +-- buckets[2]   -> URLRecord -> NULL
 *       ...
 *       +-- buckets[100] -> URLRecord -> NULL
 * 
 * Fields:
 *  - buckets: Array of pointers to the head nodes of singly linked lists.
 *  - count: Total number of records currently stored in the hash table.
 */
typedef struct HashMap {
    URLRecord *buckets[TABLE_SIZE];
    size_t count;
} HashMap;

/**
 * Snapshot of hash table distribution metrics for DSA analysis.
 */
typedef struct HashMapStats {
    size_t totalRecords;       /* Total records stored in table */
    size_t tableSize;          /* Fixed bucket capacity (TABLE_SIZE) */
    size_t usedBuckets;        /* Buckets containing at least one record */
    size_t collidedBuckets;    /* Buckets containing more than one record */
    size_t collisions;         /* Total collision count: totalRecords - usedBuckets */
    size_t maxChainLength;     /* Length of the longest linked list chain */
    double loadFactor;         /* Ratio of totalRecords to tableSize */
} HashMapStats;

/* ==================================================
 * 1. Hash Function
 * ================================================== */

/**
 * Polynomial rolling hash function.
 * Computes: hash = (hash * 31 + (unsigned char)character) % TABLE_SIZE
 * 
 * This mathematical formula converts an arbitrary alphanumeric shortCode
 * into a valid bucket array index in the range [0, TABLE_SIZE - 1].
 * Horner's rule with prime multiplier 31 ensures good distribution.
 * 
 * @param shortCode Null-terminated short code string.
 * @return Bucket index between 0 and TABLE_SIZE - 1.
 */
size_t hashmap_hash(const char *shortCode);

/* ==================================================
 * 2. Lifecycle & Initialization
 * ================================================== */

/**
 * Initialize a HashMap instance.
 * Sets all bucket head pointers to NULL and element count to 0.
 * 
 * @param map Pointer to HashMap structure to initialize.
 */
void hashmap_init(HashMap *map);

/**
 * Dynamically allocate and initialize a new HashMap.
 * 
 * @return Pointer to newly allocated HashMap, or NULL on allocation failure.
 */
HashMap* hashmap_create(void);

/**
 * Free all URLRecord nodes across all bucket chains and reset the table.
 * 
 * @param map Pointer to HashMap.
 */
void hashmap_clear(HashMap *map);

/**
 * Free all stored records and free the dynamically allocated HashMap itself.
 * 
 * @param map Pointer to HashMap.
 */
void hashmap_destroy(HashMap *map);

/* ==================================================
 * 3. Core Operations (Insert, Get, Remove)
 * ================================================== */

/**
 * Insert a URLRecord into the hash table.
 * Rejects duplicate short codes and invalid inputs.
 * 
 * @param map Pointer to HashMap.
 * @param record Pointer to dynamically allocated URLRecord to insert.
 * @return 1 on success, 0 on failure (NULL input, invalid code, or duplicate).
 */
int hashmap_insert(HashMap *map, URLRecord *record);

/**
 * Retrieve a URLRecord from the hash table by its shortCode.
 * Hashes the key and searches ONLY the corresponding bucket chain.
 * 
 * @param map Pointer to HashMap.
 * @param shortCode The lookup key.
 * @return Pointer to matching URLRecord, or NULL if not found.
 */
URLRecord* hashmap_get(HashMap *map, const char *shortCode);

/**
 * Search the hash table (const-safe alias for hashmap_get).
 * 
 * @param map Pointer to HashMap.
 * @param shortCode The lookup key.
 * @return Pointer to matching URLRecord, or NULL if not found.
 */
URLRecord* hashmap_search(const HashMap *map, const char *shortCode);

/**
 * Remove and free a URLRecord matching shortCode from the hash table.
 * Decrements map element count upon successful removal.
 * 
 * @param map Pointer to HashMap.
 * @param shortCode The key of the record to delete.
 * @return 1 if found and removed, 0 if not found or invalid input.
 */
int hashmap_remove(HashMap *map, const char *shortCode);

/* ==================================================
 * 4. Traversal & Statistics
 * ================================================== */

/**
 * Traverse every stored record across all bucket chains.
 * Invokes the callback function exactly once for each record.
 * 
 * @param map Pointer to HashMap.
 * @param callback Function pointer invoked for each URLRecord.
 */
void hashmap_foreach(const HashMap *map, void (*callback)(const URLRecord *record));

/**
 * Calculate distribution and collision metrics for DSA analysis.
 * 
 * @param map Pointer to HashMap.
 * @return Populated HashMapStats structure.
 */
HashMapStats hashmap_get_stats(const HashMap *map);

/**
 * Display formatted hash table statistics to stdout.
 * 
 * @param map Pointer to HashMap.
 */
void hashmap_print_stats(const HashMap *map);

#endif /* HASHMAP_H */
