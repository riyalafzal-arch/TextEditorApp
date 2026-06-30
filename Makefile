# Makefile for the Smart Notepad (macOS / Linux, gcc or clang).
#
# Usage:
#   make            build ./notepad
#   make run        build then run
#   make clean      remove build artifacts
#
# Windows users: see the README for the MinGW one-liner.

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
TARGET   = notepad
SRCS     = main.c trie.c bst.c platform.c
OBJS     = $(SRCS:.c=.o)
HEADERS  = trie.h bst.h platform.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Every object depends on the headers (simple but safe).
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
