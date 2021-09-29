# Super SameBoy

Super SameBoy is a Qt-based interface of SameBoy, a free, highly accurate Game
Boy and Game Boy Color emulator.

SameBoy can be found at https://github.com/LIJI32/SameBoy

You will need Qt5 (Qt6 does not have QController yet). SameBoy also requires an
installation of RGBDS to build the boot ROMs. This project also uses Python (for
building only - it doesn't use it at runtime). And, of course, you need a copy
of SameBoy's source code.

NOTE: Presently, clang is recommended for building this project. This is because
building SameBoy with GCC has resulted in the size of the `GB_gameboy_s` struct
being the wrong size. A build script is put in place to ensure the struct sizes
are synced between both Super SameBoy (which is written in C++) and SameBoy
(which is written in C).
