# Downloadable firmware

The current preview is **1.8.0-rc.1**. Download the official `.irisfw` bundle and checksums from [GitHub Releases](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.1).

Use the Iris file for Mosaico Ideas / Vibe Mode. A raw application BIN is also attached for compatible official application-update workflows; it is not a whole-flash image and must never be written over the retained factory/Vibe Mode firmware. Do not guess flash offsets.

The preview includes fifty authored floors, Chinese 24px dialogue and the “五十层试炼” title subtitle. Save v8 rejects v7 and older map records. Extended hardware and balance playtesting remains open.

Source, build instructions and test coverage are in the root README. Developer package creation uses `tools/package_firmware.py` with the pinned official workspace; it builds and inspects the offline system-update bundle without touching a device.
