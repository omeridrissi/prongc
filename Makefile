CC = gcc
CFLAGS = -Wall -Wextra -O3 -I/usr/lib/llvm-21/include -Iinc/ 
LDFLAGS = -L/usr/lib/llvm-21/lib -lclang

SRC_DIR = src
OBJ_DIR = build/obj
BIN_DIR = build
TARGET = $(BIN_DIR)/prongc

# Installation paths
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

.PHONY: all clean rebuild install uninstall

all: $(TARGET)

$(TARGET): $(OBJ) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

rebuild: clean all

install: $(TARGET)
	@echo "Installing prongc to $(DESTDIR)$(BINDIR)"
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/prongc
	@echo "Installation complete. Run 'prongc'."

uninstall:
	@echo "Removing prongc from $(DESTDIR)$(BINDIR)"
	rm -f $(DESTDIR)$(BINDIR)/prongc
	@echo "Uninstall complete."
