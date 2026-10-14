# Kid-mode prototype verification — 2026-10-14

The evidence below separates the **0.22 layout revision** from the **0.21
baseline**. The user performed 0.21 hardware smoke tests. Prototype **0.22**
has independent host/build and SuperCard-aware emulator evidence, but is
**not hardware verified**.

Target: GBA SP + SuperCard SD (`BOARD=sd`). This is a software/emulator-verified
prototype, not a physical-cart compatibility or upstream release claim.

## Independently checked 0.22 gates

- Red first: the legacy-centering assertion failed on 0.21; converter square-size
  assertion failed; new wrapping helper tests failed to compile before the helper
  existed. Logs are ignored local `tests/kid-{fullheight,converter,wrap}-red.log`.
- Parent independently ran `make -C tests kid_tests kid_return_tests loader_tests`
  with ASan/UBSan successfully,
  including both BMP orientations/dimensions, malformed sizes/indices, converter
  crop/EXIF/transparency roundtrips, real-font wrapping, production menu panel
  guards, unchanged input, launch, history, checked save and return regressions.
- ARM GNU 14.2.1 SD build with Python 3.12 and `COMPRESSION_RATIO=10` passes:
  **518656 / 524288 bytes** (5632 bytes free), firmware EWRAM **228372 / 257024**,
  IWRAM **11224 / 32768**; IGM remains **50496 / 61440** EWRAM and **2528 / 4096**
  IWRAM. Linker gates pass. Parent also checked the SD image length/alignment,
  GBA header checksum and embedded firmware checksum. Final packaging rebuilds
  with the final commit slug and checks it against the matching source archive.
- Parent rebuilt and executed all six upstream host tests successfully using
  their original recipes, excluding only unavailable coverage-report commands.
  No coverage percentage is claimed.
- Parent regenerated all eleven personal covers from original images, not the
  old letterboxed BMPs; each passed the production C artwork reader. A personal
  artwork upgrade is separate from the firmware/source package; no ROMs or saves
  are distributed in either package.
- Using the same SuperCard-aware gpSP core described below and disposable FAT16
  images, parent directly inspected actual firmware captures for all eleven
  covers. Verified full-height left artwork, wrapped right-panel titles, no
  arrows, and Left/Right navigation. Additional inspected captures cover legacy
  160x104 artwork, malformed-art placeholders, empty-library help, a pending-save
  error confined to the right panel, and the marker-absent normal menu.
- No live SD contents or installed firmware were changed by these checks.
  New 0.22 physical-cart operation remains unverified.
- Separate sequential spec and code-quality reviews found no actionable
  blockers. The quality reviewer independently ran sanitizer-backed artwork,
  UTF-8 wrapping, production-layout and converter interoperability checks in
  a temporary directory; all passed. Parent inspected the scoped final diff.

## Independently checked 0.21 gates

- `make -C tests kid_tests kid_return_tests loader_tests`: production library,
  artwork/converter interoperability, marker/render/input, launch, loader,
  history/attempt/return protocol, actual pending-startup dispatch, checked saves,
  and in-game SD mapping/action tests passed. ASan/UBSan are enabled; documented
  narrowly scoped instrumentation exceptions cover existing nested callbacks
  and an unrelated upstream signed header-checksum shift.
- All six upstream host tests rebuilt and executed successfully: cimpl, util,
  utf_util, sha256, cheats, crc. Coverage-report recipes were omitted because
  `lcov`/`genhtml` are unavailable; no coverage percentage is claimed.
- ARM GNU 14.2.Rel1 / GCC 14.2.1, Python 3.12 via `uv`:

  ```sh
  uv run --no-project --python 3.12 -- make BOARD=sd COMPRESSION_RATIO=10
  ```

- SD firmware is **518144 / 524288 bytes**, leaving **6144 bytes**. Firmware
  EWRAM is **227916 / 257024**, IWRAM **11224 / 32768**. In-game payload EWRAM
  is **50496 / 61440**, IWRAM **2528 / 4096**. Linker stack/payload/header-offset
  gates passed. Firmware length/alignment, GBA header checksum, and embedded
  firmware checksum were independently validated.
- Sequential independent spec and code-quality reviews resolved all actionable
  findings, including disabled SD access, failed history-close eligibility,
  matching-save close errors, fresh save-directory creation, and smaller-save
  compatibility. Whitespace/diff checks passed.

## SuperCard-aware emulator evidence

Used upstream gpSP `supercard-emu-dirty` at
`dbaf87b99547e538df62f330691693bd6d5f1c88`, interpreter core, disposable FAT16
images, and synthetic homebrew/artwork (no commercial ROMs). CPU-only GBA
emulation was not substituted for cart emulation.

Directly inspected captured frames for the picture/title picker, navigation,
placeholder/invalid art, empty library, failed launch, normal mode with marker
removed, last-game autoboot, hold-B override, stale membership, attempt-guard
recovery, and the two-action in-game menu.

The synthetic SRAM game writes byte `0x4b` and changes from green to blue when
that byte is loaded from its ordinary save. On a fresh SD image, Choose another
game created `/SAVEGAME`, wrote a 128KiB save, returned to the picker without an
autostart loop, and consumed the return marker. A subsequent cold boot restored
the save (blue frame). A second return preserved a matching `.1.sav` backup.
Malformed pending-save data blocked both autostart and A launch and remained
byte-for-byte intact.

The emulator maps SD images privately. Verification explicitly captured the
mapped SD bytes before exiting and used that captured image for subsequent cold
boots; persistence was not inferred from unchanged backing files.

## Not verified / not performed

Beyond the user's prior 0.21 hardware smoke tests, physical SD/media/battery behavior,
commercial-game-specific patch/IGM/DirectSave compatibility, and arbitrary
power-off safety remain unverified. The in-game menu is game-dependent and
reset returns to the firmware actually flashed on the cart. No hardware was
flashed by the development/verification work; no release/tag/publish/main-branch
merge was performed. Cheats are
outside v1. See [SD setup and controls](KID-MODE-SD.md).
