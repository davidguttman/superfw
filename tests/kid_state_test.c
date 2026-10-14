/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kid_mode.h"
#include "common.h"
static unsigned char record[512];
static unsigned length, skip_size;
static bool skip_dir;
static bool rom=true, title=true, pending;
static unsigned flush_error, flush_calls;
unsigned flush_pending_sram_checked(void) { flush_calls++; return flush_error; }
bool kid_sidecar_path(const char *path, const char *extension, char *out) {
  assert(!strcmp(extension,"txt")); strcpy(out,path); strcpy(out+strlen(out)-3,"txt"); return true;
}
static bool exists, skip, fail_open, fail_write, fail_close, fail_unlink, short_read, stat_error;
static bool attempt, attempt_dir, attempt_close, attempt_stat, attempt_unlink;
static unsigned attempt_size;
#define ATTEMPT_FILE "/.superfw/kid-attempt"
FRESULT f_stat(const TCHAR *path, FILINFO *info) {
  if (!strcmp(path,ATTEMPT_FILE)) {
    if(attempt_stat) return FR_DISK_ERR;
    if(!attempt) return FR_NO_FILE;
    if(info) { memset(info,0,sizeof(*info)); info->fattrib=attempt_dir ? AM_DIR : 0; info->fsize=attempt_size; }
    return FR_OK;
  }
  if (stat_error) return FR_DISK_ERR;
  bool is_rom=!strcmp(path,"/Games/Puzzle.gba"), is_title=!strcmp(path,"/Games/Puzzle.txt");
  bool present = is_rom ? rom : is_title ? title : !strcmp(path,PENDING_SAVE_FILEPATH) ? pending : !strcmp(path,KID_SKIP_FILE) ? skip : exists;
  if (!present) return FR_NO_FILE;
  if (info) { memset(info,0,sizeof(*info)); info->fattrib=skip_dir && !strcmp(path,KID_SKIP_FILE) ? AM_DIR : 0; info->fsize=is_rom ? 65536 : !strcmp(path,KID_SKIP_FILE) ? skip_size : length; }
  return FR_OK;
}
FRESULT f_open(FIL *fd,const TCHAR *path,BYTE mode) {
  if (fail_open) return FR_DISK_ERR;
  fd->fptr=0;
  if (!strcmp(path,ATTEMPT_FILE)) { assert(mode & FA_WRITE); attempt=true; fd->fptr=1; return FR_OK; }
  if (!strcmp(path,KID_SKIP_FILE)) { assert(mode & FA_WRITE); skip=true; return FR_OK; }
  if (mode & FA_WRITE) { exists=true; length=0; }
  return exists ? FR_OK : FR_NO_FILE;
}
FRESULT f_read(FIL *fd,void *out,UINT amount,UINT *n) {
  (void)fd; *n=length < amount ? length : amount; if(short_read && *n) --*n; memcpy(out,record,*n); return FR_OK;
}
FRESULT f_write(FIL *fd,const void *in,UINT amount,UINT *n) {
  (void)fd; *n=fail_write ? amount/2 : amount; memcpy(record,in,*n); length=*n; return fail_write ? FR_DISK_ERR : FR_OK;
}
FRESULT f_close(FIL *fd) { return (fd->fptr ? attempt_close : fail_close) ? FR_DISK_ERR : FR_OK; }
FRESULT f_unlink(const TCHAR *path) {
  if(fail_unlink) return FR_DISK_ERR;
  if(!strcmp(path,ATTEMPT_FILE)) { if(attempt_unlink) return FR_DISK_ERR; bool was=attempt; attempt=false; return was ? FR_OK : FR_NO_FILE; }
  if(!strcmp(path,PENDING_SAVE_FILEPATH)) { pending=false; return FR_OK; }
  if(!strcmp(path,KID_SKIP_FILE)) { bool was=skip; skip=false; return was ? FR_OK : FR_NO_FILE; }
  bool was=exists; exists=false; return was ? FR_OK : FR_NO_FILE;
}
int main(void) {
  char path[KID_RECORD_PATH_BYTES]; bool bypass;
  kid_library library={0}; library.count=1; strcpy(library.entries[0].path,"/Games/Puzzle.gba");
  const char *notice=NULL;
  /* Actual production protocol: bytes reach disk before close reports failure,
   * and record cleanup also fails. The next two boots must not retry them. */
  assert(kid_last_stage("/Games/Puzzle.gba"));
  fail_close=true; assert(!kid_last_write("/Games/Puzzle.gba") && length==264);
  assert(!memcmp(record,"KID1",4)); /* A genuinely valid persisted record. */
  fail_close=false; fail_unlink=true; assert(!kid_last_clear());
  assert(kid_last_read(path)!=KID_STATE_OK);
  assert(kid_boot_select(&library,false,true,&notice)==-1 && attempt && exists);
  assert(kid_boot_select(&library,false,true,&notice)==-1 && attempt && exists);
  fail_unlink=false;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists);
  /* A crash after staging or after full promotion bytes but before guard
   * deletion must also suppress this boot and any later repeat eligibility. */
  assert(kid_last_stage("/Games/Puzzle.gba") && attempt);
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists);
  attempt_unlink=true;
  assert(!kid_last_write("/Games/Puzzle.gba") && attempt && exists && length==264);
  assert(kid_last_read(path)!=KID_STATE_OK);
  assert(kid_boot_select(&library,false,true,&notice)==-1 && attempt && !exists);
  attempt_unlink=false;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists);
  assert(kid_boot_select(&library,false,true,&notice)==-1);
  assert(kid_last_write("/Games/Puzzle.gba") && !attempt);
  /* Unreadable guards remain even after invalidation, never becoming a
   * one-shot skip which exposes the valid old record on the following boot. */
  attempt=true; attempt_stat=true;
  assert(kid_boot_select(&library,true,true,&notice)==-1 && attempt && !exists);
  assert(kid_boot_select(&library,false,true,&notice)==-1 && attempt && !exists);
  attempt_stat=false;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt);
  assert(kid_last_write("/Games/Puzzle.gba")); attempt=true; attempt_size=1;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists); attempt_size=0;
  assert(kid_last_write("/Games/Puzzle.gba")); attempt=true; attempt_dir=true;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists); attempt_dir=false;
  assert(kid_last_write("/Games/Puzzle.gba")); attempt_close=true;
  assert(!kid_last_stage("/Games/Puzzle.gba") && attempt && exists);
  attempt_close=false;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !attempt && !exists);
  assert(kid_last_read(path)==KID_STATE_MISSING);
  assert(kid_last_stage("/Games/Puzzle.gba"));
  assert(kid_last_read(path)==KID_STATE_INVALID);
  assert(kid_last_write("/Games/Puzzle.gba"));
  assert(kid_last_read(path)==KID_STATE_OK && !strcmp(path,"/Games/Puzzle.gba"));
  record[20]^=1; assert(kid_last_read(path)==KID_STATE_INVALID);
  assert(kid_last_write("/Games/Puzzle.gba")); short_read=true;
  assert(kid_last_read(path)==KID_STATE_ERROR); short_read=false;
  assert(!kid_last_write("/Elsewhere/Game.gba"));
  assert(!kid_last_write("/Games/../Game.gba"));
  assert(kid_skip_consume(&bypass)==KID_STATE_MISSING && !bypass);
  assert(kid_skip_write()); assert(kid_skip_consume(&bypass)==KID_STATE_OK && bypass && !skip);
  assert(kid_skip_consume(&bypass)==KID_STATE_MISSING && !bypass);
  skip=true; fail_unlink=true;
  assert(kid_skip_consume(&bypass)==KID_STATE_ERROR && bypass && skip);
  fail_unlink=false; skip_size=1;
  assert(kid_skip_consume(&bypass)==KID_STATE_INVALID && bypass && !skip); skip_size=0;
  skip=true; skip_dir=true; assert(kid_skip_consume(&bypass)==KID_STATE_INVALID && bypass); skip_dir=false;
  stat_error=true;
  assert(kid_skip_consume(&bypass)==KID_STATE_ERROR && bypass);
  stat_error=false; fail_close=true; assert(!kid_last_write("/Games/Puzzle.gba"));
  assert(!kid_skip_write()); fail_close=false; fail_write=true;
  assert(!kid_last_write("/Games/Puzzle.gba")); fail_write=false;
  assert(attempt && kid_last_read(path)!=KID_STATE_OK); /* partial stage/write */
  assert(kid_last_clear() && !exists);
  fail_open=true; assert(!kid_last_stage("/Games/Puzzle.gba") && !kid_skip_write()); fail_open=false;
  skip=false; assert(kid_boot_select(&library,false,true,&notice)==-1); /* first boot */
  assert(kid_last_write("/Games/Puzzle.gba"));
  assert(kid_boot_select(&library,false,true,&notice)==0);
  assert(kid_boot_select(&library,true,true,&notice)==-1 && exists); /* B preserves history */
  assert(kid_boot_select(&library,false,false,&notice)==-1 && notice);
  assert(kid_skip_write()); assert(kid_boot_select(&library,false,true,&notice)==-1 && !skip);
  assert(kid_boot_select(&library,false,true,&notice)==0); /* one time only */
  title=false; assert(kid_boot_select(&library,false,true,&notice)==-1 && !exists); title=true;
  assert(kid_last_write("/Games/Puzzle.gba")); rom=false;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !exists); rom=true;
  assert(kid_last_write("/Games/Puzzle.gba")); library.count=0;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !exists); library.count=1;
  assert(kid_last_write("/Games/Puzzle.gba")); library.io_error=1;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !exists); library.io_error=0;
  assert(kid_last_write("/Games/Puzzle.gba")); record[0]^=1;
  assert(kid_boot_select(&library,false,true,&notice)==-1 && !exists);
  pending=true; flush_error=2; assert(!kid_pending_ready() && pending && flush_calls==1);
  flush_error=0; fail_unlink=true; assert(!kid_pending_ready() && pending && flush_calls==2);
  fail_unlink=false; assert(kid_pending_ready() && !pending && flush_calls==3);
  assert(kid_pending_ready() && flush_calls==3);
  stat_error=true; assert(!kid_pending_ready() && flush_calls==3);
  puts("kid record/one-shot production protocol tests passed");
}
