#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "url_service.h"
#include "storage.h"

/* ==================================================
 * Safe Input Helpers
 * ================================================== */

/**
 * Safely read a full line from standard input using fgets().
 * Strips trailing '\r' and '\n' characters.
 * 
 * @param buffer Destination character buffer.
 * @param size Buffer capacity.
 * @return 1 on success, 0 on EOF or input error.
 */
static int read_line(char *buffer, size_t size) {
    if (buffer == NULL || size == 0) {
        return 0;
    }

    if (!fgets(buffer, (int)size, stdin)) {
        return 0; /* EOF or read error */
    }

    size_t len = strlen(buffer);
    while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        buffer[len - 1] = '\0';
        len--;
    }

    return 1;
}

/**
 * Safely parse a menu choice integer within [min, max].
 * 
 * @param choice Output pointer for parsed value.
 * @param min Minimum valid number.
 * @param max Maximum valid number.
 * @return 1 if valid number within range, 0 if invalid input, -1 on EOF.
 */
static int read_int_choice(int *choice, int min, int max) {
    char input[128];
    if (!read_line(input, sizeof(input))) {
        return -1; /* EOF */
    }

    /* Skip leading whitespace */
    char *p = input;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    if (*p == '\0') {
        return 0; /* Empty input */
    }

    char *endPtr = NULL;
    long val = strtol(p, &endPtr, 10);

    /* Check if entire non-whitespace input was parsed */
    while (*endPtr && isspace((unsigned char)*endPtr)) {
        endPtr++;
    }

    if (*endPtr != '\0') {
        return 0; /* Extra non-numeric characters present */
    }

    if (val < min || val > max) {
        return 0; /* Value out of allowed range */
    }

    *choice = (int)val;
    return 1;
}

/**
 * Prompt user for a yes/no confirmation.
 * 
 * @param prompt Display string.
 * @return 1 if user confirmed ('y' or 'Y'), 0 otherwise.
 */
static int read_yes_no(const char *prompt) {
    printf("%s", prompt);
    char buf[32];
    if (!read_line(buf, sizeof(buf))) {
        return 0;
    }

    char *p = buf;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    return (*p == 'y' || *p == 'Y');
}

/* ==================================================
 * Menu Handlers
 * ================================================== */

static void handle_shorten(URLService *service, const char *dataFile) {
    char urlBuffer[MAX_URL_LEN + 64];
    printf("\nEnter the long URL: ");
    if (!read_line(urlBuffer, sizeof(urlBuffer))) {
        return;
    }

    char shortCode[SHORT_CODE_LEN];
    URLStatus status = url_service_shorten(service, urlBuffer, shortCode, sizeof(shortCode));
    if (status != URL_SUCCESS) {
        printf("\nError: %s.\n", url_service_strerror(status));
        return;
    }

    printf("\n--------------------------------------------------------------\n");
    printf("URL shortened successfully!\n\n");
    printf("Original URL : %s\n", urlBuffer);
    printf("Short Code   : %s\n", shortCode);
    printf("Short URL    : %s\n", shortCode);
    printf("--------------------------------------------------------------\n");
    printf("(Note: Use the Short Code within this application to retrieve or search)\n");

    /* Auto-save on successful shortening */
    storage_save_to_file(service, dataFile);
}

static void handle_retrieve(URLService *service, const char *dataFile) {
    char codeBuffer[64];
    printf("\nEnter short code: ");
    if (!read_line(codeBuffer, sizeof(codeBuffer))) {
        return;
    }

    const char *resolved = url_service_resolve(service, codeBuffer);
    if (resolved == NULL) {
        printf("\nShort code not found.\n");
        return;
    }

    const URLRecord *rec = url_service_get_record(service, codeBuffer);
    printf("\n--------------------------------------------------------------\n");
    printf("URL FOUND\n");
    printf("--------------------------------------------------------------\n");
    printf("Short Code    : %s\n", codeBuffer);
    printf("Original URL  : %s\n", resolved);
    printf("Access Count  : %lu\n", rec ? rec->clickCount : 1);
    printf("--------------------------------------------------------------\n");

    /* Auto-save updated access/click count */
    storage_save_to_file(service, dataFile);
}

static void handle_search(const URLService *service) {
    while (1) {
        printf("\n--------------------------------------------------------------\n");
        printf("Search URL\n");
        printf("--------------------------------------------------------------\n");
        printf("1. Search by Short Code\n");
        printf("2. Search by Original URL\n");
        printf("3. Back\n");
        printf("--------------------------------------------------------------\n");
        printf("Enter your choice: ");

        int choice = 0;
        int status = read_int_choice(&choice, 1, 3);
        if (status <= 0) {
            if (status == -1) {
                break; /* EOF */
            }
            printf("Invalid choice. Please enter a number from 1 to 3.\n");
            continue;
        }

        if (choice == 3) {
            break;
        }

        if (choice == 1) {
            char codeBuffer[64];
            printf("\nEnter short code: ");
            if (!read_line(codeBuffer, sizeof(codeBuffer))) {
                break;
            }

            const URLRecord *rec = url_service_search_by_code(service, codeBuffer);
            if (rec == NULL) {
                printf("\nNo matching record found.\n");
            } else {
                printf("\n--------------------------------------------------------------\n");
                printf("Short Code    : %s\n", rec->shortCode);
                printf("Original URL  : %s\n", rec->originalURL);
                printf("Click Count   : %lu\n", rec->clickCount);
                printf("--------------------------------------------------------------\n");
            }
        } else if (choice == 2) {
            char urlBuffer[MAX_URL_LEN + 64];
            printf("\nEnter original URL: ");
            if (!read_line(urlBuffer, sizeof(urlBuffer))) {
                break;
            }

            printf("\n[Scanning stored records across all hash table buckets...]\n");
            const URLRecord *rec = url_service_search_by_url(service, urlBuffer);
            if (rec == NULL) {
                printf("No matching record found.\n");
            } else {
                printf("\n--------------------------------------------------------------\n");
                printf("Short Code    : %s\n", rec->shortCode);
                printf("Original URL  : %s\n", rec->originalURL);
                printf("Click Count   : %lu\n", rec->clickCount);
                printf("--------------------------------------------------------------\n");
            }
        }
    }
}

static void handle_delete(URLService *service, const char *dataFile) {
    char codeBuffer[64];
    printf("\nEnter short code to delete: ");
    if (!read_line(codeBuffer, sizeof(codeBuffer))) {
        return;
    }

    const URLRecord *rec = url_service_get_record(service, codeBuffer);
    if (rec == NULL) {
        printf("\nRecord not found.\n");
        return;
    }

    printf("\n--------------------------------------------------------------\n");
    printf("Short Code    : %s\n", rec->shortCode);
    printf("Original URL  : %s\n", rec->originalURL);
    printf("Click Count   : %lu\n", rec->clickCount);
    printf("--------------------------------------------------------------\n");

    if (!read_yes_no("Are you sure you want to delete this record? (y/n): ")) {
        printf("Deletion cancelled.\n");
        return;
    }

    URLStatus status = url_service_delete(service, codeBuffer);
    if (status == URL_SUCCESS) {
        printf("Record deleted successfully.\n");
        storage_save_to_file(service, dataFile);
    } else {
        printf("Record not found.\n");
    }
}

static void handle_display_all(const URLService *service) {
    size_t total = url_service_get_count(service);
    if (total == 0) {
        printf("\nNo URLs stored.\n");
        return;
    }

    printf("\n================================================================\n");
    printf("                       STORED URLS\n");
    printf("================================================================\n");
    printf("%-5s %-12s %-8s %s\n", "No.", "Short Code", "Clicks", "Original URL");
    printf("----------------------------------------------------------------\n");

    size_t index = 0;
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        const URLRecord *curr = service->map.buckets[i];
        while (curr != NULL) {
            index++;
            printf("%-5zu %-12s %-8lu %s\n",
                   index, curr->shortCode, curr->clickCount, curr->originalURL);
            curr = curr->next;
        }
    }

    printf("================================================================\n");
    printf("Total Records: %zu\n", total);
}

static void handle_display_stats(const URLService *service) {
    HashMapStats stats = url_service_get_stats(service);

    printf("\n==============================================================\n");
    printf("                    HASH TABLE STATISTICS\n");
    printf("==============================================================\n");
    printf("Table Size        : %zu\n", stats.tableSize);
    printf("Total Records     : %zu\n", stats.totalRecords);
    printf("Used Buckets      : %zu\n", stats.usedBuckets);
    printf("Collided Buckets  : %zu\n", stats.collidedBuckets);
    printf("Total Collisions  : %zu\n", stats.collisions);
    printf("Maximum Chain     : %zu\n", stats.maxChainLength);
    printf("Load Factor       : %.4f\n", stats.loadFactor);
    printf("==============================================================\n");
    printf("\n--- DSA Concepts Explained ---\n");
    printf(" - Table Size       : Fixed number of buckets in the hash table (%zu).\n", stats.tableSize);
    printf(" - Used Buckets     : Buckets containing at least one stored record.\n");
    printf(" - Collided Buckets : Buckets with chain length > 1 (hash collisions occurred).\n");
    printf(" - Total Collisions : Total records that collided into already occupied buckets.\n");
    printf(" - Maximum Chain    : Longest singly linked list chain in any bucket.\n");
    printf(" - Load Factor (a)  : Ratio of total records to table capacity (a = n / m).\n");
    printf("                      Average lookup time complexity: O(1 + a).\n");
    printf("------------------------------\n");
}

static void handle_save(const URLService *service, const char *dataFile) {
    int saved = storage_save_to_file(service, dataFile);
    if (saved >= 0) {
        printf("\nData saved successfully (%d records stored in '%s').\n", saved, dataFile);
    } else {
        printf("\nError: Could not save data (%s).\n", storage_strerror(saved));
    }
}

/* ==================================================
 * API Mode Handlers for Web / Script Integration
 * ================================================== */

static void print_json_escaped_string(const char *str) {
    putchar('"');
    if (str != NULL) {
        for (const char *p = str; *p != '\0'; p++) {
            switch (*p) {
                case '"':  fputs("\\\"", stdout); break;
                case '\\': fputs("\\\\", stdout); break;
                case '\b': fputs("\\b", stdout); break;
                case '\f': fputs("\\f", stdout); break;
                case '\n': fputs("\\n", stdout); break;
                case '\r': fputs("\\r", stdout); break;
                case '\t': fputs("\\t", stdout); break;
                default:
                    if ((unsigned char)*p < 32) {
                        printf("\\u%04x", (unsigned char)*p);
                    } else {
                        putchar(*p);
                    }
                    break;
            }
        }
    }
    putchar('"');
}

static int handle_api_command(int argc, char *argv[]) {
    const char *dataFile = DEFAULT_DATA_FILE;
    int argOffset = 0;

    if (argc >= 2 && strcmp(argv[0], "--data-file") == 0) {
        dataFile = argv[1];
        argOffset = 2;
    }

    if (argOffset >= argc) {
        printf("{\"success\":false,\"error\":\"No API action specified\"}\n");
        return 1;
    }

    const char *action = argv[argOffset];

    if (strcmp(action, "shorten") == 0) {
        if (argOffset + 1 >= argc) {
            printf("{\"success\":false,\"error\":\"Missing URL parameter\"}\n");
            return 1;
        }
        const char *url = argv[argOffset + 1];
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        char shortCode[SHORT_CODE_LEN];
        URLStatus status = url_service_shorten(&service, url, shortCode, sizeof(shortCode));
        if (status != URL_SUCCESS) {
            printf("{\"success\":false,\"error\":");
            print_json_escaped_string(url_service_strerror(status));
            printf("}\n");
            url_service_destroy(&service);
            return 0;
        }

        storage_save_to_file(&service, dataFile);
        const URLRecord *rec = url_service_get_record(&service, shortCode);

        printf("{\"success\":true,\"shortCode\":");
        print_json_escaped_string(shortCode);
        printf(",\"originalURL\":");
        print_json_escaped_string(url);
        printf(",\"clickCount\":%lu}\n", rec ? rec->clickCount : 0);

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "resolve") == 0) {
        if (argOffset + 1 >= argc) {
            printf("{\"success\":false,\"error\":\"Missing short code parameter\"}\n");
            return 1;
        }
        const char *code = argv[argOffset + 1];
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        const char *resolved = url_service_resolve(&service, code);
        if (resolved == NULL) {
            printf("{\"success\":false,\"error\":\"URL record not found\"}\n");
            url_service_destroy(&service);
            return 0;
        }

        storage_save_to_file(&service, dataFile);
        const URLRecord *rec = url_service_get_record(&service, code);

        printf("{\"success\":true,\"shortCode\":");
        print_json_escaped_string(code);
        printf(",\"originalURL\":");
        print_json_escaped_string(resolved);
        printf(",\"clickCount\":%lu}\n", rec ? rec->clickCount : 1);

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "search-code") == 0) {
        if (argOffset + 1 >= argc) {
            printf("{\"success\":false,\"error\":\"Missing short code parameter\"}\n");
            return 1;
        }
        const char *code = argv[argOffset + 1];
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        const URLRecord *rec = url_service_search_by_code(&service, code);
        if (rec == NULL) {
            printf("{\"success\":false,\"error\":\"URL record not found\"}\n");
        } else {
            printf("{\"success\":true,\"record\":{\"shortCode\":");
            print_json_escaped_string(rec->shortCode);
            printf(",\"originalURL\":");
            print_json_escaped_string(rec->originalURL);
            printf(",\"clickCount\":%lu}}\n", rec->clickCount);
        }

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "search-url") == 0) {
        if (argOffset + 1 >= argc) {
            printf("{\"success\":false,\"error\":\"Missing URL parameter\"}\n");
            return 1;
        }
        const char *url = argv[argOffset + 1];
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        const URLRecord *rec = url_service_search_by_url(&service, url);
        if (rec == NULL) {
            printf("{\"success\":false,\"error\":\"URL record not found\"}\n");
        } else {
            printf("{\"success\":true,\"record\":{\"shortCode\":");
            print_json_escaped_string(rec->shortCode);
            printf(",\"originalURL\":");
            print_json_escaped_string(rec->originalURL);
            printf(",\"clickCount\":%lu}}\n", rec->clickCount);
        }

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "delete") == 0) {
        if (argOffset + 1 >= argc) {
            printf("{\"success\":false,\"error\":\"Missing short code parameter\"}\n");
            return 1;
        }
        const char *code = argv[argOffset + 1];
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        URLStatus status = url_service_delete(&service, code);
        if (status != URL_SUCCESS) {
            printf("{\"success\":false,\"error\":");
            print_json_escaped_string(url_service_strerror(status));
            printf("}\n");
        } else {
            storage_save_to_file(&service, dataFile);
            printf("{\"success\":true,\"message\":\"Record deleted successfully\"}\n");
        }

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "list") == 0) {
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        size_t total = url_service_get_count(&service);
        printf("{\"success\":true,\"total\":%lu,\"records\":[", (unsigned long)total);
        int first = 1;
        size_t serial = 0;
        for (size_t i = 0; i < TABLE_SIZE; i++) {
            const URLRecord *curr = service.map.buckets[i];
            while (curr != NULL) {
                serial++;
                if (!first) {
                    putchar(',');
                }
                first = 0;
                printf("{\"serial\":%lu,\"shortCode\":", (unsigned long)serial);
                print_json_escaped_string(curr->shortCode);
                printf(",\"originalURL\":");
                print_json_escaped_string(curr->originalURL);
                printf(",\"clickCount\":%lu,\"bucketIndex\":%lu}", curr->clickCount, (unsigned long)i);
                curr = curr->next;
            }
        }
        printf("]}\n");

        url_service_destroy(&service);
        return 0;
    } else if (strcmp(action, "stats") == 0) {
        URLService service;
        url_service_init(&service);
        storage_load_from_file(&service, dataFile);

        HashMapStats stats = url_service_get_stats(&service);
        printf("{\"success\":true,\"tableSize\":%lu,\"totalRecords\":%lu,\"usedBuckets\":%lu,"
               "\"emptyBuckets\":%lu,\"collidedBuckets\":%lu,\"collisions\":%lu,"
               "\"maxChainLength\":%lu,\"loadFactor\":%.4f,\"buckets\":[",
               (unsigned long)stats.tableSize,
               (unsigned long)stats.totalRecords,
               (unsigned long)stats.usedBuckets,
               (unsigned long)(stats.tableSize - stats.usedBuckets),
               (unsigned long)stats.collidedBuckets,
               (unsigned long)stats.collisions,
               (unsigned long)stats.maxChainLength,
               stats.loadFactor);

        for (size_t i = 0; i < TABLE_SIZE; i++) {
            if (i > 0) {
                putchar(',');
            }
            size_t chainLen = url_list_length(service.map.buckets[i]);
            printf("{\"bucket\":%lu,\"count\":%lu,\"codes\":[", (unsigned long)i, (unsigned long)chainLen);
            const URLRecord *curr = service.map.buckets[i];
            int firstCode = 1;
            while (curr != NULL) {
                if (!firstCode) {
                    putchar(',');
                }
                firstCode = 0;
                print_json_escaped_string(curr->shortCode);
                curr = curr->next;
            }
            printf("]}");
        }
        printf("]}\n");

        url_service_destroy(&service);
        return 0;
    } else {
        printf("{\"success\":false,\"error\":\"Unknown API action\"}\n");
        return 1;
    }
}

/* ==================================================
 * Interactive CLI Menu Loop
 * ================================================== */

static int run_interactive_menu(void) {
    printf("==============================================================\n");
    printf("       TinyURL – Hash Table-Based URL Shortening System\n");
    printf("       Data Structures and Algorithms (DSA) Project in C\n");
    printf("==============================================================\n");

    const char *dataFile = DEFAULT_DATA_FILE;

    /* 1. Initialize URLService */
    URLService service;
    url_service_init(&service);

    /* 2. Load records from persistent data file */
    int loaded = storage_load_from_file(&service, dataFile);
    if (loaded < 0) {
        printf("\nNote: Could not load '%s' (%s). Starting with fresh database.\n",
               dataFile, storage_strerror(loaded));
        url_service_init(&service);
    } else {
        printf("\nLoaded %d stored URL record(s) from '%s'.\n", loaded, dataFile);
    }

    /* 3. Main Menu Loop */
    int running = 1;
    while (running) {
        printf("\n--------------------------------------------------------------\n");
        printf("                     TinyURL MENU\n");
        printf("--------------------------------------------------------------\n");
        printf("1. Shorten a URL\n");
        printf("2. Retrieve Original URL\n");
        printf("3. Search URL\n");
        printf("4. Delete Short URL\n");
        printf("5. Display All URLs\n");
        printf("6. Display Statistics\n");
        printf("7. Save Data\n");
        printf("8. Exit\n");
        printf("--------------------------------------------------------------\n");
        printf("Enter your choice: ");

        int choice = 0;
        int status = read_int_choice(&choice, 1, 8);
        if (status <= 0) {
            if (status == -1) {
                /* Handle EOF (Ctrl+D) gracefully */
                printf("\nEOF received. Exiting application.\n");
                break;
            }
            printf("Invalid choice. Please enter a number from 1 to 8.\n");
            continue;
        }

        switch (choice) {
            case 1:
                handle_shorten(&service, dataFile);
                break;
            case 2:
                handle_retrieve(&service, dataFile);
                break;
            case 3:
                handle_search(&service);
                break;
            case 4:
                handle_delete(&service, dataFile);
                break;
            case 5:
                handle_display_all(&service);
                break;
            case 6:
                handle_display_stats(&service);
                break;
            case 7:
                handle_save(&service, dataFile);
                break;
            case 8:
                if (read_yes_no("\nSave changes before exiting? (y/n): ")) {
                    handle_save(&service, dataFile);
                }
                running = 0;
                break;
            default:
                break;
        }
    }

    /* 4. Cleanup memory and shutdown */
    url_service_destroy(&service);
    printf("\nThank you for using TinyURL!\n");

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--api") == 0) {
            return handle_api_command(argc - 2, argv + 2);
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage:\n");
            printf("  %s                                Run interactive CLI menu\n", argv[0]);
            printf("  %s --api shorten <url>            Shorten a long URL\n", argv[0]);
            printf("  %s --api resolve <code>           Resolve short code & increment clicks\n", argv[0]);
            printf("  %s --api search-code <code>       Search record by short code\n", argv[0]);
            printf("  %s --api search-url <url>         Search record by original URL\n", argv[0]);
            printf("  %s --api delete <code>            Delete short code\n", argv[0]);
            printf("  %s --api list                     List all stored URL records\n", argv[0]);
            printf("  %s --api stats                    Retrieve hash table statistics\n", argv[0]);
            return 0;
        }
        fprintf(stderr, "Unknown argument '%s'. Run with --help for usage.\n", argv[1]);
        return 1;
    }

    return run_interactive_menu();
}
