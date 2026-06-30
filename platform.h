/* platform.h — Abstraksi terminal lintas-OS.
 *
 * Bagian lain program tidak pernah menyentuh termios, conio, atau Windows
 * Console API secara langsung; mereka hanya memanggil tiga fungsi ini dan
 * membaca konstanta tombol bernama di bawah. Semua percabangan
 * #ifdef _WIN32 vs POSIX berada di platform.c.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

/* Konstanta bernama yang dikembalikan read_key() untuk tombol non-cetak.
 * Nilainya berada jauh di atas rentang byte 0-255 supaya tidak pernah
 * tertukar dengan karakter biasa yang diketik. */
enum {
    KEY_ENTER = 1000,   /* Return / Enter              */
    KEY_BACKSPACE,      /* Backspace (8 atau 127)      */
    KEY_TAB,            /* Tab — terima saran          */
    KEY_CTRL_S,         /* Ctrl+S — simpan             */
    KEY_CTRL_Q,         /* Ctrl+Q — keluar             */
    KEY_ESC,            /* Esc — keluar (alt. Ctrl+Q)  */
    KEY_UP,             /* Panah atas — pilih sebelum  */
    KEY_DOWN,           /* Panah bawah — pilih sesudah */
    KEY_LEFT,           /* Panah kiri — pilih sebelum  */
    KEY_RIGHT,          /* Panah kanan — pilih sesudah */
    KEY_UNKNOWN         /* apa pun yang ingin diabaikan */
};

/* Menaruh terminal ke mode raw, satu-karakter-sekali-baca:
 *  - POSIX: termios dengan mode kanonik + echo dimatikan.
 *  - Windows: mengaktifkan penanganan escape ANSI pada konsol.
 * Selalu pasangkan dengan disable_raw_mode() sebelum keluar. */
void enable_raw_mode(void);

/* Mengembalikan terminal ke kondisi sebelum enable_raw_mode().
 * Aman dipanggil lebih dari sekali. */
void disable_raw_mode(void);

/* Memblokir untuk satu penekanan tombol lalu mengembalikannya: karakter
 * cetak sebagai nilainya sendiri, atau salah satu konstanta KEY_* di atas
 * untuk tombol khusus. */
int  read_key(void);

#endif /* PLATFORM_H */
