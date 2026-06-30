/* platform.c — OS terminal abstraction implementation.
 *
 * Two completely separate implementations selected at compile time
 * with #ifdef _WIN32. Both expose the identical interface declared in
 * platform.h so main.c stays OS-agnostic.
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
    /* Input is read with _getch(), which is already unbuffered and
     * non-echoing, so we only need to teach the OUTPUT console how to
     * interpret the ANSI escape codes we emit for cursor control. */
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

    /* Windows reports special keys (arrows, function keys, etc.) as a
     * two-byte sequence beginning with 0x00 or 0xE0. Swallow the
     * second byte and report the key as unknown — we do not navigate. */
    if (c == 0x00 || c == 0xE0) {
        _getch();
        return KEY_UNKNOWN;
    }

    switch (c) {
        case '\r': case '\n': return KEY_ENTER;
        case '\t':            return KEY_TAB;
        case 8:   case 127:   return KEY_BACKSPACE;   /* both forms */
        case 19:              return KEY_CTRL_S;       /* Ctrl+S */
        case 17:              return KEY_CTRL_Q;       /* Ctrl+Q */
        default:              return c;
    }
}

#else
/* ------------------------------ POSIX ------------------------------ */
#include <termios.h>
#include <unistd.h>

static struct termios g_orig;
static int g_raw = 0;

void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &g_orig) != 0) return;

    struct termios raw = g_orig;
    /* Local flags: drop ICANON (read per-key, not per-line) and ECHO
     * (we draw characters ourselves so suggestions can be redrawn). */
    raw.c_lflag &= ~(ICANON | ECHO);
    /* Input flags: drop IXON so Ctrl+S (19) and Ctrl+Q (17) reach us
     * instead of being eaten as XOFF/XON flow control. */
    raw.c_iflag &= ~(IXON);
    raw.c_cc[VMIN]  = 1;   /* block until at least one byte           */
    raw.c_cc[VTIME] = 0;   /* ...with no inter-byte timeout           */

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
        case 8:   case 127:   return KEY_BACKSPACE;   /* both forms */
        case 19:              return KEY_CTRL_S;       /* Ctrl+S */
        case 17:              return KEY_CTRL_Q;       /* Ctrl+Q */
    }

    /* ESC introduces an escape sequence (e.g. an arrow key). We do not
     * support navigation, so consume the "[X" tail and drop it rather
     * than letting stray '[' and letters land in the document. */
    if (c == 27) {
        unsigned char seq;
        if (read(STDIN_FILENO, &seq, 1) == 1 && seq == '[') {
            /* discard the final byte; result intentionally unused */
            ssize_t ignored = read(STDIN_FILENO, &seq, 1);
            (void)ignored;
        }
        return KEY_UNKNOWN;
    }

    return c;
}

#endif /* _WIN32 */
