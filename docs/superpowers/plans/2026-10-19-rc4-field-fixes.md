# RC4 field fixes

Goal: repair five reported RC3 defects while preserving v8 checkpoints, demand rendering and the official update contract.

The user has requested these fixes and provided both factory backups and a third-party Iris package as battery references. Core implementation remains with the primary agent.

- [x] Reproduce combat-column overlap using the real RGB565 renderer. Separate only the demon king labels and values vertically; ordinary monster panels remain pixel-identical. Check all eight monsters in both languages.
- [x] Author a short bilingual princess exchange after the king falls, then enter the victory page. Allow revisiting her after continuing exploration. Keep outcome music and restart/settings return behavior correct.
- [x] Remove the bypassable 49F yellow door at (15,3). Preserve old v8 opened-door evidence for key accounting without permitting arbitrary flags on empty cells. Verify unopened and opened RC3 saves.
- [x] Raise the fallen melody one octave, move bass into the audible range and reduce its balance; keep the quiet character without suppressing audibility. Render actual DSP WAV and check speaker-band energy, clipping and seams.
- [x] Rebuild battery acquisition around the pinned official BSP initialization/profile verification. Read checked voltage/current/status/operation/SOC/remaining/full/design fields, reject invalid/incoherent snapshots and remove FC-to-100 fabrication. Record pre/post initialization diagnostics and bounded periodic raw telemetry. Never invent voltage-derived percentages or erase/calibrate the device ad hoc.
- [x] Run the affected regressions, full campaign suite, official simulator and firmware build. Report remaining real charging/discharging validation explicitly.

Battery references: factory backup `D:/espbulid/espclaw_backup_v04/factory__0x20000.bin` contains profile verification/update strings; provided `D:/0e257e8f-61a0-4306-979d-d578722d4d1c.irisfw` release 1.0.5 contains the same BSP battery attach/read and BQ27220 profile verification paths. Binary strings establish included components, not an exact reconstruction of application logic.
