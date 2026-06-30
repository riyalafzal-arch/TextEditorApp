/* main.c — Smart Notepad.
 *
 * A tiny live-typing notepad that demonstrates two tree ADTs:
 *
 *   - a Trie (trie.h/.c) drives live word autocompletion, and
 *   - a Binary Search Tree (bst.h/.c) drives a synonym thesaurus.
 *
 * This file owns only the *application*: the document buffer, the
 * input loop, and the terminal rendering. All OS-specific terminal
 * work lives behind platform.h, and the data structures know nothing
 * about the screen. That separation is deliberate — it is what the
 * data-structures course is about.
 */
#include "trie.h"
#include "bst.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Tunables ----------------------------------------------------- */
#define MAX_SUGGEST     5            /* completions shown at once       */
#define DICT_PATH       "dictionary.txt"
#define THES_PATH       "thesaurus.txt"

/* ---- ANSI escape helpers (enabled on Windows via platform.c) ------ */
#define ESC_SAVE_CURSOR     "\033" "7"   /* DECSC: save cursor position */
#define ESC_RESTORE_CURSOR  "\033" "8"   /* DECRC: restore it           */
#define ESC_CLEAR_TO_END    "\033[0J"    /* erase cursor -> end of screen */

/* =================================================================== */
/*  Document buffer — a simple growable byte array (append-only).      */
/* =================================================================== */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} Doc;

/* The completion list currently drawn on screen. It is the single
 * source of truth for "what the user can accept": the arrow keys move
 * `sel` through the list and Tab accepts items[sel]. count == 0 means no
 * list is showing, so the arrows and Tab do nothing special. `nexts`
 * caches the possible next letters so the block can be redrawn (on an
 * arrow press) without re-querying the Trie. */
typedef struct {
    char items[MAX_SUGGEST][TRIE_MAX_WORD];
    int  count;
    int  sel;                          /* highlighted index, 0..count-1 */
    char nexts[TRIE_ALPHABET + 1];     /* possible next letters         */
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

/* Append one byte, growing the buffer if needed. */
static void doc_push(Doc *d, char c)
{
    if (d->len + 1 >= d->cap) {
        size_t ncap = d->cap * 2;
        char *n = (char *)realloc(d->data, ncap);
        if (!n) return;            /* out of memory: silently drop char */
        d->data = n;
        d->cap  = ncap;
    }
    d->data[d->len++] = c;
    d->data[d->len]   = '\0';
}

/* Remove and return the last byte (0 if the buffer is empty). */
static char doc_pop(Doc *d)
{
    if (d->len == 0) return 0;
    char c = d->data[--d->len];
    d->data[d->len] = '\0';
    return c;
}

/* Copy the "current word" — the run of non-whitespace characters at
 * the very end of the document — into `out`. Computed straight from
 * the buffer so it stays correct even after backspacing across spaces. */
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

/* Length of the document's last line, in characters (used to place the
 * cursor when a newline is backspaced away). */
static size_t doc_last_line_len(const Doc *d)
{
    size_t i = d->len;
    while (i > 0 && d->data[i - 1] != '\n') i--;
    return d->len - i;
}

/* =================================================================== */
/*  Small string helpers.                                              */
/* =================================================================== */

/* Lowercase `in` into `out`, returning true only if every character is
 * a letter (i.e. the word is pure a-z and so query-able in the Trie). */
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

/* Lowercase letters in place; leave other bytes untouched. Used to
 * normalise a finished word before a thesaurus lookup. */
static void lower_in_place(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

/* Strip leading and trailing ASCII whitespace from `s` in place. */
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

/* Draw `block` in the suggestion area directly below the cursor, then
 * return the cursor to where the user is typing. Passing NULL just
 * clears the area. The sequence is always:
 *   save cursor -> move below -> erase to end of screen -> print ->
 *   restore cursor -> flush
 * so stale suggestions can never linger between keystrokes. */
static void draw_below(const char *block)
{
    fputs(ESC_SAVE_CURSOR,    stdout);   /* remember typing position */
    fputs("\r\n",             stdout);   /* drop to the line below   */
    fputs(ESC_CLEAR_TO_END,   stdout);   /* wipe everything below    */
    if (block && *block)
        fputs(block, stdout);            /* may contain \r\n lines   */
    fputs(ESC_RESTORE_CURSOR, stdout);   /* back to the cursor       */
    fflush(stdout);                      /* show it now (raw mode)   */
}

/* Draw the suggestion block from the current g_sugg state, highlighting
 * the selected item with [brackets]. Clears the area when no list is
 * active. Called both when the list is rebuilt (a new keystroke) and
 * when only the highlight moves (an arrow press). */
static void draw_suggestion_block(void)
{
    if (g_sugg.count <= 0) { draw_below(NULL); return; }

    char block[1024];
    int off = snprintf(block, sizeof block, "  completions:");
    for (int i = 0; i < g_sugg.count && off < (int)sizeof block - 4; i++) {
        if (i == g_sugg.sel)             /* the highlighted choice */
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

/* Recompute the autocompletion list for the word being typed and draw
 * it, resetting the highlight to the first item. Clears the area when
 * there is nothing to show. */
static void render_completions(const Doc *doc, const Trie *trie)
{
    char word[TRIE_MAX_WORD];
    doc_current_word(doc, word, sizeof word);

    /* Nothing to offer -> remember "no list" so the arrows/Tab stay inert. */
    g_sugg.count = 0;
    g_sugg.sel   = 0;

    /* Only suggest once the user has committed to a word (>= 2 chars). */
    if (strlen(word) < 2) { draw_below(NULL); return; }

    char prefix[TRIE_MAX_WORD];
    if (!to_lower_alpha(word, prefix))   { draw_below(NULL); return; }
    if (!trie_has_prefix(trie, prefix))  { draw_below(NULL); return; }

    /* Collect straight into the on-screen state so what the user sees is
     * exactly what the arrows/Tab will act on. */
    g_sugg.count = trie_collect(trie, prefix, g_sugg.items, MAX_SUGGEST);
    trie_next_letters(trie, prefix, g_sugg.nexts);
    g_sugg.sel   = 0;

    draw_suggestion_block();
}

/* Look up a finished word in the thesaurus and show its synonyms in
 * the suggestion area, or clear the area if there are none. */
static void render_synonyms(const char *finished_word, const BST *bst)
{
    /* The word is finished: there is no completion list to pick from. */
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

/* The one-time banner printed above the typing area. */
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
/*  Data loading (both fail gracefully on a missing/empty file).       */
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
        trie_insert(trie, line);     /* non a-z words are rejected inside */
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
        /* Each line is  "word: syn1, syn2, syn3". */
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
/*  Save (Ctrl+S): leave raw mode, prompt, write, re-enter raw mode.   */
/* =================================================================== */
static void do_save(const Doc *doc)
{
    disable_raw_mode();                 /* canonical input for fgets()  */

    fputs("\r\n", stdout);
    fputs(ESC_CLEAR_TO_END, stdout);    /* clear any suggestion block   */
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

    enable_raw_mode();                  /* back to live typing          */
}

/* =================================================================== */
/*  Accept a suggestion: replace the typed partial word with the chosen */
/*  completion. `index` is 0-based into the on-screen list (Tab uses 0, */
/*  the number keys use 1..count-1). Does nothing if the index is out   */
/*  of range, so callers do not need to pre-validate.                   */
/* =================================================================== */
static void accept_suggestion(Doc *doc, const BST *bst, int index)
{
    if (index < 0 || index >= g_sugg.count) return;
    const char *full = g_sugg.items[index];

    char word[TRIE_MAX_WORD];
    doc_current_word(doc, word, sizeof word);
    size_t typed = strlen(word);

    /* Erase the partial word from the screen and the document... */
    for (size_t i = 0; i < typed; i++) {
        fputs("\b \b", stdout);
        doc_pop(doc);
    }
    /* ...then type the full completion plus a trailing space. */
    for (const char *p = full; *p; p++) {
        doc_push(doc, *p);
        putchar(*p);
    }
    doc_push(doc, ' ');
    putchar(' ');

    fflush(stdout);
    render_synonyms(full, bst);         /* word is finished: thesaurus  */
}

/* =================================================================== */
/*  Backspace: delete one character and fix the screen.                */
/* =================================================================== */
static void handle_backspace(Doc *doc, const Trie *trie)
{
    if (doc->len == 0) return;

    char removed = doc_pop(doc);
    if (removed == '\n') {
        /* Walk back up to the end of the now-previous line. Document
         * lines start in column 1, so column = lastlinelen + 1. */
        size_t col = doc_last_line_len(doc) + 1;
        printf("\033[A\033[%zuG", col);   /* cursor up, then to column  */
    } else {
        fputs("\b \b", stdout);           /* rub out the character      */
    }
    fflush(stdout);

    render_completions(doc, trie);        /* word shrank: refresh hints */
}

/* =================================================================== */
/*  Program entry point.                                               */
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
    /* Guarantee the terminal is restored on ANY exit path, including
     * error exits and exit() from deep in the code. */
    atexit(disable_raw_mode);

    print_header();
    if (nwords == 0)
        fputs(" (note: no dictionary loaded - autocomplete disabled)\r\n",
              stdout);
    if (nthes == 0)
        fputs(" (note: no thesaurus loaded - synonyms disabled)\r\n",
              stdout);
    fputs("\r\n", stdout);               /* blank line: typing starts here */
    fflush(stdout);

    Doc doc;
    doc_init(&doc);
    if (!doc.data) { fprintf(stderr, "Out of memory.\n"); return 1; }

    int running = 1;
    while (running) {
        int k = read_key();

        switch (k) {
        case KEY_CTRL_Q:
        case KEY_ESC:                          /* Esc also quits */
            running = 0;
            break;

        case KEY_CTRL_S:
            do_save(&doc);
            break;

        case KEY_TAB:
            accept_suggestion(&doc, bst, g_sugg.sel); /* accept highlighted */
            break;

        case KEY_UP:
        case KEY_LEFT:                          /* move highlight back */
            if (g_sugg.count > 0) {
                g_sugg.sel = (g_sugg.sel - 1 + g_sugg.count) % g_sugg.count;
                draw_suggestion_block();
            }
            break;

        case KEY_DOWN:
        case KEY_RIGHT:                         /* move highlight forward */
            if (g_sugg.count > 0) {
                g_sugg.sel = (g_sugg.sel + 1) % g_sugg.count;
                draw_suggestion_block();
            }
            break;

        case KEY_BACKSPACE:
            handle_backspace(&doc, trie);
            break;

        case KEY_ENTER: {
            /* The word before the newline is now finished. */
            char word[BST_MAX_WORD];
            doc_current_word(&doc, word, sizeof word);
            doc_push(&doc, '\n');
            fputs("\r\n", stdout);
            fflush(stdout);
            render_synonyms(word, bst);
            break;
        }

        case ' ': {
            /* Space also finishes the current word. */
            char word[BST_MAX_WORD];
            doc_current_word(&doc, word, sizeof word);
            doc_push(&doc, ' ');
            putchar(' ');
            fflush(stdout);
            render_synonyms(word, bst);
            break;
        }

        default:
            /* Any printable ASCII character is typed into the document
             * (digits included — selection is done with the arrows now). */
            if (k >= 32 && k < 127) {
                doc_push(&doc, (char)k);
                putchar((char)k);
                fflush(stdout);
                render_completions(&doc, trie);   /* live autocomplete */
            }
            /* KEY_UNKNOWN and stray control codes are ignored. */
            break;
        }
    }

    /* Tidy exit: clear the suggestion area, restore the terminal, free. */
    draw_below(NULL);
    fputs("\r\n-- notepad closed --\r\n", stdout);
    fflush(stdout);

    disable_raw_mode();
    doc_free(&doc);
    trie_free(trie);
    bst_free(bst);
    return 0;
}
