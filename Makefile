# Makefile untuk Smart Notepad (macOS / Linux, gcc atau clang).
#
# Cara pakai:
#   make            build ./program
#   make run        build lalu langsung jalankan
#   make clean      hapus hasil build
#
# Pengguna Windows: lihat README untuk perintah MinGW.

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
TARGET   = program
SRCS     = main.c trie.c bst.c platform.c
OBJS     = $(SRCS:.c=.o)
HEADERS  = trie.h bst.h platform.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Setiap object bergantung pada semua header (sederhana tapi aman).
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
