# Kid-mode SD library and artwork format

This prototype includes the picture picker, last-played startup, the upstream
GBA launch path, and a two-action kid in-game menu. No physical SuperCard SD/GBA
SP verification has been performed. Firmware does not decode PNG or JPEG.

## Quick start

1. Keep your SD contents/saves and original cart firmware backed up. For hardware
   installation, follow the [upstream flashing guide](https://superfw.davidgf.net/docs/install/flash/)
   with the kid-mode marker absent; kid mode deliberately cannot flash firmware.
2. Put a GBA game in `/Games`, for example `Mario.gba`, with `Mario.txt` beside it.
   The text file contains only its friendly title, such as `Super Mario`.
3. Optionally create `Mario.bmp` using the converter below. Missing pictures get
   a placeholder.
4. Create the empty file `/.superfw/kidmode` and reboot. On first boot, choose a
   game with **Left/Right** and **A**. Later boots start the last-played game.
5. Hold **B** during startup for the picker. In games supporting the in-game menu,
   the default **L+R+Start** opens **Keep playing / Choose another game**; use
   **Up/Down** and **A**, or **B** to keep playing.

Removing `/.superfw/kidmode` restores normal SuperFW at the next boot. Games
still require their ordinary in-game saving; this is not savestate resume.

## Enable the picture picker

Create a **regular empty file** at `/.superfw/kidmode` on the card. A directory,
nonempty file, missing marker, or unsuccessful marker stat does not enable it.
Remove the marker to return to the ordinary upstream menu at next boot. The
marker is read once immediately after SD mount, before pending SRAM handling; firmware never writes normal config
files to enable this mode.

The picker shows one 160×104 picture (or placeholder), its friendly title, and
left/right controls. **Left/Right** wraps through the alphabetically sorted
library; **A** plays. Other buttons have no picker actions or normal-menu escape.
Navigation wins over simultaneous A, so a newly selected unseen game never
launches. Empty libraries stay in a friendly screen. Partial/traversal-limited
libraries and read errors retain available games and show a brief notice.

Launch keeps upstream patch selection, save filename calculation, per-ROM RTC,
and supported direct-saving preferences. Runtime autoload/autosave are enabled,
cheats are never read or applied, and backups are at least two (at most upstream
sixteen). These overrides are **not persisted**. Missing Auto/Engine patches are
generated and reloaded silently. An unavailable explicitly selected Database
patch, generation failure, header/read failure, or save preparation error leaves
the child in the picker; no unsafe unpatched fallback is used. A curated ROM
recognized as SuperFW is rejected rather than opening the ordinary firmware UI.
Pending SRAM handling runs before autostart or manual launch. Kid mode checks
all flush/backup-rotation failures and keeps the sentinel and SRAM on failure;
the identical-save shortcut is accepted only after successful open, complete
reads, and close. A genuine missing, different, or declared shorter save permits a checked write,
with the original retained as a backup; premature short reads and comparison I/O
errors block it rather than risking backup rotation. Checked
SRAM writes create missing parent directories (including a fresh `/SAVEGAME`)
before opening the temporary save; failed creation/stat or a non-directory parent
blocks return/launch and retains the pending sentinel and SRAM.
A friendly message blocks launching until the underlying error is resolved and
the firmware boots again. SRAM tests are also skipped on that failure. Normal
marker-absent startup retains its legacy sentinel-removal policy.

A failed ROM load can overwrite the library, artwork, fonts, and patch database.
The selected path is retained in ordinary RAM; firmware rescans and reloads its
artwork on failure. It restores the small resident font pack (non-ASCII titles
may show replacement glyphs until reboot) and does not parse a possibly damaged
PatchDB on further attempts. Auto then uses or generates an engine cache;
explicit Database selection may require a reboot. The normal marker-absent
menu/launch path keeps its upstream behavior except shared progress/patch-read
robustness fixes and bounded pending-sentinel parsing/close handling.

## Startup and safe game choice

Kid history is separate from normal Recent ROMs. `/.superfw/kid-last.bin` is a
bounded **264-byte** record: a little-endian 32-bit `KID1` magic (`0x3144494b`),
a 256-byte NUL-terminated/zero-padded absolute ROM path, and a little-endian
32-bit FNV-1a checksum over all 256 path bytes. Only `/Games/.../Name.gba` paths are
accepted (no traversal components). Before any launch header/preload, patch,
save, or ROM work, firmware creates and successfully closes a separate empty
`/.superfw/kid-attempt` guard, then stages the record with invalid magic. Only
the fully loaded/patched ROM is promoted to a valid record; the guard is removed
only after all record bytes were written and its close succeeded, immediately
before the non-returning game boot. Guard creation/close, record write/close,
and guard removal errors block launch. A full valid record whose close or
cleanup failed remains **ineligible while the attempt guard exists**.

At startup any present, malformed, stale, or unreadable attempt guard suppresses
autostart. Firmware invalidates the remembered record **before** removing the
guard; failed invalidation retains the guard, and an unreadable guard is retained
even if invalidation succeeds. Thus consuming the guard cannot expose a failed
promotion on a following boot. A crash during staging/loading/promotion follows
the same picker-only recovery. This guard is distinct from the successful-return
`kid-skip-once` marker, which intentionally preserves last history.

On power-on, the last game starts only if it is in the freshly scanned bounded
curated library and its regular ROM and title sidecar are still present. Library
read errors prevent autostart. Blank/malformed/oversized title text retains the
same safe basename fallback as the library reader; it is not a title-content
validation requirement. First boot, invalid history, removed ROM/title,
unavailable game, or failed launch displays the picker without repeated launch
attempts. **Hold B through firmware startup** to show the picker without
forgetting the remembered game. Autostart loads the ordinary game save: it is
not savestate resume, autosuspend, or arbitrary power-off safety.

The game-dependent upstream IGM IRQ hook and configured hotkey remain in use
(default **L+R+Start**); not every game supports this hook. In kid mode the menu
shows only **Keep playing** and **Choose another game**. Use **Up/Down** to
select, **A** to confirm; **B** resumes. No cheats, savestates, technical menus,
or reset-without-saving controls are reachable through these controls.

Choose another game refreshes the ordinary SRAM save with at least two backups,
then closes an empty `/.superfw/kid-skip-once` handoff file, removes the already
flushed pending-save sentinel (avoiding a second save/rotation at boot), and
uses the existing reset routine to map the **actually flashed firmware** and
wipe RAM. On any save/rotation/handoff failure it stays in-game, reports the
error, and permits resuming; the pending sentinel is retained. DirectSave games
and no-save games never dump SRAM on this path. The one-shot marker is consumed
at the next kid boot and skips autostart exactly once. Stale/invalid/unreadable
markers also suppress autostart conservatively; failure to consume leaves the
picker rather than a return-to-game loop. Removing the enable marker still
restores normal mode; normal mode does not consume kid history/handoff files.

**Prototype boundary:** returning requires this firmware to be flashed to the
cart. A temporary chain-load or emulator menu test cannot prove the physical
flashed return path, physical SD writes, or game-specific compatibility. No
cart flash is performed by this work.

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
The picker handles both empty and partial libraries explicitly.

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
needs **16640 pixel bytes + 440 palette bytes**, also halfword-written. Combined
picker cartridge scratch is **37570 bytes**, overlaid within the first 2MiB at
`0x08000000`, with font storage left at `0x08e80000`. There are
no large static firmware allocations. The small `kid_scan_scratch` structure
(**1456 bytes** with the current ARM FatFs configuration) must reside in
ordinary EWRAM: FatFs/string code writes bytes there, which cannot
be directed at the cartridge's 16-bit bus. Reuse this scratch after scanning.
FatFs `FIL` and a 160-byte artwork row buffer are temporary ordinary-RAM stack
objects. Caller owns mapping/lifetime of these buffers; normal ROM loading can
overwrite them after the selected path has been copied to ordinary RAM.

The marker/input/render tests use production `kid_mode.c`; the launch harness
includes the actual `menu.c` launch functions with engine/I/O doubles and
simulates cartridge scratch overwrite on load failure. It checks cache refresh,
patch-policy failures, SRAM-save ordering, RTC/directsave preferences, cheats
zero, and actual IRQ hook availability. Its ASan settings disable global roots
for section GC and fake-stack instrumentation for upstream GCC nested callbacks.
The library/input/render tests keep their ordinary ASan/UBSan instrumentation.

Standalone ARM7 Thumb compilation with `-Os -fno-builtin` measured **2178 bytes
of text, zero data/BSS** for this reader module. Compiler stack estimates were
48 bytes for scanning plus 712 bytes for its title-reading helper, and 856 bytes
for artwork loading, excluding FatFs callees. These are estimates, not final
linked/compressed firmware size or hardware memory-use measurements.

Run the production-C reader/library checks and converter interoperability with:

```sh
make -C tests kid_tests kid_return_tests loader_tests
```

This focused ASan/UBSan target uses real `src/kid_library.c` and the real FatFs
types with mocked read-only file/directory operations. It does not require
`lcov` or change the upstream `all` target. Host verification is not evidence
of physical SD I/O, SDRAM mapping, mode-4 rendering, or game compatibility.


`kid_return_tests` exercises the real shared history/handoff protocol, boot
policy, checked SRAM writer/backup rotation, and two-action IGM input/actions
with injected open/read/write/close/rename/unlink failures. `loader_tests` also
checks that kid history promotion happens only after successful ROM I/O and
before reset, that promotion errors block reset, and that the runtime flag and
DirectSave/no-save patterns are patched into the actual payload header.
The production history tests specifically persist all 264 valid record bytes,
fail close and record deletion, and verify that repeated boots remain in the
picker with retained history ineligible. The checked-save tests cover matching
bytes plus failed comparison close, missing save directories and failed mkdir,
while verifying SRAM preservation and unchanged normal-mode comparison behavior.

SD build gate (ARM GNU 14.2 toolchain on PATH):

```sh
uv run --no-project --python 3.12 -- make BOARD=sd COMPRESSION_RATIO=10
```

The linker enforces the 512KiB SD flash image limit, firmware EWRAM/IWRAM+stack
limits, IGM 60KiB EWRAM/4KiB IWRAM limits, and unchanged fixed trampoline offsets.

Independent software/emulator evidence and limits: [verification report](KID-MODE-VERIFICATION.md).
