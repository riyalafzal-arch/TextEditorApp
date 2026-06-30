/* main.c — Smart Notepad.
 *
 * Notepad mini dengan pengetikan live yang mendemonstrasikan dua ADT pohon:
 *
 *   - Trie (trie.h/.c) menggerakkan autocomplete kata secara live, dan
 *   - Binary Search Tree (bst.h/.c) menggerakkan tesaurus sinonim.
 *
 * File ini hanya memegang *aplikasinya*: buffer dokumen, loop input, dan
 * rendering terminal. Semua urusan terminal yang khusus-OS berada di balik
 * platform.h, dan struktur datanya sama sekali tidak tahu soal layar.
 * Pemisahan ini disengaja — inilah inti dari mata kuliah struktur data.
 */
#include "trie.h"
#include "bst.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Nilai yang bisa disetel ------------------------------------- */
#define MAX_SUGGEST     5            /* jumlah saran yang tampil sekaligus */
#define DICT_PATH       "dictionary.txt"
#define THES_PATH       "thesaurus.txt"

/* ---- Bantuan escape ANSI (diaktifkan di Windows via platform.c) --- */
#define ESC_SAVE_CURSOR     "\033" "7"   /* DECSC: simpan posisi kursor   */
#define ESC_RESTORE_CURSOR  "\033" "8"   /* DECRC: kembalikan kursor      */
#define ESC_CLEAR_TO_END    "\033[0J"    /* hapus dari kursor -> akhir layar */

/* =================================================================== */
/*  Buffer dokumen — array byte sederhana yang bisa tumbuh (append).   */
/* =================================================================== */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Doc;

/* Daftar saran yang sedang tergambar di layar. Inilah satu-satunya sumber
 * kebenaran untuk "apa yang bisa diterima pengguna": tombol panah
 * menggeser `sel` di sepanjang daftar dan Tab menerima items[sel].
 * count == 0 berarti tidak ada daftar yang tampil, jadi panah dan Tab
 * tidak melakukan apa-apa. `nexts` menyimpan huruf-huruf berikutnya yang
 * mungkin, supaya blok bisa digambar ulang (saat panah ditekan) tanpa
 * perlu bertanya lagi ke Trie. */
typedef struct {
    char items[MAX_SUGGEST][TRIE_MAX_WORD];
    int  count;
    int  sel;                          /* indeks yang disorot, 0..count-1 */
    char nexts[TRIE_ALPHABET + 1];     /* huruf berikutnya yang mungkin   */
} Suggestions;

static Suggestions g_sugg = { .count = 0, .sel = 0 };

static void doc_init(Doc *d)
{
    d->cap  = 1024;
    d->len  = 0;
    d->data = (char *)malloc(d->cap);
    if (d->data) d->data[0] = '\0';
}

static void doc_free(Doc *d)
{
    free(d->data);
    d->data = NULL;
    d->len = d->cap = 0;
}

/* Menambahkan satu byte, membesarkan buffer kalau perlu. */
static void doc_push(Doc *d, char c)
{
    if (d->len + 1 >= d->cap) {
        size_t ncap = d->cap * 2;
        char *n = (char *)realloc(d->data, ncap);
        if (!n) return;            /* kehabisan memori: karakter diabaikan */
        d->data = n;
        d->cap  = ncap;
    }
    d->data[d->len++] = c;
    d->data[d->len]   = '\0';
}

/* Menghapus dan mengembalikan byte terakhir (0 kalau buffer kosong). */
static char doc_pop(Doc *d)
{
    if (d->len == 0) return 0;
    char c = d->data[--d->len];
    d->data[d->len] = '\0';
    return c;
}

/* Menyalin "kata saat ini" — deretan karakter bukan-spasi di paling ujung
 * dokumen — ke `out`. Dihitung langsung dari buffer supaya tetap benar
 * bahkan setelah backspace melewati spasi. */
static void doc_current_word(const Doc *d, char *out, size_t cap)
{
    size_t i = d->len;
    while (i > 0) {
        char c = d->data[i - 1];
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') break;
        i--;
    }
    size_t n = d->len - i;
    if (n >= cap) n = cap - 1;
    memcpy(out, d->data + i, n);
    out[n] = '\0';
}

/* Panjang baris terakhir dokumen, dalam karakter (dipakai untuk menaruh
 * kursor saat sebuah newline dihapus dengan backspace). */
static size_t doc_last_line_len(const Doc *d)
{
    size_t i = d->len;
    while (i > 0 && d->data[i - 1] != '\n') i--;
    return d->len - i;
}

/* =================================================================== */
/*  Bantuan string kecil.                                              */
/* =================================================================== */

/* Mengubah `in` jadi huruf kecil ke `out`, mengembalikan true hanya jika
 * setiap karakter adalah huruf (artinya kata murni a-z sehingga bisa
 * ditanyakan ke Trie). */
static bool to_lower_alpha(const char *in, char *out)
{
    int i = 0;
    for (; in[i]; i++) {
        char c = in[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c < 'a' || c > 'z') return false;
        out[i] = c;
    }
    out[i] = '\0';
    return i > 0;
}

/* Mengubah huruf jadi huruf kecil di tempat; byte lain dibiarkan. Dipakai
 * untuk menormalkan kata yang sudah selesai sebelum pencarian tesaurus. */
static void lower_in_place(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* Membuang spasi ASCII di awal dan akhir `s` di tempat. */
static void trim(char *s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

/* =================================================================== */
/*  Rendering.                                                         */
/* =================================================================== */

/* Menggambar `block` di area saran tepat di bawah kursor, lalu
 * mengembalikan kursor ke tempat pengguna mengetik. Memberi NULL hanya
 * membersihkan area itu. Urutannya selalu:
 *   simpan kursor -> turun ke bawah -> hapus sampai akhir layar -> cetak ->
 *   kembalikan kursor -> flush
 * sehingga saran lama tidak pernah tertinggal di antara ketukan. */
static void draw_below(const char *block)
{
    fputs(ESC_SAVE_CURSOR,    stdout);   /* ingat posisi mengetik     */
    fputs("\r\n",             stdout);   /* turun ke baris di bawah   */
    fputs(ESC_CLEAR_TO_END,   stdout);   /* bersihkan semua di bawah  */
    if (block && *block)
        fputs(block, stdout);            /* boleh berisi baris \r\n   */
    fputs(ESC_RESTORE_CURSOR, stdout);   /* kembali ke kursor         */
    fflush(stdout);                      /* tampilkan sekarang (raw)  */
}

/* Menggambar blok saran dari keadaan g_sugg saat ini, menyorot item yang
 * terpilih dengan [kurung siku]. Membersihkan area kalau tidak ada daftar
 * yang aktif. Dipanggil baik saat daftar dibangun ulang (ketukan baru)
 * maupun saat hanya sorotannya yang bergeser (penekanan panah). */
static void draw_suggestion_block(void)
{
    if (g_sugg.count <= 0) { draw_below(NULL); return; }

    char block[1024];
    int off = snprintf(block, sizeof block, "  completions:");
    for (int i = 0; i < g_sugg.count && off < (int)sizeof block - 4; i++) {
        if (i == g_sugg.sel)             /* pilihan yang sedang disorot */
            off += snprintf(block + off, sizeof block - off,
                            " [%s]", g_sugg.items[i]);
        else
            off += snprintf(block + off, sizeof block - off,
                            "  %s ", g_sugg.items[i]);
    }

    off += snprintf(block + off, sizeof block - off,
                    "   (arrows: select, Tab: accept)\r\n  next letters:");
    for (int i = 0; g_sugg.nexts[i] && off < (int)sizeof block - 2; i++)
        off += snprintf(block + off, sizeof block - off, " %c", g_sugg.nexts[i]);

    draw_below(block);
}

/* Menghitung ulang daftar autocomplete untuk kata yang sedang diketik lalu
 * menggambarnya, dengan menyetel sorotan kembali ke item pertama.
 * Membersihkan area kalau tidak ada yang bisa ditampilkan. */
static void render_completions(const Doc *doc, const Trie *trie)
{
    char word[TRIE_MAX_WORD];
    doc_current_word(doc, word, sizeof word);

    /* Tidak ada yang ditawarkan -> tandai "tak ada daftar" supaya
     * panah/Tab tidak berbuat apa-apa. */
    g_sugg.count = 0;
    g_sugg.sel   = 0;

    /* Baru menyarankan setelah pengguna mantap pada sebuah kata (>= 2 huruf). */
    if (strlen(word) < 2) { draw_below(NULL); return; }

    char prefix[TRIE_MAX_WORD];
    if (!to_lower_alpha(word, prefix))   { draw_below(NULL); return; }
    if (!trie_has_prefix(trie, prefix))  { draw_below(NULL); return; }

    /* Kumpulkan langsung ke keadaan yang tampil di layar, sehingga yang
     * dilihat pengguna persis yang akan ditindaklanjuti panah/Tab. */
    g_sugg.count = trie_collect(trie, prefix, g_sugg.items, MAX_SUGGEST);
    trie_next_letters(trie, prefix, g_sugg.nexts);
    g_sugg.sel   = 0;

    draw_suggestion_block();
}

/* Mencari kata yang sudah selesai di tesaurus dan menampilkan sinonimnya
 * di area saran, atau membersihkan area itu kalau tidak ada. */
static void render_synonyms(const char *finished_word, const BST *bst)
{
    /* Kata sudah selesai: tidak ada daftar saran yang bisa dipilih. */
    g_sugg.count = 0;

    if (!finished_word[0]) { draw_below(NULL); return; }

    char key[BST_MAX_WORD];
    strncpy(key, finished_word, sizeof key - 1);
    key[sizeof key - 1] = '\0';
    lower_in_place(key);

    const SynList *s = bst_search(bst, key);
    if (!s || s->count == 0) { draw_below(NULL); return; }

    char block[512];
    int off = snprintf(block, sizeof block, "  Synonyms of %s: ", key);
    for (int i = 0; i < s->count && off < (int)sizeof block - 2; i++)
        off += snprintf(block + off, sizeof block - off,
                        "%s%s", i ? ", " : "", s->words[i]);

    draw_below(block);
}

/* Banner sekali-tampil yang dicetak di atas area mengetik. */
static void print_header(void)
{
    fputs("\r\n", stdout);
    fputs("==== Smart Notepad ==========================================\r\n", stdout);
    fputs(" Trie -> live autocomplete     BST -> synonym thesaurus\r\n", stdout);
    fputs(" Arrows: select   Tab: accept   Ctrl+S: save   Esc/Ctrl+Q: quit\r\n", stdout);
    fputs("-------------------------------------------------------------\r\n", stdout);
    fflush(stdout);
}

/* =================================================================== */
/*  Pemuatan data (keduanya gagal dengan anggun bila file hilang/kosong). */
/* =================================================================== */

static int load_dictionary(Trie *trie, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof line, f)) {
        trim(line);
        if (line[0] == '\0') continue;
        trie_insert(trie, line);     /* kata bukan a-z ditolak di dalam */
        count++;
    }
    fclose(f);
    return count;
}

static int load_thesaurus(BST *bst, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[512];
    int count = 0;
    while (fgets(line, sizeof line, f)) {
        /* Tiap baris berformat  "kata: sin1, sin2, sin3". */
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';

        char word[BST_MAX_WORD];
        trim(line);
        lower_in_place(line);
        if (line[0] == '\0') continue;
        strncpy(word, line, sizeof word - 1);
        word[sizeof word - 1] = '\0';

        SynList syns;
        syns.count = 0;
        char *tok = strtok(colon + 1, ",");
        while (tok && syns.count < BST_MAX_SYNONYMS) {
            trim(tok);
            if (*tok) {
                strncpy(syns.words[syns.count], tok, BST_MAX_SYN_LEN - 1);
                syns.words[syns.count][BST_MAX_SYN_LEN - 1] = '\0';
                syns.count++;
            }
            tok = strtok(NULL, ",");
        }

        if (syns.count > 0) {
            bst_insert(bst, word, &syns);
            count++;
        }
    }
    fclose(f);
    return count;
}

/* =================================================================== */
/*  Simpan (Ctrl+S): keluar mode raw, tanya nama, tulis, masuk raw lagi. */
/* =================================================================== */
static void do_save(const Doc *doc)
{
    disable_raw_mode();                 /* input kanonik untuk fgets()  */

    fputs("\r\n", stdout);
    fputs(ESC_CLEAR_TO_END, stdout);    /* bersihkan blok saran apa pun */
    fputs("Save as: ", stdout);
    fflush(stdout);

    char fname[260];
    if (fgets(fname, sizeof fname, stdin)) {
        trim(fname);
        if (fname[0]) {
            FILE *f = fopen(fname, "wb");
            if (f) {
                fwrite(doc->data, 1, doc->len, f);
                fclose(f);
                printf("Saved %zu bytes to \"%s\".\r\n", doc->len, fname);
            } else {
                printf("Could not open \"%s\" for writing.\r\n", fname);
            }
        } else {
            fputs("Save cancelled.\r\n", stdout);
        }
    }
    fflush(stdout);

    enable_raw_mode();                  /* kembali ke pengetikan live    */
}

/* =================================================================== */
/*  Menerima sebuah saran: mengganti kata parsial yang diketik dengan    */
/*  saran yang dipilih. `index` berbasis-0 ke dalam daftar di layar      */
/*  (Tab memakai g_sugg.sel, yaitu item yang sedang disorot oleh panah). */
/*  Tidak melakukan apa-apa kalau index di luar jangkauan, jadi pemanggil */
/*  tidak perlu memvalidasi lebih dulu.                                  */
/* =================================================================== */
static void accept_suggestion(Doc *doc, const BST *bst, int index)
{
    if (index < 0 || index >= g_sugg.count) return;
    const char *full = g_sugg.items[index];

    char word[TRIE_MAX_WORD];
    doc_current_word(doc, word, sizeof word);
    size_t typed = strlen(word);

    /* Hapus kata parsial dari layar dan dari dokumen... */
    for (size_t i = 0; i < typed; i++) {
        fputs("\b \b", stdout);
        doc_pop(doc);
    }
    /* ...lalu ketikkan kata lengkapnya plus satu spasi di belakang. */
    for (const char *p = full; *p; p++) {
        doc_push(doc, *p);
        putchar(*p);
    }
    doc_push(doc, ' ');
    putchar(' ');

    fflush(stdout);
    render_synonyms(full, bst);         /* kata selesai: cari tesaurus  */
}

/* =================================================================== */
/*  Backspace: menghapus satu karakter dan membenahi tampilan layar.   */
/* =================================================================== */
static void handle_backspace(Doc *doc, const Trie *trie)
{
    if (doc->len == 0) return;

    char removed = doc_pop(doc);
    if (removed == '\n') {
        /* Naik kembali ke ujung baris yang kini jadi baris sebelumnya.
         * Baris dokumen mulai di kolom 1, jadi kolom = panjangBarisAkhir + 1. */
        size_t col = doc_last_line_len(doc) + 1;
        printf("\033[A\033[%zuG", col);   /* kursor naik, lalu ke kolom  */
    } else {
        fputs("\b \b", stdout);           /* hapus karakternya           */
    }
    fflush(stdout);

    render_completions(doc, trie);        /* kata mengecil: segarkan saran */
}

/* =================================================================== */
/*  Titik masuk program.                                               */
/* =================================================================== */
int main(void)
{
    Trie *trie = trie_create();
    BST  *bst  = bst_create();
    if (!trie || !bst) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    int nwords = load_dictionary(trie, DICT_PATH);
    int nthes  = load_thesaurus(bst, THES_PATH);

    enable_raw_mode();
    /* Menjamin terminal dikembalikan pada SEMUA jalur keluar, termasuk
     * keluar karena error dan exit() dari mana pun di dalam kode. */
    atexit(disable_raw_mode);

    print_header();
    if (nwords == 0)
        fputs(" (note: no dictionary loaded - autocomplete disabled)\r\n",
              stdout);
    if (nthes == 0)
        fputs(" (note: no thesaurus loaded - synonyms disabled)\r\n",
              stdout);
    fputs("\r\n", stdout);               /* baris kosong: mulai mengetik di sini */
    fflush(stdout);

    Doc doc;
    doc_init(&doc);
    if (!doc.data) { fprintf(stderr, "Out of memory.\n"); return 1; }

    int running = 1;
    while (running) {
        int k = read_key();

        switch (k) {
        case KEY_CTRL_Q:
        case KEY_ESC:                          /* Esc juga keluar */
            running = 0;
            break;

        case KEY_CTRL_S:
            do_save(&doc);
            break;

        case KEY_TAB:
            accept_suggestion(&doc, bst, g_sugg.sel); /* terima yang disorot */
            break;

        case KEY_UP:
        case KEY_LEFT:                          /* geser sorotan mundur */
            if (g_sugg.count > 0) {
                g_sugg.sel = (g_sugg.sel - 1 + g_sugg.count) % g_sugg.count;
                draw_suggestion_block();
            }
            break;

        case KEY_DOWN:
        case KEY_RIGHT:                         /* geser sorotan maju */
            if (g_sugg.count > 0) {
                g_sugg.sel = (g_sugg.sel + 1) % g_sugg.count;
                draw_suggestion_block();
            }
            break;

        case KEY_BACKSPACE:
            handle_backspace(&doc, trie);
            break;

        case KEY_ENTER: {
            /* Kata sebelum newline kini sudah selesai. */
            char word[BST_MAX_WORD];
            doc_current_word(&doc, word, sizeof word);
            doc_push(&doc, '\n');
            fputs("\r\n", stdout);
            fflush(stdout);
            render_synonyms(word, bst);
            break;
        }

        case ' ': {
            /* Spasi juga menyelesaikan kata saat ini. */
            char word[BST_MAX_WORD];
            doc_current_word(&doc, word, sizeof word);
            doc_push(&doc, ' ');
            putchar(' ');
            fflush(stdout);
            render_synonyms(word, bst);
            break;
        }

        default:
            /* Karakter ASCII cetak apa pun diketik ke dokumen (angka pun
             * termasuk — pemilihan sekarang dilakukan dengan tombol panah). */
            if (k >= 32 && k < 127) {
                doc_push(&doc, (char)k);
                putchar((char)k);
                fflush(stdout);
                render_completions(&doc, trie);   /* autocomplete live */
            }
            /* KEY_UNKNOWN dan kode kontrol yang nyasar diabaikan. */
            break;
        }
    }

    /* Keluar dengan rapi: bersihkan area saran, kembalikan terminal, bebaskan. */
    draw_below(NULL);
    fputs("\r\n-- notepad closed --\r\n", stdout);
    fflush(stdout);

    disable_raw_mode();
    doc_free(&doc);
    trie_free(trie);
    bst_free(bst);
    return 0;
}
