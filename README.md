# Marduk

Marduk is an emulator for the NABU Personal Computer. CPU, VDP, and PSG cores are third-party; SDL2 handles I/O, and Gtk+ is used for dialogs on Linux.

## Build Status

| Windows | Linux | macOS |
|---------|-------|-------|
| ![Windows CI](https://github.com/visrealm/marduk/actions/workflows/ci-windows.yml/badge.svg) | ![Linux CI](https://github.com/visrealm/marduk/actions/workflows/ci-linux.yml/badge.svg) | ![macOS CI](https://github.com/visrealm/marduk/actions/workflows/ci-macos.yml/badge.svg) |

## ROM Files

- Default: `opennabu.bin` (OpenNabu IPL) included in the repo root.
- Alternatives: `NabuPC-U53-90020060-RevA-2732.bin` (`-4`), `NabuPC-U53-90020060-RevB-2764.bin` (`-8`), or any custom ROM via `-B <file>`.

## Building with CMake

Requirements:
- CMake 3.16+
- SDL2 dev files (auto-fetched by default via `FetchContent`)
- Gtk+ 3 dev files on Linux (not needed on Windows or macOS)
- C99 toolchain

Configure & build:
```sh
cmake -S . -B build
cmake --build build
```
Tips:
- Visual Studio: `cmake -S . -B build -A x64`.
- MinGW: `cmake -S . -B build -G "MinGW Makefiles"`.
- Disable SDL2 fetch if you have it installed: `-DMARDUK_FETCH_SDL2=OFF`.
- For non-default SDL2/GTK locations, set `SDL2_DIR` and/or `PKG_CONFIG_PATH`.

## Running

Place the desired ROM alongside the executable or use flags:
- Default ROM: just run `marduk`.
- Custom ROM: `marduk -B mygame.bin`.
- Other options: `-4`, `-8`, `-S <server>`, `-P <port>`, `-p <lptfile>`, `-a <diskA>`, `-b <diskB>`, `-x <cpm exec>`, `-j/-J` to toggle joystick routing.

## License

MIT-style; see `license.txt` and source headers. SDL2 and other third-party components have compatible licenses.
