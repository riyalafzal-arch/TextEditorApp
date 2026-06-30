/* trie.h — ADT Trie untuk autocomplete kata.
 *
 * Ini modul struktur data murni: tidak tahu apa-apa soal notepad,
 * terminal, atau rendering. Tugasnya cuma menyimpan kata huruf kecil
 * a-z dan menjawab pertanyaan seputar prefix tentang kata-kata itu.
 *
 * Fitur yang ditenagai ADT ini: autocomplete kata secara live.
 */
#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>

#define TRIE_ALPHABET 26   /* 'a'..'z' */
#define TRIE_MAX_WORD 64   /* panjang maksimum kata yang disimpan/dicari */

/* Satu node. Satu slot anak untuk tiap huruf alfabet. */
typedef struct TrieNode {
    struct TrieNode *children[TRIE_ALPHABET];
    bool is_word;          /* true bila sebuah kata utuh berakhir di node ini */
} TrieNode;

/* Pegangan (handle) Trie. Pemanggil memperlakukannya sebagai opaque. */
typedef struct Trie {
    TrieNode *root;
} Trie;

/* Membuat Trie kosong (hanya satu node akar). Mengembalikan NULL jika
 * alokasi gagal. */
Trie *trie_create(void);

/* Membebaskan setiap node dalam Trie beserta handle-nya. Aman untuk NULL. */
void  trie_free(Trie *t);

/* Menyisipkan satu kata. Huruf diubah ke huruf kecil; karakter apa pun
 * yang bukan a-z (angka, tanda baca, aksen) membuat seluruh kata
 * dilewati, sehingga Trie tetap murni a-z Inggris. */
void  trie_insert(Trie *t, const char *word);

/* Mengembalikan true bila minimal ada satu kata tersimpan yang diawali
 * `prefix`. */
bool  trie_has_prefix(const Trie *t, const char *prefix);

/* Mengumpulkan sampai `max` kata utuh yang diawali `prefix`.
 * Kata ditulis ke results[0..), urut alfabet dan terpendek dulu.
 * Tiap slot harus muat TRIE_MAX_WORD byte.
 * Mengembalikan jumlah kata yang berhasil dikumpulkan. */
int   trie_collect(const Trie *t, const char *prefix,
                   char results[][TRIE_MAX_WORD], int max);

/* Mendaftar huruf-huruf anak langsung dari node yang dicapai `prefix`
 * — yaitu "huruf berikutnya yang mungkin" setelah yang sudah diketik.
 * Huruf ditulis urut alfabet, diakhiri NUL, ke `out` (perlu ruang
 * TRIE_ALPHABET + 1 byte).
 * Mengembalikan berapa huruf yang ditulis. */
int   trie_next_letters(const Trie *t, const char *prefix, char *out);

#endif /* TRIE_H */
