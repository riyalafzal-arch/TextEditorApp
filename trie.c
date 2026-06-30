/* trie.c — Trie ADT implementation.
 *
 * A Trie (prefix tree) stores words by sharing common prefixes. Each
 * edge is a letter; walking from the root spells out a word. A node
 * flagged is_word marks where a real word ends. This makes
 * "everything starting with these letters" extremely cheap, which is
 * exactly what live autocompletion needs.
 */
#include "trie.h"

#include <stdlib.h>
#include <string.h>

/* Map a character to a child index 0..25, or -1 if it is not a-z.
 * Uppercase letters are folded to lowercase so the Trie stays a-z. */
static int letter_index(char c)
{
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}

/* Allocate a zeroed node (all children NULL, is_word false). */
static TrieNode *node_create(void)
{
    return (TrieNode *)calloc(1, sizeof(TrieNode));
}

Trie *trie_create(void)
{
    Trie *t = (Trie *)malloc(sizeof(Trie));
    if (!t) return NULL;
    t->root = node_create();
    if (!t->root) { free(t); return NULL; }
    return t;
}

/* Recursively free a node and all of its descendants. */
static void node_free(TrieNode *n)
{
    if (!n) return;
    for (int i = 0; i < TRIE_ALPHABET; i++)
        node_free(n->children[i]);
    free(n);
}

void trie_free(Trie *t)
{
    if (!t) return;
    node_free(t->root);
    free(t);
}

/* ADT operation: INSERT.
 * Walk/create one node per letter, then flag the final node as a word.
 * If any character is not a-z the word is rejected (we simply return
 * without modifying the tree). */
void trie_insert(Trie *t, const char *word)
{
    if (!t || !word) return;

    /* First pass: validate the word is all a-z and non-empty. */
    for (const char *p = word; *p; p++)
        if (letter_index(*p) < 0) return;
    if (word[0] == '\0') return;

    TrieNode *cur = t->root;
    for (const char *p = word; *p; p++) {
        int idx = letter_index(*p);
        if (!cur->children[idx]) {
            cur->children[idx] = node_create();
            if (!cur->children[idx]) return;   /* out of memory: give up */
        }
        cur = cur->children[idx];
    }
    cur->is_word = true;
}

/* Walk down the tree following `prefix`. Returns the node reached, or
 * NULL if the path does not exist (or the prefix is not a-z). */
static const TrieNode *find_node(const Trie *t, const char *prefix)
{
    if (!t || !prefix) return NULL;
    const TrieNode *cur = t->root;
    for (const char *p = prefix; *p; p++) {
        int idx = letter_index(*p);
        if (idx < 0) return NULL;
        cur = cur->children[idx];
        if (!cur) return NULL;
    }
    return cur;
}

/* ADT operation: HAS_PREFIX. */
bool trie_has_prefix(const Trie *t, const char *prefix)
{
    return find_node(t, prefix) != NULL;
}

/* Depth-first collection of complete words below `node`.
 * `buf` already holds the prefix in buf[0..depth); we extend it as we
 * descend. Visiting is_word before children yields shortest-first,
 * and scanning children 0..25 yields alphabetical order. */
static void collect_dfs(const TrieNode *node, char *buf, int depth,
                        char results[][TRIE_MAX_WORD], int max, int *count)
{
    if (*count >= max) return;

    if (node->is_word) {
        buf[depth] = '\0';
        strncpy(results[*count], buf, TRIE_MAX_WORD - 1);
        results[*count][TRIE_MAX_WORD - 1] = '\0';
        (*count)++;
        if (*count >= max) return;
    }

    if (depth >= TRIE_MAX_WORD - 1) return;   /* no room to extend buf */

    for (int i = 0; i < TRIE_ALPHABET && *count < max; i++) {
        if (node->children[i]) {
            buf[depth] = (char)('a' + i);
            collect_dfs(node->children[i], buf, depth + 1,
                        results, max, count);
        }
    }
}

/* ADT operation: COLLECT up to N words under a prefix. */
int trie_collect(const Trie *t, const char *prefix,
                 char results[][TRIE_MAX_WORD], int max)
{
    const TrieNode *node = find_node(t, prefix);
    if (!node || max <= 0) return 0;

    char buf[TRIE_MAX_WORD];
    size_t plen = strlen(prefix);
    if (plen >= TRIE_MAX_WORD) plen = TRIE_MAX_WORD - 1;
    memcpy(buf, prefix, plen);

    int count = 0;
    collect_dfs(node, buf, (int)plen, results, max, &count);
    return count;
}

/* ADT operation: list the immediate child letters of a prefix node. */
int trie_next_letters(const Trie *t, const char *prefix, char *out)
{
    const TrieNode *node = find_node(t, prefix);
    int k = 0;
    if (node) {
        for (int i = 0; i < TRIE_ALPHABET; i++)
            if (node->children[i])
                out[k++] = (char)('a' + i);
    }
    out[k] = '\0';
    return k;
}
