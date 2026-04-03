CC      := gcc
AR      := ar
ARFLAGS := rcs

SRC_DIR     := src
INC_DIR     := include
BUILD_DIR   := build
OBJ_DIR     := $(BUILD_DIR)/obj

BIN_NAME := prongc
BUILD_PATH := $(BUILD_DIR)/$(BIN_NAME)

# Flags
CFLAGS := -O3 -Wall -Wextra -I$(INC_DIR)

# Source and object files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

all: $(BUILD_PATH)

$(BUILD_PATH): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
