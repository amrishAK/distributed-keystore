CC = gcc
CFLAGS = -I./include -I../../src/keystore -Wall -Wextra -g

UNITY_SRC = ./include/unity.c
TEST_SRC := $(wildcard ./unit_tests/*.c)
KEYSTORE_SRC := $(wildcard ../../src/keystore/*/*.c)
TEST_BIN = ./key_store_test

.PHONY: all test clean

all: $(TEST_BIN)
$(TEST_BIN): $(UNITY_SRC) $(TEST_SRC) $(KEYSTORE_SRC)
	$(CC) $(CFLAGS) -o $(TEST_BIN) \
		$(UNITY_SRC) \
		$(TEST_SRC) \
		$(KEYSTORE_SRC)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(TEST_BIN) *.o *.out *.exe