/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kid_mode.h"
#include "gbahw.h"
#include "common.h"
#include <string.h>
bool kid_boot_enabled, kid_boot_bypass;
bool kid_boot_pending_ok = true;

bool kid_mode_detect(void) {
  FILINFO info;
  return f_stat(KID_MARKER, &info) == FR_OK &&
         !(info.fattrib & AM_DIR) && info.fsize == 0;
}

unsigned kid_picker_input(unsigned keys, unsigned count, unsigned *selected) {
  if (!count) return KID_INPUT_NONE;
  unsigned direction = keys & (KEY_BUTTLEFT | KEY_BUTTRIGHT);
  if (direction) {
    if (direction == (KEY_BUTTLEFT | KEY_BUTTRIGHT)) return KID_INPUT_NONE;
    *selected = (*selected + (direction == KEY_BUTTRIGHT ? 1 : count - 1)) % count;
    return KID_INPUT_CHANGED;
  }
  return keys & KEY_BUTTA ? KID_INPUT_PLAY : KID_INPUT_NONE;
}

const char *kid_text_line(const char *text, unsigned width,
                         unsigned (*cap)(const char *, unsigned),
                         char line[KID_TITLE_BYTES]) {
  while (*text == ' ') text++;
  unsigned fit = cap(text, width);
  if (fit >= KID_TITLE_BYTES) {
    fit = KID_TITLE_BYTES-1;
    while (((unsigned char)text[fit] & 0xc0) == 0x80) fit--;
  }
  unsigned end = fit;
  if (text[fit] && text[fit] != ' ')
    for (unsigned i=0; i<fit; i++) if (text[i]==' ') end=i;
  while (end && text[end-1]==' ') end--;
  memcpy(line, text, end);
  line[end]=0;
  if (end) text+=end;
  else if (*text) {
    /* A single glyph cannot fit: consume it without drawing outside the box. */
    text++;
    while (((unsigned char)*text & 0xc0)==0x80) text++;
  }
  while (*text==' ') text++;
  return text;
}

unsigned kid_progress_pixels(unsigned done, unsigned total) {
  return !total ? 0 : done >= total ? 200 : done * 200 / total;
}

void kid_picture_render(volatile uint16_t *frame, const uint16_t *art, bool valid) {
  for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT / 2; i++)
    frame[i] = 0x1111; /* UI background palette 17. */
  for (unsigned y = 0; y < KID_ART_HEIGHT; y++)
    for (unsigned x = 0; x < KID_ART_WIDTH / 2; x++) {
      /* All stores are halfwords: both VRAM and cartridge SDRAM are bus16. */
      uint16_t value = valid ? art[y * (KID_ART_WIDTH / 2) + x] :
        ((y < 4 || y >= KID_ART_HEIGHT - 4 || x < 2 || x >= KID_ART_WIDTH / 2 - 2) ? 0x1313 : 0x1010);
      frame[(y + KID_ART_Y) * (SCREEN_WIDTH / 2) + KID_ART_X / 2 + x] = value;
    }
}

bool kid_member_valid(const char *path, FILINFO *rom_info) {
  char sidecar[KID_PATH_BYTES]; FILINFO title;
  return f_stat(path,rom_info)==FR_OK && !(rom_info->fattrib & AM_DIR) &&
    rom_info->fsize>=sizeof(t_rom_header) && rom_info->fsize<=MAX_GBA_ROM_SIZE &&
    kid_sidecar_path(path,"txt",sidecar) && f_stat(sidecar,&title)==FR_OK && !(title.fattrib & AM_DIR);
}
int kid_boot_select(const kid_library *library, bool held_b, bool pending_ok, const char **notice) {
  bool attempted; unsigned attempt=kid_attempt_consume(&attempted);
  if (attempt!=KID_STATE_MISSING) *notice="Could not remember your game";
  bool bypass; unsigned skip=kid_skip_consume(&bypass);
  if (skip==KID_STATE_ERROR || skip==KID_STATE_INVALID) *notice="Please choose a game";
  if (!pending_ok) { *notice="Could not save. Ask a grown-up"; return -1; }
  if (held_b || bypass || attempted) return -1;
  char path[KID_PATH_BYTES]; unsigned read=kid_last_read(path);
  if (read==KID_STATE_MISSING) return -1;
  if (read!=KID_STATE_OK || library->io_error) {
    *notice="Could not remember your game";
    if (!kid_last_clear()) *notice="Could not remember. Ask a grown-up";
    return -1;
  }
  for (unsigned i=0;i<library->count;i++) if (!strcmp(path,library->entries[i].path)) {
    FILINFO info;
    if (kid_member_valid(path,&info)) return i;
    break;
  }
  if (!kid_last_clear()) *notice="Could not remember. Ask a grown-up";
  return -1;
}
