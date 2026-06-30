/* platform.c — Implementasi abstraksi terminal OS.
 *
 * Dua implementasi yang benar-benar terpisah, dipilih saat kompilasi
 * dengan #ifdef _WIN32. Keduanya menyediakan antarmuka yang sama persis
 * seperti yang dideklarasikan di platform.h, sehingga main.c tetap
 * netral terhadap OS.
 */
#include "platform.h"

#include <stdio.h>

#ifdef _WIN32
/* ----------------------------- Windows ----------------------------- */
#include <windows.h>
#include <conio.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE g_hOut = NULL;
static DWORD  g_orig_out_mode = 0;
static int    g_have_mode = 0;

void enable_raw_mode(void)
{
    /* Input dibaca pakai _getch(), yang sudah unbuffered dan tanpa echo,
     * jadi kita cuma perlu mengajari konsol OUTPUT supaya bisa menafsirkan
     * kode escape ANSI yang kita kirim untuk mengatur kursor. */
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (g_hOut != INVALID_HANDLE_VALUE &&
        GetConsoleMode(g_hOut, &g_orig_out_mode)) {
        g_have_mode = 1;
        SetConsoleMode(g_hOut,
                        g_orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}

void disable_raw_mode(void)
{
    if (g_have_mode) {
        SetConsoleMode(g_hOut, g_orig_out_mode);
        g_have_mode = 0;
    }
}

int read_key(void)
{
    int c = _getch();

    /* Windows melaporkan tombol khusus (panah, tombol fungsi, dll.) sebagai
     * urutan dua byte yang diawali 0x00 atau 0xE0. Byte kedua menentukan
     * tombolnya; kita petakan tombol panah dan abaikan sisanya. */
    if (c == 0x00 || c == 0xE0) {
        int c2 = _getch();
        switch (c2) {
            case 72: return KEY_UP;
            case 80: return KEY_DOWN;
            case 75: return KEY_LEFT;
            case 77: return KEY_RIGHT;
            default: return KEY_UNKNOWN;
        }
    }

    switch (c) {
        case '\r': case '\n': return KEY_ENTER;
        case '\t':            return KEY_TAB;
        case 27:              return KEY_ESC;          /* Esc — keluar */
        case 8:   case 127:   return KEY_BACKSPACE;    /* dua-duanya bisa */
        case 19:              return KEY_CTRL_S;        /* Ctrl+S */
        case 17:              return KEY_CTRL_Q;        /* Ctrl+Q */
        default:              return c;
    }
}

#else
/* ------------------------------ POSIX ------------------------------ */
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static struct termios g_orig;
static int g_raw = 0;

/* Mengembalikan nilai bukan-nol bila ada minimal satu byte menunggu di
 * stdin dalam `ms` milidetik. Dipakai untuk membedakan Esc tunggal
 * (keluar) dari awal sebuah urutan escape seperti tombol panah (Esc [ A). */
static int input_pending(int ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec  = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &g_orig) != 0) return;

    struct termios raw = g_orig;
    /* Flag lokal: matikan ICANON (baca per-tombol, bukan per-baris) dan
     * ECHO (kita menggambar karakter sendiri supaya saran bisa digambar
     * ulang). */
    raw.c_lflag &= ~(ICANON | ECHO);
    /* Flag input: matikan IXON supaya Ctrl+S (19) dan Ctrl+Q (17) sampai
     * ke kita, bukan ditelan sebagai kontrol aliran XOFF/XON. */
    raw.c_iflag &= ~(IXON);
    raw.c_cc[VMIN]  = 1;   /* blokir sampai ada minimal satu byte        */
    raw.c_cc[VTIME] = 0;   /* ...tanpa batas waktu antar-byte            */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0)
        g_raw = 1;
}

void disable_raw_mode(void)
{
    if (g_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
        g_raw = 0;
    }
}

int read_key(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
        return KEY_UNKNOWN;

    switch (c) {
        case '\r': case '\n': return KEY_ENTER;
        case '\t':            return KEY_TAB;
        case 8:   case 127:   return KEY_BACKSPACE;   /* dua-duanya bisa */
        case 19:              return KEY_CTRL_S;       /* Ctrl+S */
        case 17:              return KEY_CTRL_Q;       /* Ctrl+Q */
    }

    /* ESC bisa berupa penekanan Esc tunggal (keluar) atau awal sebuah
     * urutan escape seperti tombol panah ("Esc [ A"). Kalau hampir tidak
     * ada byte yang menyusul, anggap itu Esc tunggal. */
    if (c == 27) {
        if (!input_pending(30))
            return KEY_ESC;                 /* Esc tunggal -> keluar */

        unsigned char seq0;
        if (read(STDIN_FILENO, &seq0, 1) != 1) return KEY_ESC;
        if (seq0 == '[') {
            unsigned char seq1;
            if (read(STDIN_FILENO, &seq1, 1) == 1) {
                switch (seq1) {
                    case 'A': return KEY_UP;
                    case 'B': return KEY_DOWN;
                    case 'C': return KEY_RIGHT;
                    case 'D': return KEY_LEFT;
                }
            }
        }
        return KEY_UNKNOWN;                  /* escape lain: abaikan */
    }

    return c;
}

#endif /* _WIN32 */
