/* trie.h — Trie ADT for word autocompletion.
 *
 * This is a pure data-structure module: it knows nothing about the
 * notepad, the terminal, or rendering. It only stores lowercase a-z
 * words and answers prefix questions about them.
 *
 * Feature powered by this ADT: live word autocompletion.
 */
#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>

#define TRIE_ALPHABET 26   /* 'a'..'z' */
#define TRIE_MAX_WORD 64   /* longest word we will ever store or query   */

/* A single node. One child slot per letter of the alphabet. */
typedef struct TrieNode {
    struct TrieNode *children[TRIE_ALPHABET];
    bool is_word;          /* true when a complete word ends at this node */
} TrieNode;

/* The Trie handle. Callers treat this as opaque. */
typedef struct Trie {
    TrieNode *root;
} Trie;

/* Create an empty Trie (single root node). Returns NULL on allocation
 * failure. */
Trie *trie_create(void);

/* Free every node in the Trie and the handle itself. Safe on NULL. */
void  trie_free(Trie *t);

/* Insert one word. Letters are lowercased; any character that is not
 * a-z (digits, punctuation, accents) makes the whole word be skipped,
 * keeping the Trie strictly English a-z. */
void  trie_insert(Trie *t, const char *word);

/* Return true if at least one stored word begins with `prefix`. */
bool  trie_has_prefix(const Trie *t, const char *prefix);

/* Collect up to `max` complete words that start with `prefix`.
 * Words are written, alphabetically and shortest-first, into
 * results[0..). Each slot must hold TRIE_MAX_WORD bytes.
 * Returns the number of words actually collected. */
int   trie_collect(const Trie *t, const char *prefix,
                   char results[][TRIE_MAX_WORD], int max);

/* List the immediate child letters of the node reached by `prefix`
 * — i.e. the "possible next letters" after what the user has typed.
 * Letters are written in alphabetical order, NUL-terminated, into
 * `out` (which needs room for TRIE_ALPHABET + 1 bytes).
 * Returns how many letters were written. */
int   trie_next_letters(const Trie *t, const char *prefix, char *out);

#endif /* TRIE_H */
