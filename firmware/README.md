# Downloadable firmware

Current preview: **1.8.0-rc.5**, with switchable **English and Chinese**. Download [RC5 firmware and checksums](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.5).

- **Mosaico Ideas installation:** `mosaico-magic-tower-1.8.0-rc.5.irisfw`, schema `esp-iris-system-update/0.2`, Recovery ABI 2.
- **Matching application BIN:** `mosaico-magic-tower-1.8.0-rc.5.bin`. This is not a whole-flash image.
- **Verification:** `SHA256SUMS.txt` and `release-manifest.json` in the release.

First install official **system v0.5.0**, including **Vibe Mode 0.2 / Recovery ABI 2**, with the [official firmware updater](https://mosaico.espressif.com/zh/firmware-update/). An old RC2 installation on a legacy base cannot directly install this ABI 2 package. This application does not migrate the base; official full-system installation may clear existing data.

RC5 starts battery initialization after the first frame and automatically retries failures. The exact manual AI-key Vibe/poweroff/normal-boot route passed on a development build of this fix; failed initialization recovered at about 20 seconds. Intermittent black-screen boots and long-term gauge accuracy still require observation. Maps, music, operation-driven rendering and v8 checkpoints are unchanged from RC4.

[Previous RC4 for Iris 0.2](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.4-iris02), [original legacy ABI 1 RC4](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.4) and [RC2](https://github.com/Wzdhehe/mosaico-magic-tower/releases/tag/v1.8.0-rc.2) remain available. Do not relabel an old schema or write an application BIN over retained Vibe Mode.

Save v8 remains compatible; v7 and older records are rejected. Game-record compatibility does not promise preservation across separate full-system migration. Diagnostic captures and Ideas submission materials remain outside tracked source. See the [Iris 0.2 build guide](../docs/IRIS02_BUILD.md) for pinned dependencies, the IDF cleanup-lock patch and official offline export.
