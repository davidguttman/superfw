/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kid_state.h"
#include <stdint.h>
#include "save.h"
#include "common.h"
#include <string.h>
#pragma GCC optimize ("Os")
typedef struct { uint32_t magic; char path[KID_RECORD_PATH_BYTES]; uint32_t hash; } kid_record;
_Static_assert(sizeof(kid_record)==264, "Kid record protocol size");
#define KID_RECORD_MAGIC 0x3144494b /* KID1 */
static uint32_t hash_path(const char *path) {
  uint32_t hash=2166136261u;
  for (unsigned i=0;i<KID_RECORD_PATH_BYTES;i++) hash=(hash ^ (uint8_t)path[i])*16777619u;
  return hash;
}
static bool valid_path(const char *path) {
  unsigned n=0;
  while (n<KID_RECORD_PATH_BYTES && path[n]) {
    if ((uint8_t)path[n]<32 || path[n]=='\\' || path[n]==127) return false;
    n++;
  }
  if (n<12 || n>=KID_RECORD_PATH_BYTES || strncmp(path,"/Games/",7)) return false;
  for (unsigned i=7;i<n;i++)
    if (path[i]=='/' && (path[i-1]=='/' || i==n-1)) return false;
  const char *part=path+7;
  while (part) {
    const char *end=strchr(part,'/'); unsigned len=end ? (unsigned)(end-part) : strlen(part);
    if (!len || (len==1 && part[0]=='.') || (len==2 && part[0]=='.' && part[1]=='.')) return false;
    part=end ? end+1 : NULL;
  }
  const char *ext=path+n-4;
  return ext[0]=='.' && (ext[1]|32)=='g' && (ext[2]|32)=='b' && (ext[3]|32)=='a';
}
bool kid_remove_file(const char *path) {
  FRESULT r=f_unlink(path);
  return r==FR_OK || r==FR_NO_FILE || r==FR_NO_PATH;
}
bool kid_last_clear(void) { return kid_remove_file(KID_LAST_FILE); }
unsigned kid_last_read(char path[KID_RECORD_PATH_BYTES]) {
  path[0]=0;
  FILINFO info; FRESULT r=f_stat(KID_LAST_FILE,&info);
  FRESULT guard=f_stat(KID_ATTEMPT_FILE,NULL);
  if (guard!=FR_NO_FILE && guard!=FR_NO_PATH)
    return guard==FR_OK ? KID_STATE_INVALID : KID_STATE_ERROR;
  if (r==FR_NO_FILE || r==FR_NO_PATH) return KID_STATE_MISSING;
  if (r!=FR_OK) return KID_STATE_ERROR;
  if ((info.fattrib & AM_DIR) || info.fsize!=sizeof(kid_record)) return KID_STATE_INVALID;
  FIL fd; kid_record record; UINT read=0;
  if (f_open(&fd,KID_LAST_FILE,FA_READ)!=FR_OK) return KID_STATE_ERROR;
  r=f_read(&fd,&record,sizeof(record),&read);
  FRESULT closed=f_close(&fd);
  if (r!=FR_OK || closed!=FR_OK || read!=sizeof(record)) return KID_STATE_ERROR;
  if (record.magic!=KID_RECORD_MAGIC || record.hash!=hash_path(record.path) || !valid_path(record.path))
    return KID_STATE_INVALID;
  memcpy(path,record.path,KID_RECORD_PATH_BYTES);
  return KID_STATE_OK;
}
static bool write_marker(const char *path) {
  FIL fd;
  if (f_open(&fd,path,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return false;
  return f_close(&fd)==FR_OK;
}
static bool write_record(const char *path, bool ready) {
  if (!valid_path(path)) return false;
  /* Also guard direct promotion callers: a full write followed by failed
   * close must never expose valid magic as eligible history. */
  if (!write_marker(KID_ATTEMPT_FILE)) return false;
  kid_record record={.magic=ready ? KID_RECORD_MAGIC : 0};
  strcpy(record.path,path); record.hash=hash_path(record.path);
  FIL fd; UINT written=0;
  if (f_open(&fd,KID_LAST_FILE,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK) return false;
  FRESULT r=f_write(&fd,&record,sizeof(record),&written);
  FRESULT closed=f_close(&fd);
  return r==FR_OK && closed==FR_OK && written==sizeof(record) &&
    (!ready || kid_remove_file(KID_ATTEMPT_FILE));
}
bool kid_last_write(const char *path) { return write_record(path,true); }
bool kid_last_stage(const char *path) { return write_record(path,false); }
bool kid_skip_write(void) { return write_marker(KID_SKIP_FILE); }
unsigned kid_attempt_consume(bool *bypass) {
  FILINFO info; FRESULT r=f_stat(KID_ATTEMPT_FILE,&info);
  *bypass=r!=FR_NO_FILE && r!=FR_NO_PATH;
  if (!*bypass) return KID_STATE_MISSING;
  /* Never let consuming a stale guard make a failed promotion eligible on
   * the next boot. On unreadable guard, retain it even after invalidation. */
  if (!kid_last_clear() || r!=FR_OK || !kid_remove_file(KID_ATTEMPT_FILE))
    return KID_STATE_ERROR;
  return (info.fattrib & AM_DIR) || info.fsize ? KID_STATE_INVALID : KID_STATE_OK;
}
unsigned kid_skip_consume(bool *bypass) {
  FILINFO info; FRESULT r=f_stat(KID_SKIP_FILE,&info);
  *bypass=r!=FR_NO_FILE && r!=FR_NO_PATH;
  if (!*bypass) return KID_STATE_MISSING;
  if (r!=FR_OK || !kid_remove_file(KID_SKIP_FILE)) return KID_STATE_ERROR;
  return (info.fattrib & AM_DIR) || info.fsize ? KID_STATE_INVALID : KID_STATE_OK;
}

bool kid_pending_ready(void) {
  FRESULT r=f_stat(PENDING_SAVE_FILEPATH,NULL);
  if (r==FR_NO_FILE || r==FR_NO_PATH) return true;
  return r==FR_OK && !flush_pending_sram_checked() && kid_remove_file(PENDING_SAVE_FILEPATH);
}
