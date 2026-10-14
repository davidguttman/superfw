/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../src/ingame_menu.c"
unsigned ingame_kid_mode, ingame_ds_used;
uint32_t savefile_backups, cheat_base_addr;
unsigned ingame_menu_lang, has_rtc_support;
static unsigned drawn_count;
static const char *drawn[8];
void draw_text_idx8_bus16(const char *text, uint8_t *dst, unsigned width, uint8_t color) {
  (void)dst; (void)width; (void)color; drawn[drawn_count++]=text;
}
char savefile_pattern[256];
static bool save_ok, marker_ok, unlink_ok;
static unsigned saves, skips, unlinks, resets;
void set_supercard_mode(unsigned mode, bool write, bool sd) {
  assert(mode == MAPPED_SDRAM && write); (void)sd;
}
bool write_save_sram_rotate_checked(const char *path,unsigned backups) {
  assert(!strcmp(path,"/Games/Puzzle")); assert(backups>=2); saves++; assert(!skips && !unlinks && !resets); return save_ok;
}
bool kid_skip_write(void) { skips++; assert(!unlinks && !resets); return marker_ok; }
bool kid_remove_file(const char *path) { assert(!strcmp(path,PENDING_SAVE_FILEPATH)); unlinks++; assert(skips && !resets); return unlink_ok; }
bool program_sram_dump(const char *path, unsigned backups) { (void)path; (void)backups; return true; }
void reset_fw(void) { resets++; assert(skips && unlinks); }
static void setup(void) {
  save_ok=marker_ok=unlink_ok=true; saves=skips=unlinks=resets=0;
  ingame_kid_mode=1; ingame_ds_used=0; savefile_backups=0;
  strcpy(savefile_pattern,"/Games/Puzzle"); memset(&popup,0,sizeof(popup)); copt=0;
}
int main(void) {
  setup(); uint8_t frame[240*160]; drawn_count=0; draw_main_menu(frame,0);
  assert(drawn_count==2 && !strcmp(drawn[0],"Keep playing") && !strcmp(drawn[1],"Choose another game"));
  ingame_kid_mode=0; drawn_count=0; draw_main_menu(frame,0); assert(drawn_count==6);
  setup(); assert(kid_ingame_input(KEY_BUTTB)); assert(!saves && !resets);
  setup(); assert(kid_ingame_input(KEY_BUTTA)); assert(!saves && !resets);
  setup(); assert(!kid_ingame_input(KEY_BUTTDOWN) && copt==1);
  assert(!kid_ingame_input(KEY_BUTTDOWN) && copt==0);
  assert(!kid_ingame_input(KEY_BUTTUP) && copt==1);
  assert(!kid_ingame_input(KEY_BUTTSTA|KEY_BUTTSEL|KEY_BUTTL|KEY_BUTTR) && copt==1);
  setup(); copt=1; save_ok=false; assert(!kid_ingame_input(KEY_BUTTA)); assert(saves==1 && !skips && !unlinks && !resets && popup.msg);
  setup(); copt=1; marker_ok=false; assert(!kid_ingame_input(KEY_BUTTA)); assert(saves==1 && skips==1 && !unlinks && !resets && popup.msg);
  setup(); copt=1; unlink_ok=false; assert(!kid_ingame_input(KEY_BUTTA)); assert(saves==1 && skips==1 && unlinks==1 && !resets && popup.msg);
  setup(); copt=1; ingame_ds_used=1; assert(!kid_ingame_input(KEY_BUTTA)); assert(!saves && resets==1);
  setup(); copt=1; savefile_pattern[0]=0; assert(!kid_ingame_input(KEY_BUTTA)); assert(!saves && resets==1);
  setup(); copt=1; assert(!kid_ingame_input(KEY_BUTTA)); assert(saves==1 && skips==1 && unlinks==1 && resets==1);
  puts("kid IGM two-action/save-before-handoff production tests passed");
}
