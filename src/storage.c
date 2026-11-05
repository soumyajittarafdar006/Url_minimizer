#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper to strip trailing newline/carriage return characters */
static void trim_newline(char *str) {
    if (str == NULL) {
        return;
    }
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

int storage_save_to_file(const URLService *service, const char *filename) {
    if (service == NULL || filename == NULL) {
        return STORAGE_ERR_NULL_PARAM;
    }

    /* Create temporary filename for atomic save */
    char tmpFilename[1024];
    int n = snprintf(tmpFilename, sizeof(tmpFilename), "%s.tmp", filename);
    if (n < 0 || (size_t)n >= sizeof(tmpFilename)) {
        return STORAGE_ERR_NULL_PARAM;
    }

    FILE *fp = fopen(tmpFilename, "w");
    if (fp == NULL) {
        return STORAGE_ERR_FILE_OPEN;
    }

    size_t totalRecords = url_service_get_count(service);
    unsigned long nextId = url_service_get_next_id(service);

    /* 1. Write file header and metadata */
    if (fprintf(fp, "TINYURL_V1\nNEXT_ID: %lu\nRECORD_COUNT: %zu\n",
                nextId, totalRecords) < 0) {
        fclose(fp);
        remove(tmpFilename);
        return STORAGE_ERR_WRITE_FAILED;
    }

    /* 2. Traverse all buckets and write each record */
    size_t writtenCount = 0;
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        const URLRecord *curr = service->map.buckets[i];
        while (curr != NULL) {
            if (fprintf(fp,
                        "[RECORD]\n"
                        "CODE: %s\n"
                        "CLICKS: %lu\n"
                        "URL: %s\n",
                        curr->shortCode,
                        curr->clickCount,
                        curr->originalURL) < 0) {
                fclose(fp);
                remove(tmpFilename);
                return STORAGE_ERR_WRITE_FAILED;
            }
            writtenCount++;
            curr = curr->next;
        }
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        remove(tmpFilename);
        return STORAGE_ERR_WRITE_FAILED;
    }

    if (fclose(fp) != 0) {
        remove(tmpFilename);
        return STORAGE_ERR_WRITE_FAILED;
    }

    /* 3. Atomic replacement: rename temporary file to final target */
    remove(filename); /* Safe on Windows and POSIX before rename */
    if (rename(tmpFilename, filename) != 0) {
        remove(tmpFilename);
        return STORAGE_ERR_RENAME_FAILED;
    }

    return (int)writtenCount;
}

int storage_load_from_file(URLService *service, const char *filename) {
    if (service == NULL || filename == NULL) {
        return STORAGE_ERR_NULL_PARAM;
    }

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        /* File does not exist yet: initialize an empty database and return success */
        url_service_destroy(service);
        url_service_init(service);
        return 0;
    }

    /* Load into a temporary URLService first to prevent partially corrupting live state */
    URLService tempService;
    url_service_init(&tempService);

    char line[MAX_URL_LEN + 128];

    /* 1. Validate magic header */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_INVALID_HEADER;
    }
    trim_newline(line);
    if (strcmp(line, "TINYURL_V1") != 0) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_INVALID_HEADER;
    }

    /* 2. Read NEXT_ID */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_READ_FAILED;
    }
    trim_newline(line);
    unsigned long fileNextId = 0;
    if (sscanf(line, "NEXT_ID: %lu", &fileNextId) != 1 || fileNextId == 0) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_CORRUPT_DATA;
    }

    /* 3. Read RECORD_COUNT */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_READ_FAILED;
    }
    trim_newline(line);
    unsigned long expectedRecordsUL = 0;
    if (sscanf(line, "RECORD_COUNT: %lu", &expectedRecordsUL) != 1) {
        fclose(fp);
        url_service_destroy(&tempService);
        return STORAGE_ERR_CORRUPT_DATA;
    }
    size_t expectedRecords = (size_t)expectedRecordsUL;

    /* 4. Read each record */
    size_t recordsLoaded = 0;
    while (recordsLoaded < expectedRecords) {
        /* Expect [RECORD] tag */
        if (!fgets(line, sizeof(line), fp)) {
            break; /* Premature EOF */
        }
        trim_newline(line);
        if (strcmp(line, "[RECORD]") != 0) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }

        /* Expect CODE: <shortCode> */
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }
        trim_newline(line);
        char shortCode[SHORT_CODE_LEN];
        if (sscanf(line, "CODE: %7s", shortCode) != 1) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }

        /* Expect CLICKS: <unsigned long> */
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }
        trim_newline(line);
        unsigned long clickCount = 0;
        if (sscanf(line, "CLICKS: %lu", &clickCount) != 1) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }

        /* Expect URL: <originalURL> */
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }
        trim_newline(line);
        if (strncmp(line, "URL: ", 5) != 0) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }
        const char *originalURL = line + 5;

        /* Import into tempService (validates code, URL format, and prevents duplicate keys) */
        URLStatus importStatus = url_service_import_record(
            &tempService, shortCode, originalURL, clickCount);

        if (importStatus != URL_SUCCESS) {
            fclose(fp);
            url_service_destroy(&tempService);
            return STORAGE_ERR_CORRUPT_DATA;
        }

        recordsLoaded++;
    }

    fclose(fp);

    /* Verify loaded records match expected count in header */
    if (recordsLoaded != expectedRecords) {
        url_service_destroy(&tempService);
        return STORAGE_ERR_CORRUPT_DATA;
    }

    /* Ensure nextId is at least as high as fileNextId */
    if (tempService.nextId < fileNextId) {
        tempService.nextId = fileNextId;
    }

    /* 5. Commit state: replace live service with validated tempService */
    url_service_destroy(service);
    *service = tempService;

    return (int)recordsLoaded;
}

const char* storage_strerror(int status) {
    switch ((StorageStatus)status) {
        case STORAGE_SUCCESS:
            return "Operation succeeded";
        case STORAGE_ERR_NULL_PARAM:
            return "Null pointer argument";
        case STORAGE_ERR_FILE_OPEN:
            return "Unable to open data file";
        case STORAGE_ERR_WRITE_FAILED:
            return "Failed to write data to file";
        case STORAGE_ERR_READ_FAILED:
            return "Failed to read data from file";
        case STORAGE_ERR_INVALID_HEADER:
            return "Invalid or unsupported file header";
        case STORAGE_ERR_CORRUPT_DATA:
            return "File contains corrupted or malformed data";
        case STORAGE_ERR_RENAME_FAILED:
            return "Atomic rename of data file failed";
        default:
            return "Unknown storage error";
    }
}
