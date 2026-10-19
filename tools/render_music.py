#!/usr/bin/env python3
"""Export WAVs by calling the same portable C core used by the ESP adapter.

No Python DSP or alternate score renderer. Requires a C11 compiler and Python's
standard library only. Example: python tools/render_music.py --loops 1
"""
import argparse
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import wave
ROOT = Path(__file__).resolve().parents[1]
RATE = 24000
NAMES = ["title-lantern-at-the-gate", "gate", "water", "forge", "court", "crown", "king", "home", "victory-dawn-above-the-tower", "fallen-one-more-lantern"]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--loops", type=int, default=1)
    parser.add_argument("--volume", type=int, default=100)
    args = parser.parse_args()
    if not 1 <= args.loops <= 10 or not 0 <= args.volume <= 100:
        parser.error("loops must be 1..10, volume must be 0..100")
    dest = ROOT / "artifacts/music"
    dest.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("CC") or shutil.which("gcc") or "gcc"
    library = dest / ("game_music.dll" if os.name == "nt" else "game_music.so")
    command = [cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic", "-shared"]
    if os.name != "nt":
        command.append("-fPIC")
    command += ["-I", str(ROOT / "main"), str(ROOT / "main/game_music.c"), "-o", str(library)]
    temporary = ROOT / "artifacts/tmp"
    temporary.mkdir(parents=True, exist_ok=True)
    build_env = os.environ.copy()
    build_env.update(TEMP=str(temporary), TMP=str(temporary))
    subprocess.run(command, check=True, env=build_env)
    if os.name == "nt" and hasattr(os, "add_dll_directory"):
        dll_directory = os.add_dll_directory(str(Path(shutil.which(cc) or cc).resolve().parent))
    core = ctypes.CDLL(str(library))
    handle = ctypes.c_void_p
    core.game_music_create.argtypes = [ctypes.c_void_p, ctypes.POINTER(handle)]
    core.game_music_create.restype = ctypes.c_bool
    core.game_music_delete.argtypes = [handle]
    core.game_music_set_track.argtypes = [handle, ctypes.c_uint]
    core.game_music_set_track.restype = ctypes.c_bool
    core.game_music_set_volume.argtypes = [handle, ctypes.c_uint]
    core.game_music_set_volume.restype = ctypes.c_bool
    core.game_music_render.argtypes = [handle, ctypes.POINTER(ctypes.c_int16), ctypes.c_size_t]
    core.game_music_render.restype = ctypes.c_bool
    core.game_music_loop_samples.argtypes = [ctypes.c_uint]
    core.game_music_loop_samples.restype = ctypes.c_uint32
    core.game_music_track_name.argtypes = [ctypes.c_uint]
    core.game_music_track_name.restype = ctypes.c_char_p
    core.game_music_memory_bytes.restype = ctypes.c_size_t
    metadata = {"sample_rate": RATE, "channels": 1, "bits": 16, "core_heap_bytes": core.game_music_memory_bytes(),
                "volume": args.volume, "loops": args.loops, "compiler_command": command,
                "timing_note": "Host render includes ctypes/chunk orchestration; not ESP CPU or thermal measurement.", "tracks": []}
    pcm = (ctypes.c_int16 * 240)()
    for track, filename in enumerate(NAMES):
        instance = handle()
        if not core.game_music_create(None, ctypes.byref(instance)):
            raise MemoryError("music core allocation failed")
        try:
            assert core.game_music_set_volume(instance, 0)
            assert core.game_music_set_track(instance, track)
            assert core.game_music_set_volume(instance, args.volume)
            frames = core.game_music_loop_samples(track) * args.loops
            raw = bytearray()
            start = time.perf_counter()
            for offset in range(0, frames, 240):
                count = min(240, frames - offset)
                assert core.game_music_render(instance, pcm, count)
                raw.extend(struct.pack("<" + "h" * count, *pcm[:count]))
            elapsed = time.perf_counter() - start
        finally:
            core.game_music_delete(instance)
        output = dest / (filename + ".wav")
        with wave.open(str(output), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(RATE)
            wav.writeframes(raw)
        values = struct.unpack("<" + "h" * frames, raw)
        rms = math.sqrt(sum(v * v for v in values) / frames)
        row = {"track": track, "name": core.game_music_track_name(track).decode(), "file": str(output.relative_to(ROOT)),
               "duration_seconds": frames / RATE, "peak": max(abs(v) for v in values), "mean": sum(values) / frames,
               "rms": rms, "sha256_pcm": hashlib.sha256(raw).hexdigest(), "host_render_seconds": elapsed}
        metadata["tracks"].append(row)
        print(f"{row['name']}: {row['duration_seconds']:.3f}s peak {row['peak']} RMS {rms:.1f} -> {output}")
    (dest / "preview-metrics.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
if __name__ == "__main__":
    main()
