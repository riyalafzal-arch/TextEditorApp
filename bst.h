/* bst.h — ADT Binary Search Tree yang dipakai sebagai tesaurus.
 *
 * Modul struktur data murni: menyimpan pasangan (kata -> daftar sinonim)
 * berkunci pada kata, terurut dengan strcmp, dan menjawab pencarian kata
 * secara persis (exact match).
 *
 * Fitur yang ditenagai ADT ini: pencarian sinonim ("tesaurus").
 */
#ifndef BST_H
#define BST_H

#define BST_MAX_WORD      64   /* panjang maksimum kata kepala (key) */
#define BST_MAX_SYNONYMS  16   /* jumlah sinonim per kata            */
#define BST_MAX_SYN_LEN   64   /* panjang maksimum satu sinonim      */

/* Daftar sinonim berkapasitas tetap untuk satu kata. */
typedef struct SynList {
    char words[BST_MAX_SYNONYMS][BST_MAX_SYN_LEN];
    int  count;
} SynList;

/* Node BST: kata kepala (key) beserta sinonim-sinonimnya. */
typedef struct BSTNode {
    char word[BST_MAX_WORD];
    SynList syns;
    struct BSTNode *left, *right;
} BSTNode;

/* Pegangan (handle) BST. Pemanggil memperlakukannya sebagai opaque. */
typedef struct BST {
    BSTNode *root;
} BST;

/* Membuat BST kosong. Mengembalikan NULL jika alokasi gagal. */
BST *bst_create(void);

/* Membebaskan setiap node beserta handle-nya. Aman untuk NULL. */
void bst_free(BST *t);

/* Menyisipkan `word` beserta `syns`. Key diurutkan dengan strcmp.
 * Kalau `word` sudah ada, daftar sinonimnya diganti. */
void bst_insert(BST *t, const char *word, const SynList *syns);

/* Mencari `word` secara persis. Mengembalikan daftar sinonimnya, atau
 * NULL kalau kata tidak ada di pohon. */
const SynList *bst_search(const BST *t, const char *word);

#endif /* BST_H */
