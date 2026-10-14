/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "kid_library.h"

typedef struct { char path[1024]; const unsigned char *data; size_t size; int dir; } node;
static node nodes[3000];
static unsigned nn, opened, closed;
static kid_library library;
static kid_scan_scratch scratch;
static int read_fail_after = -1, read_calls;
static const char *open_fail_path;
static void reset(void) { nn = opened = closed = read_calls = 0; read_fail_after = -1; open_fail_path = NULL; }
static void add(const char *path, const void *data, size_t size, int dir) {
  assert(nn < 3000); assert(strlen(path) < sizeof(nodes[0].path));
  strcpy(nodes[nn].path, path); nodes[nn].data = data; nodes[nn].size = size; nodes[nn++].dir = dir;
}
static int find(const char *path) {
  for (unsigned i = 0; i < nn; i++) if (!strcasecmp(path, nodes[i].path)) return i;
  return -1;
}
FRESULT f_open(FIL *f, const TCHAR *path, BYTE mode) {
  assert(mode == FA_READ); int i = find(path);
  if (i < 0) return FR_NO_FILE;
  if (nodes[i].dir) return FR_DENIED;
  if (open_fail_path && !strcasecmp(path, open_fail_path)) return FR_DISK_ERR;
  memset(f, 0, sizeof(*f)); f->obj.sclust = i; f->obj.objsize = nodes[i].size; opened++; return FR_OK;
}
FRESULT f_stat(const TCHAR *path, FILINFO *info) {
  int i = find(path); if (i < 0) return FR_NO_FILE;
  memset(info, 0, sizeof(*info));
  info->fattrib = nodes[i].dir ? AM_DIR : 0; info->fsize = nodes[i].size;
  return FR_OK;
}
FRESULT f_close(FIL *f) { (void)f; closed++; return FR_OK; }
FRESULT f_lseek(FIL *f, FSIZE_t ofs) { f->fptr = ofs; return FR_OK; }
FRESULT f_read(FIL *f, void *out, UINT n, UINT *read) {
  if (read_fail_after >= 0 && read_calls++ >= read_fail_after) { *read = 0; return FR_DISK_ERR; }
  node *v = &nodes[f->obj.sclust];
  if (f->fptr > v->size) { *read = 0; return FR_OK; }
  if (n > v->size - f->fptr) n = v->size - f->fptr;
  if (n) memcpy(out, v->data + f->fptr, n);
  f->fptr += n; *read = n; return FR_OK;
}
FRESULT f_opendir(DIR *d, const TCHAR *path) {
  int i = find(path); if (i < 0 || !nodes[i].dir) return FR_NO_PATH;
  memset(d, 0, sizeof(*d)); d->obj.sclust = i; opened++; return FR_OK;
}
FRESULT f_closedir(DIR *d) { (void)d; closed++; return FR_OK; }
FRESULT f_readdir(DIR *d, FILINFO *info) {
  const char *base = nodes[d->obj.sclust].path; size_t len = strlen(base);
  memset(info, 0, sizeof(*info));
  while (d->dptr < nn) {
    node *v = &nodes[d->dptr++];
    if (strncmp(base, v->path, len) || v->path[len] != '/') continue;
    const char *name = v->path + len + 1;
    if (strchr(name, '/')) continue;
    assert(strlen(name) < sizeof(info->fname)); strcpy(info->fname, name);
    info->fattrib = v->dir ? AM_DIR : 0; info->fsize = v->size; break;
  }
  return FR_OK;
}
static void scan(void) { kid_library_scan(&library, &scratch); assert(opened == closed); }
static void library_tests(void) {
  reset(); add("/Games", NULL, 0, 1);
  add("/Games/raw.gba", NULL, 0, 0);
  add("/Games/orphan.txt", "No ROM", 6, 0);
  add("/Games/other.gb", NULL, 0, 0); add("/Games/other.txt", "Other", 5, 0);
  add("/Games/z.GBA", NULL, 0, 0); add("/Games/z.TXT", " \tZelda\r\n", 9, 0);
  add("/Games/b.gba", NULL, 0, 0); add("/Games/b.txt", "alpha", 5, 0);
  add("/Games/a.gba", NULL, 0, 0); add("/Games/a.txt", "Alpha", 5, 0);
  add("/Games/sub", NULL, 0, 1);
  add("/Games/sub/z.gba", NULL, 0, 0); add("/Games/sub/z.txt", "Nested", 6, 0);
  scan(); assert(library.count == 4);
  assert(!strcmp(library.entries[0].path, "/Games/a.gba"));
  assert(!strcmp(library.entries[1].title, "alpha"));
  assert(!strcmp(library.entries[2].title, "Nested"));
  assert(!strcmp(library.entries[3].title, "Zelda"));
  char side[KID_PATH_BYTES]; assert(kid_sidecar_path(library.entries[2].path, "bmp", side));
  assert(!strcmp(side, "/Games/sub/z.bmp"));
  reset(); add("/Games", NULL, 0, 1);
  add("/Games/Blank.gba", NULL, 0, 0); add("/Games/Blank.txt", "\r\n \t", 4, 0);
  add("/Games/Utf.gba", NULL, 0, 0); add("/Games/Utf.txt", "\xef\xbb\xbfPok\xc3\xa9mon", 11, 0);
  add("/Games/Bad.gba", NULL, 0, 0); add("/Games/Bad.txt", "\xc0\xaf", 2, 0);
  unsigned char oversized[256]; memset(oversized, 'x', sizeof(oversized));
  add("/Games/Huge.gba", NULL, 0, 0); add("/Games/Huge.txt", oversized, 256, 0);
  char longtitle[81]; memset(longtitle, 'x', 60); for (int i=60;i<80;i+=2) { longtitle[i]='\xc3'; longtitle[i+1]='\xa9'; } longtitle[80]=0;
  add("/Games/Long.gba", NULL, 0, 0); add("/Games/Long.txt", longtitle, 80, 0);
  scan(); assert(library.count == 5);
  assert(!strcmp(library.entries[0].title, "Bad")); assert(!strcmp(library.entries[1].title, "Blank"));
  assert(!strcmp(library.entries[2].title, "Huge")); assert(!strcmp(library.entries[3].title, "Pok\xc3\xa9mon"));
  assert(strlen(library.entries[4].title) == 62);
  reset(); scan(); assert(!library.count && !library.io_error);
  reset(); add("/Games", NULL, 0, 1); add("/Games/a",NULL,0,1); add("/Games/a/b",NULL,0,1); add("/Games/a/b/c",NULL,0,1); add("/Games/a/b/c/d",NULL,0,1);
  add("/Games/a/b/c/in.gba",NULL,0,0); add("/Games/a/b/c/in.txt","In",2,0);
  add("/Games/a/b/c/d/out.gba",NULL,0,0); add("/Games/a/b/c/d/out.txt","Out",3,0);
  scan(); assert(library.count == 1 && library.limited);
  reset(); add("/Games", NULL, 0, 1);
  char path[128];
  for (int i=70;i>=0;i--) { snprintf(path,sizeof(path),"/Games/%03d.gba",i); add(path,NULL,0,0); snprintf(path,sizeof(path),"/Games/%03d.txt",i); add(path,"Same",4,0); }
  scan(); assert(library.count == KID_MAX_GAMES && library.limited); assert(!strcmp(library.entries[0].path,"/Games/000.gba")); assert(!strcmp(library.entries[63].path,"/Games/063.gba"));
  reset(); add("/Games", NULL, 0, 1);
  for (unsigned i=0;i<1100;i++) { snprintf(path,sizeof(path),"/Games/file%u.dat",i); add(path,NULL,0,0); }
  scan(); assert(library.items == KID_MAX_ITEMS && library.limited);
  reset(); add("/Games", NULL, 0, 1);
  for (unsigned i=0;i<80;i++) { snprintf(path,sizeof(path),"/Games/dir%u",i); add(path,NULL,0,1); }
  scan(); assert(library.dirs == KID_MAX_DIRS && library.limited);
  reset(); add("/Games", NULL, 0, 1);
  char overpath[300]; strcpy(overpath,"/Games/"); memset(overpath+7,'x',249); strcpy(overpath+256,".gba");
  add(overpath,NULL,0,0); scan(); assert(!library.count && library.limited);
  reset(); add("/Games", NULL, 0, 1); add("/Games/Fault.gba",NULL,0,0); add("/Games/Fault.txt","Friendly",8,0);
  read_fail_after=0; scan(); assert(library.count==1 && library.io_error && !strcmp(library.entries[0].title,"Fault"));
  reset(); add("/Games", NULL, 0, 1); add("/Games/OpenFault.gba",NULL,0,0); add("/Games/OpenFault.txt","Friendly",8,0);
  add("/Games/Missing.gba",NULL,0,0);
  open_fail_path="/Games/OpenFault.txt"; scan();
  assert(library.count==1 && library.io_error && !strcmp(library.entries[0].title,"OpenFault"));
  reset(); add("/Games", NULL, 0, 1); add("/Games/NoText.gba",NULL,0,0); add("/Games/NoText.txt",NULL,0,1);
  scan(); assert(!library.count && library.io_error);
  const char *invalid[] = {"\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "a\x7f", "a\0b"};
  unsigned lengths[] = {3,4,2,2,3};
  for (unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);i++) {
    reset(); add("/Games",NULL,0,1); add("/Games/Safe.gba",NULL,0,0); add("/Games/Safe.txt",invalid[i],lengths[i],0);
    scan(); assert(library.count==1 && !strcmp(library.entries[0].title,"Safe"));
  }
  for (unsigned code=0x80;code<=0x9f;code++) {
    const unsigned char control[] = {'A', 0xc2, (unsigned char)code, 'B'};
    reset(); add("/Games",NULL,0,1); add("/Games/Safe.gba",NULL,0,0); add("/Games/Safe.txt",control,sizeof(control),0);
    scan(); assert(library.count==1 && !strcmp(library.entries[0].title,"Safe"));
  }
  reset(); add("/Games",NULL,0,1); add("/Games/Unicode.gba",NULL,0,0); add("/Games/Unicode.txt","A\xc2\xa0" "B",4,0);
  scan(); assert(library.count==1 && !strcmp(library.entries[0].title,"A\xc2\xa0" "B"));
  reset(); add("/Games",NULL,0,1); add("/Games/Space.gba",NULL,0,0); add("/Games/Space.txt","  First\t \r\nSecond  ",19,0);
  scan(); assert(!strcmp(library.entries[0].title,"First Second"));
}

#define BMP_OFFSET (54 + 8)
#define BMP_BYTES (BMP_OFFSET + KID_ART_WIDTH * KID_ART_HEIGHT)
static unsigned char bmp[BMP_BYTES];
static void put16(unsigned at, unsigned v) { bmp[at]=v; bmp[at+1]=v>>8; }
static void put32(unsigned at, uint32_t v) { put16(at,v); put16(at+2,v>>16); }
static void good_bmp(int topdown) {
  memset(bmp,0,sizeof(bmp)); bmp[0]='B';bmp[1]='M';put32(2,sizeof(bmp));put32(10,BMP_OFFSET);put32(14,40);
  put32(18,KID_ART_WIDTH);put32(22,topdown ? (uint32_t)-KID_ART_HEIGHT : KID_ART_HEIGHT);put16(26,1);put16(28,8);put32(34,KID_ART_WIDTH*KID_ART_HEIGHT);put32(46,2);
  bmp[54+2]=255; bmp[58+1]=255;
  memset(bmp+BMP_OFFSET,1,KID_ART_WIDTH); /* red elsewhere, green first disk row */
}
static uint16_t pixels[KID_ART_WORDS + 2], palette[KID_ART_COLORS + 2];
static bool load(void) {
  reset(); add("/art.bmp",bmp,sizeof(bmp),0);
  pixels[0]=pixels[KID_ART_WORDS+1]=0xabcd; palette[0]=palette[KID_ART_COLORS+1]=0xabcd;
  for (unsigned i=1;i<=KID_ART_WORDS;i++) pixels[i]=0xaaaa;
  for (unsigned i=1;i<=KID_ART_COLORS;i++) palette[i]=0xbbbb;
  bool ok=kid_art_load("/art.bmp",pixels+1,palette+1);
  assert(opened == closed); assert(pixels[0]==0xabcd && pixels[KID_ART_WORDS+1]==0xabcd); assert(palette[0]==0xabcd && palette[KID_ART_COLORS+1]==0xabcd);
  if (!ok) { for (unsigned i=1;i<=KID_ART_WORDS;i++) assert(pixels[i]==0xaaaa); for (unsigned i=1;i<=KID_ART_COLORS;i++) assert(palette[i]==0xbbbb); }
  return ok;
}
static void art_tests(void) {
  good_bmp(0); assert(load()); assert(pixels[1] == 0x1414); assert(pixels[KID_ART_WORDS-79] == 0x1515); assert(palette[1]==31 && palette[2]==31*32);
  good_bmp(1); assert(load()); assert(pixels[1]==0x1515 && pixels[81]==0x1414);
  good_bmp(0); put32(10,53); assert(!load());
  good_bmp(0); put32(10,0xfffffff0); assert(!load());
  good_bmp(0); put32(18,159); assert(!load());
  good_bmp(0); put32(22,105); assert(!load());
  good_bmp(0); put32(30,1); assert(!load());
  good_bmp(0); put16(28,24); assert(!load());
  good_bmp(0); put32(14,108); assert(!load());
  good_bmp(0); put32(2,sizeof(bmp)+1); assert(!load());
  good_bmp(0); put32(6,1); assert(!load());
  good_bmp(0); put32(34,17); assert(!load());
  good_bmp(0); put32(50,3); assert(!load());
  good_bmp(0); put16(26,2); assert(!load());
  good_bmp(0); put32(46,221); assert(!load());
  good_bmp(0); put32(46,0); assert(!load());
  good_bmp(0); bmp[54+3]=1; assert(!load());
  good_bmp(0); bmp[58+3]=255; assert(!load());
  good_bmp(0); bmp[BMP_OFFSET+5]=2; assert(!load());
  good_bmp(0); bmp[0]='X'; assert(!load());
  good_bmp(0); reset(); add("/art.bmp",bmp,sizeof(bmp)-1,0); assert(!kid_art_load("/art.bmp",pixels+1,palette+1)); assert(opened==closed);
  for (unsigned i=0;i<54;i++) { reset(); add("/art.bmp",bmp,i,0); assert(!kid_art_load("/art.bmp",pixels+1,palette+1)); assert(opened==closed); }
  reset(); assert(!kid_art_load("/missing.bmp",pixels+1,palette+1));
  good_bmp(0); reset(); add("/art.bmp",bmp,sizeof(bmp),0); read_fail_after=2; assert(!kid_art_load("/art.bmp",pixels+1,palette+1)); assert(opened==closed);
}
int main(int argc, char **argv) {
  if (argc==2 || argc==3) {
    FILE *f=fopen(argv[1],"rb"); assert(f); assert(!fseek(f,0,SEEK_END)); long n=ftell(f); assert(n>0); rewind(f);
    unsigned char *data=malloc(n); assert(data); assert(fread(data,1,n,f)==(size_t)n); fclose(f);
    reset(); add("/converted.bmp",data,n,0); assert(kid_art_load("/converted.bmp",pixels+1,palette+1));
    if (argc==2) { assert(palette[1]==31); assert(pixels[1]==0x1414); }
    for (unsigned i=1;i<=KID_ART_WORDS;i++) { unsigned a=pixels[i]&255,b=pixels[i]>>8; assert(a>=20 && a<=239 && b>=20 && b<=239); }
    assert(opened==closed); free(data); puts("converter BMP accepted by production C reader"); return 0;
  }
  library_tests(); art_tests(); puts("kid library/artwork tests passed"); return 0;
}
