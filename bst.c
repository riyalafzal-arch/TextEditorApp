/* bst.c — Binary Search Tree ADT implementation (thesaurus).
 *
 * A BST keeps keys ordered: for every node, all words in the left
 * subtree sort before it and all words in the right subtree sort
 * after it (by strcmp). That ordering turns a word lookup into a
 * simple "go left or go right" walk — ideal for a thesaurus keyed on
 * the word itself.
 */
#include "bst.h"

#include <stdlib.h>
#include <string.h>

BST *bst_create(void)
{
    BST *t = (BST *)malloc(sizeof(BST));
    if (!t) return NULL;
    t->root = NULL;
    return t;
}

/* Recursively free a node and both subtrees. */
static void node_free(BSTNode *n)
{
    if (!n) return;
    node_free(n->left);
    node_free(n->right);
    free(n);
}

void bst_free(BST *t)
{
    if (!t) return;
    node_free(t->root);
    free(t);
}

/* Allocate and populate a leaf node holding `word` + `syns`. */
static BSTNode *node_create(const char *word, const SynList *syns)
{
    BSTNode *n = (BSTNode *)malloc(sizeof(BSTNode));
    if (!n) return NULL;
    strncpy(n->word, word, BST_MAX_WORD - 1);
    n->word[BST_MAX_WORD - 1] = '\0';
    n->syns = *syns;                 /* copy the whole fixed-size list */
    n->left = n->right = NULL;
    return n;
}

/* ADT operation: INSERT.
 * Recursive descent: smaller keys go left, larger go right, equal keys
 * have their synonym list overwritten (a re-definition). */
static BSTNode *insert_rec(BSTNode *node, const char *word,
                           const SynList *syns)
{
    if (!node)
        return node_create(word, syns);

    int cmp = strcmp(word, node->word);
    if (cmp < 0)
        node->left  = insert_rec(node->left,  word, syns);
    else if (cmp > 0)
        node->right = insert_rec(node->right, word, syns);
    else
        node->syns = *syns;          /* same word: replace synonyms */

    return node;
}

void bst_insert(BST *t, const char *word, const SynList *syns)
{
    if (!t || !word || !syns || word[0] == '\0') return;
    t->root = insert_rec(t->root, word, syns);
}

/* ADT operation: SEARCH.
 * Iterative walk: compare, then step left or right until found or the
 * subtree runs out. */
const SynList *bst_search(const BST *t, const char *word)
{
    if (!t || !word) return NULL;
    const BSTNode *cur = t->root;
    while (cur) {
        int cmp = strcmp(word, cur->word);
        if (cmp == 0) return &cur->syns;
        cur = (cmp < 0) ? cur->left : cur->right;
    }
    return NULL;
}
