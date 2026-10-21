# Iris 0.2 integration and export

Updated 2026-10-20. The current global Mosaico Ideas upload accepts
`esp-iris-system-update/0.2`; the original RC4 export used `/v1`.
These are different product contracts, not interchangeable format labels.

## Pinned dependencies

Place this project at `projects/magic_tower` in an isolated official workspace.
Keep the workspace's relative `submodule/` paths:

| Dependency | Revision |
|---|---|
| `esp-mosaico/esp-mosaico-utils` | `73b4a37df6d7354cff339daf11b0aaa58ef582a2` |
| ESP-IDF | `7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe` |
| Raylib Lite Engine | `ca3200a6b7f63e2bdea07851e19fdbd3a42eb742` |
| ESP-Mosaico BSP | `a2c985ad698b3f04a21c63e006b43e7e948bc1d7` |
| GSP compiler / runtime | `0.6.1` / `1.5.1` |

Use the existing engine display-ownership patch referenced in the root README.
The public engine CMake API supports the new recovery component directory;
the game keeps its verified multi-component rendering engine. The official
tools' `cmake/gsp_compiler.cmake` resolves the matching compiler before IDF.
Iris 0.2 requires `CONFIG_TINYUSB_CDC_COUNT=2` for its separate console and
data interfaces; the project defaults explicitly select this. When reusing a
generated configuration, verify the effective value rather than assuming a
defaults-file change overwrote it.
There are no changes to production game C sources, map data or music for this
packaging migration.

An official project initializer may create Hello World `app_hello` and
`app_display` components. They are not part of this game. Use this repository's
file layout; do not retain those generated example components or add their
unused `ui_apps` resource partition. The game's generated GSP canvas and all
resources remain embedded in the application.

## Offline build

Apply [`idf-i2c-device-cleanup-lock.patch`](../tools/patches/idf-i2c-device-cleanup-lock.patch)
at the pinned ESP-IDF root before building the background battery startup fix.
It moves the device-removal busy check inside the existing bus mutex, so a
failed gauge attachment waits for an in-flight synchronous touch transfer
instead of abandoning its allocation. It preserves rejection of outstanding
asynchronous work and releases the mutex on that error path. No bus reset or
battery profile change is introduced. Validate the actual patched function with
`IDF_PATH=<pinned-idf> python tests/test_i2c_cleanup.py`.

Run the pinned utilities entry point with the workspace explicitly selected:

```text
python submodule/esp-mosaico-utils/mosaico-tools/mosaico.py --workspace . game build --project projects/magic_tower --idf-path <matching-idf>
python projects/magic_tower/tools/package_firmware.py --workspace . --project projects/magic_tower --idf-path <matching-idf>
```

The packaging helper invokes the official `system-update-bundle` target and
inspector. It checks the SDK's declared schema before building, checks the
exported plan afterward, and validates the effective application configuration.
See `build/release-bundle-plan.json` for the result and log location.
The explicit `--legacy-v1` flag is only for a separate old ABI 1 workspace;
it does not make this ABI 2 source compatible with an old base.

Required effective configuration:

```text
ESP_IRIS_RECOVERY_ABI=2
ESP_IRIS_PRODUCT_CONTRACT=esp-mosaico/v2
ESP_IRIS_LAYOUT_ID=mosaico-retained-test-2m-v2
```

The retained prefix uses `vibe_mode` (`app/test`) at `0x20000`, size `0x1c0000`.
The application bundle does not contain that image or a bootloader. Game NVS
stays at `0x200000`, size `0x10000`; `main_app` (`app/ota_0`) starts at `0x210000`.
The game still uses v8 checkpoints, with unchanged game storage keys and bytes.
This does not promise checkpoint preservation across a separate base-firmware
recovery procedure, which may erase flash.

## Compatibility and evidence

### Windows host connection

The pinned 0.2 host tools assume that pySerial exposes CDC interface names.
Windows usbser may omit those names, causing both functions to be classified
as control links and installation to fail before writing. The local fix is
[`iris-0.2-windows-cdc.patch`](../tools/patches/iris-0.2-windows-cdc.patch),
applied at the pinned utilities root. It derives the known Iris functions from
Windows USB MI numbers (0 = console, 2 = data), checks VID/PID, and rejects
conflicting or unknown identities. It does not infer a role from COM numbering
or relax the live device/boot binding checks.

The six Windows regression tests and related passive enumeration/native-flash
tests pass: 53 passed, 7 skipped on the development host. Actual control and
data links were bound to the same device and Boot ID; the official System
Update then installed the unchanged RC4 Iris 0.2 release and returned healthy.
This patch changes host tooling only, not the release bundle or device protocol.

Use the `-iris02` release files only with Vibe Mode 0.2 / Recovery ABI 2.
The original unsuffixed RC4 files remain legacy ABI 1 files. The official
migration explicitly requires rebuilding applications and bundles; do not
edit a manifest string to circumvent it. Application packaging and base
firmware installation are separate tasks.

Offline compilation, contract validation and inspection do not prove website
upload acceptance, device health or long-term battery accuracy. No device or
base firmware is changed by the commands above. Follow the official migration
guide separately before installing on a device with an incompatible base.

Official sources at the pinned revision:

- [Tooling migration](https://github.com/esp-mosaico/esp-mosaico-utils/blob/73b4a37df6d7354cff339daf11b0aaa58ef582a2/mosaico-tools/docs/migration-0.2.md)
- [Vibe Mode migration](https://github.com/esp-mosaico/esp-mosaico-utils/blob/73b4a37df6d7354cff339daf11b0aaa58ef582a2/esp-mosaico-vibe-mode/docs/migration-0.2.md)
- [Application integration](https://github.com/esp-mosaico/esp-mosaico-utils/blob/73b4a37df6d7354cff339daf11b0aaa58ef582a2/mosaico-tools/docs/application-integration.md)
