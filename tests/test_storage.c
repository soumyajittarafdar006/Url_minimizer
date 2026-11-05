#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "storage.h"
#include "url_service.h"

static int st_tests_run = 0;
static int st_tests_passed = 0;

#define ST_ASSERT_TRUE(condition, message) do { \
    st_tests_run++; \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        return 0; \
    } else { \
        st_tests_passed++; \
    } \
} while (0)

#define ST_ASSERT_FALSE(condition, message) ST_ASSERT_TRUE(!(condition), message)

#define ST_ASSERT_EQUAL_INT(actual, expected, message) do { \
    st_tests_run++; \
    if ((actual) != (expected)) { \
        printf("  [FAIL] %s:%d: %s (expected %ld, got %ld)\n", \
               __FILE__, __LINE__, message, (long)(expected), (long)(actual)); \
        return 0; \
    } else { \
        st_tests_passed++; \
    } \
} while (0)

#define ST_ASSERT_EQUAL_STR(actual, expected, message) do { \
    st_tests_run++; \
    if (strcmp((actual), (expected)) != 0) { \
        printf("  [FAIL] %s:%d: %s (expected '%s', got '%s')\n", \
               __FILE__, __LINE__, message, expected, actual); \
        return 0; \
    } else { \
        st_tests_passed++; \
    } \
} while (0)

/* Test A: Empty / Nonexistent file */
static int test_nonexistent_file(void) {
    printf("Running test: Nonexistent File Handling...\n");

    const char *test_file = "data/test_nonexistent.dat";
    remove(test_file);

    URLService service;
    url_service_init(&service);

    int loaded = storage_load_from_file(&service, test_file);
    ST_ASSERT_EQUAL_INT(loaded, 0, "Nonexistent file must load 0 records without error");
    ST_ASSERT_EQUAL_INT(url_service_get_count(&service), 0, "Service record count must be 0");

    url_service_destroy(&service);
    return 1;
}

/* Test B & E: Save One Record and Preserve Click Count */
static int test_save_one_record(void) {
    printf("Running test: Save Single Record & Click Count...\n");

    const char *test_file = "data/test_single.dat";
    remove(test_file);

    URLService service1;
    url_service_init(&service1);

    char code[SHORT_CODE_LEN];
    url_service_shorten(&service1, "https://gnu.org", code, sizeof(code));

    /* Simulate 3 user resolutions */
    url_service_resolve(&service1, code);
    url_service_resolve(&service1, code);
    url_service_resolve(&service1, code);

    int saved = storage_save_to_file(&service1, test_file);
    ST_ASSERT_EQUAL_INT(saved, 1, "Must save 1 record");
    url_service_destroy(&service1);

    /* Load into fresh service */
    URLService service2;
    url_service_init(&service2);

    int loaded = storage_load_from_file(&service2, test_file);
    ST_ASSERT_EQUAL_INT(loaded, 1, "Must load 1 record");
    ST_ASSERT_EQUAL_INT(url_service_get_count(&service2), 1, "Service count must be 1");

    const URLRecord *rec = url_service_get_record(&service2, code);
    ST_ASSERT_TRUE(rec != NULL, "Record must exist in reloaded service");
    ST_ASSERT_EQUAL_STR(rec->originalURL, "https://gnu.org", "URL must match");
    ST_ASSERT_EQUAL_INT(rec->clickCount, 3, "clickCount must be preserved as 3");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test C: Save Multiple Records */
static int test_save_multiple_records(void) {
    printf("Running test: Save Multiple Records...\n");

    const char *test_file = "data/test_multi.dat";
    remove(test_file);

    URLService service1;
    url_service_init(&service1);

    const char *urls[] = {
        "https://site1.org/alpha",
        "https://site2.org/beta",
        "https://site3.org/gamma",
        "https://site4.org/delta",
        "https://site5.org/epsilon"
    };
    char codes[5][SHORT_CODE_LEN];
    size_t count = sizeof(urls) / sizeof(urls[0]);

    for (size_t i = 0; i < count; i++) {
        url_service_shorten(&service1, urls[i], codes[i], sizeof(codes[i]));
    }

    int saved = storage_save_to_file(&service1, test_file);
    ST_ASSERT_EQUAL_INT(saved, (int)count, "Must save all 5 records");
    url_service_destroy(&service1);

    /* Reload into fresh service */
    URLService service2;
    url_service_init(&service2);

    int loaded = storage_load_from_file(&service2, test_file);
    ST_ASSERT_EQUAL_INT(loaded, (int)count, "Must load all 5 records");
    ST_ASSERT_EQUAL_INT(url_service_get_count(&service2), count, "Count must match 5");

    for (size_t i = 0; i < count; i++) {
        const URLRecord *rec = url_service_get_record(&service2, codes[i]);
        ST_ASSERT_TRUE(rec != NULL, "Every record must be retrievable");
        ST_ASSERT_EQUAL_STR(rec->originalURL, urls[i], "Every original URL must match");
    }

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test D: nextId Persistence & Conflict Prevention */
static int test_next_id_persistence(void) {
    printf("Running test: nextId Persistence & Conflict Prevention...\n");

    const char *test_file = "data/test_nextid.dat";
    remove(test_file);

    URLService service1;
    url_service_init(&service1);

    char code1[SHORT_CODE_LEN];
    char code2[SHORT_CODE_LEN];
    char code3[SHORT_CODE_LEN];

    url_service_shorten(&service1, "https://url1.com", code1, sizeof(code1));
    url_service_shorten(&service1, "https://url2.com", code2, sizeof(code2));
    url_service_shorten(&service1, "https://url3.com", code3, sizeof(code3));

    storage_save_to_file(&service1, test_file);
    url_service_destroy(&service1);

    /* Fresh service: load and shorten another URL */
    URLService service2;
    url_service_init(&service2);
    storage_load_from_file(&service2, test_file);

    char newCode[SHORT_CODE_LEN];
    URLStatus status = url_service_shorten(&service2, "https://url4.com", newCode, sizeof(newCode));
    ST_ASSERT_EQUAL_INT(status, URL_SUCCESS, "Shortening in loaded service must succeed");

    /* Ensure new code is completely distinct from existing codes */
    ST_ASSERT_TRUE(strcmp(newCode, code1) != 0, "New code must not match code1");
    ST_ASSERT_TRUE(strcmp(newCode, code2) != 0, "New code must not match code2");
    ST_ASSERT_TRUE(strcmp(newCode, code3) != 0, "New code must not match code3");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test F: Deletion Persistence */
static int test_deletion_persistence(void) {
    printf("Running test: Deletion Persistence...\n");

    const char *test_file = "data/test_deletion.dat";
    remove(test_file);

    URLService service1;
    url_service_init(&service1);

    char codeA[SHORT_CODE_LEN];
    char codeB[SHORT_CODE_LEN];
    char codeC[SHORT_CODE_LEN];

    url_service_shorten(&service1, "https://itemA.com", codeA, sizeof(codeA));
    url_service_shorten(&service1, "https://itemB.com", codeB, sizeof(codeB));
    url_service_shorten(&service1, "https://itemC.com", codeC, sizeof(codeC));

    /* Delete codeB */
    url_service_delete(&service1, codeB);

    storage_save_to_file(&service1, test_file);
    url_service_destroy(&service1);

    /* Reload */
    URLService service2;
    url_service_init(&service2);
    storage_load_from_file(&service2, test_file);

    ST_ASSERT_EQUAL_INT(url_service_get_count(&service2), 2, "Count should be 2 after deleting 1");
    ST_ASSERT_TRUE(url_service_get_record(&service2, codeB) == NULL, "codeB must NOT exist");
    ST_ASSERT_TRUE(url_service_get_record(&service2, codeA) != NULL, "codeA must exist");
    ST_ASSERT_TRUE(url_service_get_record(&service2, codeC) != NULL, "codeC must exist");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test G: Empty Database */
static int test_empty_database(void) {
    printf("Running test: Empty Database Save & Load...\n");

    const char *test_file = "data/test_empty_db.dat";
    remove(test_file);

    URLService service1;
    url_service_init(&service1);
    int saved = storage_save_to_file(&service1, test_file);
    ST_ASSERT_EQUAL_INT(saved, 0, "Saving empty service must save 0 records");
    url_service_destroy(&service1);

    URLService service2;
    url_service_init(&service2);
    int loaded = storage_load_from_file(&service2, test_file);
    ST_ASSERT_EQUAL_INT(loaded, 0, "Loading empty database must return 0 records");
    ST_ASSERT_EQUAL_INT(url_service_get_count(&service2), 0, "Record count must be 0");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test H: Corrupt & Malformed Files */
static int test_corrupt_files(void) {
    printf("Running test: Corrupted File Rejection...\n");

    const char *corrupt_file = "data/test_corrupt.dat";

    /* Case 1: Bad header */
    {
        FILE *fp = fopen(corrupt_file, "w");
        fprintf(fp, "INVALID_HEADER\nNEXT_ID: 5\nRECORD_COUNT: 0\n");
        fclose(fp);

        URLService service;
        url_service_init(&service);
        int result = storage_load_from_file(&service, corrupt_file);
        ST_ASSERT_TRUE(result < 0, "Invalid header must return error");
        url_service_destroy(&service);
    }

    /* Case 2: Truncated file */
    {
        FILE *fp = fopen(corrupt_file, "w");
        fprintf(fp, "TINYURL_V1\nNEXT_ID: 5\nRECORD_COUNT: 1\n[RECORD]\nCODE: a1\n");
        fclose(fp);

        URLService service;
        url_service_init(&service);
        int result = storage_load_from_file(&service, corrupt_file);
        ST_ASSERT_TRUE(result < 0, "Truncated file must return error");
        url_service_destroy(&service);
    }

    /* Case 3: Duplicate keys inside data file */
    {
        FILE *fp = fopen(corrupt_file, "w");
        fprintf(fp,
                "TINYURL_V1\n"
                "NEXT_ID: 10\n"
                "RECORD_COUNT: 2\n"
                "[RECORD]\n"
                "CODE: dup\n"
                "CLICKS: 1\n"
                "URL: https://dup1.com\n"
                "[RECORD]\n"
                "CODE: dup\n"
                "CLICKS: 2\n"
                "URL: https://dup2.com\n");
        fclose(fp);

        URLService service;
        url_service_init(&service);
        int result = storage_load_from_file(&service, corrupt_file);
        ST_ASSERT_TRUE(result < 0, "Duplicate short code in file must be rejected");
        url_service_destroy(&service);
    }

    /* Case 4: Invalid URL inside data file */
    {
        FILE *fp = fopen(corrupt_file, "w");
        fprintf(fp,
                "TINYURL_V1\n"
                "NEXT_ID: 5\n"
                "RECORD_COUNT: 1\n"
                "[RECORD]\n"
                "CODE: code1\n"
                "CLICKS: 0\n"
                "URL: invalid_url_without_scheme\n");
        fclose(fp);

        URLService service;
        url_service_init(&service);
        int result = storage_load_from_file(&service, corrupt_file);
        ST_ASSERT_TRUE(result < 0, "Invalid URL scheme must be rejected");
        url_service_destroy(&service);
    }

    remove(corrupt_file);
    return 1;
}

/* Test I: Long URLs */
static int test_long_urls(void) {
    printf("Running test: Long URL Persistence...\n");

    const char *test_file = "data/test_long.dat";
    remove(test_file);

    /* Construct a 1500-char valid URL */
    char longUrl[MAX_URL_LEN];
    snprintf(longUrl, sizeof(longUrl), "https://example.com/search?query=");
    size_t curLen = strlen(longUrl);
    while (curLen < 1500 && curLen < MAX_URL_LEN - 1) {
        longUrl[curLen++] = 'a';
    }
    longUrl[curLen] = '\0';

    URLService service1;
    url_service_init(&service1);

    char code[SHORT_CODE_LEN];
    URLStatus status = url_service_shorten(&service1, longUrl, code, sizeof(code));
    ST_ASSERT_EQUAL_INT(status, URL_SUCCESS, "Shortening long URL should succeed");

    storage_save_to_file(&service1, test_file);
    url_service_destroy(&service1);

    URLService service2;
    url_service_init(&service2);
    storage_load_from_file(&service2, test_file);

    const URLRecord *rec = url_service_get_record(&service2, code);
    ST_ASSERT_TRUE(rec != NULL, "Long URL record must be restored");
    ST_ASSERT_EQUAL_STR(rec->originalURL, longUrl, "Restored long URL must match exactly");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

/* Test J: Special Characters in URLs */
static int test_special_characters(void) {
    printf("Running test: Special Characters in URLs...\n");

    const char *test_file = "data/test_special.dat";
    remove(test_file);

    const char *specialUrl =
        "https://example.com:8080/path/to_resource-v1.0.html?key=val&foo=bar_123#sec-1.2%20test";

    URLService service1;
    url_service_init(&service1);

    char code[SHORT_CODE_LEN];
    url_service_shorten(&service1, specialUrl, code, sizeof(code));

    storage_save_to_file(&service1, test_file);
    url_service_destroy(&service1);

    URLService service2;
    url_service_init(&service2);
    storage_load_from_file(&service2, test_file);

    const URLRecord *rec = url_service_get_record(&service2, code);
    ST_ASSERT_TRUE(rec != NULL, "Special character URL record must be restored");
    ST_ASSERT_EQUAL_STR(rec->originalURL, specialUrl, "Special character URL must match exactly");

    url_service_destroy(&service2);
    remove(test_file);
    return 1;
}

int main(void) {
    printf("==================================================================\n");
    printf("   TinyURL DSA Test Suite: File Persistence Layer\n");
    printf("==================================================================\n");

    int all_passed = 1;

    all_passed &= test_nonexistent_file();
    all_passed &= test_save_one_record();
    all_passed &= test_save_multiple_records();
    all_passed &= test_next_id_persistence();
    all_passed &= test_deletion_persistence();
    all_passed &= test_empty_database();
    all_passed &= test_corrupt_files();
    all_passed &= test_long_urls();
    all_passed &= test_special_characters();

    printf("==================================================================\n");
    printf(" Storage Test Summary: %d/%d assertions passed.\n", st_tests_passed, st_tests_run);

    if (all_passed && st_tests_passed == st_tests_run) {
        printf(" Result: ALL STORAGE TESTS PASSED SUCCESSFULLY!\n");
        printf("==================================================================\n");
        return EXIT_SUCCESS;
    } else {
        printf(" Result: SOME STORAGE TESTS FAILED.\n");
        printf("==================================================================\n");
        return EXIT_FAILURE;
    }
}
