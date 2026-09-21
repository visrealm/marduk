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

/* pico9918-core behind marduk's VDP entry points. */

#include "vdp_bridge.h"

#include "pico9918.h"
#include "pico9918_frame.h"
#include "pico9918_config.h"
#include "gpu/gpu.h"
#include "overlay/diag.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VDP_H_VIRTUAL 640u
#define VDP_V_OUTPUT  480u

/* The NABU is NTSC. */
#define VDP_SCANLINES  262u
#define VDP_FRAME_RATE 60.0f

/* A real board reports its die temperature; a desktop has nothing to read. */
#define VDP_TEMPERATURE 40.0f

#define VDP_CONFIG_PATH "pico9918.cfg"

PICO9918_FRAME_ASSERT_GEOMETRY(VDP_H_VIRTUAL);

_Static_assert(VDP_CHIP_TMS9918A == (int)PICO9918_CHIP_TMS9918A &&
               VDP_CHIP_F18A     == (int)PICO9918_CHIP_F18A &&
               VDP_CHIP_PICO9918 == (int)PICO9918_CHIP_PICO9918 &&
               VDP_CHIP_PICO9918_PRO == (int)PICO9918_CHIP_PICO9918_PRO,
               "VDP_CHIP_* must match pico9918_chip_t");

/* The PICO9918_INST macros require the instance to be named tms9918. */
#if !PICO9918_SINGLE_INSTANCE
static pico9918_t *tms9918 = 0;
#endif

static int instance_ready = 0;

static uint32_t *offscreen;

static uint32_t bgr12_argb[4096];

static pico9918_chip_t chip = PICO9918_CHIP_TMS9918A;

/* Our validated copy of the block, and the instance's live one. */
static uint8_t  config[PICO9918_CONFIG_BYTES];
static uint8_t *device_config;
static int      config_pending;

static unsigned int line;

static PICO9918_FRAME_LINE_BUFFER(pixels, VDP_H_VIRTUAL);

static pico9918_scanline_params_t params;
static pico9918_frame_display_t   display_cfg;
static pico9918_frame_geometry_t  geometry;

/* field_lines includes the porch. */
static unsigned int lines_per_call = 1u;
static unsigned int field_lines    = VDP_SCANLINES;

static void vdp_bridge_init_pixel_map(void)
{
  unsigned int v;

  for (v = 0; v < 4096u; ++v)
    bgr12_argb[v] = 0xff000000u | pico9918_pixel_rgb888((PICO9918_PIXEL_T)v);
}

static void vdp_bridge_blit_line(uint32_t *dest)
{
  unsigned int x;

  for (x = 0; x < VDP_H_VIRTUAL; ++x)
    dest[x] = bgr12_argb[pixels[x] & 0x0fffu];
}

static void vdp_bridge_recompute_cadence(void)
{
  lines_per_call = (display_cfg.vPixelScale >= 2u) ? 1u : 2u;
  field_lines    = VDP_SCANLINES * lines_per_call;
  params.vVirtualPixels = display_cfg.vVirtualPixels;
}

/* vPixelScale and vVirtualPixels are seeded here, then owned by the library. */
static void vdp_bridge_configure(void)
{
  params.hVirtualPixels       = (uint16_t)VDP_H_VIRTUAL;
  params.interlaced           = false;
  params.interlacedFieldOrder = 0u;

  display_cfg.displayPixels  = (int)VDP_V_OUTPUT;
  display_cfg.interlaced     = false;
  display_cfg.vPixelScale    = 2u;
  display_cfg.vVirtualPixels = (uint16_t)(VDP_V_OUTPUT / 2u);

  geometry = pico9918_frame_geometry(PICO9918_INST &display_cfg);
  vdp_bridge_recompute_cadence();
}

void vdp_bridge_set_framebuffer(uint32_t *display)
{
  offscreen = display;
}

/* Board revision, major nibble then minor.  The PRO is the v2.x board, which is where
   the library reads the RP2350 from. */
static uint8_t vdp_hw_version(void)
{
  return vdp_bridge_chip() == VDP_CHIP_PICO9918_PRO ? 0x20u : 0x10u;
}

/* Only the PICO9918 boards carry the config port.  Below them the block is whatever
   the library defaults to and no guest can reach it, so there is nothing to keep. */
static int vdp_has_config(void)
{
  return vdp_bridge_chip() >= VDP_CHIP_PICO9918;
}

static void vdp_bridge_config_store(void)
{
  FILE *file;

  config_pending = 0;

  if (!vdp_has_config())
    return;

  file = fopen(VDP_CONFIG_PATH, "wb");
  if (!file)
    return;

  pico9918_config_prepare_save(config, vdp_hw_version());
  fwrite(config, 1, PICO9918_CONFIG_BYTES, file);
  fclose(file);
}

void vdp_bridge_flush(void)
{
  if (config_pending)
    vdp_bridge_config_store();
}

static void vdp_bridge_config_load(void)
{
  FILE *file;

  pico9918_config_defaults(config);

  if (!vdp_has_config())
    return;

  file = fopen(VDP_CONFIG_PATH, "rb");
  if (file)
  {
    if (fread(config, 1, PICO9918_CONFIG_BYTES, file) != PICO9918_CONFIG_BYTES)
      pico9918_config_defaults(config);
    fclose(file);
  }

  if (pico9918_config_validate(config, vdp_hw_version()))
  {
    /* Left set, it would re-save through the callback on every boot. */
    config[PICO9918_CONF_SAVE_FORCED] = 0;
    vdp_bridge_config_store();
  }
}

static void vdp_bridge_config_saved(pico9918_t *inst, uint8_t *live, uint8_t key,
                                    void *userdata)
{
  (void)inst;
  (void)userdata;

  /* Cancel means discard what the configurator staged, not persist it. */
  if (key == PICO9918_CONF_PENDING_CANCEL)
  {
    memcpy(live + PICO9918_CONFIG_FIRST_SETTABLE,
           config + PICO9918_CONFIG_FIRST_SETTABLE,
           PICO9918_CONFIG_BYTES - PICO9918_CONFIG_FIRST_SETTABLE);
    return;
  }

  memcpy(config + PICO9918_CONFIG_FIRST_SETTABLE,
         live + PICO9918_CONFIG_FIRST_SETTABLE,
         PICO9918_CONFIG_BYTES - PICO9918_CONFIG_FIRST_SETTABLE);

  /* Deferred: this runs from the per-scanline GPU step, not a place to block. */
  config_pending = 1;
}

/* Restores the settings the startup diagnostics screen takes away. */
static void vdp_bridge_config_reload(pico9918_t *inst, void *userdata)
{
  (void)inst;
  (void)userdata;

  if (!device_config)
    return;

  memcpy(device_config, config, PICO9918_CONFIG_BYTES);
  pico9918_diag_config_updated(PICO9918_INST_ONLY);
}

static void vdp_bridge_diag_setup(void)
{
  static int initialised = 0;
  const uint8_t hw = vdp_hw_version();
  char hardware[8];
  char firmware[16];

  if (!initialised)
  {
    pico9918_diag_init();
    initialised = 1;
  }

  snprintf(firmware, sizeof(firmware), "%u.%u.%u", PICO9918_CORE_VER_MAJOR,
           PICO9918_CORE_VER_MINOR, PICO9918_CORE_VER_PATCH);
  snprintf(hardware, sizeof(hardware), "%u.%u", hw >> 4, hw & 0x0fu);
  pico9918_diag_set_version_info(hardware, firmware);

  pico9918_diag_set_output_name("480P ", "@60");
  pico9918_diag_set_clock_hz(252000000.0f);
}

/* The immediate apply consumes the mark set_chip leaves, so the first end of frame
   does not repeat it. */
static void vdp_bridge_seed(void)
{
  device_config = pico9918_config(PICO9918_INST_ONLY);

  vdp_bridge_config_load();
  memcpy(device_config, config, PICO9918_CONFIG_BYTES);

  pico9918_set_chip(PICO9918_INST chip);
  pico9918_config_apply_now(PICO9918_INST true);
  pico9918_diag_config_updated(PICO9918_INST_ONLY);
}

void vdp_bridge_set_chip(int requested)
{
  if (requested < PICO9918_CHIP_TMS9918A)
    requested = PICO9918_CHIP_TMS9918A;
  if (requested > PICO9918_CHIP_MAX)
    requested = PICO9918_CHIP_MAX;

  chip = (pico9918_chip_t)requested;

  if (instance_ready)
    pico9918_set_chip(PICO9918_INST chip);
}

int vdp_bridge_chip(void)
{
  return instance_ready ? (int)pico9918_chip(PICO9918_INST_ONLY) : (int)chip;
}

int vdp_bridge_init(void)
{
  if (instance_ready)
    return 1;

#if PICO9918_SINGLE_INSTANCE
  pico9918_init();
#else
  tms9918 = pico9918_new();
  if (!tms9918)
    return 0;
#endif

  pico9918_gpu_init(PICO9918_INST_ONLY);
  pico9918_gpu_set_config_save_callback(PICO9918_INST vdp_bridge_config_saved, 0);
  pico9918_frame_set_config_reload_callback(PICO9918_INST vdp_bridge_config_reload, 0);
  pico9918_set_chip(PICO9918_INST chip);
  vdp_bridge_configure();
  vdp_bridge_init_pixel_map();
  instance_ready = 1;

  return 1;
}

static uint32_t vdp_bridge_gpu_ips(void)
{
  unsigned long ips = PICO9918_GPU_IPS_PRO;
  const char *env = getenv("MARDUK_GPU_IPS");

  /* strtoul wraps a negative to ULONG_MAX rather than failing. */
  if (env && *env && *env != '-')
  {
    char *end = 0;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(env, &end, 0);
    if (end != env && *end == '\0' && errno != ERANGE && parsed != 0ul)
      ips = parsed;
  }

  return (ips > UINT32_MAX) ? UINT32_MAX : (uint32_t)ips;
}

void vdp_bridge_reset(void)
{
  line = 0u;

  /* Clears VR56, so a program still running stops here. */
  pico9918_reset(PICO9918_INST_ONLY);
  pico9918_gpu_init(PICO9918_INST_ONLY);

  vdp_bridge_diag_setup();
  vdp_bridge_seed();
  vdp_bridge_configure();

  /* Resolved here rather than per scanline - it reads the environment. */
  pico9918_gpu_set_clock(PICO9918_INST vdp_bridge_gpu_ips());
}

void vdp_bridge_shutdown(void)
{
  if (!instance_ready)
    return;

  vdp_bridge_flush();

#if !PICO9918_SINGLE_INSTANCE
  pico9918_destroy(tms9918);
  tms9918 = 0;
#endif
  instance_ready = 0;
  device_config = 0;
  offscreen = 0;
}

void vdp_bridge_writedata(uint8_t value)
{
  pico9918_write_data(PICO9918_INST value);
}

void vdp_bridge_writectrl(uint8_t value)
{
  pico9918_write_addr(PICO9918_INST value);
}

uint8_t vdp_bridge_readdata(void)
{
  return pico9918_read_data(PICO9918_INST_ONLY);
}

uint8_t vdp_bridge_readctrl(void)
{
  return pico9918_read_status(PICO9918_INST_ONLY);
}

/* One virtual (VGA) line, written out vPixelScale times. */
static void vdp_bridge_virtual_line(void)
{
  if (line < params.vVirtualPixels)
  {
    unsigned int scale, rep;

    scale = display_cfg.vPixelScale ? display_cfg.vPixelScale : 1u;
    for (rep = 0; rep < scale; ++rep)
    {
      const unsigned int dy = line * scale + rep;

      if (dy >= VDP_V_OUTPUT)
        continue;

      /* False means the buffer is untouched, so this row is still the last one's. */
      if (pico9918_frame_output_line(PICO9918_INST dy, &params, pixels))
      {
        if (offscreen)
          vdp_bridge_blit_line(&offscreen[dy * VDP_H_VIRTUAL]);
      }
      else if (offscreen)
        memcpy(&offscreen[dy * VDP_H_VIRTUAL],
               &offscreen[(dy - 1u) * VDP_H_VIRTUAL],
               VDP_H_VIRTUAL * sizeof(uint32_t));
    }

    /* Inside the visible field: in row-30 modes the trigger sits at vVirtualPixels,
       where end-of-frame raises it instead. */
    if (line == geometry.triggerScanline)
      pico9918_frame_end_of_scanline(PICO9918_INST_ONLY);
  }

  if (line == params.vVirtualPixels)
    pico9918_frame_porch(PICO9918_INST_ONLY);

  if (++line >= field_lines)
  {
    line = 0u;
    geometry = pico9918_frame_end(PICO9918_INST VDP_TEMPERATURE, VDP_FRAME_RATE,
                                  &display_cfg);
    vdp_bridge_recompute_cadence();
  }
}

int vdp_bridge_loop(void)
{
  unsigned int calls;
  unsigned int i;

  /* Latched: a mode change inside the loop rewrites lines_per_call. */
  calls = lines_per_call;
  for (i = 0; i < calls; ++i)
    vdp_bridge_virtual_line();

  return pico9918_interrupt_status(PICO9918_INST_ONLY) ? 1 : 0;
}

/* A locked device sends a write to R19 to R3 instead, so it can never arm the scanline
   source and R0 bit 4 stands for nothing there. */
int vdp_bridge_irq_enabled(void)
{
  if (pico9918_reg_value(PICO9918_INST TMS_REG_1) & TMS_R1_INT_ENABLE)
    return 1;

  return pico9918_unlocked(PICO9918_INST_ONLY) &&
         (pico9918_reg_value(PICO9918_INST TMS_REG_0) & TMS_R0_INT_SCANLINE) != 0;
}
