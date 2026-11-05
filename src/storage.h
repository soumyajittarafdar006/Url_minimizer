#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include "url_service.h"

/**
 * Default persistent data file path.
 */
#define DEFAULT_DATA_FILE "data/urls.dat"

/**
 * Storage operation status / error codes.
 */
typedef enum StorageStatus {
    STORAGE_SUCCESS = 0,             /* Save or load succeeded */
    STORAGE_ERR_NULL_PARAM = -1,      /* Null service or filename */
    STORAGE_ERR_FILE_OPEN = -2,       /* Unable to open file */
    STORAGE_ERR_WRITE_FAILED = -3,    /* Write failure during serialization */
    STORAGE_ERR_READ_FAILED = -4,     /* Read failure during deserialization */
    STORAGE_ERR_INVALID_HEADER = -5,  /* Missing or invalid header/version */
    STORAGE_ERR_CORRUPT_DATA = -6,    /* Malformed record or corrupt field */
    STORAGE_ERR_RENAME_FAILED = -7    /* Atomic replacement failed */
} StorageStatus;

/**
 * Save all URL records and generator state to a file.
 * Writes to a temporary file (<filename>.tmp) and atomically renames to prevent partial writes.
 * 
 * @param service Pointer to URLService containing state to persist.
 * @param filename File path to write data to.
 * @return Number of records saved (>= 0), or negative StorageStatus on error.
 */
int storage_save_to_file(const URLService *service, const char *filename);

/**
 * Load all URL records and generator state from a file into a URLService.
 * If the file does not exist, initializes an empty service and returns 0.
 * If file corruption is detected, leaves service untouched and returns negative error code.
 * 
 * @param service Pointer to URLService to populate.
 * @param filename File path to read data from.
 * @return Number of records loaded (>= 0), or negative StorageStatus on error.
 */
int storage_load_from_file(URLService *service, const char *filename);

/**
 * Return human-readable message for a StorageStatus error code.
 * 
 * @param status StorageStatus code.
 * @return String description.
 */
const char* storage_strerror(int status);

#endif /* STORAGE_H */
