/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise actual startup dispatch: kid failures retain the pending sentinel;
 * marker-absent startup deliberately keeps the upstream unlink policy. */
#include <assert.h>
#include <stdio.h>
#include "kid_mode.h"
#ifndef KID_MAIN_SOURCE
#define KID_MAIN_SOURCE "../src/main.c"
#endif
#define main firmware_main
#include KID_MAIN_SOURCE
#undef main
bool kid_boot_enabled, kid_boot_bypass, kid_boot_pending_ok=true;
static bool pending;
static unsigned flush_error, flushes, unlinks;
FRESULT f_stat(const TCHAR *path,FILINFO *info) {
  (void)info; assert(!strcmp(path,PENDING_SAVE_FILEPATH)); return pending ? FR_OK : FR_NO_FILE;
}
FRESULT f_unlink(const TCHAR *path) { assert(!strcmp(path,PENDING_SAVE_FILEPATH)); pending=false; unlinks++; return FR_OK; }
unsigned flush_pending_sram(void) { flushes++; return flush_error; }
unsigned flush_pending_sram_checked(void) { return flush_pending_sram(); }
void draw_text_idx8_bus16(const char *s,uint8_t *dst,unsigned pitch,uint8_t color) { (void)s; (void)dst; (void)pitch; (void)color; }
void dma_memset16(volatile void *dst,uint16_t value,uint16_t count) { (void)dst; (void)value; (void)count; }
void wait_ms(unsigned ms) { (void)ms; }
int main(void) {
  kid_boot_enabled=true; pending=true; flush_error=ERR_SAVE_FLUSH_WRITEFAIL;
  check_pending_saves(); assert(pending && !unlinks && !kid_boot_pending_ok && flushes==1);
  flush_error=0; check_pending_saves(); assert(!pending && unlinks==1 && kid_boot_pending_ok && flushes==2);
  kid_boot_enabled=false; pending=true; flush_error=ERR_SAVE_FLUSH_WRITEFAIL;
  check_pending_saves(); assert(!pending && unlinks==2 && flushes==3);
  puts("kid/normal actual startup pending dispatch tests passed");
}
