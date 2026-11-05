#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "url_service.h"

static int svc_tests_run = 0;
static int svc_tests_passed = 0;

#define SVC_ASSERT_TRUE(condition, message) do { \
    svc_tests_run++; \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        svc_tests_passed++; \
    } \
} while (0)

#define SVC_ASSERT_FALSE(condition, message) SVC_ASSERT_TRUE(!(condition), message)

#define SVC_ASSERT_EQUAL_INT(actual, expected, message) do { \
    svc_tests_run++; \
    if ((actual) != (expected)) { \
        printf("  [FAIL] %s:%d: %s (expected %ld, got %ld)\n", \
               __FILE__, __LINE__, message, (long)(expected), (long)(actual)); \
        return 0; \
    } else { \
        svc_tests_passed++; \
    } \
} while (0)

#define SVC_ASSERT_EQUAL_STR(actual, expected, message) do { \
    svc_tests_run++; \
    if (strcmp((actual), (expected)) != 0) { \
        printf("  [FAIL] %s:%d: %s (expected '%s', got '%s')\n", \
               __FILE__, __LINE__, message, expected, actual); \
        return 0; \
    } else { \
        svc_tests_passed++; \
    } \
} while (0)

/* Test A: Service Initialization */
static int test_service_initialization(void) {
    printf("Running test: Service Initialization...\n");

    URLService service;
    url_service_init(&service);

    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 0, "Initial record count must be 0");
    SVC_ASSERT_EQUAL_INT(service.nextId, 1, "nextId should initialize to 1");

    url_service_destroy(&service);
    return 1;
}

/* Test B: URL Shortening */
static int test_url_shortening(void) {
    printf("Running test: URL Shortening...\n");

    URLService service;
    url_service_init(&service);

    char shortCode[SHORT_CODE_LEN];
    URLStatus status = url_service_shorten(
        &service,
        "https://www.example.com/welcome",
        shortCode,
        sizeof(shortCode)
    );

    SVC_ASSERT_EQUAL_INT(status, URL_SUCCESS, "Shortening valid URL should return URL_SUCCESS");
    SVC_ASSERT_TRUE(strlen(shortCode) > 0, "Generated short code must be non-empty");

    /* Verify short code is alphanumeric */
    for (size_t i = 0; i < strlen(shortCode); i++) {
        SVC_ASSERT_TRUE(isalnum((unsigned char)shortCode[i]), "Code must contain only alphanumeric characters");
    }

    /* Verify record exists in hash table and has initial clickCount = 0 */
    const URLRecord *rec = url_service_get_record(&service, shortCode);
    SVC_ASSERT_TRUE(rec != NULL, "Record must exist in service");
    SVC_ASSERT_EQUAL_STR(rec->originalURL, "https://www.example.com/welcome", "Stored URL must match input");
    SVC_ASSERT_EQUAL_INT(rec->clickCount, 0, "Initial clickCount must be 0");
    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 1, "Service record count must be 1");

    url_service_destroy(&service);
    return 1;
}

/* Test C: Multiple URLs & Unique Short Codes */
static int test_multiple_urls(void) {
    printf("Running test: Multiple URLs & Unique Codes...\n");

    URLService service;
    url_service_init(&service);

    const char *urls[] = {
        "https://kernel.org",
        "https://gnu.org/software/gcc",
        "https://en.wikipedia.org/wiki/Hash_table",
        "https://github.com/torvalds",
        "https://developer.mozilla.org"
    };
    size_t count = sizeof(urls) / sizeof(urls[0]);
    char codes[5][SHORT_CODE_LEN];

    for (size_t i = 0; i < count; i++) {
        URLStatus status = url_service_shorten(&service, urls[i], codes[i], sizeof(codes[i]));
        SVC_ASSERT_EQUAL_INT(status, URL_SUCCESS, "Shorten must succeed");
    }

    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), count, "Record count must match insertions");

    /* Verify uniqueness across all generated codes */
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            SVC_ASSERT_TRUE(strcmp(codes[i], codes[j]) != 0, "Generated short codes must all be distinct");
        }
    }

    /* Verify each record is retrievable */
    for (size_t i = 0; i < count; i++) {
        const URLRecord *rec = url_service_get_record(&service, codes[i]);
        SVC_ASSERT_TRUE(rec != NULL, "Each record must be retrievable");
        SVC_ASSERT_EQUAL_STR(rec->originalURL, urls[i], "Retrieved original URL must match");
    }

    url_service_destroy(&service);
    return 1;
}

/* Test D: Invalid URLs */
static int test_invalid_urls(void) {
    printf("Running test: Invalid URL Rejection...\n");

    URLService service;
    url_service_init(&service);

    char code[SHORT_CODE_LEN];

    /* NULL */
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, NULL, code, sizeof(code)),
                         URL_ERR_NULL_PARAM, "NULL URL must return URL_ERR_NULL_PARAM");

    /* Empty string */
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, "", code, sizeof(code)),
                         URL_ERR_INVALID_URL, "Empty string must return URL_ERR_INVALID_URL");

    /* Missing scheme */
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, "hello", code, sizeof(code)),
                         URL_ERR_INVALID_URL, "'hello' must return URL_ERR_INVALID_URL");

    /* Scheme with no host */
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, "http://", code, sizeof(code)),
                         URL_ERR_INVALID_URL, "'http://' must return URL_ERR_INVALID_URL");
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, "https://", code, sizeof(code)),
                         URL_ERR_INVALID_URL, "'https://' must return URL_ERR_INVALID_URL");

    /* Buffer too small */
    SVC_ASSERT_EQUAL_INT(url_service_shorten(&service, "https://valid.com", code, 1),
                         URL_ERR_BUFFER_TOO_SMALL, "Tiny buffer must return URL_ERR_BUFFER_TOO_SMALL");

    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 0, "No records should have been stored");

    url_service_destroy(&service);
    return 1;
}

/* Test E: URL Resolution & Click Counting */
static int test_resolution_and_clicks(void) {
    printf("Running test: Resolution and Click Counting...\n");

    URLService service;
    url_service_init(&service);

    char code[SHORT_CODE_LEN];
    url_service_shorten(&service, "https://example.com/target", code, sizeof(code));

    /* Check initial clicks via read-only inspect (must not increment) */
    const URLRecord *recBefore = url_service_get_record(&service, code);
    SVC_ASSERT_EQUAL_INT(recBefore->clickCount, 0, "clickCount must start at 0");

    /* Resolve once -> clickCount increments to 1 */
    const char *resolvedUrl = url_service_resolve(&service, code);
    SVC_ASSERT_TRUE(resolvedUrl != NULL, "Resolving valid code should return URL");
    SVC_ASSERT_EQUAL_STR(resolvedUrl, "https://example.com/target", "Resolved URL must match target");

    const URLRecord *rec1 = url_service_get_record(&service, code);
    SVC_ASSERT_EQUAL_INT(rec1->clickCount, 1, "clickCount must increment to 1 after first resolution");

    /* Resolve second time -> clickCount increments to 2 */
    url_service_resolve(&service, code);
    const URLRecord *rec2 = url_service_get_record(&service, code);
    SVC_ASSERT_EQUAL_INT(rec2->clickCount, 2, "clickCount must increment to 2 after second resolution");

    url_service_destroy(&service);
    return 1;
}

/* Test F: Invalid Resolution */
static int test_invalid_resolution(void) {
    printf("Running test: Invalid Resolution...\n");

    URLService service;
    url_service_init(&service);

    /* Resolve nonexistent code */
    const char *result = url_service_resolve(&service, "ghost");
    SVC_ASSERT_TRUE(result == NULL, "Resolving nonexistent code must return NULL");

    /* Resolve NULL */
    SVC_ASSERT_TRUE(url_service_resolve(&service, NULL) == NULL, "Resolving NULL must return NULL");

    url_service_destroy(&service);
    return 1;
}

/* Test G: Search Operations */
static int test_search_operations(void) {
    printf("Running test: Search by Code and Original URL...\n");

    URLService service;
    url_service_init(&service);

    char code1[SHORT_CODE_LEN];
    char code2[SHORT_CODE_LEN];

    url_service_shorten(&service, "https://search1.org", code1, sizeof(code1));
    url_service_shorten(&service, "https://search2.org", code2, sizeof(code2));

    /* Search by short code */
    const URLRecord *byCode = url_service_search_by_code(&service, code1);
    SVC_ASSERT_TRUE(byCode != NULL, "Search by code should find record");
    SVC_ASSERT_EQUAL_STR(byCode->originalURL, "https://search1.org", "Matching original URL expected");

    /* Search by original URL */
    const URLRecord *byUrl = url_service_search_by_url(&service, "https://search2.org");
    SVC_ASSERT_TRUE(byUrl != NULL, "Search by original URL should find record");
    SVC_ASSERT_EQUAL_STR(byUrl->shortCode, code2, "Matching short code expected");

    /* Search nonexistent URL */
    const URLRecord *byNone = url_service_search_by_url(&service, "https://nonexistent.org");
    SVC_ASSERT_TRUE(byNone == NULL, "Search for nonexistent URL must return NULL");

    url_service_destroy(&service);
    return 1;
}

/* Test H: Deletion */
static int test_deletion(void) {
    printf("Running test: Record Deletion...\n");

    URLService service;
    url_service_init(&service);

    char codeA[SHORT_CODE_LEN];
    char codeB[SHORT_CODE_LEN];

    url_service_shorten(&service, "https://siteA.com", codeA, sizeof(codeA));
    url_service_shorten(&service, "https://siteB.com", codeB, sizeof(codeB));
    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 2, "Count should be 2");

    /* Delete codeA */
    URLStatus delStatus = url_service_delete(&service, codeA);
    SVC_ASSERT_EQUAL_INT(delStatus, URL_SUCCESS, "Deleting codeA should succeed");
    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 1, "Count should decrease to 1");

    /* Verify codeA is gone */
    SVC_ASSERT_TRUE(url_service_resolve(&service, codeA) == NULL, "Deleted code should not resolve");
    SVC_ASSERT_TRUE(url_service_get_record(&service, codeA) == NULL, "Deleted code should not be inspectable");

    /* Verify codeB is intact */
    const URLRecord *recB = url_service_get_record(&service, codeB);
    SVC_ASSERT_TRUE(recB != NULL, "Other record must remain intact");
    SVC_ASSERT_EQUAL_STR(recB->originalURL, "https://siteB.com", "codeB URL should be intact");

    /* Delete nonexistent code */
    URLStatus ghostStatus = url_service_delete(&service, "ghost");
    SVC_ASSERT_EQUAL_INT(ghostStatus, URL_ERR_NOT_FOUND, "Deleting nonexistent code must return URL_ERR_NOT_FOUND");
    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 1, "Count must remain unchanged");

    url_service_destroy(&service);
    return 1;
}

/* Test I: Statistics & Record Counts */
static int test_statistics(void) {
    printf("Running test: Service Statistics...\n");

    URLService service;
    url_service_init(&service);

    char code[SHORT_CODE_LEN];
    for (int i = 0; i < 10; i++) {
        char url[64];
        snprintf(url, sizeof(url), "https://stats%d.com", i);
        url_service_shorten(&service, url, code, sizeof(code));
    }

    HashMapStats stats = url_service_get_stats(&service);
    SVC_ASSERT_EQUAL_INT(stats.totalRecords, 10, "Stats totalRecords must be 10");
    SVC_ASSERT_TRUE(stats.usedBuckets > 0, "Used buckets must be greater than 0");
    SVC_ASSERT_TRUE(stats.loadFactor > 0.0, "Load factor must be positive");

    /* Delete 3 records and verify stats update */
    url_service_destroy(&service);
    SVC_ASSERT_EQUAL_INT(url_service_get_count(&service), 0, "Count must be 0 after destroy");

    return 1;
}

int main(void) {
    printf("==================================================================\n");
    printf("   TinyURL DSA Test Suite: URL Service / Application Layer\n");
    printf("==================================================================\n");

    int all_passed = 1;

    all_passed &= test_service_initialization();
    all_passed &= test_url_shortening();
    all_passed &= test_multiple_urls();
    all_passed &= test_invalid_urls();
    all_passed &= test_resolution_and_clicks();
    all_passed &= test_invalid_resolution();
    all_passed &= test_search_operations();
    all_passed &= test_deletion();
    all_passed &= test_statistics();

    printf("==================================================================\n");
    printf(" Service Test Summary: %d/%d assertions passed.\n", svc_tests_passed, svc_tests_run);

    if (all_passed && svc_tests_passed == svc_tests_run) {
        printf(" Result: ALL SERVICE TESTS PASSED SUCCESSFULLY!\n");
        printf("==================================================================\n");
        return EXIT_SUCCESS;
    } else {
        printf(" Result: SOME SERVICE TESTS FAILED.\n");
        printf("==================================================================\n");
        return EXIT_FAILURE;
    }
}
