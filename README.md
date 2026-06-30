# Smart Notepad

Aplikasi "notepad pintar" berbasis terminal (CLI) yang ditulis dengan bahasa C
untuk mata kuliah struktur data. Kamu mengetik bebas ke dalam sebuah dokumen
yang terus bertambah, dan program membantumu secara *live* (per karakter)
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
  baris **di bawah kursor**: sampai 5 saran kata dari Trie (bernomor) plus
  **huruf-huruf berikutnya** yang mungkin. Blok ini digambar ulang setiap
  ketukan, jadi tidak pernah basi, dan kursor tetap di tempatmu mengetik.
- Tekan **Spasi** atau **Enter** untuk menyelesaikan kata. Kata yang selesai
  dicari di tesaurus BST; kalau ada sinonimnya, ditampilkan di bawah kursor.
- Tekan **Tab** untuk menerima saran teratas: kata parsial diganti dengan kata
  lengkap, ditambah spasi, lalu sinonimnya dicari.

Ini notepad **tambah-saja** (append-only): tidak ada navigasi tombol panah atau
mengedit di tengah teks yang sudah diketik.

### Kontrol

| Tombol         | Aksi                                              |
| -------------- | ------------------------------------------------- |
| huruf apa pun  | diketik ke dokumen (autocomplete live)            |
| Spasi / Enter  | menyelesaikan kata saat ini (cari sinonim)        |
| Tab            | menerima saran **nomor 1** (teratas)              |
| angka `1`–`5`  | menerima saran sesuai nomornya di daftar          |
| Backspace      | menghapus karakter terakhir                       |
| Ctrl+S         | simpan (akan menanyakan nama file)                |
| Ctrl+Q         | keluar dengan rapi                                |

Daftar saran tampil bernomor, mis. `1.small 2.smart`. Tekan `Tab` untuk ambil
yang teratas, atau tekan **angkanya langsung** (`2` untuk `smart`, dst).

> Catatan kecil: angka `1`–`5` hanya "membajak" tombolmu **selama daftar saran
> sedang tampil**. Kalau kamu memang ingin mengetik angka tepat setelah sebuah
> kata, selesaikan dulu katanya dengan spasi (daftar saran hilang), baru ketik
> angkanya.

Terminal selalu dikembalikan ke mode normal saat keluar.

---

## Cara compile & menjalankan

### macOS / Linux (gcc atau clang)

Cara paling gampang — satu baris langsung jalan:

```sh
gcc *.c -o program && ./program
```

Kalau `*.c` malah error (mis. `Invalid argument`), sebut file-nya langsung —
ini paling aman:

```sh
gcc main.c trie.c bst.c platform.c -o program && ./program
```

Atau pakai Makefile yang sudah disediakan:

```sh
make          # build ./program
./program     # jalankan
```

(Bisa juga `make run` untuk build + jalankan sekaligus.)

> Catatan: jalankan dari dalam folder proyek ini supaya `dictionary.txt` dan
> `thesaurus.txt` ikut terbaca. Kalau `gcc` belum ada, ganti dengan `cc` atau
> `clang` (`cc *.c -o program`).

### Windows (MinGW gcc)

Pasang [MinGW-w64](https://www.mingw-w64.org/) supaya `gcc` ada di PATH, lalu
dari `cmd`, PowerShell, atau MINGW64 (Git Bash):

```sh
gcc main.c trie.c bst.c platform.c -o program.exe
program.exe
```

> Kenapa file-nya disebut satu per satu, bukan `*.c`? Di `cmd`/PowerShell
> (dan saat folder yang dipilih salah), tanda `*.c` tidak diubah jadi daftar
> file, jadi gcc menerima teks mentah `*.c` dan error
> `cc1.exe: fatal error: *.c: Invalid argument`. Menyebut nama file langsung
> selalu aman.

Kode sumber yang sama bisa di-compile tanpa diubah: `platform.c` otomatis
memilih Windows Console API (`_getch` dari `conio.h`, plus
`ENABLE_VIRTUAL_TERMINAL_PROCESSING` untuk kode kursor ANSI) lewat
`#ifdef _WIN32`, dan `termios` POSIX di sistem lainnya. Jalankan di jendela
konsol asli (Command Prompt atau Windows Terminal), bukan di panel output IDE.

---

## File data

Kedua file dimuat saat startup dan berupa teks biasa, jadi gampang diganti
dengan yang lebih besar. File yang hilang atau kosong ditangani dengan aman
(fitur terkait dimatikan saja, dengan catatan di layar).

- **`dictionary.txt`** — satu kata Inggris per baris (±200 kata umum sudah
  diisi). Dimuat ke **Trie**. Hanya kata huruf kecil `a`–`z` yang disimpan.
- **`thesaurus.txt`** — baris berformat `kata: sinonim1, sinonim2, sinonim3`
  (±40 entri sudah diisi). Dimuat ke **BST**.

Untuk memakai data lebih besar, cukup ganti file ini (nama/format tetap sama)
atau ubah konstanta `DICT_PATH` / `THES_PATH` di bagian atas `main.c`.

---

## Troubleshooting

**`cc1.exe: fatal error: *.c: Invalid argument`** (atau `*.c: No such file`)
Tanda `*.c` tidak ke-expand jadi daftar file — biasanya karena kamu sedang
**bukan di folder yang berisi file `.c`**. Cek dulu:

```sh
ls *.c     # harus muncul: main.c trie.c bst.c platform.c
```

Kalau tidak muncul, pindah ke folder yang benar (`cd nama-folder-proyek`) lalu
ulangi. Cara paling pasti: sebut nama file langsung saat compile —
`gcc main.c trie.c bst.c platform.c -o program`.

**Autocomplete / sinonim tidak muncul**
Pastikan kamu menjalankan program dari folder yang sama dengan `dictionary.txt`
dan `thesaurus.txt`. Kalau file itu tidak ketemu, program tetap jalan tapi
menampilkan catatan bahwa fitur tersebut dimatikan.

---

## Susunan kode

ADT sengaja dipisah ketat dari logika aplikasi:

| File                        | Tanggung jawab                                        |
| --------------------------- | ----------------------------------------------------- |
| `trie.h` / `trie.c`         | ADT Trie: insert, has-prefix, collect, next-letters   |
| `bst.h` / `bst.c`           | ADT BST: insert (kata + sinonim), search              |
| `platform.h` / `platform.c` | Abstraksi terminal OS (mode raw + baca tombol)        |
| `main.c`                    | loop notepad, buffer dokumen, rendering               |
