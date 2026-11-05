#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "url.h"

/* Simple test framework macros */
static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_FALSE(condition, message) ASSERT_TRUE(!(condition), message)

#define ASSERT_EQUAL_STR(actual, expected, message) do { \
    tests_run++; \
    if (strcmp((actual), (expected)) != 0) { \
        printf("  [FAIL] %s:%d: %s (expected '%s', got '%s')\n", \
               __FILE__, __LINE__, message, expected, actual); \
        return 0; \
    } else { \
        tests_passed++; \
    } \
} while (0)

#define ASSERT_EQUAL_INT(actual, expected, message) do { \
    tests_run++; \
    if ((actual) != (expected)) { \
        printf("  [FAIL] %s:%d: %s (expected %ld, got %ld)\n", \
               __FILE__, __LINE__, message, (long)(expected), (long)(actual)); \
        return 0; \
    } else { \
        tests_passed++; \
    } \
} while (0)

/* Test 1: URL Validation */
static int test_url_validation(void) {
    printf("Running test: URL Validation...\n");

    /* Valid URLs */
    ASSERT_TRUE(url_is_valid("http://example.com"), "HTTP scheme should be valid");
    ASSERT_TRUE(url_is_valid("https://example.com"), "HTTPS scheme should be valid");
    ASSERT_TRUE(url_is_valid("https://github.com/torvalds/linux"), "HTTPS with path should be valid");
    ASSERT_TRUE(url_is_valid("https://example.org?query=test&page=1"), "HTTPS with query string should be valid");

    /* Invalid URLs */
    ASSERT_FALSE(url_is_valid(NULL), "NULL URL should be rejected");
    ASSERT_FALSE(url_is_valid(""), "Empty string should be rejected");
    ASSERT_FALSE(url_is_valid("ftp://example.com"), "Non-HTTP/HTTPS schemes should be rejected");
    ASSERT_FALSE(url_is_valid("http://"), "Scheme with empty host should be rejected");
    ASSERT_FALSE(url_is_valid("https://"), "Scheme with empty host should be rejected");
    ASSERT_FALSE(url_is_valid("http://invalid url.com"), "URL with spaces should be rejected");
    ASSERT_FALSE(url_is_valid("http://invalid\nurl.com"), "URL with newline should be rejected");

    return 1;
}

/* Test 2: Short-Code Validation */
static int test_short_code_validation(void) {
    printf("Running test: Short-Code Validation...\n");

    /* Valid short codes */
    ASSERT_TRUE(url_is_valid_short_code("a"), "Single character code should be valid");
    ASSERT_TRUE(url_is_valid_short_code("1"), "Single digit code should be valid");
    ASSERT_TRUE(url_is_valid_short_code("abc12"), "Alphanumeric code should be valid");
    ASSERT_TRUE(url_is_valid_short_code("AbC123Z"), "Mixed-case 7-char code should be valid");

    /* Invalid short codes */
    ASSERT_FALSE(url_is_valid_short_code(NULL), "NULL short code should be rejected");
    ASSERT_FALSE(url_is_valid_short_code(""), "Empty short code should be rejected");
    ASSERT_FALSE(url_is_valid_short_code("abc-12"), "Code with hyphen should be rejected");
    ASSERT_FALSE(url_is_valid_short_code("abc_12"), "Code with underscore should be rejected");
    ASSERT_FALSE(url_is_valid_short_code("abc 12"), "Code with space should be rejected");
    ASSERT_FALSE(url_is_valid_short_code("12345678"), "Code exceeding maximum length should be rejected");

    return 1;
}

/* Test 3: Base-62 Generation */
static int test_base62_generation(void) {
    printf("Running test: Base-62 Code Generation...\n");

    char buffer[16];

    /* Test 0 */
    url_generate_code(0, buffer, sizeof(buffer));
    ASSERT_EQUAL_STR(buffer, "0", "0 should encode to '0'");

    /* Test 1 */
    url_generate_code(1, buffer, sizeof(buffer));
    ASSERT_EQUAL_STR(buffer, "1", "1 should encode to '1'");

    /* Test 61 (last single-digit Base-62 char 'z') */
    url_generate_code(61, buffer, sizeof(buffer));
    ASSERT_EQUAL_STR(buffer, "z", "61 should encode to 'z'");

    /* Test 62 ('10') */
    url_generate_code(62, buffer, sizeof(buffer));
    ASSERT_EQUAL_STR(buffer, "10", "62 should encode to '10'");

    /* Test 125 (2 * 62 + 1 -> '21') */
    url_generate_code(125, buffer, sizeof(buffer));
    ASSERT_EQUAL_STR(buffer, "21", "125 should encode to '21'");

    /* Test buffer overflow prevention with small buffer */
    char smallBuf[3];
    url_generate_code(125, smallBuf, sizeof(smallBuf));
    ASSERT_EQUAL_STR(smallBuf, "21", "Buffer of size 3 should hold 2 chars + null");

    /* Truncation boundary */
    char tinyBuf[2];
    url_generate_code(125, tinyBuf, sizeof(tinyBuf));
    ASSERT_EQUAL_INT(strlen(tinyBuf), 1, "Tiny buffer must be strictly null terminated");

    return 1;
}

/* Test 4: Record Creation, Contents, and clickCount Initialization */
static int test_record_creation(void) {
    printf("Running test: Record Creation & Initialization...\n");

    /* Valid creation */
    URLRecord *rec = url_create_record("g00gl", "https://google.com");
    ASSERT_TRUE(rec != NULL, "Valid record creation should return non-NULL pointer");
    ASSERT_EQUAL_STR(rec->shortCode, "g00gl", "Record shortCode should match input");
    ASSERT_EQUAL_STR(rec->originalURL, "https://google.com", "Record originalURL should match input");
    ASSERT_EQUAL_INT(rec->clickCount, 0, "clickCount must initialize to 0");
    ASSERT_TRUE(rec->next == NULL, "next pointer must initialize to NULL");
    url_free_record(rec);

    /* Invalid creation cases */
    ASSERT_TRUE(url_create_record(NULL, "https://google.com") == NULL, "NULL shortCode must fail");
    ASSERT_TRUE(url_create_record("g00gl", NULL) == NULL, "NULL originalURL must fail");
    ASSERT_TRUE(url_create_record("", "https://google.com") == NULL, "Empty shortCode must fail");
    ASSERT_TRUE(url_create_record("g00gl", "") == NULL, "Empty originalURL must fail");
    ASSERT_TRUE(url_create_record("invalid!", "https://google.com") == NULL, "Non-alphanumeric code must fail");
    ASSERT_TRUE(url_create_record("g00gl", "not_a_valid_url") == NULL, "Invalid URL scheme must fail");

    return 1;
}

/* Test 5: Empty List Operations */
static int test_empty_list(void) {
    printf("Running test: Empty List Operations...\n");

    URLRecord *head = NULL;

    ASSERT_EQUAL_INT(url_list_length(head), 0, "Empty list length must be 0");
    ASSERT_TRUE(url_list_find(head, "abc") == NULL, "Find in empty list must return NULL");
    ASSERT_FALSE(url_list_remove(&head, "abc"), "Remove from empty list must return 0");

    /* Safe clear on NULL / empty list */
    url_list_clear(&head);
    ASSERT_TRUE(head == NULL, "Head should remain NULL after clear");

    return 1;
}

/* Test 6: Inserting Multiple Records & Searching First/Middle/Last/Nonexistent */
static int test_insertion_and_search(void) {
    printf("Running test: Insertion and Searching (First, Middle, Last, Nonexistent)...\n");

    URLRecord *head = NULL;
    URLRecord *rec1 = url_create_record("codeA", "https://example.com/a");
    URLRecord *rec2 = url_create_record("codeB", "https://example.com/b");
    URLRecord *rec3 = url_create_record("codeC", "https://example.com/c");

    ASSERT_TRUE(rec1 && rec2 && rec3, "Test records must allocate successfully");

    /* Append records */
    ASSERT_TRUE(url_list_append(&head, rec1), "Append 1st record should succeed");
    ASSERT_TRUE(url_list_append(&head, rec2), "Append 2nd record should succeed");
    ASSERT_TRUE(url_list_append(&head, rec3), "Append 3rd record should succeed");

    ASSERT_EQUAL_INT(url_list_length(head), 3, "List length must be 3");

    /* Search first node */
    URLRecord *foundFirst = url_list_find(head, "codeA");
    ASSERT_TRUE(foundFirst == rec1, "Searching first node must return rec1");

    /* Search middle node */
    URLRecord *foundMiddle = url_list_find(head, "codeB");
    ASSERT_TRUE(foundMiddle == rec2, "Searching middle node must return rec2");

    /* Search last node */
    URLRecord *foundLast = url_list_find(head, "codeC");
    ASSERT_TRUE(foundLast == rec3, "Searching last node must return rec3");

    /* Search nonexistent code */
    URLRecord *foundNone = url_list_find(head, "nonexistent");
    ASSERT_TRUE(foundNone == NULL, "Searching nonexistent code must return NULL");

    /* Clean up */
    url_list_clear(&head);
    ASSERT_TRUE(head == NULL, "List head must be NULL after clear");

    return 1;
}

/* Test 7: Deletion Edge Cases */
static int test_deletion_cases(void) {
    printf("Running test: Deletion Edge Cases...\n");

    /* Case A: Deleting the only node */
    {
        URLRecord *head = NULL;
        URLRecord *rec = url_create_record("only1", "https://example.com/only");
        url_list_append(&head, rec);
        ASSERT_EQUAL_INT(url_list_length(head), 1, "List length should be 1");

        int removed = url_list_remove(&head, "only1");
        ASSERT_TRUE(removed == 1, "Removing only node must return 1");
        ASSERT_TRUE(head == NULL, "Head must become NULL when only node is removed");
        ASSERT_EQUAL_INT(url_list_length(head), 0, "List length must become 0");
    }

    /* Case B: Deleting the first node of multiple nodes */
    {
        URLRecord *head = NULL;
        url_list_append(&head, url_create_record("first", "https://example.com/first"));
        url_list_append(&head, url_create_record("second", "https://example.com/second"));
        url_list_append(&head, url_create_record("third", "https://example.com/third"));

        int removed = url_list_remove(&head, "first");
        ASSERT_TRUE(removed == 1, "Removing first node must return 1");
        ASSERT_EQUAL_INT(url_list_length(head), 2, "List length must become 2");
        ASSERT_EQUAL_STR(head->shortCode, "second", "New head must be 'second'");
        ASSERT_TRUE(url_list_find(head, "first") == NULL, "'first' must no longer exist");

        url_list_clear(&head);
    }

    /* Case C: Deleting a middle node */
    {
        URLRecord *head = NULL;
        url_list_append(&head, url_create_record("node1", "https://example.com/1"));
        url_list_append(&head, url_create_record("node2", "https://example.com/2"));
        url_list_append(&head, url_create_record("node3", "https://example.com/3"));

        int removed = url_list_remove(&head, "node2");
        ASSERT_TRUE(removed == 1, "Removing middle node must return 1");
        ASSERT_EQUAL_INT(url_list_length(head), 2, "List length must become 2");
        ASSERT_TRUE(url_list_find(head, "node2") == NULL, "'node2' must no longer exist");
        ASSERT_TRUE(url_list_find(head, "node1") != NULL, "'node1' must still exist");
        ASSERT_TRUE(url_list_find(head, "node3") != NULL, "'node3' must still exist");
        ASSERT_EQUAL_STR(head->next->shortCode, "node3", "node1->next must point to node3");

        url_list_clear(&head);
    }

    /* Case D: Deleting the last node */
    {
        URLRecord *head = NULL;
        url_list_append(&head, url_create_record("node1", "https://example.com/1"));
        url_list_append(&head, url_create_record("node2", "https://example.com/2"));
        url_list_append(&head, url_create_record("node3", "https://example.com/3"));

        int removed = url_list_remove(&head, "node3");
        ASSERT_TRUE(removed == 1, "Removing last node must return 1");
        ASSERT_EQUAL_INT(url_list_length(head), 2, "List length must become 2");
        ASSERT_TRUE(url_list_find(head, "node3") == NULL, "'node3' must no longer exist");
        ASSERT_TRUE(head->next->next == NULL, "node2->next must now be NULL");

        url_list_clear(&head);
    }

    /* Case E: Attempting to delete a nonexistent node */
    {
        URLRecord *head = NULL;
        url_list_append(&head, url_create_record("node1", "https://example.com/1"));
        url_list_append(&head, url_create_record("node2", "https://example.com/2"));

        int removed = url_list_remove(&head, "ghost");
        ASSERT_FALSE(removed, "Removing nonexistent node must return 0");
        ASSERT_EQUAL_INT(url_list_length(head), 2, "List length must remain 2");

        url_list_clear(&head);
    }

    return 1;
}

/* Test 8: Freeing Entire List */
static int test_freeing_entire_list(void) {
    printf("Running test: Freeing Entire List...\n");

    URLRecord *head = NULL;
    char code[8];

    for (int i = 0; i < 10; i++) {
        snprintf(code, sizeof(code), "k%d", i);
        url_list_append(&head, url_create_record(code, "https://example.com"));
    }

    ASSERT_EQUAL_INT(url_list_length(head), 10, "List should contain 10 nodes");
    url_list_clear(&head);
    ASSERT_TRUE(head == NULL, "Head pointer must be reset to NULL after url_list_clear");
    ASSERT_EQUAL_INT(url_list_length(head), 0, "List length must be 0 after clear");

    return 1;
}

int main(void) {
    printf("==================================================================\n");
    printf("   TinyURL DSA Test Suite: URLRecord & Singly Linked List\n");
    printf("==================================================================\n");

    int all_passed = 1;

    all_passed &= test_url_validation();
    all_passed &= test_short_code_validation();
    all_passed &= test_base62_generation();
    all_passed &= test_record_creation();
    all_passed &= test_empty_list();
    all_passed &= test_insertion_and_search();
    all_passed &= test_deletion_cases();
    all_passed &= test_freeing_entire_list();

    printf("==================================================================\n");
    printf(" Test Summary: %d/%d assertions passed.\n", tests_passed, tests_run);

    if (all_passed && tests_passed == tests_run) {
        printf(" Result: ALL TESTS PASSED SUCCESSFULLY!\n");
        printf("==================================================================\n");
        return EXIT_SUCCESS;
    } else {
        printf(" Result: SOME TESTS FAILED.\n");
        printf("==================================================================\n");
        return EXIT_FAILURE;
    }
}
