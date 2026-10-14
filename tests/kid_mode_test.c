/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kid_mode.h"
#include "gbahw.h"

static FRESULT stat_result;
static BYTE stat_attr;
static FSIZE_t stat_size;
FRESULT f_stat(const TCHAR *path, FILINFO *info) {
  assert(!strcmp(path, KID_MARKER));
  info->fattrib = stat_attr;
  info->fsize = stat_size;
  return stat_result;
}

int main(void) {
  stat_result = FR_NO_FILE;
  assert(!kid_mode_detect());
  stat_result = FR_DISK_ERR;
  assert(!kid_mode_detect());
  stat_result = FR_OK; stat_attr = AM_DIR;
  assert(!kid_mode_detect());
  stat_attr = 0; stat_size = 1;
  assert(!kid_mode_detect());
  stat_size = 0;
  assert(kid_mode_detect());
  stat_attr = AM_ARC | AM_HID | AM_RDO;
  assert(kid_mode_detect());

  unsigned selected = 0;
  for (unsigned keys = 0; keys < 1024; keys++) {
    selected = 0;
    assert(kid_picker_input(keys, 0, &selected) == KID_INPUT_NONE);
    assert(selected == 0);
  }
  assert(kid_picker_input(KEY_BUTTLEFT, 3, &selected) == KID_INPUT_CHANGED);
  assert(selected == 2);
  assert(kid_picker_input(KEY_BUTTRIGHT, 3, &selected) == KID_INPUT_CHANGED);
  assert(selected == 0);
  assert(kid_picker_input(KEY_BUTTA, 3, &selected) == KID_INPUT_PLAY);
  assert(kid_picker_input(KEY_BUTTLEFT | KEY_BUTTA, 3, &selected) == KID_INPUT_CHANGED);
  assert(selected == 2); /* Navigation wins; never play unseen selection. */
  assert(kid_picker_input(KEY_BUTTLEFT | KEY_BUTTRIGHT, 3, &selected) == KID_INPUT_NONE);
  for (unsigned keys = 0; keys < 1024; keys++) {
    if (!(keys & (KEY_BUTTLEFT | KEY_BUTTRIGHT | KEY_BUTTA))) {
      selected = 1;
      assert(kid_picker_input(keys, 3, &selected) == KID_INPUT_NONE);
      assert(selected == 1);
    }
  }
  assert(kid_progress_pixels(0, 0) == 0);
  assert(kid_progress_pixels(1, 0) == 0);
  assert(kid_progress_pixels(500, 100) == 200);
  assert(kid_progress_pixels(50, 100) == 100);

  struct { uint16_t before; uint16_t frame[240*160/2]; uint16_t after; } screen;
  uint16_t art[KID_ART_WORDS];
  for (unsigned i = 0; i < KID_ART_WORDS; i++) art[i] = 0x1514;
  screen.before = screen.after = 0xabba;
  kid_picture_render(screen.frame, art, true);
  for (unsigned y = 0; y < 160; y++)
    for (unsigned x = 0; x < 240; x += 2) {
      uint16_t expected = (y >= KID_ART_Y && y < KID_ART_Y+104 && x >= KID_ART_X && x < KID_ART_X+160) ? 0x1514 : 0x1111;
      assert(screen.frame[(y*240+x)/2] == expected);
    }
  assert(screen.before == 0xabba && screen.after == 0xabba);
  kid_picture_render(screen.frame, art, false);
  for (unsigned i = 0; i < 240*160/2; i++) {
    unsigned lo = screen.frame[i] & 255, hi = screen.frame[i] >> 8;
    assert(lo < 20 && hi < 20); /* No invalid artwork is displayed. */
  }
  assert(screen.before == 0xabba && screen.after == 0xabba);
  puts("kid mode marker/input/render tests passed");
}
