/*
 * VDP bridge - pico9918-core behind marduk's VDP entry points.
 *
 * Copyright (c) 2026 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/pico9918-core
 */

#ifndef H_VDP_BRIDGE
#define H_VDP_BRIDGE

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which chip the instance answers as, in ascending capability.  Numerically
   identical to pico9918_chip_t. */
#define VDP_CHIP_TMS9918A  0
#define VDP_CHIP_F18A      1
#define VDP_CHIP_PICO9918  2

/* Set before the first reset.  A reset preserves the personality. */
void          vdp_bridge_set_chip(int chip);

/* What the instance answers as: a request the build cannot honour is clamped. */
int           vdp_bridge_chip(void);

/* marduk's 640x480 ARGB offscreen.  Nothing is drawn until this is set. */
void          vdp_bridge_set_framebuffer(uint32_t *display);

/* Where the board's 256-byte config block is kept between sessions. */
void          vdp_bridge_set_config_path(const char *path);

/* scanlines: the machine's line count, 262 NTSC or 313 PAL.  0 keeps the current. */
void          vdp_bridge_reset(unsigned int scanlines);

/* Release the instance and write out any pending config. */
void          vdp_bridge_shutdown(void);

void          vdp_bridge_writedata(unsigned char value);
void          vdp_bridge_writectrl(unsigned char value);
unsigned char vdp_bridge_readdata(void);
unsigned char vdp_bridge_readctrl(void);

/* One emulated scanline: renders into the offscreen, advances the GPU, and returns
   the level of the interrupt line. */
int           vdp_bridge_loop(void);

/* Whether the VDP is still asking to be interrupted (R1 bit 5). */
int           vdp_bridge_irq_enabled(void);

/* Persist the config block if the guest changed it.  Call once per frame. */
void          vdp_bridge_flush(void);

/* Bring a core up and render one scanline.  Zero if it would not start. */
int           vdp_bridge_selftest(void);

#ifdef __cplusplus
}
#endif

#endif /* H_VDP_BRIDGE */
