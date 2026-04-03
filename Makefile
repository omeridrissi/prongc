CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lclang

SRC = prong.c
OUT = build/prong

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -rf build
