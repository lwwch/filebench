CWD 				:= $(shell pwd)
BUILD_DIR 	:= $(CWD)/build
CC 					?= $(shell which gcc)
BUILD_TYPE 	?= DEBUG

CFLAGS = -std=c11 -Wall -Werror -Wextra
ifeq ($(BUILD_TYPE), DEBUG)
	CFLAGS += -O0 -g -DDEBUG
else ifeq ($(BUILD_TYPE), RELEASE)
	CFLAGS += -O3 -mtune=native -march=native
else
	$(error Invalid build type $(BUILD_TYPE))
endif

default: benchmark

$(BUILD_DIR):
	mkdir -p $@

.PHONY: benchmark
benchmark: $(BUILD_DIR)/benchmark

$(BUILD_DIR)/benchmark: benchmark.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
