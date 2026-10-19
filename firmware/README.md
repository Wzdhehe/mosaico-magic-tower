# Downloadable firmware

The published preview is **1.8.0-rc.2**. Source candidate **1.8.0-rc.4** includes the battery/charging HUD, outcome music, princess dialogue and combat-panel fixes. Its latest battery tolerance change retries rejected samples once and retains the last valid reading for less than 15 seconds; it still requires device acceptance. Download the published `.irisfw` bundle and checksums from [GitHub Releases](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.2). The source candidate is not a new published release; binaries and local diagnostic/marketing artifacts are not committed to this repository.

Use the Iris file for Mosaico Ideas / Vibe Mode. A raw application BIN is also attached for compatible official application-update workflows; it is not a whole-flash image and must never be written over the retained factory/Vibe Mode firmware. Do not guess flash offsets.

The preview includes fifty authored floors, Chinese 24px dialogue, the “五十层试炼” title subtitle, wall-colored concealed doors, and an open viewport edge on sides with more map. Save v8 rejects v7 and older map records. Extended hardware and balance playtesting remains open.

Source, build instructions and test coverage are in the root README. Developer package creation uses `tools/package_firmware.py` with the pinned official workspace; it builds and inspects the offline system-update bundle without touching a device.
