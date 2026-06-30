# Smart Notepad

A small cross-platform command-line "smart notepad" written in C for a
data-structures course. You type freely into a growing document and the
program helps you live, character-by-character, using two classic tree ADTs:

| Feature                         | Data structure        |
| ------------------------------- | --------------------- |
| Live word **autocompletion**    | **Trie** (prefix tree) |
| Word **synonyms (thesaurus)**   | **Binary Search Tree** |

The terminal UI is intentionally plain — no colors, no ghost text — but
typing is **live** (each keystroke is handled immediately, not line by line).

---

## How it behaves

- Start typing into an empty document. Everything you type is appended to a
  document buffer (letters, words, sentences, paragraphs).
- Once the **current word** reaches 2+ letters, a suggestion block appears on
  the line(s) **below the cursor**: up to 5 numbered Trie completions plus the
  possible **next letters**. It is redrawn on every keystroke, so it never goes
  stale, and the cursor stays where you are typing.
- Press **Space** or **Enter** to finish a word. The finished word is looked up
  in the BST thesaurus; if it has synonyms they are shown below the cursor.
- Press **Tab** to accept the top completion: the partial word is replaced with
  the full word, a space is added, and its synonyms are looked up.

This is an **append-only** notepad: there is no arrow-key navigation or editing
in the middle of already-typed text.

### Controls

| Key        | Action                                            |
| ---------- | ------------------------------------------------- |
| any letter | type into the document (live autocomplete)        |
| Space / Enter | finish the current word (thesaurus lookup)     |
| Tab        | accept the top completion                         |
| Backspace  | delete the last character                         |
| Ctrl+S     | save (prompts for a filename)                     |
| Ctrl+Q     | quit cleanly                                       |

The terminal is always restored to normal mode on exit.

---

## Building and running

### macOS / Linux (gcc or clang)

With the provided Makefile:

```sh
make
./notepad
```

Or the raw one-line command (no Makefile needed):

```sh
cc -std=c11 -Wall -Wextra -O2 -o notepad main.c trie.c bst.c platform.c
./notepad
```

(`cc` is gcc or clang on almost every system; either works.)

### Windows (MinGW gcc)

Install [MinGW-w64](https://www.mingw-w64.org/) so that `gcc` is on your PATH,
then from a `cmd` or PowerShell prompt:

```sh
gcc -std=c11 -Wall -Wextra -O2 -o notepad.exe main.c trie.c bst.c platform.c
notepad.exe
```

The same source compiles unchanged: `platform.c` selects the Windows Console
API (`_getch` from `conio.h`, plus `ENABLE_VIRTUAL_TERMINAL_PROCESSING` for the
ANSI cursor codes) via `#ifdef _WIN32`, and POSIX `termios` everywhere else.
Run it in a real console window (the classic terminal or Windows Terminal), not
inside an IDE's output pane.

---

## Data files

Both files are loaded at startup and are plain text, so they are easy to swap
for larger ones. Missing or empty files are handled gracefully (the relevant
feature is just disabled, with a note on screen).

- **`dictionary.txt`** — one English word per line (~200 common words seeded).
  Loaded into the **Trie**. Only lowercase `a`–`z` words are kept.
- **`thesaurus.txt`** — lines formatted `word: synonym1, synonym2, synonym3`
  (~40 entries seeded). Loaded into the **BST**.

To use bigger data, just replace these files (keeping the same names/format) or
edit the `DICT_PATH` / `THES_PATH` constants at the top of `main.c`.

---

## Source layout

The ADTs are kept strictly separate from the application logic:

| File                    | Responsibility                                        |
| ----------------------- | ----------------------------------------------------- |
| `trie.h` / `trie.c`     | Trie ADT: insert, has-prefix, collect, next-letters   |
| `bst.h` / `bst.c`       | BST ADT: insert (word + synonyms), search             |
| `platform.h` / `platform.c` | OS terminal abstraction (raw mode + key reading)  |
| `main.c`                | notepad loop, document buffer, rendering              |
