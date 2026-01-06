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

  The CPU, VDP and PSG are emulated via third-party code, which I have
  imported with minimal adaptation.  Also, libsdl2 is used for the front end
  I/O code.  Gtk+ is used for dialog boxes (except on Windows where the native
  API is used instead).
  
  The modem emulation is reasonably complete.  There is not, at date, floppy
  disk emulation, but it is being developed.

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

Building with CMake
===================

  Requirements:
    * CMake 3.16 or newer
    * (optional) Preinstalled SDL2 development files; if missing, CMake can
      download and build SDL2 automatically
    * Gtk+ 3 development files on Linux (not needed on Windows or macOS)
    * A C99 toolchain; on Windows you can use either Visual Studio or MinGW

  Configure and build:
    cmake -S . -B build
    cmake --build build

  Tips:
    * With Visual Studio generators, choose an architecture (for example,
      cmake -S . -B build -A x64).
    * With MinGW, pass -G "MinGW Makefiles" when configuring.
    * By default CMake will download SDL2 (FetchContent). To force using a
      preinstalled SDL2, configure with -DMARDUK_FETCH_SDL2=OFF.
    * If SDL2 or Gtk+ are installed in a non-default location, point CMake to
      them using SDL2_DIR and/or PKG_CONFIG_PATH.
