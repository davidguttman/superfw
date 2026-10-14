# Kid mode: agreed prototype design

Status: design, not implemented or hardware verified.

## Target and scope

SuperFW fork for a six-year-old using a GBA SP and SuperCard SD (`BOARD=sd`). Keep upstream loading, patching and ordinary game-save behavior; simplify the firmware UI rather than replace the engine. Do not bundle commercial games or artwork. No cheats UI or automatic cheat presets in v1. Do not flash hardware as part of development.

## SD-card contract

- Empty `/.superfw/kidmode` file present at startup enables kid mode. Absent means normal SuperFW behavior. This is a runtime override; do not persist altered normal-mode preferences. No parent unlock menu is needed.
- Titles and pictures are sidecars beside each ROM, matched by filename stem within the same directory:

  ```text
  /Games/Mario.gba
  /Games/Mario.txt
  /Games/Mario.bmp
  ```

- `.txt` contains only the friendly display title, not an INI file, ROM path, artwork path, order field, or manifest. A matching title file opts the ROM into the kid library.
- Alphabetical display-title order. Preserve ROM and save filenames. Sidecars in different directories never collide.
- Missing or invalid artwork uses a placeholder. Blank title may fall back to ROM basename; malformed or oversized input must never overrun bounds. Exact bounded title/artwork limits must be documented with implementation.
- Use fixed-size BMP artwork and provide an offline PNG/JPEG-to-BMP preparation tool. Determine dimensions/palette from the actual framebuffer constraints; do not add firmware PNG/JPEG decoding.
- Initial library scope: GBA `.gba` ROMs under `/Games` (including bounded subdirectory traversal). This is an implementation default to document, not a requirement to move or rename save files. Other emulator/platform libraries are outside v1.

## Child experience

- One large picture and short title at a time; left/right selects and A launches.
- No folders, raw file manager, deletion, firmware flashing, technical loading options, cheats, or normal settings reachable through kid-mode controls.
- Empty library gives a friendly non-destructive screen, not an escape into the normal file manager.
- Last-played startup: automatically launch the last game chosen in kid mode, only while it remains a valid member of the kid library. Track kid history separately from normal Recent ROMs.
- Hold B at startup to bypass autostart and show the picture picker.
- First boot, missing/removed game, invalid last-game record, or launch failure falls back to the picker without a launch loop.
- In-game menu exposes Keep playing and Choose another game. The latter preserves the ordinary game save before returning, and skips autostart on that return. Implement a bounded, verifiable one-time handoff; do not infer cold-versus-soft boot from an untested assumption.
- Autostart means normal game boot with its existing save, not savestate resume. Do not promise arbitrary power-off safety.

## Safety and compatibility

- Pending SRAM-save handling precedes autostart; failure must not be hidden by an automatic game launch.
- Keep suitable backups without overwriting the user's normal configuration. No destructive save-format changes.
- Upstream in-game menu support is game-dependent. Return-to-firmware uses the firmware actually flashed on the cart; temporary chain-loading cannot prove the flashed return path.
- Keep the kid-mode-disabled path equivalent to upstream behavior. Review board layout/size limits and in-game payload memory restrictions before adding data.
- Mark physical SuperCard/GBA checks unverified until actually performed. Emulator/host checks are not evidence of physical SD I/O or game-specific patch compatibility.

## Ordered implementation slices

1. Establish unmodified SD build/test baseline and document toolchain and commands.
2. Add bounded sidecar library, title/artwork readers, offline artwork converter, and host tests (red before green). No boot/menu integration in this slice.
3. Add kid-mode detection, picture rendering, navigation, and launch integration, with disabled-mode regressions and captured visual evidence.
4. Add last-played policy/persistence, B override, safe one-time return handoff, and simplified in-game menu. Test failures, stale data, save ordering, and no-loop behavior.
5. Independent spec/code-quality reviews per slice; final tests/build/size gates and SD preparation guide, then a clearly labeled prototype for physical testing.

No merge, release, tag or cart flashing is implied by a prototype build. Preserve upstream notices and license materials.
