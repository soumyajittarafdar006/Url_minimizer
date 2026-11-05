#ifndef URL_SERVICE_H
#define URL_SERVICE_H

#include <stddef.h>
#include "url.h"
#include "hashmap.h"

/**
 * Service-level status / error codes.
 * Ensures robust error reporting without magic numbers.
 */
typedef enum URLStatus {
    URL_SUCCESS = 0,               /* Operation succeeded */
    URL_ERR_NULL_PARAM,            /* A required pointer argument was NULL */
    URL_ERR_INVALID_URL,           /* URL fails syntax or scheme validation */
    URL_ERR_INVALID_CODE,          /* Short code is invalid or malformed */
    URL_ERR_NOT_FOUND,             /* Short code or URL was not found */
    URL_ERR_CONFLICT,              /* Short code collision could not be resolved */
    URL_ERR_ALLOC_FAILED,          /* Memory allocation (malloc) failed */
    URL_ERR_BUFFER_TOO_SMALL       /* Destination buffer is too small to hold short code */
} URLStatus;

/**
 * URLService maintains application-level state.
 * 
 * Fields:
 *  - map: The underlying Hash Table providing separate chaining storage.
 *  - nextId: Monotonically increasing counter for deterministic Base-62 short-code generation.
 */
typedef struct URLService {
    HashMap map;
    unsigned long nextId;
} URLService;

/* ==================================================
 * 1. Lifecycle Management
 * ================================================== */

/**
 * Initialize a URLService instance.
 * Initializes the underlying HashMap and sets nextId counter to 1.
 * 
 * @param service Pointer to URLService instance.
 */
void url_service_init(URLService *service);

/**
 * Destroy a URLService instance.
 * Frees all dynamically allocated records in the hash table and resets state.
 * 
 * @param service Pointer to URLService instance.
 */
void url_service_destroy(URLService *service);

/* ==================================================
 * 2. Core Service Operations
 * ================================================== */

/**
 * Import a pre-existing record into the service (used during persistence loading).
 * Preserves the exact clickCount and updates nextId to stay ahead of loaded codes.
 * 
 * @param service Pointer to URLService.
 * @param shortCode The short code.
 * @param originalURL The target long URL.
 * @param clickCount Stored retrieval count.
 * @return URL_SUCCESS or URLStatus error.
 */
URLStatus url_service_import_record(
    URLService *service,
    const char *shortCode,
    const char *originalURL,
    unsigned long clickCount
);

/**
 * Get the current nextId generation counter.
 */
unsigned long url_service_get_next_id(const URLService *service);

/**
 * Set the nextId generation counter directly.
 */
void url_service_set_next_id(URLService *service, unsigned long nextId);

/**
 * Shorten an original URL:
 * 1. Validates the original URL format.
 * 2. Deterministically generates an alphanumeric short code using Base-62.
 * 3. Handles collisions by advancing the ID sequence until a unique code is found.
 * 4. Allocates a URLRecord and inserts it into the hash table.
 * 5. Copies the generated code to shortCodeOutput.
 * 
 * @param service Pointer to URLService.
 * @param originalURL Destination URL to shorten.
 * @param shortCodeOutput Output buffer to receive the generated short code.
 * @param outputSize Size of shortCodeOutput buffer in bytes.
 * @return URL_SUCCESS or appropriate URLStatus error code.
 */
URLStatus url_service_shorten(
    URLService *service,
    const char *originalURL,
    char *shortCodeOutput,
    size_t outputSize
);

/**
 * Resolve a short code to its original URL.
 * Increments clickCount ONLY upon successful resolution.
 * 
 * @param service Pointer to URLService.
 * @param shortCode The short code to resolve.
 * @return Pointer to original URL string, or NULL if shortCode is invalid/not found.
 */
const char* url_service_resolve(
    URLService *service,
    const char *shortCode
);

/**
 * Retrieve a URLRecord for read-only inspection WITHOUT incrementing clickCount.
 * 
 * @param service Pointer to URLService.
 * @param shortCode The short code to look up.
 * @return Pointer to matching URLRecord, or NULL if not found.
 */
const URLRecord* url_service_get_record(
    const URLService *service,
    const char *shortCode
);

/* ==================================================
 * 3. Search Operations
 * ================================================== */

/**
 * Search for a record by its short code (O(1 + alpha) average).
 * 
 * @param service Pointer to URLService.
 * @param shortCode Lookup key.
 * @return Pointer to URLRecord, or NULL if not found.
 */
const URLRecord* url_service_search_by_code(
    const URLService *service,
    const char *shortCode
);

/**
 * Search for a record by its original URL (O(TABLE_SIZE + n) linear scan).
 * Traverses all buckets and linked-list chains. Returns the first matching record.
 * 
 * @param service Pointer to URLService.
 * @param originalURL The target URL to locate.
 * @return Pointer to first matching URLRecord, or NULL if not found.
 */
const URLRecord* url_service_search_by_url(
    const URLService *service,
    const char *originalURL
);

/* ==================================================
 * 4. Deletion
 * ================================================== */

/**
 * Delete a URLRecord by its short code.
 * Safely unlinks and deallocates the record through the hash table layer.
 * 
 * @param service Pointer to URLService.
 * @param shortCode Key of record to delete.
 * @return URL_SUCCESS or URLStatus error code.
 */
URLStatus url_service_delete(
    URLService *service,
    const char *shortCode
);

/* ==================================================
 * 5. Traversal and Statistics
 * ================================================== */

/**
 * Iterate over all stored records, invoking callback once per record.
 * 
 * @param service Pointer to URLService.
 * @param callback Function to invoke for each URLRecord.
 */
void url_service_foreach(
    const URLService *service,
    void (*callback)(const URLRecord *record)
);

/**
 * Get total number of active URL records in the service.
 */
size_t url_service_get_count(const URLService *service);

/**
 * Retrieve hash table distribution metrics for DSA analysis.
 */
HashMapStats url_service_get_stats(const URLService *service);

/**
 * Print formatted statistics to stdout.
 */
void url_service_print_stats(const URLService *service);

/**
 * Return human-readable message for a URLStatus code.
 */
const char* url_service_strerror(URLStatus status);

#endif /* URL_SERVICE_H */
