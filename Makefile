CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g -Isrc

SRC_DIR = src
OBJ_DIR = obj
TEST_DIR = tests

TARGET = tinyurl
TEST_LIST = test_list_runner
TEST_HASHMAP = test_hashmap_runner
TEST_SERVICE = test_service_runner
TEST_STORAGE = test_storage_runner
TEST_CLI = test_cli_runner

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/url.c $(SRC_DIR)/hashmap.c $(SRC_DIR)/url_service.c $(SRC_DIR)/storage.c
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TEST_LIST_OBJS = $(OBJ_DIR)/test_linked_list.o $(OBJ_DIR)/url.o
TEST_HASHMAP_OBJS = $(OBJ_DIR)/test_hashmap.o $(OBJ_DIR)/hashmap.o $(OBJ_DIR)/url.o
TEST_SERVICE_OBJS = $(OBJ_DIR)/test_url_service.o $(OBJ_DIR)/url_service.o $(OBJ_DIR)/hashmap.o $(OBJ_DIR)/url.o
TEST_STORAGE_OBJS = $(OBJ_DIR)/test_storage.o $(OBJ_DIR)/storage.o $(OBJ_DIR)/url_service.o $(OBJ_DIR)/hashmap.o $(OBJ_DIR)/url.o
TEST_CLI_OBJS = $(OBJ_DIR)/test_cli_integration.o $(OBJ_DIR)/storage.o $(OBJ_DIR)/url_service.o $(OBJ_DIR)/hashmap.o $(OBJ_DIR)/url.o

.PHONY: all clean run test web

all: $(TARGET)

web: $(TARGET)
	python server.py

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_LIST): $(TEST_LIST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_HASHMAP): $(TEST_HASHMAP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_SERVICE): $(TEST_SERVICE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_STORAGE): $(TEST_STORAGE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_CLI): $(TEST_CLI_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_LIST) $(TEST_HASHMAP) $(TEST_SERVICE) $(TEST_STORAGE) $(TEST_CLI)
	@echo "=================================================================="
	@echo " Running Suite 1: Linked-List & Record Tests"
	@echo "=================================================================="
	./$(TEST_LIST)
	@echo ""
	@echo "=================================================================="
	@echo " Running Suite 2: Hash Table & Collision Handling Tests"
	@echo "=================================================================="
	./$(TEST_HASHMAP)
	@echo ""
	@echo "=================================================================="
	@echo " Running Suite 3: URL Service & Application Layer Tests"
	@echo "=================================================================="
	./$(TEST_SERVICE)
	@echo ""
	@echo "=================================================================="
	@echo " Running Suite 4: File Persistence & Storage Tests"
	@echo "=================================================================="
	./$(TEST_STORAGE)
	@echo ""
	@echo "=================================================================="
	@echo " Running Suite 5: CLI Workflow & Integration Tests"
	@echo "=================================================================="
	./$(TEST_CLI)

clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_LIST) $(TEST_HASHMAP) $(TEST_SERVICE) $(TEST_STORAGE) $(TEST_CLI) test_runner
