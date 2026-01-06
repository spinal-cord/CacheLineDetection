# Makefile for CacheLineDetection on macOS

CC = gcc
CFLAGS = -O2 -Wall -std=c99 -DPLATFORM_MACOS=1
TARGET = cacheline_detect
SRC_DIR = Cache\ Line\ Detection

SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/cache.c $(SRC_DIR)/fast_math.c $(SRC_DIR)/format.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)

.PHONY: all clean

