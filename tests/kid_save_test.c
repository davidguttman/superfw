/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual SRAM writer, checked rotation, sentinel parser, and boot guard. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>
#ifndef KID_SAVE_SOURCE
#define KID_SAVE_SOURCE "../src/save.c"
#endif
#include KID_SAVE_SOURCE
#include "kid_state.h"
static const char *sentinel="/Games/Puzzle\nbackup_count=0";
static bool pending, fail_write, fail_close, fail_unlink, fail_open, fail_read, pending_close, short_read;
static bool saved, different, compare_close, compare_open, compare_read, compare_short;
static FSIZE_t compare_size, compare_short_offset, original_backup_size;
static bool original_backed_up;
static bool parent_exists, parent_file, parent_stat, mkdir_error;
static unsigned mkdir_calls;
static const char *save_template="/Games/Puzzle";
static unsigned rename_failure, renames, writes, flush_bytes;
FRESULT f_stat(const TCHAR *path,FILINFO *info) {
  if (!strcmp(path,PENDING_SAVE_FILEPATH)) return pending ? FR_OK : FR_NO_FILE;
  assert(!strcmp(path,"/Games") || !strcmp(path,"/SAVEGAME") || !strcmp(path,"/Games/Puzzles"));
  if(parent_stat) return FR_DISK_ERR;
  if(!parent_exists) return FR_NO_PATH;
  memset(info,0,sizeof(*info)); info->fattrib=parent_file ? 0 : AM_DIR; return FR_OK;
}
FRESULT f_mkdir(const TCHAR *path) { assert(!strcmp(path,"/SAVEGAME") || !strcmp(path,"/Games")); mkdir_calls++; if(mkdir_error) return FR_DISK_ERR; parent_exists=true; return FR_OK; }
FRESULT f_open(FIL *fd,const TCHAR *path,BYTE mode) {
  memset(fd,0,sizeof(*fd));
  if (!strcmp(path,PENDING_SAVE_FILEPATH)) { fd->obj.objsize=strlen(sentinel); fd->obj.attr=1; return pending ? FR_OK : FR_NO_FILE; }
  char expected[MAX_FN_LEN]; snprintf(expected,sizeof(expected),"%s%s",save_template,mode==FA_READ ? ".sav" : ".tmp.sav"); assert(!strcmp(path,expected));
  if (mode==FA_READ) { fd->obj.attr=3; fd->obj.objsize=compare_size; return compare_open ? FR_DISK_ERR : saved ? FR_OK : FR_NO_FILE; }
  fd->obj.attr=2; if(!parent_exists) return FR_NO_PATH;
  return fail_open ? FR_DISK_ERR : FR_OK;
}
FRESULT f_read(FIL *fd,void *out,UINT amount,UINT *n) {
  if(fd->obj.attr==3) {
    *n=MIN(amount,fd->obj.objsize-fd->fptr);
    if(compare_short && fd->fptr>=compare_short_offset && *n) --*n;
    memset(out,different ? 0x4c : 0x4b,*n); fd->fptr+=*n;
    return compare_read ? FR_DISK_ERR : FR_OK;
  }
  assert(fd->obj.attr==1); *n=strlen(sentinel); if(*n>amount)*n=amount; if(short_read && *n) --*n; memcpy(out,sentinel,*n); return fail_read ? FR_DISK_ERR : FR_OK;
}
FRESULT f_write(FIL *fd,const void *in,UINT amount,UINT *n) {
  assert(fd->obj.attr==2); writes++; *n=fail_write ? 0 : amount;
  for(unsigned i=0;i<amount;i++) assert(((const unsigned char*)in)[i]==0x4b);
  flush_bytes+=*n; return fail_write ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *fd) { return (compare_close && fd->obj.attr==3) || (fail_close && fd->obj.attr==2) || (pending_close && fd->obj.attr==1) ? FR_DISK_ERR : FR_OK; }
FRESULT f_unlink(const TCHAR *path) {
  if(fail_unlink) return FR_DISK_ERR;
  if(!strcmp(path,PENDING_SAVE_FILEPATH)) { pending=false; return FR_OK; }
  return FR_NO_FILE;
}
FRESULT f_rename(const TCHAR *from,const TCHAR *to) {
  renames++; if(rename_failure==renames) return FR_DISK_ERR;
  char original[MAX_FN_LEN],backup[MAX_FN_LEN];
  snprintf(original,sizeof(original),"%s.sav",save_template);
  snprintf(backup,sizeof(backup),"%s.1.sav",save_template);
  if(saved && !strcmp(from,original)) {
    assert(!strcmp(to,backup)); original_backed_up=true; original_backup_size=compare_size;
  }
  return FR_OK;
}
void set_supercard_mode(unsigned mode,bool w,bool sd) { (void)mode; (void)w; (void)sd; }
void create_basepath(const char *path) { assert(!strcmp(path,save_template)); }
uint32_t parseuint(const char *str) { return strtoul(str,NULL,10); }
int npf_snprintf(char *buf,size_t size,const char *format,...) {
  va_list args; va_start(args,format); int n=vsnprintf(buf,size,format,args); va_end(args); return n;
}
static void setup(void) {
  pending=true; fail_write=fail_close=fail_unlink=fail_open=fail_read=pending_close=short_read=false;
  saved=different=compare_close=compare_open=compare_read=compare_short=false;
  compare_size=128*1024; compare_short_offset=original_backup_size=0; original_backed_up=false;
  parent_exists=true; parent_file=parent_stat=mkdir_error=false; mkdir_calls=0; save_template="/Games/Puzzle";
  rename_failure=renames=writes=flush_bytes=0;
}
int main(void) {
  assert(mmap((void*)0x0e000000,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
  memset((void*)0x0e000000,0x4b,65536);
  /* Actual IGM checked-writer entry, before any boot helper creates dirs. */
  setup(); parent_exists=false; save_template="/SAVEGAME/Apple";
  assert(write_save_sram_rotate_checked(save_template,2) && mkdir_calls==1 && parent_exists && flush_bytes==128*1024 && renames==4 && pending);
  setup(); parent_exists=false; mkdir_error=true; save_template="/SAVEGAME/Apple";
  assert(!write_save_sram_rotate_checked(save_template,2) && mkdir_calls==1 && !writes && pending);
  setup(); parent_file=true; assert(!write_save_sram_rotate_checked(save_template,2) && !writes && !mkdir_calls);
  setup(); parent_stat=true; assert(!write_save_sram_rotate_checked(save_template,2) && !writes && !mkdir_calls);
  setup(); save_template="/Games/Puzzles/Puzzle";
  assert(write_save_sram_rotate_checked(save_template,2) && !mkdir_calls && flush_bytes==128*1024);
  /* Matching bytes do not prove a successful comparison if close failed. */
  setup(); saved=true; compare_close=true; assert(!kid_pending_ready() && pending && !writes && !renames);
  setup(); saved=true; assert(kid_pending_ready() && !pending && !writes && !renames);
  /* Imported/DirectSave saves can end before 128 KiB with a matching prefix.
     Clean declared EOF must rotate the original into backup, not block boot. */
  const FSIZE_t shorter_sizes[]={32*1024,64*1024,0,1,1023,1025,128*1024-1};
  for(unsigned i=0;i<sizeof(shorter_sizes)/sizeof(*shorter_sizes);i++) {
    setup(); saved=true; compare_size=shorter_sizes[i];
    assert(kid_pending_ready() && !pending && flush_bytes==128*1024 && renames==4);
    assert(original_backed_up && original_backup_size==shorter_sizes[i]);
  }
  /* Declared EOF never excuses failed close or premature FR_OK short reads. */
  setup(); saved=true; compare_size=32*1024; compare_close=true;
  assert(!kid_pending_ready() && pending && !writes && !renames);
  setup(); saved=true; compare_size=64*1024; compare_short=true;
  assert(!kid_pending_ready() && pending && !writes && !renames);
  setup(); saved=true; compare_size=1025; compare_short=true; compare_short_offset=1024;
  assert(!kid_pending_ready() && pending && !writes && !renames);
  setup(); saved=true; compare_size=32*1024; compare_read=true;
  assert(!kid_pending_ready() && pending && !writes && !renames);
  /* Comparison retains the accepted 128 KiB prefix behavior for oversized files. */
  setup(); saved=true; compare_size=128*1024+1;
  assert(kid_pending_ready() && !pending && !writes && !renames);
  setup(); saved=true; compare_open=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); saved=true; compare_read=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); saved=true; compare_short=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); saved=true; different=true; compare_close=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); saved=true; different=true; assert(kid_pending_ready() && !pending && flush_bytes==128*1024 && renames==4);
  /* Legacy normal mode still ignores a matching comparison's close error. */
  setup(); saved=true; compare_close=true; assert(!flush_pending_sram() && pending && !writes);
  setup(); saved=true; compare_size=32*1024; assert(!compare_save_sram("/Games/Puzzle.sav"));
  setup(); saved=true; compare_size=128*1024+1; assert(compare_save_sram("/Games/Puzzle.sav"));
  setup(); assert(kid_pending_ready() && !pending && flush_bytes==128*1024 && renames==4);
  setup(); fail_read=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); short_read=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); pending_close=true; assert(!kid_pending_ready() && pending && !writes);
  setup(); fail_write=true; assert(!kid_pending_ready() && pending && !renames);
  setup(); fail_close=true; assert(!kid_pending_ready() && pending && !renames);
  setup(); fail_open=true; assert(!kid_pending_ready() && pending && !writes);
  for(unsigned i=1;i<=4;i++) { setup(); rename_failure=i; assert(!kid_pending_ready() && pending); }
  setup(); fail_unlink=true; assert(!kid_pending_ready() && pending);
  /* Disabled mode retains upstream rotation's legacy error policy. */
  setup(); rename_failure=1; assert(rotate_savefile("/Games/Puzzle",2));
  setup(); sentinel="broken"; assert(!kid_pending_ready() && pending && !writes);
  sentinel="/Games/Puzzle\nbackup_count=99"; setup(); assert(!kid_pending_ready() && pending && !writes);
  for(unsigned i=0;i<65536;i++) assert(((volatile unsigned char*)0x0e000000)[i]==0x4b);
  puts("kid pending SRAM/checked rotation production tests passed");
}
