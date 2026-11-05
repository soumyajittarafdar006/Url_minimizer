# TinyURL – Hash Table-Based URL Shortening System

> A C99 command-line URL shortener demonstrating hash tables, separate chaining, linked lists, Base-62 encoding, persistence, and collision analysis.

---

## Overview

**TinyURL** is an educational, pure C (C99) implementation of a URL-shortening system designed to demonstrate fundamental Data Structures and Algorithms (DSA).

This project is:
- **Local**: Runs entirely on your local machine without external network calls.
- **Command-Line Based**: Driven by an interactive, menu-based terminal interface.
- **Pure C99**: Compiles using GCC with strict standard warning flags (`-Wall -Wextra -pedantic -std=c99`).
- **Educational DSA Focus**: Implements all data structures, hash functions, collision chains, Base-62 encoding, and persistence mechanisms from scratch.
- **Not a Production Internet Service**: It does not run a web server, open network sockets, connect to a database engine, or create internet-accessible hyperlinks. Instead, generated short codes function directly within the application to resolve, redirect, and track access analytics.

---

## Features

- **URL Shortening**: Maps arbitrary destination URLs to compact alphanumeric identifiers.
- **Deterministic Base-62 Code Generation**: Uses an increasing numeric counter (`nextId`) and Base-62 conversion (`0-9`, `A-Z`, `a-z`) for predictable, compact codes.
- **Hash Table Storage**: Fixed-capacity bucket array (`TABLE_SIZE = 101`, prime capacity).
- **Separate Chaining Collision Handling**: Singly linked list chains attached to each bucket resolve collisions without clustering.
- **Short-Code Lookup**: Average $\mathcal{O}(1 + \alpha)$ resolution mapping short codes to destination URLs.
- **Click Counting**: Automatically tracks and increments access frequency on each successful resolution.
- **Two-Dimensional Search**:
  - By Short Code via direct hash indexing ($\mathcal{O}(1 + \alpha)$ average).
  - By Original URL via linear bucket scanning ($\mathcal{O}(m + n)$).
- **Node Deletion**: Unlinks linked list nodes, frees dynamic memory cleanly, and decrements element counts.
- **Flat-File Persistence**: Saves and restores state to `data/urls.dat` across program runs, including `nextId` counter preservation and atomic file writes.
- **Collision & Distribution Analytics**: Computes occupied buckets, collision count, collided buckets, maximum chain length, and load factor ($\alpha$).
- **Robust Input Validation**: Line-buffered input reading via `fgets()` and numeric parsing via `strtol()` to prevent buffer overflows, malformed input crashes, and infinite loops.
- **Automated Test Coverage**: 5 automated test suites with 490 assertions verifying every module under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Architecture

```
+------------------------------------------------------------------+
|                           User Interface                         |
|                           (src/main.c)                           |
+------------------------------------------------------------------+
                                  |
                                  v
+------------------------------------------------------------------+
|                     Application Service Layer                    |
|                   (src/url_service.c / .h)                       |
|   - Deterministic nextId generator state                         |
|   - URL syntax validation & error codes (URLStatus enum)         |
|   - Access/click counting rules                                  |
+------------------------------------------------------------------+
            |                                           |
            v                                           v
+-----------------------+                   +-----------------------+
|   Persistence Layer   |                   |    Hash Table Layer   |
| (src/storage.c / .h)  |                   | (src/hashmap.c / .h)  |
| - Atomic file save    |                   | - 101 Bucket Heads    |
| - Two-phase load      |                   | - Polynomial hash     |
| - State validation    |                   | - Separate chaining   |
+-----------------------+                   +-----------------------+
                                                        |
                                                        v
                                            +-----------------------+
                                            | URLRecord Nodes / SLL |
                                            |    (src/url.c / .h)   |
                                            | - Singly linked lists |
                                            | - Base-62 encode/dec  |
                                            | - malloc() / free()   |
                                            +-----------------------+
```

### Layer Responsibilities

1. **User Interface (`src/main.c`)**: Manages the interactive menu loop, validates user input, triggers auto-saving on state changes, and formats table output.
2. **Application Service (`src/url_service.c/.h`)**: Coordinates application logic, manages the `nextId` counter, validates URL formats, handles click counting, and exposes high-level APIs.
3. **Storage Layer (`src/storage.c/.h`)**: Serializes and deserializes the service state to disk (`data/urls.dat`) using atomic temporary-file replacement and two-phase loading.
4. **Hash Table Layer (`src/hashmap.c/.h`)**: Implements the bucket array, polynomial rolling hash function, separate chaining collision resolution, and collision metrics.
5. **URLRecord Node Layer (`src/url.c/.h`)**: Defines the singly linked list node structure, handles memory allocation/freeing, URL syntax validation, and Base-62 encoding/decoding.

---

## Data Structures

### Hash Table
The hash table is defined as:

```c
#define TABLE_SIZE 101

typedef struct HashMap {
    URLRecord *buckets[TABLE_SIZE];   /* Array of bucket head pointers */
    size_t count;                     /* Total stored records */
} HashMap;
```

- **Bucket Array**: A fixed array of 101 pointers, where each pointer is either `NULL` or points to the head of a singly linked list.
- **Prime Capacity**: `101` was selected as a prime number to reduce clustering when keys share mathematical factors.

### Separate Chaining
When multiple short codes hash to the identical bucket index, collision resolution is handled via **separate chaining**:

```
HashMap buckets[101]
┌─────┐
│  0  │ ──> NULL
├─────┤
│  1  │ ──> [ "1" | "https://google.com" | clicks: 3 ] ──> [ "1z" | "https://gnu.org" | clicks: 1 ] ──> NULL
├─────┤
│ ... │
├─────┤
│ 100 │ ──> [ "A" | "https://kernel.org" | clicks: 0 ] ──> NULL
└─────┘
```

Chaining prevents open-addressing clustering issues and handles dynamic key insertion without table resizing limits.

### Singly Linked List
Nodes in each bucket chain form a singly linked list. Nodes are appended to the chain tail on insertion, traversed sequentially during lookup, and unlinked/freed during deletion.

### URLRecord Structure
Each node in a bucket chain contains:

```c
typedef struct URLRecord {
    char shortCode[SHORT_CODE_LEN];   /* 8 bytes: up to 7 alphanumeric chars + '\0' */
    char originalURL[MAX_URL_LEN];    /* 2048 bytes: destination URL */
    unsigned long clickCount;         /* Access frequency counter */
    struct URLRecord *next;           /* Pointer to next node in collision chain */
} URLRecord;
```

---

## Hash Function

Short codes are mapped to bucket indices using a **Polynomial Rolling Hash Function** (Horner's rule):

$$\text{hash} = \left(\sum_{i=0}^{k-1} S[i] \cdot 31^{k-1-i}\right) \pmod{\text{TABLE\_SIZE}}$$

### Implementation ([`src/hashmap.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/src/hashmap.c#L24-L37))
```c
size_t hashmap_hash(const char *shortCode) {
    if (shortCode == NULL) {
        return 0;
    }

    size_t hash = 0;
    while (*shortCode != '\0') {
        hash = (hash * 31 + (unsigned char)*shortCode) % TABLE_SIZE;
        shortCode++;
    }

    return hash;
}
```

### Why This Works
- **Multiplier 31**: A prime multiplier that spreads input character bits across the hash integer space and can be optimized by compilers into `(hash << 5) - hash`.
- **Modulo 101**: Applied iteratively on every character step to prevent integer overflow while guaranteeing that the return index is strictly within $[0, \text{TABLE\_SIZE} - 1]$.

---

## Short Code Generation

Short codes are generated deterministically using an increasing integer counter (`service->nextId`, initialized to 1) converted into Base-62 notation using the alphabet:

`0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz`

### Conversion Algorithm ([`src/url.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/src/url.c#L69-L97))
```c
void url_generate_code(unsigned long number, char *output, size_t outputSize);
```
- Repeatedly divides `number` by 62, taking remainders as Base-62 digits.
- The digits are reversed to form standard big-endian representation.
- Example mapping:
  - `ID 1` $\rightarrow$ `"1"`
  - `ID 10` $\rightarrow$ `"A"`
  - `ID 35` $\rightarrow$ `"Z"`
  - `ID 36` $\rightarrow$ `"a"`
  - `ID 61` $\rightarrow$ `"z"`
  - `ID 62` $\rightarrow$ `"10"`

### Uniqueness Guarantee
- Because `nextId` increments on every generation, every generated code is distinct.
- Before committing an insertion, the service queries `hashmap_get()`. If the code already exists (e.g. from imported data), `nextId` advances until an unused code is found.

---

## Persistence

Data is stored on disk in `data/urls.dat` using a line-delimited structured text format:

```text
TINYURL_V1
NEXT_ID: 3
RECORD_COUNT: 2
[RECORD]
CODE: 1
CLICKS: 4
URL: https://www.google.com
[RECORD]
CODE: 2
CLICKS: 0
URL: https://github.com/torvalds/linux
```

### Persistence Principles
1. **Human-Readable & Portable**: Eliminates binary struct padding and endianness differences across platforms.
2. **Next-ID Restoration**: `NEXT_ID` is persisted. On loading, `nextId` is restored to $\max(\text{fileNextId},\, \max(\text{recordId}) + 1)$ via `url_decode_code()`, preventing code collisions on subsequent shortening operations.
3. **No Bucket Indices Persisted**: Bucket indices are derived data ($\text{index} = \text{hash}(\text{code})$) and are recalculated dynamically on load.
4. **Atomic Saving**: Data is written to `data/urls.dat.tmp`, flushed, closed, and then atomically renamed to `data/urls.dat` using C99 `rename()`. A crash or interruption cannot leave a corrupted half-written file.
5. **Two-Phase Loading**: Records are parsed into an isolated temporary `URLService`. If any line is corrupted, truncated, or duplicate, the file is rejected and the active service remains untouched.

---

## Command-Line Interface (CLI)

The application provides an interactive menu with 8 options:

1. **Shorten a URL**: Prompts for a long URL, validates the format, generates a Base-62 short code, inserts it into the hash table, and auto-saves.
2. **Retrieve Original URL**: Looks up a short code, increments `clickCount` by 1, displays the original URL, and auto-saves.
3. **Search URL**:
   - *By Short Code*: Fast $\mathcal{O}(1 + \alpha)$ average lookup without incrementing `clickCount`.
   - *By Original URL*: Scans all 101 bucket chains sequentially ($\mathcal{O}(m + n)$).
4. **Delete Short URL**: Displays matching record details, requests user confirmation (`y/n`), unlinks the node from its bucket chain, frees heap memory, and auto-saves.
5. **Display All URLs**: Formatted tabular view showing row numbers, short codes, click counts, and destination URLs.
6. **Display Statistics**: Prints table size, total records, occupied buckets, collided buckets, total collisions, maximum chain length, and load factor ($\alpha$) with educational explanations.
7. **Save Data**: Explicitly writes current state to `data/urls.dat`.
8. **Exit**: Prompts to save any unsaved changes before cleanly freeing all allocated memory.

---

## Input Validation

- **Safe Line Reading**: Uses `fgets()` into sized buffers, stripping trailing `\r` and `\n`. Never uses the obsolete `gets()` or token-based `scanf("%s")`.
- **Numeric Parsing**: Menu choices are parsed using `strtol()`. Leading/trailing whitespace is handled, and non-numeric inputs (`abc`, `1abc`, empty inputs, out-of-range integers) are rejected with a helpful message without crashing or looping.
- **URL Syntax Checks**: Verifies non-empty strings, supported schemes (`http://` or `https://`), non-empty host portions, and absence of control characters or spaces.
- **Short-Code Checks**: Verifies non-empty alphanumeric characters (`[0-9a-zA-Z]`) within buffer limits.
- **EOF Handling**: Gracefully catches `Ctrl+D` (end-of-file), frees memory, and exits cleanly.

---

## Complexity Analysis

| Operation | Function | Average Case | Worst Case | Space Complexity |
|---|---|---|---|---|
| **Hash Computation** | `hashmap_hash()` | $\mathcal{O}(k)$ | $\mathcal{O}(k)$ | $\mathcal{O}(1)$ |
| **Shorten (Hash + Insert)** | `url_service_shorten()` | $\mathcal{O}(1 + \alpha)$ | $\mathcal{O}(n)$ | $\mathcal{O}(1)$ auxiliary |
| **Resolve (Lookup + Click)** | `url_service_resolve()` | $\mathcal{O}(1 + \alpha)$ | $\mathcal{O}(n)$ | $\mathcal{O}(1)$ |
| **Search by Short Code** | `url_service_search_by_code()` | $\mathcal{O}(1 + \alpha)$ | $\mathcal{O}(n)$ | $\mathcal{O}(1)$ |
| **Search by Original URL** | `url_service_search_by_url()` | $\mathcal{O}(m + n)$ | $\mathcal{O}(m + n)$ | $\mathcal{O}(1)$ |
| **Delete Record** | `url_service_delete()` | $\mathcal{O}(1 + \alpha)$ | $\mathcal{O}(n)$ | $\mathcal{O}(1)$ auxiliary |
| **Display All Records** | `hashmap_foreach()` | $\mathcal{O}(m + n)$ | $\mathcal{O}(m + n)$ | $\mathcal{O}(1)$ |
| **Statistics Calculation** | `hashmap_get_stats()` | $\mathcal{O}(m + n)$ | $\mathcal{O}(m + n)$ | $\mathcal{O}(1)$ |
| **Save to Disk** | `storage_save_to_file()` | $\mathcal{O}(m + n)$ | $\mathcal{O}(m + n)$ | $\mathcal{O}(1)$ auxiliary |
| **Load from Disk** | `storage_load_from_file()` | $\mathcal{O}(n)$ | $\mathcal{O}(n^2)$ | $\mathcal{O}(n)$ |

*Notation:*
- $m = \text{TABLE\_SIZE} = 101$
- $n = \text{total number of stored records}$
- $k = \text{length of short code string (up to 7 characters)}$
- $\alpha = \frac{n}{m} = \text{load factor}$

In the average case where the polynomial hash function distributes keys evenly across buckets, $\alpha \ll m$, giving $\mathcal{O}(1)$ expected lookup, insertion, and deletion time.

---

## Testing

The project includes 5 comprehensive test suites covering **490 assertions**:

| Test Suite | File | Assertions | Focus Areas |
|---|---|:---:|---|
| **1. Linked List** | [`tests/test_linked_list.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/tests/test_linked_list.c) | **76 / 76** | Node allocation/freeing, head/middle/tail/only-node deletions, Base-62 encoding/decoding, URL validation. |
| **2. Hash Table** | [`tests/test_hashmap.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/tests/test_hashmap.c) | **276 / 276** | Polynomial hash bounds & determinism, programmatic collision generation & chaining, duplicate rejection, chain traversal. |
| **3. URL Service** | [`tests/test_url_service.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/tests/test_url_service.c) | **67 / 67** | Shortening workflow, click count incrementing, read-only search, error status codes, Base-62 generator state. |
| **4. Storage** | [`tests/test_storage.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/tests/test_storage.c) | **41 / 41** | Nonexistent file fallback, multi-record persistence, nextId preservation, corrupt file rollback, long URLs (1500+ chars), URL query/special characters. |
| **5. CLI Integration** | [`tests/test_cli_integration.c`](file:///home/sagnik-datta-bhowmick/Documents/urlshortener/tests/test_cli_integration.c) | **30 / 30** | End-to-end user workflows: shorten $\rightarrow$ retrieve $\rightarrow$ search $\rightarrow$ delete $\rightarrow$ save $\rightarrow$ restart $\rightarrow$ verify persistence. |
| **Total** | | **490 / 490** | **100% Passed** |

---

## Sanitizer Verification

All test suites and the main application compile cleanly under GCC with zero warnings:

```bash
gcc -Wall -Wextra -pedantic -std=c99 -g -fsanitize=address,undefined
```

### Verified Sanitizer Results
- **Memory Leaks**: 0
- **Heap Buffer Overflows**: 0
- **Use-After-Free**: 0
- **Invalid Reads / Writes**: 0
- **Undefined Behaviors**: 0

---

## Build and Run

### Prerequisites
- GCC compiler
- GNU Make
- Linux, macOS, or WSL

### Build Application
```bash
make
```

### Run Application
```bash
./tinyurl
```

### Run All 5 Test Suites
```bash
make test
```

### Clean Build
```bash
make clean && make
```

---

## Example Usage

```text
==============================================================
       TinyURL – Hash Table-Based URL Shortening System
       Data Structures and Algorithms (DSA) Project in C
==============================================================

Loaded 0 stored URL record(s) from 'data/urls.dat'.

--------------------------------------------------------------
                     TinyURL MENU
--------------------------------------------------------------
1. Shorten a URL
2. Retrieve Original URL
3. Search URL
4. Delete Short URL
5. Display All URLs
6. Display Statistics
7. Save Data
8. Exit
--------------------------------------------------------------
Enter your choice: 1

Enter the long URL: https://github.com/torvalds/linux

--------------------------------------------------------------
URL shortened successfully!

Original URL : https://github.com/torvalds/linux
Short Code   : 1
Short URL    : 1
--------------------------------------------------------------
(Note: Use the Short Code within this application to retrieve or search)

Enter your choice: 2

Enter short code: 1

--------------------------------------------------------------
URL FOUND
--------------------------------------------------------------
Short Code    : 1
Original URL  : https://github.com/torvalds/linux
Access Count  : 1
--------------------------------------------------------------

Enter your choice: 6

==============================================================
                    HASH TABLE STATISTICS
==============================================================
Table Size        : 101
Total Records     : 1
Used Buckets      : 1
Collided Buckets  : 0
Total Collisions  : 0
Maximum Chain     : 1
Load Factor       : 0.0099
==============================================================

--- DSA Concepts Explained ---
 - Table Size       : Fixed number of buckets in the hash table (101).
 - Used Buckets     : Buckets containing at least one stored record.
 - Collided Buckets : Buckets with chain length > 1 (hash collisions occurred).
 - Total Collisions : Total records that collided into already occupied buckets.
 - Maximum Chain    : Longest singly linked list chain in any bucket.
 - Load Factor (a)  : Ratio of total records to table capacity (a = n / m).
                      Average lookup time complexity: O(1 + a).
------------------------------

Enter your choice: 8

Save changes before exiting? (y/n): y
Data saved successfully (1 records stored in 'data/urls.dat').

Thank you for using TinyURL!
```

---

## Web Interface & Architecture

In addition to the interactive command-line interface, **TinyURL** features a clean, modern, and minimal web interface powered directly by the C backend!

### Web Startup Command
To launch the complete application with one command:
```bash
python server.py
```
This startup command will:
1. Automatically compile the C backend using `gcc` if not already built.
2. Start the lightweight built-in Python HTTP bridge server.
3. Serve the web frontend at: **http://localhost:8000**

You can also use:
```bash
make web
```

### Web Architecture
- **Pure C Backend (`tinyurl`)**: Handles all DSA operations (hash table insertion, polynomial rolling hash, separate chaining linked list nodes, Base-62 encoding, click counting, search by short code, search by URL, deletion, and distribution statistics).
- **Lightweight Python Bridge (`server.py`)**: Uses Python's built-in standard library (`http.server`, `json`, `subprocess`) *only* to communicate with the C executable. No external libraries or database engines are used!
- **Modern Responsive Frontend (`frontend/`)**: Pure HTML5, CSS3, and Vanilla JavaScript. Features a clean, minimal design, live statistics dashboard, and an interactive 101-bucket hash table distribution visualizer.
- **Local URL Redirection Route**:
  ```
  http://localhost:8000/r/<shortCode>
  ```
  Resolves the short code in the C hash table, increments the click counter in C, saves to `data/urls.dat`, and redirects to the original destination URL.

---

## Project Structure

```
tinyurl/
│
├── src/
│   ├── main.c                # Interactive CLI menu, input parsing & C API mode
│   ├── url.c                 # URLRecord lifecycle, validation, Base-62 logic
│   ├── url.h                 # URLRecord struct and validation prototypes
│   ├── hashmap.c             # Hash function, bucket array, separate chaining
│   ├── hashmap.h             # HashMap struct and hash table declarations
│   ├── url_service.c         # Application operations, click counting, nextId
│   ├── url_service.h         # URLService struct and URLStatus enum
│   ├── storage.c             # File persistence, atomic save, load validation
│   └── storage.h             # StorageStatus enum and persistence declarations
│
├── data/
│   └── urls.dat              # Persistent storage file for all URL records
│
├── frontend/
│   ├── index.html            # Web interface layout and structure
│   ├── style.css             # Clean, minimal, modern responsive styling
│   └── script.js             # Client-side interactions & API communication
│
├── tests/
│   ├── test_linked_list.c    # Singly linked list and node deletion tests
│   ├── test_hashmap.c        # Hash table, separate chaining, collision tests
│   ├── test_url_service.c    # Service workflow and click count tests
│   ├── test_storage.c        # File I/O, corrupt file rollback, nextId tests
│   └── test_cli_integration.c # Full end-to-end lifecycle tests
│
├── server.py                 # Lightweight HTTP bridge server
├── Makefile                  # Build automation for CLI, tests, and web
└── README.md                 # Project documentation
```

*Generated object files (`obj/*.o`), runtime data (`data/*.dat`), and test executables are ignored by git.*

---

## Design Decisions

1. **Why Separate Chaining?**: Open addressing requires complex tombstone handling for deletions and can degrade significantly when clusters form. Separate chaining using singly linked lists is easy to reason about, provides straightforward deletions, and directly demonstrates linked list manipulation.
2. **Why a Fixed-Size Table (101 Buckets)?**: Keeps the memory model transparent and predictable for academic evaluation without hiding memory growth behind complex dynamic resizing heuristics.
3. **Why Base-62 Encoding?**: Base-62 (`0-9`, `A-Z`, `a-z`) produces compact, URL-safe alphanumeric strings without ambiguous punctuation or URL encoding requirements.
4. **Why Linear Search for Original URLs?**: Indexing original URLs would require a second hash table or bidirectional map. A linear bucket scan ($\mathcal{O}(m + n)$) explicitly contrasts single-index hash efficiency ($\mathcal{O}(1)$) against full-table traversals.
5. **Why Persist `nextId`?**: Preserving `nextId` ensures that subsequent program runs never generate duplicate short codes, even if records were deleted prior to shutdown.
6. **Why Bucket Indices Are Not Persisted**: Bucket indices are derived runtime data. Rehashing short codes on startup guarantees that data remains valid and decoupled from hash table configuration changes.
7. **Why a Local CLI Application?**: Web servers and database engines abstract away the exact data structures being studied. A pure C CLI focuses purely on manual pointer management, memory safety, and algorithmic complexity.

---

## Limitations

- **Fixed Table Capacity**: The hash table has a fixed capacity of 101 buckets. If thousands of records are inserted, average chain lengths grow and performance transitions toward $\mathcal{O}(n)$.
- **Linear URL Search**: Searching by destination URL is $\mathcal{O}(m + n)$ because the table is indexed solely by short code.
- **Single-Process Storage**: File persistence uses a single flat file (`data/urls.dat`) intended for single-user local access without multi-process locking.
- **No Network Layer**: As an educational DSA project, it does not serve HTTP requests or handle DNS resolution.

---

## Future Improvements

*(Potential extensions beyond the educational scope of this project)*:
- **Dynamic Rehashing**: Implement automatic table doubling when the load factor $\alpha > 0.75$.
- **Bidirectional Secondary Index**: Maintain a secondary hash table keyed on original URL for $\mathcal{O}(1)$ duplicate URL lookups.
- **LRU Cache**: Add a doubly linked list cache for frequently accessed records.
- **Prefix Trie**: Support short-code auto-completion or URL domain clustering via an alphanumeric Trie.
