#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "url.h"
#include "hashmap.h"

/* Simple test framework macros */
static int hm_tests_run = 0;
static int hm_tests_passed = 0;

#define HM_ASSERT_TRUE(condition, message) do { \
    hm_tests_run++; \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        hm_tests_passed++; \
    } \
} while (0)

#define HM_ASSERT_FALSE(condition, message) HM_ASSERT_TRUE(!(condition), message)

#define HM_ASSERT_EQUAL_INT(actual, expected, message) do { \
    hm_tests_run++; \
    if ((actual) != (expected)) { \
        printf("  [FAIL] %s:%d: %s (expected %ld, got %ld)\n", \
               __FILE__, __LINE__, message, (long)(expected), (long)(actual)); \
        return 0; \
    } else { \
        hm_tests_passed++; \
    } \
} while (0)

#define HM_ASSERT_EQUAL_STR(actual, expected, message) do { \
    hm_tests_run++; \
    if (strcmp((actual), (expected)) != 0) { \
        printf("  [FAIL] %s:%d: %s (expected '%s', got '%s')\n", \
               __FILE__, __LINE__, message, expected, actual); \
        return 0; \
    } else { \
        hm_tests_passed++; \
    } \
} while (0)

/* Test A: Initialization */
static int test_initialization(void) {
    printf("Running test: HashMap Initialization...\n");

    HashMap map;
    hashmap_init(&map);

    HM_ASSERT_EQUAL_INT(map.count, 0, "Initial count must be 0");
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        HM_ASSERT_TRUE(map.buckets[i] == NULL, "All buckets must initialize to NULL");
    }

    return 1;
}

/* Test B: Hash Function Properties */
static int test_hash_function(void) {
    printf("Running test: Polynomial Hash Function...\n");

    /* Determinism */
    size_t h1 = hashmap_hash("github");
    size_t h2 = hashmap_hash("github");
    HM_ASSERT_EQUAL_INT(h1, h2, "Hash must be deterministic for identical input");

    /* Range bounds check */
    HM_ASSERT_TRUE(h1 < TABLE_SIZE, "Hash index must be strictly less than TABLE_SIZE");

    /* Safety check on NULL */
    HM_ASSERT_EQUAL_INT(hashmap_hash(NULL), 0, "NULL input must safely return 0");

    /* Distinct keys produce valid indices in [0, TABLE_SIZE - 1] */
    const char *test_keys[] = {"a", "z", "0", "9", "tiny", "url123", "G00gle", "Amaz0n"};
    size_t num_keys = sizeof(test_keys) / sizeof(test_keys[0]);

    for (size_t i = 0; i < num_keys; i++) {
        size_t h = hashmap_hash(test_keys[i]);
        HM_ASSERT_TRUE(h < TABLE_SIZE, "Every computed hash index must be in [0, TABLE_SIZE - 1]");
    }

    return 1;
}

/* Test C: Single Record Insertion & Retrieval */
static int test_single_insertion(void) {
    printf("Running test: Single Insertion & Retrieval...\n");

    HashMap map;
    hashmap_init(&map);

    URLRecord *rec = url_create_record("goog1", "https://google.com");
    HM_ASSERT_TRUE(rec != NULL, "Record creation should succeed");

    int inserted = hashmap_insert(&map, rec);
    HM_ASSERT_TRUE(inserted == 1, "Insertion of valid record must succeed");
    HM_ASSERT_EQUAL_INT(map.count, 1, "Element count must be 1 after insertion");

    URLRecord *retrieved = hashmap_get(&map, "goog1");
    HM_ASSERT_TRUE(retrieved == rec, "Retrieved record must match inserted pointer");
    HM_ASSERT_EQUAL_STR(retrieved->originalURL, "https://google.com", "Retrieved URL must match");

    hashmap_clear(&map);
    return 1;
}

/* Test D: Multiple Insertions */
static int test_multiple_insertions(void) {
    printf("Running test: Multiple Insertions...\n");

    HashMap map;
    hashmap_init(&map);

    const char *codes[] = {"code1", "code2", "code3", "code4", "code5"};
    const char *urls[] = {
        "https://site1.com",
        "https://site2.com",
        "https://site3.com",
        "https://site4.com",
        "https://site5.com"
    };
    size_t count = sizeof(codes) / sizeof(codes[0]);

    for (size_t i = 0; i < count; i++) {
        URLRecord *rec = url_create_record(codes[i], urls[i]);
        HM_ASSERT_TRUE(hashmap_insert(&map, rec), "Insertion must succeed");
    }

    HM_ASSERT_EQUAL_INT(map.count, count, "Element count must match total insertions");

    /* Verify all records can be retrieved */
    for (size_t i = 0; i < count; i++) {
        URLRecord *found = hashmap_get(&map, codes[i]);
        HM_ASSERT_TRUE(found != NULL, "Every inserted record must be retrievable");
        HM_ASSERT_EQUAL_STR(found->originalURL, urls[i], "Retrieved URL must match expected");
    }

    hashmap_clear(&map);
    return 1;
}

/* Test E: Duplicate Short Code Rejection */
static int test_duplicate_rejection(void) {
    printf("Running test: Duplicate Key Rejection...\n");

    HashMap map;
    hashmap_init(&map);

    URLRecord *rec1 = url_create_record("sameKey", "https://original.com");
    HM_ASSERT_TRUE(hashmap_insert(&map, rec1), "Initial insertion must succeed");
    HM_ASSERT_EQUAL_INT(map.count, 1, "Element count must be 1");

    /* Attempt to insert another record with the exact same short code */
    URLRecord *rec2 = url_create_record("sameKey", "https://duplicate.com");
    int inserted = hashmap_insert(&map, rec2);
    HM_ASSERT_FALSE(inserted, "Duplicate short code insertion must be rejected");
    HM_ASSERT_EQUAL_INT(map.count, 1, "Element count must NOT increase on duplicate rejection");

    /* Verify original record remains intact */
    URLRecord *original = hashmap_get(&map, "sameKey");
    HM_ASSERT_TRUE(original == rec1, "Original record must remain in hash table");
    HM_ASSERT_EQUAL_STR(original->originalURL, "https://original.com", "Original URL must be preserved");

    /* Free the rejected record to prevent memory leak */
    url_free_record(rec2);

    hashmap_clear(&map);
    return 1;
}

/* Test F: Collision Handling with Separate Chaining */
static int test_collision_handling(void) {
    printf("Running test: Collision Handling (Separate Chaining)...\n");

    HashMap map;
    hashmap_init(&map);

    /* Programmatically find two distinct short codes that map to the same bucket index */
    char seen_codes[TABLE_SIZE][SHORT_CODE_LEN];
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        seen_codes[i][0] = '\0';
    }

    char code1[SHORT_CODE_LEN] = "";
    char code2[SHORT_CODE_LEN] = "";
    size_t colliding_bucket = 0;
    int found_collision = 0;

    for (unsigned long n = 1; n < 10000 && !found_collision; n++) {
        char test_code[SHORT_CODE_LEN];
        url_generate_code(n, test_code, sizeof(test_code));

        if (!url_is_valid_short_code(test_code)) {
            continue;
        }

        size_t h = hashmap_hash(test_code);
        if (seen_codes[h][0] != '\0' && strcmp(seen_codes[h], test_code) != 0) {
            strncpy(code1, seen_codes[h], sizeof(code1));
            strncpy(code2, test_code, sizeof(code2));
            colliding_bucket = h;
            found_collision = 1;
        } else {
            strncpy(seen_codes[h], test_code, sizeof(seen_codes[h]));
        }
    }

    HM_ASSERT_TRUE(found_collision, "Must programmatically find colliding short codes");
    HM_ASSERT_EQUAL_INT(hashmap_hash(code1), hashmap_hash(code2), "Both codes must hash to same bucket");
    HM_ASSERT_EQUAL_INT(hashmap_hash(code1), colliding_bucket, "Bucket index must match target");

    /* Insert first colliding record */
    URLRecord *rec1 = url_create_record(code1, "https://collision1.org");
    HM_ASSERT_TRUE(hashmap_insert(&map, rec1), "First colliding record insertion should succeed");

    /* Insert second colliding record into the same bucket */
    URLRecord *rec2 = url_create_record(code2, "https://collision2.org");
    HM_ASSERT_TRUE(hashmap_insert(&map, rec2), "Second colliding record insertion should succeed");

    /* Verify element count */
    HM_ASSERT_EQUAL_INT(map.count, 2, "Element count must be 2");

    /* Verify separate chaining: both records must reside in the exact same bucket chain */
    HM_ASSERT_TRUE(map.buckets[colliding_bucket] != NULL, "Bucket head must not be NULL");
    HM_ASSERT_TRUE(map.buckets[colliding_bucket]->next != NULL, "Bucket chain must contain at least 2 nodes");

    /* Verify both records can be individually retrieved via hashmap_get */
    URLRecord *ret1 = hashmap_get(&map, code1);
    URLRecord *ret2 = hashmap_get(&map, code2);
    HM_ASSERT_TRUE(ret1 == rec1, "First colliding record must be retrievable");
    HM_ASSERT_TRUE(ret2 == rec2, "Second colliding record must be retrievable");

    /* Verify collision statistics */
    HashMapStats stats = hashmap_get_stats(&map);
    HM_ASSERT_EQUAL_INT(stats.totalRecords, 2, "Stats totalRecords must be 2");
    HM_ASSERT_EQUAL_INT(stats.usedBuckets, 1, "Stats usedBuckets must be 1 (both in same bucket)");
    HM_ASSERT_EQUAL_INT(stats.collidedBuckets, 1, "Stats collidedBuckets must be 1");
    HM_ASSERT_EQUAL_INT(stats.collisions, 1, "Stats collisions must be 1");
    HM_ASSERT_EQUAL_INT(stats.maxChainLength, 2, "Stats maxChainLength must be 2");

    hashmap_clear(&map);
    return 1;
}

/* Test G: Deletion Edge Cases in Separate Chaining */
static int test_deletion(void) {
    printf("Running test: Deletion Edge Cases...\n");

    HashMap map;
    hashmap_init(&map);

    /* Programmatically find 3 distinct keys colliding into the same bucket */
    char seen[TABLE_SIZE][SHORT_CODE_LEN];
    char seen2[TABLE_SIZE][SHORT_CODE_LEN];
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        seen[i][0] = '\0';
        seen2[i][0] = '\0';
    }

    char c1[SHORT_CODE_LEN] = "", c2[SHORT_CODE_LEN] = "", c3[SHORT_CODE_LEN] = "";
    size_t target_bucket = 0;
    int found_triple = 0;

    for (unsigned long n = 1; n < 50000 && !found_triple; n++) {
        char test_code[SHORT_CODE_LEN];
        url_generate_code(n, test_code, sizeof(test_code));
        if (!url_is_valid_short_code(test_code)) continue;

        size_t h = hashmap_hash(test_code);
        if (seen[h][0] == '\0') {
            strncpy(seen[h], test_code, sizeof(seen[h]));
        } else if (seen2[h][0] == '\0' && strcmp(seen[h], test_code) != 0) {
            strncpy(seen2[h], test_code, sizeof(seen2[h]));
        } else if (strcmp(seen[h], test_code) != 0 && strcmp(seen2[h], test_code) != 0) {
            strncpy(c1, seen[h], sizeof(c1));
            strncpy(c2, seen2[h], sizeof(c2));
            strncpy(c3, test_code, sizeof(c3));
            target_bucket = h;
            found_triple = 1;
        }
    }

    HM_ASSERT_TRUE(found_triple, "Must find 3 colliding keys");

    /* Insert 3 colliding nodes: chain becomes c1 -> c2 -> c3 */
    hashmap_insert(&map, url_create_record(c1, "https://url1.com"));
    hashmap_insert(&map, url_create_record(c2, "https://url2.com"));
    hashmap_insert(&map, url_create_record(c3, "https://url3.com"));
    HM_ASSERT_EQUAL_INT(map.count, 3, "Count must be 3");

    /* 1. Delete middle node (c2) */
    int removed_middle = hashmap_remove(&map, c2);
    HM_ASSERT_TRUE(removed_middle == 1, "Deleting middle node must return 1");
    HM_ASSERT_EQUAL_INT(map.count, 2, "Count must be 2 after deleting middle");
    HM_ASSERT_TRUE(hashmap_get(&map, c2) == NULL, "c2 must no longer exist");
    HM_ASSERT_TRUE(hashmap_get(&map, c1) != NULL, "c1 must still exist");
    HM_ASSERT_TRUE(hashmap_get(&map, c3) != NULL, "c3 must still exist");

    /* 2. Delete first node (c1) */
    int removed_first = hashmap_remove(&map, c1);
    HM_ASSERT_TRUE(removed_first == 1, "Deleting first node must return 1");
    HM_ASSERT_EQUAL_INT(map.count, 1, "Count must be 1 after deleting first");
    HM_ASSERT_TRUE(hashmap_get(&map, c1) == NULL, "c1 must no longer exist");
    HM_ASSERT_TRUE(hashmap_get(&map, c3) != NULL, "c3 must still exist");
    HM_ASSERT_EQUAL_STR(map.buckets[target_bucket]->shortCode, c3, "c3 must now be bucket head");

    /* 3. Delete nonexistent code */
    int removed_ghost = hashmap_remove(&map, "nonexistent");
    HM_ASSERT_FALSE(removed_ghost, "Deleting nonexistent code must return 0");
    HM_ASSERT_EQUAL_INT(map.count, 1, "Count must remain unchanged");

    /* 4. Delete last remaining node (c3) */
    int removed_last = hashmap_remove(&map, c3);
    HM_ASSERT_TRUE(removed_last == 1, "Deleting last node must return 1");
    HM_ASSERT_EQUAL_INT(map.count, 0, "Count must be 0 after deleting all nodes");
    HM_ASSERT_TRUE(map.buckets[target_bucket] == NULL, "Bucket head must become NULL");

    hashmap_clear(&map);
    return 1;
}

/* Callback accumulator for traversal test */
static size_t traversal_count = 0;
static void count_callback(const URLRecord *record) {
    if (record != NULL) {
        traversal_count++;
    }
}

/* Test H: Traversal via hashmap_foreach */
static int test_traversal(void) {
    printf("Running test: Traversal with hashmap_foreach...\n");

    HashMap map;
    hashmap_init(&map);

    const size_t NUM_ITEMS = 20;
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        char code[SHORT_CODE_LEN];
        char url[64];
        snprintf(code, sizeof(code), "k%zu", i);
        snprintf(url, sizeof(url), "https://example%zu.com", i);
        hashmap_insert(&map, url_create_record(code, url));
    }

    traversal_count = 0;
    hashmap_foreach(&map, count_callback);
    HM_ASSERT_EQUAL_INT(traversal_count, NUM_ITEMS, "hashmap_foreach must visit every record exactly once");

    hashmap_clear(&map);
    return 1;
}

/* Test I: Clear */
static int test_clear(void) {
    printf("Running test: HashMap Clear...\n");

    HashMap map;
    hashmap_init(&map);

    for (size_t i = 0; i < 15; i++) {
        char code[SHORT_CODE_LEN];
        snprintf(code, sizeof(code), "c%zu", i);
        hashmap_insert(&map, url_create_record(code, "https://test.com"));
    }
    HM_ASSERT_EQUAL_INT(map.count, 15, "Count must be 15 before clear");

    hashmap_clear(&map);
    HM_ASSERT_EQUAL_INT(map.count, 0, "Count must be 0 after clear");
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        HM_ASSERT_TRUE(map.buckets[i] == NULL, "All buckets must be NULL after clear");
    }

    return 1;
}

int main(void) {
    printf("==================================================================\n");
    printf("   TinyURL DSA Test Suite: Hash Table & Separate Chaining\n");
    printf("==================================================================\n");

    int all_passed = 1;

    all_passed &= test_initialization();
    all_passed &= test_hash_function();
    all_passed &= test_single_insertion();
    all_passed &= test_multiple_insertions();
    all_passed &= test_duplicate_rejection();
    all_passed &= test_collision_handling();
    all_passed &= test_deletion();
    all_passed &= test_traversal();
    all_passed &= test_clear();

    printf("==================================================================\n");
    printf(" HashMap Test Summary: %d/%d assertions passed.\n", hm_tests_passed, hm_tests_run);

    if (all_passed && hm_tests_passed == hm_tests_run) {
        printf(" Result: ALL HASHMAP TESTS PASSED SUCCESSFULLY!\n");
        printf("==================================================================\n");
        return EXIT_SUCCESS;
    } else {
        printf(" Result: SOME HASHMAP TESTS FAILED.\n");
        printf("==================================================================\n");
        return EXIT_FAILURE;
    }
}
