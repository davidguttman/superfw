/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kid_mode.h"
#include "gbahw.h"

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
