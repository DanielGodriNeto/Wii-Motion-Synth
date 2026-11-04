# Host-only tests for synth_core. Needs gcc and make; no devkitPPC.
#   make -C tests          build and run unit tests and the callback guard
#   make -C tests SAN=1    same, with UndefinedBehaviorSanitizer (needs libubsan)
#   make -C tests clean    remove tests/build

CC = gcc
SAN ?= 0

CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -ffp-contract=off
ifeq ($(SAN),1)
CFLAGS += -fsanitize=undefined -fno-sanitize-recover=all
endif
LDLIBS = -lm

BUILD = build
TEST_BIN = $(BUILD)/test_synth_core
CORE_SRCS = ../synth_core.c
CORE_HDRS = ../synth_core.h test_synth_core.c

.PHONY: all test clean

all: test

test: $(TEST_BIN)
	./$(TEST_BIN)
	sh check_callback.sh ../main.c

$(TEST_BIN): $(CORE_SRCS) $(CORE_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_synth_core.c $(CORE_SRCS) $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
