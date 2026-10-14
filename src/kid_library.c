/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kid_library.h"
#include <string.h>

#pragma GCC optimize ("Os")

static unsigned lower(unsigned c) { return c >= 'A' && c <= 'Z' ? c + ('a'-'A') : c; }
static int folded_compare(const char *a, const char *b) {
  while (*a && lower((uint8_t)*a) == lower((uint8_t)*b)) { a++; b++; }
  return (int)lower((uint8_t)*a) - (int)lower((uint8_t)*b);
}

bool kid_sidecar_path(const char *rom, const char *extension, char *destination) {
  size_t length = strlen(rom);
  if (length < 5 || length >= KID_PATH_BYTES || folded_compare(rom + length - 4, ".gba") || strlen(extension) != 3)
    return false;
  memcpy(destination, rom, length - 3);
  memcpy(destination + length - 3, extension, 4);
  return true;
}

static bool whitespace(unsigned c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

/* Validate canonical UTF-8 first, so truncation can never expose a broken
 * sequence to upstream font decoding. Reject controls/NUL; accept whitespace. */
static unsigned character_bytes(const uint8_t *data, unsigned remaining) {
  unsigned n, code, minimum;
  if (data[0] < 0x80) return data[0] && (data[0] >= 32 || whitespace(data[0])) && data[0] != 127 ? 1 : 0;
  if (data[0] >= 0xc2 && data[0] <= 0xdf) { n=2; code=data[0]&31; minimum=0x80; }
  else if (data[0] >= 0xe0 && data[0] <= 0xef) { n=3; code=data[0]&15; minimum=0x800; }
  else if (data[0] >= 0xf0 && data[0] <= 0xf4) { n=4; code=data[0]&7; minimum=0x10000; }
  else return 0;
  if (n > remaining) return 0;
  for (unsigned i=1; i<n; i++) {
    if ((data[i]&0xc0) != 0x80) return 0;
    code = (code << 6) | (data[i]&63);
  }
  return code >= minimum && code <= 0x10ffff && !(code >= 0x80 && code <= 0x9f) &&
         !(code >= 0xd800 && code <= 0xdfff) ? n : 0;
}

static void basename_title(kid_entry *entry) {
  const char *name = strrchr(entry->path, '/') + 1;
  unsigned length = strlen(name) - 4, out = 0;
  while (out < length) {
    unsigned n = character_bytes((const uint8_t *)name + out, length - out);
    if (!n || out + n >= KID_TITLE_BYTES) break;
    memcpy(entry->title + out, name + out, n); out += n;
  }
  entry->title[out] = 0;
  if (!out) strcpy(entry->title, "Game");
}

static void normalize_title(kid_entry *entry, const char *text, unsigned length) {
  unsigned start = 0;
  if (length >= 3 && !memcmp(text, "\xef\xbb\xbf", 3)) start = 3;
  for (unsigned i=start; i<length;) {
    unsigned n=character_bytes((const uint8_t *)text+i, length-i);
    if (!n) return; /* Keep safe basename on malformed data. */
    i += n;
  }
  char normalized[KID_TITLE_BYTES];
  unsigned out=0; bool space=false;
  for (unsigned i=start; i<length;) {
    unsigned n=character_bytes((const uint8_t *)text+i, length-i);
    if (whitespace((uint8_t)text[i])) { space=out != 0; i++; continue; }
    if (out + n + (space ? 1 : 0) >= KID_TITLE_BYTES) break;
    if (space) normalized[out++]=' ';
    memcpy(normalized+out,text+i,n); out+=n; i+=n; space=false;
  }
  if (out) { normalized[out]=0; memcpy(entry->title,normalized,out+1); }
}

static bool read_title(kid_scan_scratch *scratch, uint16_t *io_error) {
  /* Establish opt-in before opening: an unreadable existing file still
   * selects its ROM, but a missing sidecar or directory never does. */
  FRESULT result=f_stat(scratch->sidecar,&scratch->info);
  if (result != FR_OK) {
    if (result != FR_NO_FILE && result != FR_NO_PATH) *io_error=true;
    return false;
  }
  if (scratch->info.fattrib & AM_DIR) { *io_error=true; return false; }
  basename_title(&scratch->candidate);
  FIL file;
  result=f_open(&file,scratch->sidecar,FA_READ);
  if (result != FR_OK) {
    if (result != FR_NO_FILE && result != FR_NO_PATH) *io_error=true;
    return result != FR_NO_FILE && result != FR_NO_PATH;
  }
  if (f_size(&file) <= KID_TITLE_FILE_BYTES) {
    UINT read=0, length=(UINT)f_size(&file);
    if (f_read(&file,scratch->title_data,length,&read) == FR_OK && read == length)
      normalize_title(&scratch->candidate,scratch->title_data,length);
    else *io_error=true;
  }
  if (f_close(&file) != FR_OK) *io_error=true;
  return true;
}

static int entry_compare(const kid_entry *a, const kid_entry *b) {
  int order=folded_compare(a->title,b->title);
  if (!order) order=folded_compare(a->path,b->path);
  return order ? order : strcmp(a->path,b->path);
}

static void copy_entry(kid_entry *destination, const kid_entry *source) {
  /* Mapped cartridge SDRAM does not support byte writes. */
  volatile uint16_t *out=destination->words;
  for (unsigned i=0; i<sizeof(source->words)/sizeof(source->words[0]); i++) out[i]=source->words[i];
}

/* Keep alphabetically first 64 of the visited subset, irrespective of FAT
 * enumeration order. No qsort, heap, or per-entry sort keys needed. */
static void insert_entry(kid_library *library, const kid_entry *entry) {
  unsigned position=0;
  while (position < library->count && entry_compare(&library->entries[position],entry) <= 0) position++;
  if (library->count == KID_MAX_GAMES) library->limited=true;
  else library->count++;
  if (position >= KID_MAX_GAMES) return;
  for (unsigned i=library->count-1; i>position; i--) copy_entry(&library->entries[i],&library->entries[i-1]);
  copy_entry(&library->entries[position],entry);
}

void kid_library_scan(kid_library *library, kid_scan_scratch *scratch) {
  library->count=library->items=library->dirs=library->limited=library->io_error=0;
  memset(&scratch->candidate,0,sizeof(scratch->candidate));
  FRESULT result=f_opendir(&scratch->dirs[0],KID_ROOT);
  if (result != FR_OK) { library->io_error=result != FR_NO_PATH && result != FR_NO_FILE; return; }
  strcpy(scratch->candidate.path,KID_ROOT);
  scratch->path_lengths[0]=strlen(KID_ROOT);
  unsigned depth=0; library->dirs=1;
  for (;;) {
    if (library->items == KID_MAX_ITEMS) { library->limited=true; break; }
    result=f_readdir(&scratch->dirs[depth],&scratch->info);
    if (result != FR_OK || !scratch->info.fname[0]) {
      if (result != FR_OK) library->io_error=true;
      if (f_closedir(&scratch->dirs[depth]) != FR_OK) library->io_error=true;
      if (!depth) return;
      depth--; continue;
    }
    library->items++;
    const char *name=scratch->info.fname;
    if (!strcmp(name,".") || !strcmp(name,"..") || strchr(name,'/') || strchr(name,'\\')) continue;
    unsigned base=scratch->path_lengths[depth], length=strlen(name);
    if (base + 1 + length >= KID_PATH_BYTES) { library->limited=true; continue; }
    scratch->candidate.path[base]='/';
    memcpy(scratch->candidate.path+base+1,name,length+1);
    if (scratch->info.fattrib & AM_DIR) {
      if (depth == KID_MAX_DEPTH || library->dirs == KID_MAX_DIRS) { library->limited=true; continue; }
      result=f_opendir(&scratch->dirs[depth+1],scratch->candidate.path);
      if (result != FR_OK) { library->io_error=true; continue; }
      depth++; library->dirs++; scratch->path_lengths[depth]=base+1+length;
    } else if (kid_sidecar_path(scratch->candidate.path,"txt",scratch->sidecar) && read_title(scratch,&library->io_error))
      insert_entry(library,&scratch->candidate);
  }
  do { if (f_closedir(&scratch->dirs[depth]) != FR_OK) library->io_error=true; } while (depth--);
}

static unsigned u16(const uint8_t *p) { return p[0] | ((unsigned)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return u16(p) | ((uint32_t)u16(p+2)<<16); }
static bool read_exact(FIL *file, void *data, unsigned size) {
  UINT read; return f_read(file,data,size,&read) == FR_OK && read == size;
}

bool kid_art_load(const char *path, uint16_t pixels[KID_ART_WORDS], uint16_t palette[KID_ART_COLORS]) {
  FIL file;
  if (f_open(&file,path,FA_READ) != FR_OK) return false;
  bool valid=false;
  uint8_t header[54], row[KID_ART_WIDTH];
  if (!read_exact(&file,header,sizeof(header))) goto done;
  uint32_t colors=u32(header+46), offset=u32(header+10), height=u32(header+22);
  const unsigned image_bytes=KID_ART_WIDTH*KID_ART_HEIGHT;
  /* Narrow BMP dialect deliberately rejects arbitrary DIBs, gaps, RLE,
   * 256-entry palettes, and trailing data. Arithmetic is bounded first. */
  if (header[0]!='B' || header[1]!='M' || u32(header+6) || u32(header+14)!=40 ||
      u32(header+18)!=KID_ART_WIDTH || (height != KID_ART_HEIGHT && height != (uint32_t)-KID_ART_HEIGHT) ||
      u16(header+26)!=1 || u16(header+28)!=8 || u32(header+30) ||
      !colors || colors>KID_ART_COLORS || offset != 54+4*colors ||
      f_size(&file) != offset+image_bytes || u32(header+2) != offset+image_bytes ||
      (u32(header+34) && u32(header+34)!=image_bytes) || u32(header+50)>colors) goto done;
  /* BGR0 requires the reserved byte to be zero. Reuse the row workspace;
   * reject the entire palette before touching outputs. */
  for (unsigned i=0; i<colors; i++)
    if (!read_exact(&file,row,4) || row[3]) goto done;
  /* Validate every index before touching outputs. Outputs remain scratch:
   * a subsequent I/O failure may still leave partial data, never VRAM. */
  if (f_lseek(&file,offset) != FR_OK) goto done;
  for (unsigned y=0; y<KID_ART_HEIGHT; y++) {
    if (!read_exact(&file,row,sizeof(row))) goto done;
    for (unsigned x=0; x<KID_ART_WIDTH; x++) if (row[x]>=colors) goto done;
  }
  if (f_lseek(&file,54) != FR_OK) goto done;
  for (unsigned i=0; i<KID_ART_COLORS; i++) {
    uint8_t color[4]={0};
    if (i<colors && (!read_exact(&file,color,4) || color[3])) goto done;
    palette[i]=(color[2]>>3) | ((color[1]>>3)<<5) | ((color[0]>>3)<<10);
  }
  if (f_lseek(&file,offset) != FR_OK) goto done;
  for (unsigned y=0; y<KID_ART_HEIGHT; y++) {
    if (!read_exact(&file,row,sizeof(row))) goto done;
    unsigned out_y=height == KID_ART_HEIGHT ? KID_ART_HEIGHT-1-y : y;
    for (unsigned x=0; x<KID_ART_WIDTH; x+=2) {
      /* Recheck on second pass too; never output reserved UI indices. */
      if (row[x]>=colors || row[x+1]>=colors) goto done;
      pixels[out_y*(KID_ART_WIDTH/2)+x/2]=(row[x]+KID_ART_PALETTE_FIRST) | ((row[x+1]+KID_ART_PALETTE_FIRST)<<8);
    }
  }
  valid=true;
done:
  if (f_close(&file) != FR_OK) valid=false;
  return valid;
}
