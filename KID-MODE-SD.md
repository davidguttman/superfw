# Kid-mode SD library and artwork format

This first prototype slice implements readers and host verification only. It
does **not** yet enable kid mode, change boot/menu behavior, launch games, track
last-played games, or alter in-game controls. No physical SuperCard SD/GBA SP
verification has been performed. Firmware does not decode PNG or JPEG.

## Curated library

Place a title text file next to each desired GBA ROM under `/Games`:

```text
/Games/Mario.gba
/Games/Mario.txt
/Games/Mario.bmp
/Games/Puzzles/Mario.gba
/Games/Puzzles/Mario.txt
```

The `.txt` sidecar opts its same-directory `.gba` into the library. FatFs's
case-insensitive matching permits `.GBA` and `.TXT`. Same stems in different
folders remain separate entries. A `.txt` without a matching ROM does nothing;
other ROM extensions are excluded. ROM/save names and contents are never changed.
Readers use read-only FatFs calls only.

- Root is `/Games`; descend at most **three directory levels** below it.
- At most **64 directories**, including `/Games`, and **1024 directory items**
  are visited. This is a bounded depth-first FAT enumeration. If traversal is
  limited, the subset visited depends on the SD's directory enumeration order.
- Paths occupy **256 bytes**, including NUL; longer paths are skipped.
- Keep at most **64 games**, alphabetically first among the visited subset.
  Sort compares UTF-8 bytes with ASCII case folding (not locale/accent
  transliteration); equal titles break ties by case-folded full ROM path, then
  exact path. ASCII titles have ordinary case-insensitive alphabetical order.
- Title text files are read only when at most **255 bytes**. Output titles are
  at most **63 UTF-8 bytes**, plus NUL, truncated only at character boundaries.
- Optional UTF-8 BOM is removed. Trim ASCII whitespace and collapse internal
  whitespace (including line breaks) into a single space. Blank, oversized,
  invalid UTF-8, NUL/control-containing (including U+0080–U+009F), or unreadable
  text falls back to the ROM basename, safely UTF-8 truncated; an unusable
  basename becomes `Game`.
- Missing `/Games` produces an empty library. Errors reading an existing
  directory/title are flagged (`io_error`), with usable entries retained.
  A title-sidecar directory is not an opt-in file. No writes are performed.

`kid_library.limited` reports a depth/path/directory/item/game bound hit.
The future picker should handle both empty and partial libraries explicitly.

## Artwork

Generate sidecars from an image you have permission to use:

```sh
uv run tools/kid-art.py picture.png /path/to/SD/Games/Mario.bmp
```

The tool also accepts JPEG and ordinary Pillow-readable images. It honors EXIF
orientation, composites transparency on black, fits without cropping into
**160×104**, letterboxes on black, and quantizes to at most **220 colors**. It
writes a deliberately narrow BMP dialect; BMPs saved by a general image editor
may not be compatible. No commercial ROMs/artwork are bundled.

Accepted files are `BM`, 40-byte BITMAPINFOHEADER, exactly 160×104 (positive
bottom-up or negative top-down height), one plane, 8 bits/pixel, BI_RGB/no
compression, 1–220 explicitly declared BGR0 palette entries, pixel offset exactly
`54 + 4 * palette_count`, and exactly 16640 pixel bytes. Header file size must
match actual length; image size is zero or 16640; important-color count must not
exceed palette count. No trailing data, other DIB formats, RLE, 256-entry default
palette, or out-of-palette pixels are accepted. Width is already four-byte
aligned, so rows have no padding. Maximum accepted size is **17574 bytes**.

The reader validates headers/lengths and all pixel indices before touching
outputs, then loads only the selected picture. Output is top-down packed
halfwords (two indices per word), remapped to GBA palette **20–239**. Entries
0–19 and 240–255 remain reserved for existing logo/UI colors; BGR8 colors become
RGB555. Missing/bad artwork returns `false`, signaling the future picker to use
a placeholder. On *any* failure outputs are unspecified: they are scratch data
and must not be copied to VRAM or displayed until the reader returns `true`.

## Memory and verification

The fixed library occupies **20490 bytes**, with explicit halfword entry copies
and 16-bit metadata suitable for mapped cartridge scratch SDRAM. Selected artwork
needs **16640 pixel bytes + 440 palette bytes**, also halfword-written. There are
no large static firmware allocations. The small `kid_scan_scratch` structure
(**1456 bytes** with the current ARM FatFs configuration) must reside in
ordinary EWRAM: FatFs/string code writes bytes there, which cannot
be directed at the cartridge's 16-bit bus. Reuse this scratch after scanning.
FatFs `FIL` and a 160-byte artwork row buffer are temporary ordinary-RAM stack
objects. Caller owns mapping/lifetime of these buffers; normal ROM loading can
overwrite them after the selected path has been copied to ordinary RAM.

Standalone ARM7 Thumb compilation with `-Os -fno-builtin` measured **2178 bytes
of text, zero data/BSS** for this reader module. Compiler stack estimates were
48 bytes for scanning plus 712 bytes for its title-reading helper, and 856 bytes
for artwork loading, excluding FatFs callees. These are estimates, not final
linked/compressed firmware size or hardware memory-use measurements.

Run the production-C reader/library checks and converter interoperability with:

```sh
make -C tests kid_tests
```

This focused ASan/UBSan target uses real `src/kid_library.c` and the real FatFs
types with mocked read-only file/directory operations. It does not require
`lcov` or change the upstream `all` target. Host verification is not evidence
of physical SD I/O, SDRAM mapping, mode-4 rendering, or game compatibility.
