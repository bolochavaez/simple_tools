CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
TARGET = aptest
SOURCE = aptest.c
BUILD_DIR = build

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CLFAGS) -o $(BUILD_DIR)/$(TARGET) $(SOURCE)

install:
	cp $(BUILD_DIR)/$(TARGET) /usr/local/bin/
