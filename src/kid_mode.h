/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef KID_MODE_H
#define KID_MODE_H
#include "kid_library.h"
#include "kid_state.h"
#define KID_MARKER "/.superfw/kidmode"
#define KID_ART_X 40
#define KID_ART_Y 24
/* The marker must be a regular, empty file; no config is written. */
bool kid_mode_detect(void);
/* Initialized immediately after mounting, before any SRAM operation. */
extern bool kid_boot_enabled, kid_boot_bypass, kid_boot_pending_ok;
/* -1 picker, otherwise a current curated-library index. Error notice is optional. */
int kid_boot_select(const kid_library *library, bool held_b, bool pending_ok, const char **notice);
bool kid_member_valid(const char *path, FILINFO *rom_info);
enum { KID_INPUT_NONE, KID_INPUT_CHANGED, KID_INPUT_PLAY };
unsigned kid_picker_input(unsigned keys, unsigned count, unsigned *selected);
void kid_picture_render(volatile uint16_t *frame, const uint16_t *art, bool valid);
/* Shared loader progress: handles short ROMs with total rounded down to zero. */
unsigned kid_progress_pixels(unsigned done, unsigned total);
#endif
