#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "url_service.h"
#include "storage.h"

static int cli_tests_run = 0;
static int cli_tests_passed = 0;

#define CLI_ASSERT_TRUE(condition, message) do { \
    cli_tests_run++; \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        cli_tests_passed++; \
    } \
} while (0)

#define CLI_ASSERT_FALSE(condition, message) CLI_ASSERT_TRUE(!(condition), message)

#define CLI_ASSERT_EQUAL_INT(actual, expected, message) do { \
    cli_tests_run++; \
    if ((actual) != (expected)) { \
        printf("  [FAIL] %s:%d: %s (expected %ld, got %ld)\n", \
               __FILE__, __LINE__, message, (long)(expected), (long)(actual)); \
        return 0; \
    } else { \
        cli_tests_passed++; \
    } \
} while (0)

#define CLI_ASSERT_EQUAL_STR(actual, expected, message) do { \
    cli_tests_run++; \
    if (strcmp((actual), (expected)) != 0) { \
        printf("  [FAIL] %s:%d: %s (expected '%s', got '%s')\n", \
               __FILE__, __LINE__, message, expected, actual); \
        return 0; \
    } else { \
        cli_tests_passed++; \
    } \
} while (0)

static int test_end_to_end_cli_lifecycle(void) {
    printf("Running test: Full End-to-End CLI Workflow Lifecycle...\n");

    const char *test_data = "data/test_cli_lifecycle.dat";
    remove(test_data);

    /* 1. Start empty */
    URLService service1;
    url_service_init(&service1);
    CLI_ASSERT_EQUAL_INT(url_service_get_count(&service1), 0, "1. Start empty: count must be 0");

    /* 2. Shorten multiple URLs */
    char codeA[SHORT_CODE_LEN];
    char codeB[SHORT_CODE_LEN];
    char codeC[SHORT_CODE_LEN];

    CLI_ASSERT_EQUAL_INT(url_service_shorten(&service1, "https://example.com/siteA", codeA, sizeof(codeA)),
                         URL_SUCCESS, "2. Shorten URL A");
    CLI_ASSERT_EQUAL_INT(url_service_shorten(&service1, "https://example.com/siteB", codeB, sizeof(codeB)),
                         URL_SUCCESS, "2. Shorten URL B");
    CLI_ASSERT_EQUAL_INT(url_service_shorten(&service1, "https://example.com/siteC", codeC, sizeof(codeC)),
                         URL_SUCCESS, "2. Shorten URL C");

    CLI_ASSERT_EQUAL_INT(url_service_get_count(&service1), 3, "2. Count must be 3");

    /* 3. Retrieve URLs & 4. Verify click counts */
    /* Access codeA twice */
    const char *resA1 = url_service_resolve(&service1, codeA);
    CLI_ASSERT_EQUAL_STR(resA1, "https://example.com/siteA", "3. Resolve codeA");
    url_service_resolve(&service1, codeA);

    /* Access codeB once */
    url_service_resolve(&service1, codeB);

    /* Leave codeC unaccessed (0 clicks) */

    const URLRecord *recA = url_service_get_record(&service1, codeA);
    const URLRecord *recB = url_service_get_record(&service1, codeB);
    const URLRecord *recC = url_service_get_record(&service1, codeC);

    CLI_ASSERT_EQUAL_INT(recA->clickCount, 2, "4. codeA click count must be 2");
    CLI_ASSERT_EQUAL_INT(recB->clickCount, 1, "4. codeB click count must be 1");
    CLI_ASSERT_EQUAL_INT(recC->clickCount, 0, "4. codeC click count must be 0");

    /* 5. Search by short code */
    const URLRecord *searchByCode = url_service_search_by_code(&service1, codeB);
    CLI_ASSERT_TRUE(searchByCode != NULL, "5. Search by short code found");
    CLI_ASSERT_EQUAL_STR(searchByCode->originalURL, "https://example.com/siteB", "5. Search codeB original URL");

    /* 6. Search by original URL */
    const URLRecord *searchByUrl = url_service_search_by_url(&service1, "https://example.com/siteC");
    CLI_ASSERT_TRUE(searchByUrl != NULL, "6. Search by original URL found");
    CLI_ASSERT_EQUAL_STR(searchByUrl->shortCode, codeC, "6. Search siteC returned codeC");

    /* 7. Delete a record (delete codeB) */
    URLStatus delStatus = url_service_delete(&service1, codeB);
    CLI_ASSERT_EQUAL_INT(delStatus, URL_SUCCESS, "7. Delete codeB succeeded");
    CLI_ASSERT_EQUAL_INT(url_service_get_count(&service1), 2, "7. Remaining count must be 2");
    CLI_ASSERT_TRUE(url_service_get_record(&service1, codeB) == NULL, "7. codeB is now deleted");

    /* 8. Save */
    int saved = storage_save_to_file(&service1, test_data);
    CLI_ASSERT_EQUAL_INT(saved, 2, "8. Saved 2 records to disk");

    /* 9. Destroy service (simulating application exit) */
    url_service_destroy(&service1);

    /* 10. Reload into a new service instance (simulating restart) */
    URLService service2;
    url_service_init(&service2);
    int loaded = storage_load_from_file(&service2, test_data);

    /* 11. Verify persistence */
    CLI_ASSERT_EQUAL_INT(loaded, 2, "10. Loaded 2 records from disk");
    CLI_ASSERT_EQUAL_INT(url_service_get_count(&service2), 2, "11. Service count is 2");

    /* 12. Verify deleted record remains deleted */
    CLI_ASSERT_TRUE(url_service_get_record(&service2, codeB) == NULL, "12. codeB remains deleted after reload");
    CLI_ASSERT_TRUE(url_service_resolve(&service2, codeB) == NULL, "12. resolving codeB returns NULL");

    /* 13. Verify click counts remain correct */
    const URLRecord *reloadedA = url_service_get_record(&service2, codeA);
    const URLRecord *reloadedC = url_service_get_record(&service2, codeC);
    CLI_ASSERT_TRUE(reloadedA != NULL, "13. codeA exists");
    CLI_ASSERT_TRUE(reloadedC != NULL, "13. codeC exists");
    CLI_ASSERT_EQUAL_INT(reloadedA->clickCount, 2, "13. reloadedA click count preserved at 2");
    CLI_ASSERT_EQUAL_INT(reloadedC->clickCount, 0, "13. reloadedC click count preserved at 0");

    /* 14. Add another URL after reload */
    char codeD[SHORT_CODE_LEN];
    URLStatus addStatus = url_service_shorten(&service2, "https://example.com/siteD", codeD, sizeof(codeD));
    CLI_ASSERT_EQUAL_INT(addStatus, URL_SUCCESS, "14. Shorten URL D after restart succeeded");
    CLI_ASSERT_EQUAL_INT(url_service_get_count(&service2), 3, "14. Count is now 3");

    /* 15. Verify short-code uniqueness */
    CLI_ASSERT_TRUE(strcmp(codeD, codeA) != 0, "15. codeD != codeA");
    CLI_ASSERT_TRUE(strcmp(codeD, codeB) != 0, "15. codeD != codeB (even though codeB was deleted)");
    CLI_ASSERT_TRUE(strcmp(codeD, codeC) != 0, "15. codeD != codeC");

    /* Clean up */
    url_service_destroy(&service2);
    remove(test_data);
    return 1;
}

int main(void) {
    printf("==================================================================\n");
    printf("   TinyURL DSA Test Suite: CLI Integration Workflow\n");
    printf("==================================================================\n");

    int all_passed = 1;
    all_passed &= test_end_to_end_cli_lifecycle();

    printf("==================================================================\n");
    printf(" CLI Integration Test Summary: %d/%d assertions passed.\n",
           cli_tests_passed, cli_tests_run);

    if (all_passed && cli_tests_passed == cli_tests_run) {
        printf(" Result: ALL CLI INTEGRATION TESTS PASSED SUCCESSFULLY!\n");
        printf("==================================================================\n");
        return EXIT_SUCCESS;
    } else {
        printf(" Result: SOME CLI INTEGRATION TESTS FAILED.\n");
        printf("==================================================================\n");
        return EXIT_FAILURE;
    }
}
