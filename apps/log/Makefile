TOOLCHAIN_ROOT ?= $(HOME)/arm-buildroot-linux-gnueabihf_sdk-buildroot
CC = $(TOOLCHAIN_ROOT)/bin/arm-buildroot-linux-gnueabihf-gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
CFLAGS += -Iinclude -I../common/inc -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
PTHREAD = -pthread

BIN_DIR = bin
OBJ_DIR = build

CLIENT_SRC = src/log_udp.c
DAEMON_SRC = daemon/log_daemon.c
FILE_LOGGER_SRC = ../common/src/file_logger.c
LOGSEND_SRC = src/log_send_cli.c
TEST_SRC = tests/test_log_udp.c
STRESS_SRC = tests/stress_test.c

all: $(BIN_DIR)/log_daemon $(BIN_DIR)/log_send $(BIN_DIR)/test_log_udp $(BIN_DIR)/stress_test

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/log_udp.o: $(CLIENT_SRC) include/log_udp.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/log_daemon: $(DAEMON_SRC) $(FILE_LOGGER_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREAD) $(LDFLAGS)

$(BIN_DIR)/test_log_udp: $(TEST_SRC) $(OBJ_DIR)/log_udp.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREAD) $(LDFLAGS)

$(BIN_DIR)/log_send: $(LOGSEND_SRC) $(OBJ_DIR)/log_udp.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREAD) $(LDFLAGS)

$(BIN_DIR)/stress_test: $(STRESS_SRC) $(OBJ_DIR)/log_udp.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(PTHREAD) $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR) logs

test: all
	./bin/test_log_udp

stress: all
	./bin/stress_test 20 60

.PHONY: all clean test stress
