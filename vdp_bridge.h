/*
 * Copyright (c) 2026 Troy Schrapel.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following condition:  The
 * above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef H_VDP_BRIDGE
#define H_VDP_BRIDGE

#include <stdint.h>

/* Numerically identical to pico9918_chip_t, whose 0 is the pre-A part. */
#define VDP_CHIP_TMS9918A     1
#define VDP_CHIP_F18A         2
#define VDP_CHIP_PICO9918     3
#define VDP_CHIP_PICO9918_PRO 4

void vdp_bridge_set_chip (int);
int vdp_bridge_chip (void);

void vdp_bridge_set_framebuffer (uint32_t *);

int vdp_bridge_init (void);
void vdp_bridge_reset (void);
void vdp_bridge_shutdown (void);

void vdp_bridge_writedata (uint8_t);
void vdp_bridge_writectrl (uint8_t);
uint8_t vdp_bridge_readdata (void);
uint8_t vdp_bridge_readctrl (void);

int vdp_bridge_loop (void);
int vdp_bridge_irq_enabled (void);
void vdp_bridge_flush (void);

#endif /* H_VDP_BRIDGE */
