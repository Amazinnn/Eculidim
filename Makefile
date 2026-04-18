CC      = "C:\Program Files (x86)\Dev-Cpp\MinGW32\bin\gcc"
CFLAGS  = -std=c99 -Wall -Wextra -O2 -I./include -Isrc
SRCS    = $(wildcard src/*.c)
SRCS    := $(filter-out src/test.c, $(SRCS))
OBJS    = $(SRCS:.c=.o)
OUT     = Eculidim

.PHONY: all clean test

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT) -lm

src/%.o: src/%.c include/eculid.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUT) src/*.o

test: all
	$(CC) $(CFLAGS) src/test.c $(OBJS) -o test -lm
	./test
