# SameBoy DX

SameBoy DX is a Qt-based interface of SameBoy, a free, highly accurate Game
Boy and Game Boy Color emulator.

Build requirements:
* CMake
* Python
* C11 and C++17 compiler
* Qt5 with QController (Qt6 lacks QController - will later replace it w/ SDL)
* [SDL] version 2.0.16 or later
* [SameBoy]\*
    * [RGBDS] (for building SameBoy's boot ROMs)

[SDL]:     https://www.libsdl.org/
[SameBoy]: https://github.com/LIJI32/SameBoy
[RGBDS]:   https://github.com/gbdev/rgbds

\* You need a copy of SameBoy's source code. One is tracked via `git submodule`,
   but you may also choose to use a different version of SameBoy by setting the
   `SAMEBOY_SOURCE_DIR` setting in the CMake cache like this:
   
   `$ cmake <arguments> -DSAMEBOY_SOURCE_DIR=/path/to/sameboy/source`
   
   To clone sameboy-dx with the SameBoy repo, run this command:
   
   `$ git clone https://github.com/SnowyMouse/sameboy-dx.git --recursive`

   You can update the existing submodule manually with this:
   
   `$ git submodule update --remote`
