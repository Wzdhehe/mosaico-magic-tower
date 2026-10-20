# Current Ideas / Iris 0.2 compatibility

The website rejects the published RC4 ABI1 bundle at selection/upload. Its
schema is `/v1`, matching the old workspace and accepted RC2. A newly published
public Ideas package and official utils commit
`73b4a37df6d7354cff339daf11b0aaa58ef582a2` instead use
`esp-iris-system-update/0.2`. The official 0.2 loader reproduces the rejection.
The official migration guide explicitly forbids relabeling old manifests:
wire2, Recovery ABI2, product v2 and the retained test partition must agree.

The released consumer-workspace tag v26.0.2 still pins the old utils commit;
do not mutate that working SDK or any connected device. A separate workspace
at `D:/espbulid/esp-mosaico-iris02` uses the official utils snapshot. Keep the
verified IDF and multi-component rendering engine commits unchanged. Consume
the engine's public CMake integration with the new recovery component path;
do not adopt the unrelated new single-component engine architecture.

- [x] Reproduce rejection with the official 0.2 loader; inspect real migration rules.
- [x] Add red/green export guard tests: current schema accepted, old/missing/unknown
  schemas rejected by default, legacy export only through an explicit flag.
- [x] Rebuild the game with ABI2 integration and unchanged production C sources;
  preserve the game's NVS location and resource layout after the retained prefix.
- [x] Build/inspect using the matching official 0.2 tooling. Check effective
  product contract/ABI/layout plus all component sizes, hashes and placement.
- [x] Review the integration and prepare a clearly identified Ideas-compatible
  bundle, separate legacy download notes and reproducible source metadata.

The complete 21-command campaign suite passes. Official compilation and bundle
inspection pass; all 27 production C/header hashes match RC4. New bundle:
931792 bytes, SHA-256
`4a33b2ca8b74de5cf00b8df315049945026abf3445f86d3079d9723a0cf50371`.
The official plan declares schema 0.2 and minimum recovery version 0.2.0.
Publication uses a separate `v1.8.0-rc.4-iris02` release/tag rather than
rewriting the original RC4 source tag. Its asset manifest records the exact
integration commit; the original release remains a legacy download.

No website upload, device flashing, base-firmware update or flash erasure is
authorized by this task. The user selects and uploads the corrected local file.
Offline build and parser checks do not establish server acceptance or hardware
acceptance; report those boundaries accurately.
