/* trie.c — Implementasi ADT Trie.
 *
 * Trie (pohon prefix) menyimpan kata dengan cara berbagi prefix yang
 * sama. Setiap sisi (edge) adalah satu huruf; menelusuri dari akar
 * mengeja sebuah kata. Node yang ditandai is_word menandai tempat suatu
 * kata berakhir. Ini membuat pertanyaan "semua kata yang diawali huruf
 * ini" jadi sangat murah — persis yang dibutuhkan autocomplete live.
 */
#include "trie.h"

#include <stdlib.h>
#include <string.h>

/* Memetakan satu karakter ke indeks anak 0..25, atau -1 jika bukan a-z.
 * Huruf besar diubah jadi huruf kecil supaya Trie tetap a-z. */
static int letter_index(char c)
{
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}

/* Mengalokasikan node yang sudah di-nol-kan (semua anak NULL, is_word false). */
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

/* Membebaskan sebuah node beserta semua keturunannya secara rekursif. */
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

/* Operasi ADT: INSERT (menyisipkan kata).
 * Telusuri/buat satu node per huruf, lalu tandai node terakhir sebagai
 * kata. Kalau ada karakter yang bukan a-z, kata ditolak (kita langsung
 * keluar tanpa mengubah pohon). */
void trie_insert(Trie *t, const char *word)
{
    if (!t || !word) return;

    /* Tahap pertama: pastikan kata seluruhnya a-z dan tidak kosong. */
    for (const char *p = word; *p; p++)
        if (letter_index(*p) < 0) return;
    if (word[0] == '\0') return;

    TrieNode *cur = t->root;
    for (const char *p = word; *p; p++) {
        int idx = letter_index(*p);
        if (!cur->children[idx]) {
            cur->children[idx] = node_create();
            if (!cur->children[idx]) return;   /* kehabisan memori: berhenti */
        }
        cur = cur->children[idx];
    }
    cur->is_word = true;
}

/* Menelusuri pohon mengikuti `prefix`. Mengembalikan node yang dicapai,
 * atau NULL kalau jalurnya tidak ada (atau prefix-nya bukan a-z). */
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

/* Operasi ADT: HAS_PREFIX (cek apakah ada kata berawalan prefix). */
bool trie_has_prefix(const Trie *t, const char *prefix)
{
    return find_node(t, prefix) != NULL;
}

/* Pengumpulan kata lengkap secara depth-first (DFS) di bawah `node`.
 * `buf` sudah berisi prefix di buf[0..depth); kita perpanjang sambil
 * menurun. Mengunjungi is_word sebelum anak-anaknya menghasilkan urutan
 * dari yang terpendek dulu, dan memindai anak 0..25 menghasilkan urutan
 * alfabet. */
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

    if (depth >= TRIE_MAX_WORD - 1) return;   /* tidak ada ruang memperpanjang buf */

    for (int i = 0; i < TRIE_ALPHABET && *count < max; i++) {
        if (node->children[i]) {
            buf[depth] = (char)('a' + i);
            collect_dfs(node->children[i], buf, depth + 1,
                        results, max, count);
        }
    }
}

/* Operasi ADT: COLLECT — mengumpulkan sampai N kata di bawah sebuah prefix. */
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

/* Operasi ADT: mendaftar huruf-huruf anak langsung dari sebuah node prefix. */
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
