/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the actual static menu launch path with I/O/engine doubles, not a
 * copy of its policy. --gc-sections drops unrelated normal UI entry points. */
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../src/menu.c"


t_patch_policy patcher_default;
uint8_t ingamemenu_default, rtcpatch_default, autosave_prefer_ds;
uint8_t autoload_default, autosave_default, rtcspeed_default, save_path_default;
uint32_t rtcvalue_default;
const t_igmenu ingame_menu_payload = {0};
static bool have_database, have_cache, have_sidecar, have_save, fail_write, short_read, rom_unavailable;
static bool per_rom, superfw_rom;
static unsigned save_failure, preload_failure, patch_generations, save_calls, load_calls, reload_calls, restore_calls, db_reads;
static t_patch selected_patch;
static char expected_path[KID_PATH_BYTES];
static unsigned expected_load, expected_save, expected_cheats;
static bool expected_igm, expected_rtc;
static const char *expect_savefn;

void dma_memset16(volatile void *ptr, uint16_t value, uint16_t count) {
  volatile uint16_t *p = ptr; for (unsigned i=0;i<count;i++) p[i]=value;
}
void dma_memcpy32(volatile void *dst, const void *src, uint16_t count) { memcpy((void*)dst, src, count*4); }
void set_supercard_mode(unsigned mode, bool write, bool sd) { (void)mode; (void)write; (void)sd; }
unsigned font_block_size(void) { return 2048; }
void font_restore_embedded(uint16_t *destination) { (void)destination; restore_calls++; }
FRESULT f_stat(const TCHAR *path, FILINFO *info) {
  assert(!strcmp(path, expected_path));
  info->fattrib=0; info->fsize=65536;
  return rom_unavailable ? FR_DISK_ERR : FR_OK;
}
FRESULT f_open(FIL *file, const TCHAR *path, BYTE mode) {
  (void)file; assert(!strcmp(path, expected_path)); assert(mode==FA_READ); return FR_OK;
}
FRESULT f_read(FIL *file, void *buffer, UINT amount, UINT *read) {
  (void)file; memset(buffer,0,amount); *read=short_read ? amount-1 : amount; return FR_OK;
}
FRESULT f_close(FIL *file) { (void)file; return FR_OK; }
unsigned preload_gba_rom(const char *path, uint32_t size, t_rom_header *header) {
  assert(!strcmp(path,expected_path)); assert(size==65536); memset(header,0,sizeof(*header));
  if (superfw_rom) memcpy(&header->data[SUPERFW_COMMENT_DOFFSET], "SUPERFW~DAVIDGF",16);
  return preload_failure;
}
bool patchmem_lookup(const uint8_t *gamecode, const uint8_t *dbptr, t_patch *patch) {
  (void)gamecode; (void)dbptr; db_reads++; *patch=selected_patch; return have_database;
}
bool load_rom_patches(const char *path, t_patch *patch) {
  assert(!strcmp(path,expected_path)); *patch=selected_patch; return have_sidecar;
}
bool load_cached_patches(const char *path, t_patch *patch) {
  assert(!strcmp(path,expected_path)); *patch=selected_patch; return have_cache;
}
void patchengine_init(t_patch_builder *builder, unsigned size) { memset(builder,0,sizeof(*builder)); builder->filesize=size; patch_generations++; }
bool patchengine_process_rom(const uint32_t *rom, unsigned size, t_patch_builder *builder, void(*progress)(unsigned)) {
  (void)rom; (void)size; (void)builder; progress(0); return true;
}
void patchengine_finalize(t_patch_builder *builder) { builder->p=selected_patch; }
bool write_patches_cache(const char *path, const t_patch *patch) {
  (void)patch; assert(!strcmp(path,expected_path)); if (fail_write) return false; have_cache=true; return true;
}
bool load_rom_settings(const char *path, t_rom_load_settings *settings, t_rom_launch_settings *launch) {
  assert(!strcmp(path,expected_path));
  if (per_rom) { settings->use_dsaving=true; settings->use_rtc=true; launch->rtcts=12345; launch->use_cheats=true; }
  return per_rom;
}
void sram_filename_calc(const char *path, char *out, unsigned preference) {
  assert(!strcmp(path,expected_path)); assert(preference==save_path_default); strcpy(out,"/Games/Puzzle.sav");
}
bool check_file_exists(const char *path) { assert(!strcmp(path,"/Games/Puzzle.sav")); return have_save; }
unsigned prepare_savegame(t_sram_load_policy load, t_sram_save_policy save, EnumSavetype type, t_dirsave_info *info, const char *path) {
  (void)type; memset(info,0,sizeof(*info)); save_calls++;
  assert(load==expected_load && save==expected_save); assert(!strcmp(path,"/Games/Puzzle.sav")); return save_failure;
}
unsigned load_gba_rom(const char *path, uint32_t size, const char *savefn, const t_patch *patch,
 const t_dirsave_info *ds, bool igm, const t_rtc_info *rtc, unsigned cheats, progress_fn progress) {
  load_calls++;
  assert(!strcmp(path,expected_path)); assert(size==65536); assert(cheats==expected_cheats);
  assert(igm==expected_igm && (rtc!=NULL)==expected_rtc);
  assert((ds!=NULL)==(expected_save==SaveDirect));
  if (rtc) { assert(rtc->timestamp==(per_rom ? 12345 : rtcvalue_default)); assert(rtc->ts_step==rtcspeed_default); }
  assert((savefn!=NULL)==(expect_savefn!=NULL)); if (savefn) assert(!strcmp(savefn,expect_savefn));
  if (patcher_default==PatchNone) assert(patch==NULL); else assert(patch!=NULL);
  progress(1,0); /* Short ROMs must not divide by zero. */
  memset((void*)kid_sdr,0xa5,sizeof(*kid_sdr)); /* Simulate ROM overwrite. */
  assert(!strcmp(path,expected_path)); /* Path is still ordinary RAM. */
  return ERR_LOAD_BADROM;
}
void kid_library_scan(kid_library *library, kid_scan_scratch *scratch) {
  (void)scratch; reload_calls++;
  memset(library,0,sizeof(*library)); library->count=2;
  strcpy(library->entries[0].path,"/Games/Other.gba");
  strcpy(library->entries[1].path,expected_path); strcpy(library->entries[1].title,"Puzzle");
}
bool kid_sidecar_path(const char *path, const char *extension, char *out) {
  assert(!strcmp(path,expected_path)); assert(!strcmp(extension,"bmp")); strcpy(out,"/Games/Puzzle.bmp"); return true;
}
bool kid_art_load(const char *path, uint16_t *pixels, uint16_t *palette) {
  assert(!strcmp(path,"/Games/Puzzle.bmp")); pixels[0]=0x1414; palette[0]=31; return true;
}
static void reset_case(void) {
  strcpy(expected_path,"/Games/Puzzle.gba");
  memset(&spop,0,sizeof(spop)); memset((void*)kid_sdr,0,sizeof(*kid_sdr));
  kid_sdr->library.count=1; strcpy(kid_sdr->library.entries[0].path,expected_path);
  kid_selected=0; kid_enabled=true; kid_assets_lost=false; kid_message=NULL;
  have_database=have_cache=have_sidecar=have_save=fail_write=short_read=rom_unavailable=per_rom=superfw_rom=false;
  save_failure=preload_failure=patch_generations=save_calls=load_calls=reload_calls=restore_calls=db_reads=0;
  memset(&selected_patch,0,sizeof(selected_patch)); selected_patch.save_mode=SaveTypeSRAM;
  patcher_default=PatchAuto; ingamemenu_default=1; rtcpatch_default=1; autosave_prefer_ds=0;
  autoload_default=autosave_default=1; rtcspeed_default=2; rtcvalue_default=1000; save_path_default=SaveRomName;
  expected_cheats=0; expected_load=SaveLoadReset; expected_save=SaveReboot; expected_igm=expected_rtc=false; expect_savefn="/Games/Puzzle.sav";
}
int main(void) {
  assert(mmap((void*)0x08000000,32*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
  assert(mmap((void*)0x04000000,0x1000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
  assert(mmap((void*)0x06000000,0x20000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
  assert(mmap((void*)0x07000000,0x1000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
  reset_case(); kid_launch(); /* Auto -> generate, refresh, then launch. */
  assert(patch_generations==1 && load_calls==1 && save_calls==1 && restore_calls==1);
  assert(kid_selected==1 && kid_art_valid && kid_assets_lost && reload_calls==1);
  assert(!strcmp(kid_sdr->library.entries[1].path,expected_path));
  reset_case(); have_database=true; have_save=true; expected_load=SaveLoadSav;
  selected_patch.irqh_ops=1; selected_patch.rtc_ops=1; expected_igm=expected_rtc=true; kid_launch();
  assert(patch_generations==0 && load_calls==1);
  reset_case(); have_database=true; selected_patch.irqh_ops=0; kid_launch(); assert(load_calls==1 && !expected_igm);
  reset_case(); have_sidecar=true; per_rom=true; expected_save=SaveDirect; expected_rtc=true; selected_patch.rtc_ops=1; selected_patch.save_mode=SaveTypeFlash512K; kid_launch();
  assert(load_calls==1 && patch_generations==0);
  reset_case(); patcher_default=PatchEngine; have_database=true; kid_launch(); assert(patch_generations==1 && load_calls==1);
  reset_case(); patcher_default=PatchDatabase; kid_launch(); assert(!save_calls && !load_calls && !patch_generations);
  reset_case(); fail_write=true; kid_launch(); assert(patch_generations==1 && !save_calls && !load_calls);
  reset_case(); short_read=true; kid_launch(); assert(patch_generations==1 && !load_calls);
  reset_case(); have_cache=true; save_failure=ERR_SAVE_BADSAVE; kid_launch(); assert(save_calls==1 && !load_calls && !restore_calls);
  reset_case(); rom_unavailable=true; kid_launch(); assert(!save_calls && !load_calls);
  reset_case(); preload_failure=1; kid_launch(); assert(!save_calls && !load_calls);
  reset_case(); superfw_rom=true; kid_launch(); assert(!save_calls && !load_calls);
  reset_case(); patcher_default=PatchNone; kid_launch(); assert(load_calls==1 && !patch_generations);
  reset_case(); have_cache=true; selected_patch.save_mode=SaveTypeNone; expected_load=SaveLoadDisable; expected_save=SaveDisable; expect_savefn=NULL; kid_launch(); assert(load_calls==1);
  reset_case(); kid_assets_lost=true; have_database=true; kid_launch(); assert(db_reads==0 && patch_generations==1 && load_calls==1);
  /* Marker-absent shared helper still honors upstream cheat/launch values. */
  reset_case(); kid_enabled=false; have_cache=true;
  t_rom_load_settings normal = { .patch_policy=PatchEngine, .use_igm=true, .use_rtc=true, .use_dsaving=false };
  selected_patch.irqh_ops=1; selected_patch.rtc_ops=1;
  assert(prepare_gba_info(&spop.p.load.i,&normal,expected_path,65536,true));
  strcpy(spop.p.load.l.savefn,"/Games/Puzzle.sav");
  prepare_gba_settings(&spop.p.load.l,false,rtcvalue_default,false);
  expected_igm=expected_rtc=true; expected_cheats=128;
  spop.p.load.l.use_cheats=true; spop.p.load.l.cheats_size=expected_cheats;
  unsigned shared_save_error;
  assert(launch_prepared_gba(&shared_save_error)==ERR_LOAD_BADROM && !shared_save_error);
  assert(load_calls==1 && save_calls==1 && !reload_calls && !restore_calls);
  puts("kid launch production-path tests passed");
}

