/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Test the actual SDRAM loader with FatFS/hardware doubles. */
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../src/loader.c"

bool slowsd;
uint8_t use_slowld, boot_bios_splash, use_fastew;
uint8_t hotkey_combo, lang_id, anim_speed, backup_sram_default;
const t_combo_key hotkey_list[13] = {{0}};
const t_igmenu ingame_menu_payload = { .menu_rsize = 1024 };
const uint32_t ingame_menu_payload_size = sizeof(t_igmenu);
const uint8_t directsave_payload[] = {0};
const uint32_t directsave_payload_size = sizeof(directsave_payload);

static unsigned file_size, file_pos, read_calls, fail_read, short_read;
static unsigned close_calls, launches, patches, payloads, completions, slow_copies;
static bool seek_error, close_error, open_error, writable, sd_enabled;
static uint8_t *const rom = (uint8_t *)GBA_ROM_BASE;

static uint8_t file_byte(unsigned offset) { return (uint8_t)(offset * 7 + offset / 8192 + 3); }
FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode) {
  (void)file; assert(!strcmp(path,"/game.gba") && mode == FA_READ);
  file_pos = 0; return open_error ? FR_DISK_ERR : FR_OK;
}
FRESULT f_read(FIL *file, void *buffer, UINT amount, UINT *read) {
  (void)file; read_calls++;
  assert(sd_enabled && writable);
  unsigned count = MIN(amount, file_size - file_pos);
  if (read_calls == short_read && count) count--;
  for (unsigned i = 0; i < count; i++) ((uint8_t*)buffer)[i] = file_byte(file_pos + i);
  file_pos += count; *read = count;
  return read_calls == fail_read ? FR_DISK_ERR : FR_OK;
}
FRESULT f_lseek(FIL *file, FSIZE_t offset) {
  (void)file; assert(sd_enabled && writable);
  file_pos = MIN(offset, file_size); return seek_error ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *file) {
  (void)file; close_calls++; assert(sd_enabled && writable);
  return close_error ? FR_DISK_ERR : FR_OK;
}
void set_supercard_mode(unsigned mode, bool write, bool sd) {
  assert(mode == MAPPED_SDRAM); writable = write; sd_enabled = sd;
}
void dma_memcpy32(volatile void *dst, const void *src, uint16_t count) {
  assert(writable && !sd_enabled && count && !((uintptr_t)dst & 3));
  memcpy((void*)dst, src, count * 4);
}
void rom_copy_write16(void *dst, const void *src, unsigned count) {
  /* The actual assembly copies a full 32-byte group on every iteration. */
  assert(writable && !sd_enabled && count && !(count & 31));
  slow_copies++; memcpy(dst, src, count);
}
void launch_reset(bool bios, bool fast) { (void)bios; (void)fast; launches++; }
bool patch_apply_rom(uint8_t *buffer, unsigned size, uint32_t base, bool wait,
  const t_patch *patch, bool rtc, uint32_t igm, uint32_t ds) {
  (void)buffer; (void)size; (void)base; (void)wait; (void)patch; (void)rtc; (void)igm; (void)ds;
  patches++; return true;
}
void payload_apply_rom(uint8_t *buffer, unsigned size, uint32_t base,
  const uint8_t *payload, unsigned payload_size, uint32_t offset) {
  (void)buffer; (void)size; (void)base; (void)payload; (void)payload_size; (void)offset; payloads++;
}
unsigned font_block_size(void) { return 1024; }
void memmove32(void *dst, void *src, unsigned count) { memmove(dst, src, count); }
void memcpy32(void *restrict dst, const void *restrict src, unsigned count) { memcpy(dst, src, count); }
void memset32(void *dst, uint32_t value, unsigned count) { (void)value; memset(dst, 0, count); }
void set_abort_lr(uint32_t value) { (void)value; }
void set_undef_lrsp(uint32_t lr, uint32_t sp) { (void)lr; (void)sp; }
bool sc_issdhc(void) { return true; }
uint16_t sc_rca(void) { return 1; }
void replace_extension(char *path, const char *extension) { (void)path; (void)extension; }
void savestate_filename_calc(const char *path, char *out) { (void)path; memset(out, 0, MAX_FN_LEN); }
uint32_t systime(void) { return 0; }
void write_sram_buffer(const uint8_t *buf, unsigned offset, unsigned size) { (void)buf; (void)offset; (void)size; }
static void progress(unsigned current, unsigned total) { if (current == 1 && total == 1) completions++; }

static void reset_case(unsigned size, bool slow) {
  file_size = size; file_pos = read_calls = fail_read = short_read = 0;
  close_calls = launches = patches = payloads = completions = slow_copies = 0;
  seek_error = close_error = open_error = false;
  writable = sd_enabled = slowsd = true; use_slowld = slow;
  memset(rom, 0xa5, MAX_GBA_ROM_SIZE);
}
static unsigned load(unsigned size, const t_patch *patch, const t_dirsave_info *ds) {
  return load_gba_rom("/game.gba", size, NULL, patch, ds, false, NULL, 0, progress);
}
static void expect_failed(unsigned size, const t_patch *patch, const t_dirsave_info *ds) {
  assert(load(size, patch, ds) == ERR_LOAD_BADROM);
  assert(!launches && !patches && !payloads && !completions);
  assert(close_calls == 1 && slowsd && writable && sd_enabled);
}
static void expect_bytes(unsigned start, unsigned end) {
  for (unsigned i = start; i < end; i++) {
    /* fix_gba_header intentionally changes these header fields. */
    if (i >= 0xb2 && i <= 0xbd) continue;
    assert(rom[i] == file_byte(i));
  }
}
int main(void) {
  assert(mmap(rom, MAX_GBA_ROM_SIZE, PROT_READ|PROT_WRITE,
    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) != MAP_FAILED);
  assert(mmap((void*)0x04000000, 0x1000, PROT_READ|PROT_WRITE,
    MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0) != MAP_FAILED);

  /* FR_OK short mid-load must not patch/launch stale scratch bytes. */
  reset_case(20000, false); short_read = 2;
  t_patch patch = {0};
  expect_failed(file_size, &patch, NULL);
  assert(rom[8192] == 0xa5); /* Reject the partial chunk before copying it. */

  for (unsigned slow = 0; slow < 2; slow++) {
    reset_case(20000, slow); fail_read = 2; expect_failed(file_size, &patch, NULL);
    reset_case(20000, slow); short_read = 1; expect_failed(file_size, &patch, NULL);
    reset_case(20000, slow); file_size--; expect_failed(20000, &patch, NULL);
    reset_case(20000, slow); seek_error = true; expect_failed(file_size, &patch, NULL);
    reset_case(20000, slow); close_error = true; expect_failed(file_size, &patch, NULL);

    /* All possible DMA/halfword tails; EOF rounding and appended patch room
     * are legitimate padding, not missing ROM bytes. */
    for (unsigned tail = 0; tail < 32; tail++) {
      reset_case(16384 + tail, slow);
      assert(load(file_size, &patch, NULL) == 0);
      assert(launches == 1 && patches == 1 && close_calls == 1 && completions == 1 && slowsd);
      expect_bytes(0, file_size);
      unsigned rounded = ROUND_UP2(file_size, 1024) + 1024;
      for (unsigned i = file_size; i < rounded; i++) assert(rom[i] == 0);
      assert(rom[rounded] == 0xa5); /* Do not overwrite the payload area. */
    }

    /* A nearly-full ROM needs an internal gap for DirectSave. Exercise the
     * second loop, including its non-word/32-byte-aligned final chunk. */
    patch.hole_addr = 8192; patch.hole_size = 8192;
    t_dirsave_info ds = {0};
    for (unsigned tail = 1; tail <= 3; tail++) {
      reset_case(MAX_GBA_ROM_SIZE - tail, slow);
      assert(load(file_size, &patch, &ds) == 0);
      assert(launches == 1 && patches == 1 && payloads == 1 && close_calls == 1);
      expect_bytes(0, patch.hole_addr);
      expect_bytes(patch.hole_addr + patch.hole_size, file_size);
      for (unsigned i = patch.hole_addr; i < patch.hole_addr + patch.hole_size; i++) assert(rom[i] == 0xa5);
      for (unsigned i = file_size; i < MAX_GBA_ROM_SIZE; i++) assert(rom[i] == 0);
    }
    reset_case(MAX_GBA_ROM_SIZE - 3, slow); short_read = 2;
    expect_failed(file_size, &patch, &ds); assert(rom[16384] == 0xa5);
    reset_case(MAX_GBA_ROM_SIZE - 3, slow); fail_read = 2;
    expect_failed(file_size, &patch, &ds);
    reset_case(MAX_GBA_ROM_SIZE - 3, slow); short_read = 4095;
    expect_failed(file_size, &patch, &ds);
    reset_case(MAX_GBA_ROM_SIZE - 3, slow); seek_error = true;
    expect_failed(file_size, &patch, &ds);
  }
  reset_case(256, false); short_read = 1;
  t_rom_header header;
  assert(preload_gba_rom("/game.gba", 256, &header) == ERR_LOAD_BADROM && close_calls == 1);
  reset_case(256, false); close_error = true;
  assert(preload_gba_rom("/game.gba", 256, &header) == ERR_LOAD_BADROM && close_calls == 1);
  puts("loader production-path exact-read/padding tests passed");
}
