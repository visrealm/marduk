What is Marduk?
===============

  Marduk is an attempt to emulate the obscure Canadian NABU Personal Computer.
  It has quickly grown from a barely functional emulator to a fairly complete
  (if bare-bones) and reasonably portable program.

What is NABU?
=============

  NABU was both a company, and the name of the computer they are best known
  for.

  In the early 1980s, they created a computer that would interface with the
  local cable television service to download content rather than using local
  storage - an idea that was radical for the time, and only in very recent
  years becoming common.

  The computer, and the service that it interfaced with, were available in a
  couple cities in Canada, and apparently also had a small rollout in one
  Japanese location, but was generally unsuccessful.

How did this become a thing?
============================

  In late 2022, one of these computers appeared on the YouTube channel
  "Adrian's Digital Basement", giving it a boost of notoriety that led to many
  people finding out about it for the first time, including myself.  Shortly
  thereafter, the protocols were cracked and the computer was brought to life.

  Realizing that the main hardware of the NABU was all stuff I had familiarity
  with, I decided to try writing an emulator for it.

Status
======

  The CPU and PSG are emulated via third-party code, which I have imported
  with minimal adaptation.  The VDP comes from pico9918-core, which is built
  from a submodule rather than imported.  Also, libsdl2 is used for the front
  end I/O code.  Gtk+ is used for dialog boxes (except on Windows where the
  native API is used instead).
  
  The modem emulation is reasonably complete.  There is not, at date, floppy
  disk emulation, but it is being developed.

Building
========

  Everything but the MS-DOS target builds with CMake 3.22 or newer, and needs
  a C11 compiler, Gtk+ 3 on Linux, and Python 3 (pico9918-core generates its
  overlay image arrays at configure time).  SDL2 is fetched and built unless
  you pass -DMARDUK_FETCH_SDL2=OFF to use one already installed.

  pico9918-core is a submodule, so clone with --recursive, or run
  "git submodule update --init --recursive" in an existing clone.

    cmake -S . -B build
    cmake --build build

  Makefile.dos still builds the MS-DOS target, which does not use CMake.

Key bindings
============

  Prior to version 1.0, some of these changes may be subject to change.

  F3 = Reset
  F6 = Toggle whether arrows and space route to the keyboard or P1 joystick.
  F10 = Exit
  Ins and Del = Yes and No
  PgUp and PgDn = << and >>

  Everything else should be obvious.

ROM Files
=========
  
  The current stable version of OpenNabu is included from
  https://6.buric.co/nabu/opennabu/
  but you can use a real Nabu firmware if you prefer.

  One of the following is expected:
  
    opennabu.bin (default)
    NabuPC-U53-90020060-RevA-2732.bin (-4)
    NabuPC-U53-90020060-RevB-2764.bin (-8)
    
  If you have a different firmware you can try it with the -B switch.

Selecting a VDP
===============

  pico9918-core renders the VDP, and -V picks which chip it answers as.  Each
  is the one before it plus what the real hardware adds:

    tms9918a     the part the NABU shipped, and the default
    f18a         unlockable: full register file, enhanced modes and the GPU
    pico9918     an F18A plus the config port, firmware register and overlays
    pico9918pro  the RP2350 board, which adds 80-column text at a byte a pixel

  -9 is shorthand for -V pico9918.  MARDUK_VDP_CHIP sets the same thing from
  the environment, and -V overrides it.  Names are matched without regard to
  case, and tms, 9918, pico and pro are accepted as short forms.

  Software that probes for an F18A sees exactly what the chip says it is, so a
  title with an F18A path takes it at f18a and above.

  At pico9918 and above, the 256-byte configuration block a real board keeps
  in flash is persisted to pico9918.cfg in the working directory.  The two
  lower chips have no config port, so nothing is written there.

  MARDUK_GPU_IPS sets the emulated GPU's instruction rate (default 10000000).
  It has no effect at tms9918a, which has no GPU.

  The MS-DOS target keeps vrEmuTms9918 and is a TMS9918A only; -V and -9
  report that and are otherwise ignored there.

Using a Virtual Adapter (Cable Modem Emulator)
==============================================

  The nabu.ca (or several others) virtual adapter defaults to listening on
  127.0.0.1:5816.
  
  If you need a different address, use the -S switch (e.g., -S 192.168.0.2).
  If you need a different port, use the -P switch (e.g., -P 5815).

License
=======

  Marduk is released under the terms commonly known as the "MIT" license; you
  will find them attached to every source file as well as in "license.txt".

  Basically, give credit where credit's due.  It's a little more technical
  than that though.

  (Note that SDL uses a different license, but with the same goals.)
