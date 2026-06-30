/* bst.h — Binary Search Tree ADT used as a thesaurus.
 *
 * Pure data-structure module: it stores (word -> synonym list) pairs
 * keyed on the word, ordered by strcmp, and answers exact-word lookups.
 *
 * Feature powered by this ADT: synonym ("thesaurus") lookups.
 */
#ifndef BST_H
#define BST_H

#define BST_MAX_WORD      64   /* longest head-word                */
#define BST_MAX_SYNONYMS  16   /* synonyms stored per word         */
#define BST_MAX_SYN_LEN   64   /* longest single synonym           */

/* A fixed-capacity list of synonyms for one word. */
typedef struct SynList {
    char words[BST_MAX_SYNONYMS][BST_MAX_SYN_LEN];
    int  count;
} SynList;

/* A BST node: the head-word (the key) plus its synonyms. */
typedef struct BSTNode {
    char word[BST_MAX_WORD];
    SynList syns;
    struct BSTNode *left, *right;
} BSTNode;

/* The BST handle. Callers treat this as opaque. */
typedef struct BST {
    BSTNode *root;
} BST;

/* Create an empty BST. Returns NULL on allocation failure. */
BST *bst_create(void);

/* Free every node and the handle. Safe on NULL. */
void bst_free(BST *t);

/* Insert `word` with its `syns`. Keys are ordered with strcmp.
 * If `word` already exists its synonym list is replaced. */
void bst_insert(BST *t, const char *word, const SynList *syns);

/* Search for an exact `word`. Returns its synonym list, or NULL if the
 * word is not in the tree. */
const SynList *bst_search(const BST *t, const char *word);

#endif /* BST_H */
