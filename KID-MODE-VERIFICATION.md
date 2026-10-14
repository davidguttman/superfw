# Kid-mode prototype verification — 2026-10-14

Target: GBA SP + SuperCard SD (`BOARD=sd`). This is a software/emulator-verified
prototype, not a physical-cart compatibility or upstream release claim.

## Independently checked gates

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

Physical GBA SP/SuperCard SD operation, physical SD/media/battery behavior,
commercial-game-specific patch/IGM/DirectSave compatibility, and arbitrary
power-off safety remain unverified. The in-game menu is game-dependent and
reset returns to the firmware actually flashed on the cart. No hardware was
flashed; no release/tag/publish/main-branch merge was performed. Cheats are
outside v1. See [SD setup and controls](KID-MODE-SD.md).
