# Downloadable firmware

The current preview release is **1.8.0-rc.4**, playable in **English and Chinese** with an in-game language switch. Download the official [RC4 files and checksums](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.4). [RC2 remains available](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.2).

- **Mosaico Ideas / Vibe Mode:** `mosaico-magic-tower-1.8.0-rc.4.irisfw`.
- **Compatible official application-update workflows:** `mosaico-magic-tower-1.8.0-rc.4.bin`.
- **Verification:** `SHA256SUMS.txt` and `release-manifest.json`.

RC4 includes the battery/charging HUD, outcome music, princess dialogue and combat-panel fixes. Rejected battery samples retry once, then retain the last validated reading for less than 15 seconds before displaying `--`. Long-term battery accuracy and the latest retry behavior still need hardware playtesting; publication is not a claim of completed hardware acceptance. Binaries and local diagnostic/marketing artifacts live outside the tracked source tree.

Use the Iris file for Mosaico Ideas / Vibe Mode. A raw application BIN is also attached for compatible official application-update workflows; it is not a whole-flash image and must never be written over the retained factory/Vibe Mode firmware. Do not guess flash offsets.

The preview includes fifty authored floors, Chinese 24px dialogue, the “五十层试炼” title subtitle, wall-colored concealed doors, and an open viewport edge on sides with more map. Save v8 rejects v7 and older map records. Extended hardware and balance playtesting remains open.

Source, build instructions and test coverage are in the root README. Developer package creation uses `tools/package_firmware.py` with the pinned official workspace; it builds and inspects the offline system-update bundle without touching a device.
