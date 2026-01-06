# Makefile for CacheLineDetection - Cross-platform support

CC = gcc
CFLAGS = -O2 -w -std=c99
TARGET = cacheline_detect
SRC_DIR = Cache\ Line\ Detection

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/cache.c $(SRC_DIR)/fast_math.c $(SRC_DIR)/format.c

# Auto-detect platform using predefined macros
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    CFLAGS += -DPLATFORM_MACOS=1
endif

ifeq ($(UNAME_S),Linux)
    CFLAGS += -DPLATFORM_LINUX=1 -D_POSIX_C_SOURCE=199309L
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean

