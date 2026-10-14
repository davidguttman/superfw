/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef KID_LIBRARY_H
#define KID_LIBRARY_H

#include <stdbool.h>
#include <stdint.h>
#include "fatfs/ff.h"

#define KID_ROOT "/Games"
#define KID_PATH_BYTES 256
#define KID_TITLE_BYTES 64
#define KID_TITLE_FILE_BYTES 255
#define KID_MAX_GAMES 64
#define KID_MAX_DEPTH 3
#define KID_MAX_DIRS 64
#define KID_MAX_ITEMS 1024
#define KID_ART_WIDTH 160
#define KID_ART_HEIGHT 104
#define KID_ART_COLORS 220
#define KID_ART_PALETTE_FIRST 20
#define KID_ART_WORDS (KID_ART_WIDTH * KID_ART_HEIGHT / 2)

typedef struct {
  union {
    struct { char path[KID_PATH_BYTES]; char title[KID_TITLE_BYTES]; };
    uint16_t words[(KID_PATH_BYTES + KID_TITLE_BYTES) / 2];
  };
} kid_entry;

/* Caller-owned storage: place in mapped scratch SDRAM, not static EWRAM. */
typedef struct {
  kid_entry entries[KID_MAX_GAMES];
  uint16_t count, items, dirs, limited, io_error;
} kid_library;

_Static_assert(sizeof(kid_entry) == KID_PATH_BYTES + KID_TITLE_BYTES,
               "SDRAM entries must be packed halfwords");

/* Small scan workspace MUST be ordinary RAM, not the 16-bit cart bus:
 * FatFs and string parsing perform byte writes here. Can be reused afterward. */
typedef struct {
  DIR dirs[KID_MAX_DEPTH + 1];
  unsigned path_lengths[KID_MAX_DEPTH + 1];
  FILINFO info;
  kid_entry candidate;
  char sidecar[KID_PATH_BYTES];
  char title_data[KID_TITLE_FILE_BYTES + 1];
} kid_scan_scratch;

/* Missing /Games => empty; other I/O errors flagged, usable entries retained. */
void kid_library_scan(kid_library *library, kid_scan_scratch *scratch);

/* Stem replacement; false on malformed paths or insufficient destination. */
bool kid_sidecar_path(const char *rom, const char *extension, char *destination);

/* Only selected artwork is loaded. False => caller renders placeholder.
 * On false, output is unspecified and MUST NOT be displayed/copied to VRAM.
 * Pixels are top-down, two mode-4 indices per little-endian uint16_t.
 * Palette contains KID_ART_COLORS RGB555 colors for indices 20..239.
 */
bool kid_art_load(const char *path, uint16_t pixels[KID_ART_WORDS],
                  uint16_t palette[KID_ART_COLORS]);

#endif
