/* bst.c — Implementasi ADT Binary Search Tree (tesaurus).
 *
 * BST menjaga key tetap terurut: untuk setiap node, semua kata di
 * subpohon kiri terurut sebelum node itu dan semua kata di subpohon
 * kanan terurut sesudahnya (menurut strcmp). Urutan inilah yang membuat
 * pencarian kata menjadi sekadar penelusuran "ke kiri atau ke kanan" —
 * pas untuk tesaurus yang berkunci pada kata itu sendiri.
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

/* Membebaskan sebuah node beserta kedua subpohonnya secara rekursif. */
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

/* Mengalokasikan dan mengisi sebuah node daun berisi `word` + `syns`. */
static BSTNode *node_create(const char *word, const SynList *syns)
{
    BSTNode *n = (BSTNode *)malloc(sizeof(BSTNode));
    if (!n) return NULL;
    strncpy(n->word, word, BST_MAX_WORD - 1);
    n->word[BST_MAX_WORD - 1] = '\0';
    n->syns = *syns;                 /* salin seluruh daftar berukuran tetap */
    n->left = n->right = NULL;
    return n;
}

/* Operasi ADT: INSERT (menyisipkan).
 * Penurunan rekursif: key lebih kecil ke kiri, lebih besar ke kanan,
 * key yang sama daftar sinonimnya ditimpa (didefinisikan ulang). */
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
        node->syns = *syns;          /* kata sama: ganti sinonimnya */

    return node;
}

void bst_insert(BST *t, const char *word, const SynList *syns)
{
    if (!t || !word || !syns || word[0] == '\0') return;
    t->root = insert_rec(t->root, word, syns);
}

/* Operasi ADT: SEARCH (mencari).
 * Penelusuran iteratif: bandingkan, lalu melangkah ke kiri atau ke kanan
 * sampai ketemu atau subpohonnya habis. */
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
