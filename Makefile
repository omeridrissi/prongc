CC = gcc
CFLAGS = -Wall -Wextra -O3 -I/usr/lib/llvm-21/include 
LDFLAGS = -L/usr/lib/llvm-21/lib -lclang

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

OUT = build/prongc

.PHONY: all clean

all: $(OUT)

$(OUT): $(OBJ)
	mkdir -p build
	$(CC) $(OBJ) -o $(OUT) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build *.o
