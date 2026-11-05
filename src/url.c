#include "url.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Base-62 alphabet containing 62 alphanumeric characters */
static const char BASE62_ALPHABET[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int url_is_valid(const char *url) {
    if (!url) {
        return 0;
    }

    size_t len = strlen(url);
    if (len == 0 || len >= MAX_URL_LEN) {
        return 0;
    }

    /* Verify scheme: accept http:// (length 7) or https:// (length 8) */
    const char *scheme_http = "http://";
    const char *scheme_https = "https://";
    const char *after_scheme = NULL;

    if (strncmp(url, scheme_https, 8) == 0) {
        after_scheme = url + 8;
    } else if (strncmp(url, scheme_http, 7) == 0) {
        after_scheme = url + 7;
    } else {
        return 0;
    }

    /* Host portion must not be empty (e.g. "http://" is invalid) */
    if (*after_scheme == '\0') {
        return 0;
    }

    /* Reject spaces or control characters */
    for (const char *p = url; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (c <= 32 || c >= 127) {
            return 0;
        }
    }

    return 1;
}

int url_is_valid_short_code(const char *shortCode) {
    if (!shortCode) {
        return 0;
    }

    size_t len = strlen(shortCode);
    if (len == 0 || len >= SHORT_CODE_LEN) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        if (!isalnum((unsigned char)shortCode[i])) {
            return 0;
        }
    }

    return 1;
}

void url_generate_code(unsigned long number, char *output, size_t outputSize) {
    if (!output || outputSize == 0) {
        return;
    }

    if (outputSize == 1) {
        output[0] = '\0';
        return;
    }

    char temp[64];
    size_t count = 0;

    if (number == 0) {
        temp[count++] = '0';
    } else {
        while (number > 0 && count < sizeof(temp)) {
            temp[count++] = BASE62_ALPHABET[number % 62];
            number /= 62;
        }
    }

    /* Copy in reverse order to produce big-endian Base-62 representation */
    size_t out_len = 0;
    while (count > 0 && out_len < outputSize - 1) {
        output[out_len++] = temp[--count];
    }
    output[out_len] = '\0';
}

unsigned long url_decode_code(const char *shortCode) {
    if (shortCode == NULL) {
        return 0;
    }

    unsigned long value = 0;
    for (const char *p = shortCode; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        unsigned long digit = 0;
        if (c >= '0' && c <= '9') {
            digit = (unsigned long)(c - '0');
        } else if (c >= 'A' && c <= 'Z') {
            digit = (unsigned long)(c - 'A' + 10);
        } else if (c >= 'a' && c <= 'z') {
            digit = (unsigned long)(c - 'a' + 36);
        } else {
            return 0; /* Invalid character */
        }

        value = value * 62 + digit;
    }

    return value;
}

URLRecord* url_create_record(const char *shortCode, const char *originalURL) {
    if (!shortCode || !originalURL) {
        return NULL;
    }

    /* Validate lengths and format */
    if (!url_is_valid_short_code(shortCode)) {
        return NULL;
    }

    if (!url_is_valid(originalURL)) {
        return NULL;
    }

    size_t sc_len = strlen(shortCode);
    size_t url_len = strlen(originalURL);

    URLRecord *record = (URLRecord *)malloc(sizeof(URLRecord));
    if (!record) {
        return NULL;
    }

    /* Safely copy strings with null terminators */
    memcpy(record->shortCode, shortCode, sc_len + 1);
    memcpy(record->originalURL, originalURL, url_len + 1);
    record->clickCount = 0;
    record->next = NULL;

    return record;
}

void url_free_record(URLRecord *record) {
    if (record != NULL) {
        free(record);
    }
}

int url_list_append(URLRecord **head, URLRecord *record) {
    if (!head || !record) {
        return 0;
    }

    record->next = NULL;

    /* If list is currently empty, new record becomes the head */
    if (*head == NULL) {
        *head = record;
        return 1;
    }

    /* Traverse to the end of the singly linked list */
    URLRecord *curr = *head;
    while (curr->next != NULL) {
        curr = curr->next;
    }
    curr->next = record;

    return 1;
}

URLRecord* url_list_find(URLRecord *head, const char *shortCode) {
    if (!head || !shortCode) {
        return NULL;
    }

    URLRecord *curr = head;
    while (curr != NULL) {
        if (strcmp(curr->shortCode, shortCode) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

int url_list_remove(URLRecord **head, const char *shortCode) {
    if (!head || !*head || !shortCode) {
        return 0;
    }

    URLRecord *curr = *head;
    URLRecord *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->shortCode, shortCode) == 0) {
            if (prev == NULL) {
                /* Removing head node (handles both only-node and first-of-many) */
                *head = curr->next;
            } else {
                /* Removing middle or last node */
                prev->next = curr->next;
            }

            url_free_record(curr);
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }

    return 0; /* Short code not found in list */
}

void url_list_free_all(URLRecord *head) {
    URLRecord *curr = head;
    while (curr != NULL) {
        URLRecord *next = curr->next;
        url_free_record(curr);
        curr = next;
    }
}

void url_list_clear(URLRecord **head) {
    if (!head || !*head) {
        return;
    }
    url_list_free_all(*head);
    *head = NULL;
}

size_t url_list_length(const URLRecord *head) {
    size_t count = 0;
    const URLRecord *curr = head;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

void url_print_record(const URLRecord *record) {
    if (!record) {
        printf("(null record)\n");
        return;
    }

    printf("[%s] -> %s (clicks: %lu)\n",
           record->shortCode, record->originalURL, record->clickCount);
}
