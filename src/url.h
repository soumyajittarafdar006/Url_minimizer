#ifndef URL_H
#define URL_H

#include <stddef.h>

/**
 * Buffer capacities:
 * SHORT_CODE_LEN: 8 bytes (supports up to 7 alphanumeric characters + 1 null terminator).
 * MAX_URL_LEN: 2048 bytes (supports full standard URLs).
 */
#define SHORT_CODE_LEN 8
#define MAX_URL_LEN 2048

/**
 * URLRecord represents a single node in a singly linked list.
 * In the hash table layer, this structure serves as the node for separate chaining.
 * 
 * Fields:
 *  - shortCode: Compact alphanumeric lookup key.
 *  - originalURL: The target destination URL.
 *  - clickCount: Tracks access frequency.
 *  - next: Pointer to next node in the linked list chain.
 */
typedef struct URLRecord {
    char shortCode[SHORT_CODE_LEN];
    char originalURL[MAX_URL_LEN];
    unsigned long clickCount;
    struct URLRecord *next;
} URLRecord;

/* Node Lifecycle Management */
URLRecord* url_create_record(const char *shortCode, const char *originalURL);
void url_free_record(URLRecord *record);

/* Validation Helpers */
int url_is_valid(const char *url);
int url_is_valid_short_code(const char *shortCode);

/* Base-62 Alphanumeric Encoding & Decoding */
void url_generate_code(unsigned long number, char *output, size_t outputSize);
unsigned long url_decode_code(const char *shortCode);

/* Singly Linked List Operations */
int url_list_append(URLRecord **head, URLRecord *record);
URLRecord* url_list_find(URLRecord *head, const char *shortCode);
int url_list_remove(URLRecord **head, const char *shortCode);
void url_list_free_all(URLRecord *head);
void url_list_clear(URLRecord **head);
size_t url_list_length(const URLRecord *head);

/* Display Helper */
void url_print_record(const URLRecord *record);

#endif /* URL_H */
