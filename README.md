# Marduk

Marduk is an emulator for the NABU Personal Computer. CPU, VDP, and PSG cores are third-party; SDL2 handles I/O, and Gtk+ is used for dialogs on Linux.

The VDP is rendered by [pico9918-core](https://github.com/visrealm/pico9918-core),
which can answer as any of three chips at runtime: the stock **TMS9918A**, an
**F18A**, or a **[PICO9918](https://github.com/visrealm/pico9918)**.

## Build Status

| Windows | Linux | macOS |
|---------|-------|-------|
| ![Windows CI](https://github.com/visrealm/marduk/actions/workflows/ci-windows.yml/badge.svg) | ![Linux CI](https://github.com/visrealm/marduk/actions/workflows/ci-linux.yml/badge.svg) | ![macOS CI](https://github.com/visrealm/marduk/actions/workflows/ci-macos.yml/badge.svg) |

## ROM Files

- Default: `opennabu.bin` (OpenNabu IPL) included in the repo root.
- Alternatives: `NabuPC-U53-90020060-RevA-2732.bin` (`-4`), `NabuPC-U53-90020060-RevB-2764.bin` (`-8`), or any custom ROM via `-B <file>`.

## Building with CMake

Requirements:
- CMake 3.22+
- SDL2 dev files (auto-fetched by default via `FetchContent`)
- Gtk+ 3 dev files on Linux (not needed on Windows or macOS)
- C11 toolchain
- Python 3, to generate the PICO9918 overlay image arrays

Clone with submodules - pico9918-core is one:
```sh
git clone --recursive https://github.com/visrealm/marduk.git
```
An existing clone: `git submodule update --init --recursive`.

Configure & build:
```sh
cmake -S . -B build
cmake --build build
```
Tips:
- Visual Studio: `cmake -S . -B build -A x64`.
- MinGW: `cmake -S . -B build -G "MinGW Makefiles"`.
- Disable SDL2 fetch if you have it installed: `-DMARDUK_FETCH_SDL2=OFF`.
- `Makefile.dos` builds the MS-DOS target, which is TMS9918A-only - see below.
- For non-default SDL2/GTK locations, set `SDL2_DIR` and/or `PKG_CONFIG_PATH`.

## Running

Place the desired ROM alongside the executable or use flags:
- Default ROM: just run `marduk`.
- Custom ROM: `marduk -B mygame.bin`.
- Other options: `-4`, `-8`, `-V <chip>` (VDP, see below), `-9`, `-S <server>`, `-P <port>`, `-p <lptfile>`, `-a <diskA>`, `-b <diskB>`, `-x <cpm exec>`, `-j/-J` to toggle joystick routing.

## Key bindings

Prior to version 1.0, some of these may be subject to change.

- `F3` = Reset
- `F6` = Toggle whether arrows and space route to the keyboard or P1 joystick.
- `F10` = Exit
- `F11` = Toggle fullscreen
- `Ins` and `Del` = Yes and No
- `PgUp` and `PgDn` = << and >>

Everything else should be obvious.

## Display

The frame is 640x480 whatever the window is. The DOS build renders straight to VGA
memory and reports these as ignored.

| Option | Effect |
|--------|--------|
| `-r` *(default)* | Resizable window, opened at the largest whole multiple of the frame that fits the display, up to 2x. |
| `-R` | Fixed window. |
| `-i` *(default)* | Whole scaling factors only, so pixel and scanline thickness stay even. The surplus becomes border. |
| `-I` | Any factor: the picture fills the window's 4:3 box. Aspect is still kept - the odd dimension out becomes border, never stretch. |
| `-s` | Filter the scaling, for a softer picture at a fractional factor. Off by default. |

## VDP selection

pico9918-core renders the VDP, and `-V` picks which chip it answers as. These form
a capability ladder - each is the one below it plus what the real hardware adds:

| `-V` | Chip | What it is |
|------|------|------------|
| `tms9918a` *(default)* | TMS9918A | The part the NABU shipped. Unlock refused, eight registers, no GPU, no overlays. |
| `f18a` | F18A | Unlockable: full register file, enhanced modes and the GPU. Identifies as a real F18A, and has none of the PICO9918's own extensions. |
| `pico9918` | PICO9918 | An F18A plus this board's extensions: the VR58/59 config port, the firmware register, and the splash and diagnostics overlays. |
| `pico9918pro` | PICO9918 PRO | The RP2350 board: 80-column text at a byte a pixel, so the tile palette select, ECM and the bitmap layer reach TEXT80. Answers software exactly as a PICO9918 does. |

```sh
marduk                  # a stock NABU
marduk -V f18a          # an F18A
marduk -V pico9918      # a PICO9918   (-9 is shorthand for this)
marduk -V pico9918pro   # a PICO9918 PRO
```

`MARDUK_VDP_CHIP` sets the same thing from the environment; `-V` overrides it. The
name is matched case-insensitively, and `tms`/`9918`, `pico` and `pro` are accepted
as short forms.

Software that probes for an F18A sees exactly what the chip says it is, so a title
with an F18A path takes it at `f18a` and above and its TMS9918A path at `tms9918a`.

### The MS-DOS build

`Makefile.dos` renders 320x200 at 8bpp straight into VGA memory and builds without
CMake, neither of which suits pico9918-core. So the DOS target keeps `vrEmuTms9918` -
the core pico9918-core grew out of - and is a TMS9918A and nothing else. `-V` and
`-9` report that and are otherwise ignored there.

### Other settings

At `pico9918` and above the 256-byte configuration block, which a real board keeps in
flash, is persisted to `pico9918.cfg` in the working directory. The lower two chips
have no config port, so its settings - the CRT-scanlines dim among them - neither
apply nor are written back there.

`MARDUK_GPU_IPS` sets the emulated GPU's instruction rate (default `10000000`, i.e.
10 MIPS). It has no effect at `tms9918a`, which has no GPU.

## License

Marduk is released under the terms commonly known as the "MIT" license; you will find
them attached to every source file as well as in `license.txt`.

Basically, give credit where credit's due. It's a little more technical than that
though.

(Note that SDL uses a different license, but with the same goals.)
