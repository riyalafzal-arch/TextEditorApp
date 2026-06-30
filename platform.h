/* platform.h — OS terminal abstraction.
 *
 * The rest of the program never touches termios, conio, or the Windows
 * Console API directly; it only calls these three functions and reads
 * the named key constants below. All the #ifdef _WIN32 vs POSIX
 * branching lives in platform.c.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

/* Named constants returned by read_key() for non-printable keys.
 * Their values sit well above the 0-255 byte range so they can never
 * be confused with an ordinary typed character. */
enum {
    KEY_ENTER = 1000,   /* Return / Enter             */
    KEY_BACKSPACE,      /* Backspace (8 or 127)       */
    KEY_TAB,            /* Tab — accept suggestion    */
    KEY_CTRL_S,         /* Ctrl+S — save              */
    KEY_CTRL_Q,         /* Ctrl+Q — quit              */
    KEY_ESC,            /* Esc — quit (alt. to Ctrl+Q) */
    KEY_UP,             /* Up arrow — select prev     */
    KEY_DOWN,           /* Down arrow — select next   */
    KEY_LEFT,           /* Left arrow — select prev   */
    KEY_RIGHT,          /* Right arrow — select next  */
    KEY_UNKNOWN         /* anything we choose to drop  */
};

/* Put the terminal into raw, character-at-a-time mode:
 *  - POSIX: termios with canonical mode + echo disabled.
 *  - Windows: enable ANSI escape handling on the console.
 * Always pair with disable_raw_mode() before exit. */
void enable_raw_mode(void);

/* Restore the terminal to the state it had before enable_raw_mode().
 * Safe to call more than once. */
void disable_raw_mode(void);

/* Block for one keypress and return it: a printable character as its
 * own value, or one of the KEY_* constants above for special keys. */
int  read_key(void);

#endif /* PLATFORM_H */
