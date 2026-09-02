/*
 * VDP bridge - pico9918-core behind marduk's VDP entry points.
 *
 * Copyright (c) 2026 Troy Schrapel
 *
 * This code is licensed under the MIT license
 *
 * https://github.com/visrealm/pico9918-core
 */

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

/* The board drives a full 640x480 VGA frame, which is exactly marduk's offscreen.
   The border is part of the picture: the overlays are drawn into it. */
#define VDP_H_VIRTUAL 640u
#define VDP_V_OUTPUT  480u

/* No horizontal geometry of our own: the library lays a line out in 32-bit words holding
   two 16-bit pixels, which is the one pixel width it ships.  So its own numbers
   land finished - 320 words, 640 pixels, the picture at 64..575 with 64 of border each
   side - and there is nothing to halve, re-derive or stretch afterwards. */

/* A fine-h-scrolled tile layer's last quad can reach past the line. */
#define VDP_LINE_SLACK 16u

/* First config byte the device may write back; 0-7 are identity. */
#define VDP_CONFIG_SETTABLE 8

/* Packed major(4) | minor(4) | patch(8), as pico9918_config_fields wants it. */
#define VDP_CONFIG_VERSION ((uint16_t)(((PICO9918_CORE_VER_MAJOR & 0x0f) << 12) | \
                                       ((PICO9918_CORE_VER_MINOR & 0x0f) <<  8) | \
                                        (PICO9918_CORE_VER_PATCH & 0xff)))

/* Board v1.0 as a packed nibble pair; byte 0 doubles as "is this block ours". */
#define VDP_CONFIG_MODEL      0
#define VDP_CONFIG_HW_VERSION 0x10

_Static_assert(VDP_CHIP_TMS9918A == (int)PICO9918_CHIP_TMS9918A &&
               VDP_CHIP_F18A     == (int)PICO9918_CHIP_F18A &&
               VDP_CHIP_PICO9918 == (int)PICO9918_CHIP_PICO9918,
               "VDP_CHIP_* must match pico9918_chip_t");

/* The PICO9918_INST macros require the instance to be named tms9918. */
#if !PICO9918_SINGLE_INSTANCE
static pico9918_t *tms9918 = 0;
#endif

static int instance_ready = 0;

static uint32_t *offscreen;

/* BGR12 in the low 12 bits is what the library hands back - the board's format - and
   the SDL texture takes ARGB8888.  One lookup a pixel, in the copy into the offscreen
   that had to happen anyway. */
static uint32_t bgr12_argb[4096];

static pico9918_chip_t chip = PICO9918_CHIP_TMS9918A;

/* Our validated copy of the 256-byte block, and the instance's live one. */
static uint8_t  config[CONFIG_BYTES];
static uint8_t *device_config;
static char     config_path[512];
static int      config_capturing;
static int      config_pending;

static unsigned int scanlines = 262u;
static unsigned int line;

static PICO9918_PIXEL_T pixels[VDP_H_VIRTUAL + VDP_LINE_SLACK];

static pico9918_scanline_params_t params;
static pico9918_frame_display_t   display_cfg;
static pico9918_frame_geometry_t  geometry;

/* One call per emulated scanline is one virtual line at vPixelScale 2, two under
   double rows.  field_lines includes the porch. */
static unsigned int lines_per_call = 1u;
static unsigned int field_lines    = 262u;

/* BGR12 to ARGB8888, through the library's own transform so the nibble order lives in
   one place.  Indexed by the low 12 bits, the dead copy of green in 15-12 masked off as
   the library masks it wherever it matters. */
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

static unsigned int vdp_frame_rate(void)
{
  return (scanlines > 300u) ? 50u : 60u;
}

static void vdp_bridge_recompute_cadence(void)
{
  lines_per_call = (display_cfg.vPixelScale >= 2u) ? 1u : 2u;
  field_lines    = scanlines * lines_per_call;
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

void vdp_bridge_set_config_path(const char *path)
{
  if (!path)
  {
    config_path[0] = '\0';
    return;
  }
  snprintf(config_path, sizeof(config_path), "%s", path);
}

static void vdp_bridge_config_stamp(void)
{
  config[PICO9918_CONF_PICO_MODEL]       = VDP_CONFIG_MODEL;
  config[PICO9918_CONF_HW_VERSION]       = VDP_CONFIG_HW_VERSION;
  config[PICO9918_CONF_SW_VERSION]       = (uint8_t)(VDP_CONFIG_VERSION >> 8);
  config[PICO9918_CONF_SW_PATCH_VERSION] = (uint8_t)(VDP_CONFIG_VERSION & 0xff);
}

static void vdp_bridge_config_store(void)
{
  FILE *file;

  config_pending = 0;

  if (!config_path[0])
    return;

  file = fopen(config_path, "wb");
  if (!file)
    return;

  fwrite(config, 1, CONFIG_BYTES, file);
  fclose(file);
}

void vdp_bridge_flush(void)
{
  if (config_pending)
    vdp_bridge_config_store();
}

/* Read the block back and let the library validate it: a bad one is defaulted, an
   old one migrated.  The identity is ours, not the file's, so it is restamped. */
static void vdp_bridge_config_load(void)
{
  bool was_reset = false;
  bool rewrite;
  uint8_t identity[VDP_CONFIG_SETTABLE];

  memset(config, 0, sizeof(config));

  if (config_path[0])
  {
    FILE *file = fopen(config_path, "rb");
    if (file)
    {
      if (fread(config, 1, CONFIG_BYTES, file) != CONFIG_BYTES)
        memset(config, 0, sizeof(config));
      fclose(file);
    }
  }

  rewrite = pico9918_config_validate(config,
                                     config[PICO9918_CONF_PICO_MODEL] == VDP_CONFIG_MODEL,
                                     VDP_CONFIG_VERSION, &was_reset);

  memcpy(identity, config, sizeof(identity));
  vdp_bridge_config_stamp();

  if (rewrite || memcmp(identity, config, sizeof(identity)) != 0)
    vdp_bridge_config_store();
}

/* The GPU's config-action hook, which is also the only route the public API offers
   to the instance's live block. */
static void vdp_bridge_config_saved(uint8_t *live, uint8_t key)
{
  device_config = live;

  if (config_capturing)
    return;

  /* Cancel means discard what the configurator staged, not persist it. */
  if (key == PICO9918_CONF_PENDING_CANCEL)
  {
    memcpy(live + VDP_CONFIG_SETTABLE, config + VDP_CONFIG_SETTABLE,
           CONFIG_BYTES - VDP_CONFIG_SETTABLE);
    return;
  }

  memcpy(config + VDP_CONFIG_SETTABLE, live + VDP_CONFIG_SETTABLE,
         CONFIG_BYTES - VDP_CONFIG_SETTABLE);
  vdp_bridge_config_stamp();

  /* Deferred: this runs from the per-scanline GPU step, not a place to block. */
  config_pending = 1;
}

/* Restores the settings the startup diagnostics screen takes away. */
static void vdp_bridge_config_reload(void)
{
  if (!device_config)
    return;

  memcpy(device_config, config, CONFIG_BYTES);
  pico9918_diag_config_updated(PICO9918_INST_ONLY);
}

/* The diagnostics panel's host half: the glyph table, and values only a host knows. */
static void vdp_bridge_diag_setup(void)
{
  static int initialised = 0;
  char firmware[16];

  if (!initialised)
  {
    pico9918_diag_init();
    initialised = 1;
  }

  snprintf(firmware, sizeof(firmware), "%u.%u.%u", PICO9918_CORE_VER_MAJOR,
           PICO9918_CORE_VER_MINOR, PICO9918_CORE_VER_PATCH);
  pico9918_diag_set_version_info("1.0", firmware);

  /* Retained by pointer, so literals only. */
  pico9918_diag_set_output_name("480P ", "@60");
  pico9918_diag_set_clock_hz(252000000.0f);
}

/* Leave the board as a power-on does: settings loaded, sprite limit and palette
   seeded, display off.  Seeded at the top personality then stepped down, since the
   config port is gated on it but the values are not. */
static void vdp_bridge_seed(void)
{
  config_capturing = 1;

  pico9918_set_chip(PICO9918_INST PICO9918_CHIP_PICO9918);

  /* Unlock: two consecutive VR57 writes with the low two bits clear. */
  pico9918_write_reg_value(PICO9918_INST 0x80u | 0x39u, 0x1Cu);
  pico9918_write_reg_value(PICO9918_INST 0x80u | 0x39u, 0x1Cu);

  /* Arm the save command as the configurator does, purely to be handed the block. */
  pico9918_write_reg_value(PICO9918_INST 0x80u | 58u, PICO9918_CONF_SAVE_TO_FLASH);
  pico9918_write_reg_value(PICO9918_INST 0x80u | 59u, 1u);
  pico9918_gpu_step(PICO9918_INST_ONLY);
  config_capturing = 0;

  vdp_bridge_config_load();
  if (device_config)
    memcpy(device_config, config, CONFIG_BYTES);

  pico9918_write_reg_value(PICO9918_INST 0x80u | 0x32u, 0xC0u);
  pico9918_config_apply(PICO9918_INST_ONLY);
  pico9918_diag_config_updated(PICO9918_INST_ONLY);

  pico9918_write_reg_value(PICO9918_INST 0x80u | 1u, 0x00u);
  pico9918_write_reg_value(PICO9918_INST 0x80u | 7u, 0x00u);

  pico9918_set_chip(PICO9918_INST chip);
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

static int vdp_bridge_ensure(void)
{
#if PICO9918_SINGLE_INSTANCE
  if (!instance_ready)
  {
    pico9918_init();
    pico9918_gpu_init(PICO9918_INST_ONLY);
    pico9918_gpu_set_config_save_callback(vdp_bridge_config_saved);
    pico9918_frame_set_config_reload_callback(vdp_bridge_config_reload);
    pico9918_set_chip(PICO9918_INST chip);
    vdp_bridge_configure();
    vdp_bridge_init_pixel_map();
    instance_ready = 1;
  }
  return 1;
#else
  if (!tms9918)
  {
    tms9918 = pico9918_new();
    if (!tms9918)
      return 0;

    pico9918_gpu_init(PICO9918_INST_ONLY);
    pico9918_gpu_set_config_save_callback(vdp_bridge_config_saved);
    pico9918_frame_set_config_reload_callback(vdp_bridge_config_reload);
    pico9918_set_chip(PICO9918_INST chip);
    vdp_bridge_configure();
    vdp_bridge_init_pixel_map();
    instance_ready = 1;
  }
  return 1;
#endif
}

/* The GPU's clock, which MARDUK_GPU_IPS overrides.  The library paces it from here. */
static uint32_t vdp_bridge_gpu_ips(void)
{
  unsigned long ips = PICO9918_GPU_IPS_PRO;
  const char *env = getenv("MARDUK_GPU_IPS");

  /* strtoul wraps a negative to ULONG_MAX rather than failing, so reject the sign
     before it rather than after. */
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

void vdp_bridge_reset(unsigned int machine_scanlines)
{
  if (!vdp_bridge_ensure())
    return;

  if (machine_scanlines)
    scanlines = machine_scanlines;
  line = 0u;

  /* Clears VR56, so a program still running stops here. */
  pico9918_reset(PICO9918_INST_ONLY);
  pico9918_gpu_init(PICO9918_INST_ONLY);

  vdp_bridge_diag_setup();
  vdp_bridge_seed();
  vdp_bridge_configure();

  /* Hands the GPU to the library: it runs a program from the write that arms it, so a
     detection probe reading its result back cannot miss it, and paces the rest per
     scanline.  Resolved here rather than per scanline - it reads the environment. */
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

void vdp_bridge_writedata(unsigned char value)
{
  if (vdp_bridge_ensure())
    pico9918_write_data(PICO9918_INST value);
}

void vdp_bridge_writectrl(unsigned char value)
{
  if (!vdp_bridge_ensure())
    return;

  pico9918_write_addr(PICO9918_INST value);
}

unsigned char vdp_bridge_readdata(void)
{
  return vdp_bridge_ensure() ? pico9918_read_data(PICO9918_INST_ONLY) : 0u;
}

unsigned char vdp_bridge_readctrl(void)
{
  return vdp_bridge_ensure() ? pico9918_read_status(PICO9918_INST_ONLY) : 0u;
}

/* One virtual (VGA) line, written out vPixelScale times. */
static void vdp_bridge_virtual_line(void)
{
  if (line < params.vVirtualPixels)
  {
    unsigned int scale, rep;

    /* Through the frame module, not the bare scan_line(): SR3, the GPU trigger, the
       palette LUT, the overlays and the CRT dim all live in there.  It fills the whole
       320-word line - both borders and the picture - so there is nothing left to paint,
       and it renders once per virtual line however many output rows share it. */
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

    /* Tested inside the visible field, as the library's reference host does: in
       row-30 modes the trigger sits at vVirtualPixels, where a board raises the
       interrupt from end-of-frame instead. */
    if (line == geometry.triggerScanline)
      pico9918_frame_end_of_scanline(PICO9918_INST_ONLY);
  }

  if (line == params.vVirtualPixels)
    pico9918_frame_porch(PICO9918_INST_ONLY);

  if (++line >= field_lines)
  {
    line = 0u;
    geometry = pico9918_frame_end(PICO9918_INST 40.0f,
                                  (float)vdp_frame_rate(), &display_cfg);
    vdp_bridge_recompute_cadence();
  }
}

int vdp_bridge_loop(void)
{
  unsigned int calls;
  unsigned int i;

  if (!vdp_bridge_ensure())
    return 0;

  /* Latched: a mode change inside the loop rewrites lines_per_call, and re-reading
     it would run a line of the next field before the host has blitted this one. */
  calls = lines_per_call;
  for (i = 0; i < calls; ++i)
    vdp_bridge_virtual_line();

  return pico9918_interrupt_status(PICO9918_INST_ONLY) ? 1 : 0;
}

int vdp_bridge_irq_enabled(void)
{
  return vdp_bridge_ensure() ? (pico9918_reg_value(PICO9918_INST 1) & 0x20) != 0 : 0;
}

int vdp_bridge_selftest(void)
{
  if (!vdp_bridge_ensure())
    return 0;

  pico9918_reset(PICO9918_INST_ONLY);
  pico9918_scan_line(PICO9918_INST 0);

  return (int)pico9918_line_bytes(PICO9918_INST_ONLY);
}
