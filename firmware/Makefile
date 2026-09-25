CC = cc
CFLAGS = -std=c99 -O2 -Wall -Wextra -Werror -pedantic
BUILD_DIR = build
TEST_BIN = $(BUILD_DIR)/native_test

.PHONY: all test clean

all: $(TEST_BIN)

$(TEST_BIN): app_protocol.c app_protocol.h app_heartbeat.c app_heartbeat.h native_test.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) app_protocol.c app_heartbeat.c native_test.c -o $(TEST_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN)
