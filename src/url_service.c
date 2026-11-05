#include "url_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void url_service_init(URLService *service) {
    if (service == NULL) {
        return;
    }

    hashmap_init(&service->map);
    service->nextId = 1; /* Begin Base-62 ID generation sequence at 1 */
}

void url_service_destroy(URLService *service) {
    if (service == NULL) {
        return;
    }

    hashmap_clear(&service->map);
    service->nextId = 1;
}

URLStatus url_service_import_record(
    URLService *service,
    const char *shortCode,
    const char *originalURL,
    unsigned long clickCount
) {
    if (service == NULL || shortCode == NULL || originalURL == NULL) {
        return URL_ERR_NULL_PARAM;
    }

    if (!url_is_valid_short_code(shortCode)) {
        return URL_ERR_INVALID_CODE;
    }

    if (!url_is_valid(originalURL)) {
        return URL_ERR_INVALID_URL;
    }

    /* Reject duplicate short code */
    if (hashmap_get(&service->map, shortCode) != NULL) {
        return URL_ERR_CONFLICT;
    }

    URLRecord *rec = url_create_record(shortCode, originalURL);
    if (rec == NULL) {
        return URL_ERR_ALLOC_FAILED;
    }

    rec->clickCount = clickCount;

    if (!hashmap_insert(&service->map, rec)) {
        url_free_record(rec);
        return URL_ERR_CONFLICT;
    }

    /* Ensure nextId remains ahead of any imported numeric short code */
    unsigned long id = url_decode_code(shortCode);
    if (id >= service->nextId) {
        service->nextId = id + 1;
    }

    return URL_SUCCESS;
}

unsigned long url_service_get_next_id(const URLService *service) {
    return service ? service->nextId : 1;
}

void url_service_set_next_id(URLService *service, unsigned long nextId) {
    if (service) {
        service->nextId = (nextId > 0) ? nextId : 1;
    }
}

URLStatus url_service_shorten(
    URLService *service,
    const char *originalURL,
    char *shortCodeOutput,
    size_t outputSize
) {
    if (service == NULL || originalURL == NULL || shortCodeOutput == NULL || outputSize == 0) {
        return URL_ERR_NULL_PARAM;
    }

    /* 1. Validate the original URL */
    if (!url_is_valid(originalURL)) {
        return URL_ERR_INVALID_URL;
    }

    /* 2. Deterministic Base-62 generation using increasing numeric ID.
     * In the event of a collision (e.g. from pre-existing codes),
     * increment nextId until a unique code is found. */
    char candidate[SHORT_CODE_LEN];
    int foundUnique = 0;
    const size_t MAX_COLLISION_RETRIES = 100000;

    for (size_t attempt = 0; attempt < MAX_COLLISION_RETRIES; attempt++) {
        unsigned long id = service->nextId++;
        url_generate_code(id, candidate, sizeof(candidate));

        /* Ensure candidate length fits into caller's buffer */
        if (strlen(candidate) >= outputSize) {
            return URL_ERR_BUFFER_TOO_SMALL;
        }

        /* Verify uniqueness against hash table */
        if (hashmap_get(&service->map, candidate) == NULL) {
            foundUnique = 1;
            break;
        }
    }

    if (!foundUnique) {
        return URL_ERR_CONFLICT;
    }

    /* 3. Allocate URLRecord */
    URLRecord *record = url_create_record(candidate, originalURL);
    if (record == NULL) {
        return URL_ERR_ALLOC_FAILED;
    }

    /* 4. Insert into HashMap */
    if (!hashmap_insert(&service->map, record)) {
        /* Free allocated record to prevent memory leak on insertion failure */
        url_free_record(record);
        return URL_ERR_CONFLICT;
    }

    /* 5. Copy generated code to caller's output buffer */
    strncpy(shortCodeOutput, candidate, outputSize);
    shortCodeOutput[outputSize - 1] = '\0';

    return URL_SUCCESS;
}

const char* url_service_resolve(
    URLService *service,
    const char *shortCode
) {
    if (service == NULL || shortCode == NULL) {
        return NULL;
    }

    if (!url_is_valid_short_code(shortCode)) {
        return NULL;
    }

    /* Retrieve record from hash table */
    URLRecord *record = hashmap_get(&service->map, shortCode);
    if (record == NULL) {
        return NULL;
    }

    /* Increment click count only on successful resolution */
    record->clickCount++;
    return record->originalURL;
}

const URLRecord* url_service_get_record(
    const URLService *service,
    const char *shortCode
) {
    if (service == NULL || shortCode == NULL) {
        return NULL;
    }

    /* Inspection without modifying clickCount */
    return hashmap_search(&service->map, shortCode);
}

const URLRecord* url_service_search_by_code(
    const URLService *service,
    const char *shortCode
) {
    return url_service_get_record(service, shortCode);
}

const URLRecord* url_service_search_by_url(
    const URLService *service,
    const char *originalURL
) {
    if (service == NULL || originalURL == NULL) {
        return NULL;
    }

    /* Linear traversal across all TABLE_SIZE buckets and their separate chains.
     * Algorithmic Complexity: O(TABLE_SIZE + n).
     * Returns the first matching record found. */
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        const URLRecord *curr = service->map.buckets[i];
        while (curr != NULL) {
            if (strcmp(curr->originalURL, originalURL) == 0) {
                return curr;
            }
            curr = curr->next;
        }
    }

    return NULL;
}

URLStatus url_service_delete(
    URLService *service,
    const char *shortCode
) {
    if (service == NULL || shortCode == NULL) {
        return URL_ERR_NULL_PARAM;
    }

    if (!url_is_valid_short_code(shortCode)) {
        return URL_ERR_INVALID_CODE;
    }

    /* Remove record through HashMap layer */
    if (hashmap_remove(&service->map, shortCode)) {
        return URL_SUCCESS;
    }

    return URL_ERR_NOT_FOUND;
}

void url_service_foreach(
    const URLService *service,
    void (*callback)(const URLRecord *record)
) {
    if (service == NULL || callback == NULL) {
        return;
    }

    hashmap_foreach(&service->map, callback);
}

size_t url_service_get_count(const URLService *service) {
    if (service == NULL) {
        return 0;
    }
    return service->map.count;
}

HashMapStats url_service_get_stats(const URLService *service) {
    if (service == NULL) {
        HashMapStats empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }

    return hashmap_get_stats(&service->map);
}

void url_service_print_stats(const URLService *service) {
    if (service == NULL) {
        printf("URLService: (null)\n");
        return;
    }

    hashmap_print_stats(&service->map);
}

const char* url_service_strerror(URLStatus status) {
    switch (status) {
        case URL_SUCCESS:
            return "Operation succeeded";
        case URL_ERR_NULL_PARAM:
            return "Null pointer argument";
        case URL_ERR_INVALID_URL:
            return "Invalid URL format or unsupported scheme";
        case URL_ERR_INVALID_CODE:
            return "Invalid short code format";
        case URL_ERR_NOT_FOUND:
            return "URL record not found";
        case URL_ERR_CONFLICT:
            return "Short code collision conflict";
        case URL_ERR_ALLOC_FAILED:
            return "Memory allocation failed";
        case URL_ERR_BUFFER_TOO_SMALL:
            return "Output buffer too small";
        default:
            return "Unknown error";
    }
}
