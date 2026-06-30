# Smart Notepad

Aplikasi "notepad pintar" berbasis terminal (CLI) yang ditulis dengan bahasa C
untuk mata kuliah struktur data. Pengguna mengetik bebas ke dalam sebuah dokumen
yang terus bertambah, dan program membantu secara *live* (per karakter)
menggunakan dua ADT pohon klasik:

| Fitur                                | Struktur Data            |
| ------------------------------------ | ------------------------ |
| **Autocomplete** kata secara live    | **Trie** (pohon prefix)  |
| **Sinonim (tesaurus)** kata          | **Binary Search Tree**   |

Tampilan terminalnya sengaja dibuat polos — tanpa warna, tanpa *ghost text* —
tapi pengetikannya **live** (setiap tombol langsung diproses, bukan per baris).

---

## Cara kerjanya

- Mulai mengetik di dokumen kosong. Semua yang kamu ketik ditambahkan ke buffer
  dokumen (huruf, kata, kalimat, paragraf).
- Begitu **kata saat ini** mencapai 2 huruf atau lebih, blok saran muncul di
  baris **di bawah kursor**: sampai 5 saran kata dari Trie plus
  **huruf-huruf berikutnya** yang mungkin. Blok ini digambar ulang setiap
  ketukan, jadi tidak pernah basi, dan kursor tetap di tempat pengguna mengetik.
- Tekan **Spasi** atau **Enter** untuk menyelesaikan kata. Kata yang selesai
  dicari di tesaurus BST; kalau ada sinonimnya, ditampilkan di bawah kursor.
- Tekan **Tab** untuk menerima saran teratas: kata parsial diganti dengan kata
  lengkap, ditambah spasi, lalu sinonimnya dicari.

Ini notepad **tambah-saja** (append-only): tidak ada navigasi tombol panah atau
mengedit di tengah teks yang sudah diketik.

### Kontrol

| Tombol             | Aksi                                              |
| ------------------ | ------------------------------------------------- |
| huruf apa pun      | diketik ke dokumen (autocomplete live)            |
| Spasi / Enter      | menyelesaikan kata saat ini (cari sinonim)        |
| panah ← → / ↑ ↓    | memindahkan pilihan di daftar saran               |
| Tab                | menerima saran yang sedang **disorot**            |
| Backspace          | menghapus karakter terakhir                       |
| Ctrl+S             | simpan (akan menanyakan nama file)                |
| Esc / Ctrl+Q       | keluar dengan rapi                                |

Saran yang sedang dipilih disorot dengan tanda kurung siku, mis.
`[small]  smart`. Gerakkan dengan tombol panah, lalu tekan `Tab` untuk
menerimanya. Karena pemilihan pakai panah, mengetik angka tidak lagi
terganggu sama sekali.

---

## Cara compile & menjalankan

### macOS / Linux (gcc atau clang)

```sh
gcc main.c trie.c bst.c platform.c -o program && ./program
```

## Susunan kode


| File                        | Tanggung jawab                                        |
| --------------------------- | ----------------------------------------------------- |
| `trie.h` / `trie.c`         | ADT Trie: insert, has-prefix, collect, next-letters   |
| `bst.h` / `bst.c`           | ADT BST: insert (kata + sinonim), search              |
| `platform.h` / `platform.c` | Abstraksi terminal OS (mode raw + baca tombol)        |
| `main.c`                    | loop notepad, buffer dokumen, rendering               |
