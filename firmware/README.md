# Downloadable firmware

The current preview release is **1.8.0-rc.4**, playable in **English and Chinese** with an in-game language switch. Download the official [RC4 files and checksums](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.4-iris02). [RC2 remains available](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.2).

- **Current Mosaico Ideas / Vibe Mode 0.2:** `mosaico-magic-tower-1.8.0-rc.4-iris02.irisfw` (schema `esp-iris-system-update/0.2`, Recovery ABI 2).
- **Matching ABI 2 application BIN:** `mosaico-magic-tower-1.8.0-rc.4-iris02.bin`.
- **Legacy Vibe Mode 0.1 only:** the original unsuffixed RC4 `.irisfw` / `.bin`. The current website rejects that old schema; these files do not update the base firmware.
- **Verification:** `SHA256SUMS-iris02.txt` and `release-manifest-iris02.json` in the corrected release; the original release retains its original checksums and manifest.

RC4 includes the battery/charging HUD, outcome music, princess dialogue and combat-panel fixes. Rejected battery samples retry once, then retain the last validated reading for less than 15 seconds before displaying `--`. Long-term battery accuracy and the latest retry behavior still need hardware playtesting; publication is not a claim of completed hardware acceptance. Binaries and local diagnostic/marketing artifacts live outside the tracked source tree.

Use the `-iris02.irisfw` file for the current website. It requires an already matching Vibe Mode 0.2 base. Moving a device from 0.1 to 0.2 is a separate official base-firmware migration, not something this application bundle performs. Do not relabel the old manifest or bypass the compatibility checks. Neither BIN is a whole-flash image; do not write it over retained Vibe Mode or guess offsets.

The preview includes fifty authored floors, Chinese 24px dialogue, the “五十层试炼” title subtitle, wall-colored concealed doors, and an open viewport edge on sides with more map. Save v8 rejects v7 and older map records. Extended hardware and balance playtesting remains open.

Source, build instructions and test coverage are in the root README and [Iris 0.2 build guide](../docs/IRIS02_BUILD.md). Developer package creation uses `tools/package_firmware.py` with the pinned official workspace; it checks the SDK schema before building and inspects the offline system-update bundle without touching a device. A deliberate local legacy export requires `--legacy-v1`.
