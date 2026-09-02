/* Marduk's 32-bit ARGB pixel policy for pico9918-core, force-included into the
   library build over its #ifndef-guarded desktop macros. */

#ifndef MARDUK_PICO9918_PIXEL_POLICY_H
#define MARDUK_PICO9918_PIXEL_POLICY_H

/* pram is byte-swapped RGB444: 15-12=G, 11-8=B, 3-0=R. Emit 0xAARRGGBB. */
#define PICO9918_PIXEL_FROM_RGB12(rgb) \
  ((PICO9918_PIXEL_T)((uint32_t)((((rgb) >>  8) & 0x0f) * 0x11)        | \
                      (uint32_t)((((rgb) >> 12) & 0x0f) * 0x11) <<  8  | \
                      (uint32_t)((((rgb)      ) & 0x0f) * 0x11) << 16  | \
                      0xff000000u))

/* One pixel a LUT entry: pass the palette word through and convert each half. */
#define PICO9918_PIXEL_FROM_RGB12_PAIR(packed) (packed)
#define PICO9918_PIXEL_PAIR(p)                 PICO9918_PIXEL_FROM_RGB12(p)

/* The 512-byte 80-column line into the 256-entry picture window frame.c lays out
   either border around: one pixel an entry, so every second index. */
#define PICO9918_EXPAND_INDEXED_WIDE(dst, src, n, lut) \
  do \
  { \
    const uint8_t* _s                = (const uint8_t*)(src); \
    PICO9918_PIXEL_T* _d             = (PICO9918_PIXEL_T*)(dst); \
    const PICO9918_PALETTE_LUT_T* _l = (lut); \
    for (unsigned _i = 0; _i < (unsigned)(n) / 2u; ++_i) _d[_i] = _l[_s[_i * 2u]]; \
  } while (0)

#endif /* MARDUK_PICO9918_PIXEL_POLICY_H */
