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
     * two-byte sequence beginning with 0x00 or 0xE0. The second byte
     * identifies the key; we map the arrows and drop the rest. */
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
        case 27:              return KEY_ESC;          /* Esc — quit */
        case 8:   case 127:   return KEY_BACKSPACE;    /* both forms */
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

/* Return non-zero if at least one byte is waiting on stdin within `ms`
 * milliseconds. Used to tell a lone Esc (quit) apart from the start of
 * an escape sequence such as an arrow key (Esc [ A). */
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

    /* ESC is either a lone Esc keypress (quit) or the start of an escape
     * sequence such as an arrow key ("Esc [ A"). If nothing follows
     * almost immediately, treat it as a bare Esc. */
    if (c == 27) {
        if (!input_pending(30))
            return KEY_ESC;                 /* lone Esc -> quit */

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
        return KEY_UNKNOWN;                  /* other escape: ignore */
    }

    return c;
}

#endif /* _WIN32 */
