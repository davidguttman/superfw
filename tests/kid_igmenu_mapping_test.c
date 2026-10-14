/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Production IGM, SRAM writer and handoff: assert mapping at the FIRST I/O,
 * before the SRAM bank switch could accidentally enable SD and hide a bug. */
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../src/ingame_menu.c"
#include "../src/save.c"

unsigned ingame_kid_mode, ingame_ds_used;
uint32_t savefile_backups;
char savefile_pattern[256];
static bool sd_enabled, writable, marker_closed, pending_removed;
static unsigned fs_calls, save_bytes, resets, failure;
static jmp_buf reset_target;
enum { SaveOpen = 1, SaveWrite, SaveClose, SaveRotate, MarkerOpen, MarkerClose, PendingUnlink };

void set_supercard_mode(unsigned mode, bool write, bool sd) {
  assert(mode == MAPPED_SDRAM);
  writable = write;
  sd_enabled = sd;
}
static void filesystem_operation(void) {
  assert(sd_enabled); /* Includes the very first f_open in either path. */
  assert(!resets);
  fs_calls++;
}
FRESULT f_stat(const TCHAR *path, FILINFO *info) {
  filesystem_operation();
  assert(!strcmp(path,"/Games") && fs_calls==1);
  memset(info,0,sizeof(*info)); info->fattrib=AM_DIR; return FR_OK;
}
FRESULT f_open(FIL *fd, const TCHAR *path, BYTE mode) {
  filesystem_operation();
  assert(mode == (FA_WRITE | FA_CREATE_ALWAYS));
  memset(fd, 0, sizeof(*fd));
  if (!strcmp(path, "/Games/Puzzle.tmp.sav")) {
    assert(fs_calls == 2); /* Checked parent stat is now the first I/O. */
    fd->obj.attr = 1;
    return failure == SaveOpen ? FR_DISK_ERR : FR_OK;
  }
  assert(!strcmp(path, KID_SKIP_FILE));
  if (ingame_ds_used || !savefile_pattern[0]) assert(fs_calls == 1);
  else assert(save_bytes == 128 * 1024);
  fd->obj.attr = 2;
  return failure == MarkerOpen ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *fd, const void *data, UINT size, UINT *written) {
  filesystem_operation();
  assert(fd->obj.attr == 1);
  for (unsigned i = 0; i < size; i++) assert(((const uint8_t *)data)[i] == 0x4b);
  *written = failure == SaveWrite ? 0 : size;
  save_bytes += *written;
  return failure == SaveWrite ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *fd) {
  filesystem_operation();
  if (fd->obj.attr == 1) return failure == SaveClose ? FR_DISK_ERR : FR_OK;
  assert(fd->obj.attr == 2);
  marker_closed = failure != MarkerClose;
  return marker_closed ? FR_OK : FR_DISK_ERR;
}
FRESULT f_unlink(const TCHAR *path) {
  filesystem_operation();
  if (!strcmp(path, PENDING_SAVE_FILEPATH)) {
    assert(marker_closed);
    pending_removed = failure != PendingUnlink;
    return pending_removed ? FR_OK : FR_DISK_ERR;
  }
  assert(strstr(path, ".sav"));
  return FR_NO_FILE;
}
FRESULT f_rename(const TCHAR *from, const TCHAR *to) {
  filesystem_operation();
  assert(strstr(from, ".sav") && strstr(to, ".sav"));
  return failure == SaveRotate ? FR_DISK_ERR : FR_OK;
}
FRESULT f_mkdir(const TCHAR *path) { (void)path; assert(0); return FR_DISK_ERR; }
FRESULT f_chmod(const TCHAR *path, BYTE attr, BYTE mask) {
  (void)path; (void)attr; (void)mask; assert(0); return FR_DISK_ERR;
}
int npf_snprintf(char *buf, size_t size, const char *format, ...) {
  va_list args; va_start(args, format);
  int n = vsnprintf(buf, size, format, args);
  va_end(args); return n;
}
void reset_fw(void) {
  assert(sd_enabled && marker_closed && pending_removed);
  resets++;
  longjmp(reset_target, 1); /* The real assembly reset never returns. */
}
static void setup(unsigned mode, unsigned fail) {
  sd_enabled = false; writable = true; /* IGM assembly entry: mode 0x5. */
  marker_closed = pending_removed = false;
  fs_calls = save_bytes = resets = 0; failure = fail;
  ingame_kid_mode = 1; ingame_ds_used = mode == 2; savefile_backups = 0;
  strcpy(savefile_pattern, mode == 1 ? "" : "/Games/Puzzle");
  memset(&popup, 0, sizeof(popup)); copt = 1;
}
static void assert_sram_unchanged(void) {
  for (unsigned i = 0; i < 65536; i++) assert(((const uint8_t *)0x0e000000)[i] == 0x4b);
}
static void check_mode(unsigned mode) {
  setup(mode, 0);
  if (!setjmp(reset_target)) {
    kid_ingame_input(KEY_BUTTA);
    assert(0); /* A successful handoff must reset. */
  }
  assert(resets == 1 && fs_calls && save_bytes == (mode ? 0 : 128 * 1024));
  assert_sram_unchanged();
  for (unsigned fail = mode ? MarkerOpen : SaveOpen; fail <= PendingUnlink; fail++) {
    setup(mode, fail);
    assert(!kid_ingame_input(KEY_BUTTA));
    assert(fs_calls && !sd_enabled && writable && !resets && popup.msg);
    assert_sram_unchanged();
  }
  setup(mode, 0); assert(kid_ingame_input(KEY_BUTTB));
  assert(!fs_calls && !sd_enabled && writable);
  setup(mode, 0); copt = 0; assert(kid_ingame_input(KEY_BUTTA));
  assert(!fs_calls && !sd_enabled && writable);
  setup(mode, 0); assert(!kid_ingame_input(KEY_BUTTDOWN));
  assert(!fs_calls && !sd_enabled && writable && copt == 0);
}
int main(int argc, char **argv) {
  assert(mmap((void *)0x0e000000, 65536, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) != (void *)-1);
  memset((void *)0x0e000000, 0x4b, 65536);
  if (argc == 2) check_mode(strtoul(argv[1], NULL, 10));
  else for (unsigned mode = 0; mode < 3; mode++) check_mode(mode);
  puts("kid IGM first-filesystem-access mapping/restoration production tests passed");
}
