#!/usr/bin/env python3
"""
wav2c.py  ─ Convert a mono WAV impulse-response to a C header for Daisy

Example:
    python wav2c.py "D:/path/to/IR/tunnel_entrance_f_1way_mono_processed.wav" \
           --output ir_tunnel.h --name ir_tunnel --reverse
"""
from __future__ import annotations

import argparse, sys, textwrap, subprocess, shutil, tempfile, os
from pathlib import Path

try:
    import soundfile as sf
    import numpy as np
except ImportError:
    sys.exit("✖ soundfile and numpy are required →  pip install soundfile numpy")

# ──────────────────────────────────────────────────────────────────────────────
def resample_to_48k(src: Path) -> tuple[np.ndarray, int]:
    """Resample with SoX if the file isn't 48 kHz (requires SoX installed)."""
    if not shutil.which("sox"):
        sys.exit("✖ File must be 48 kHz or install SoX so we can resample for you.")
    tmp = Path(tempfile.mktemp(suffix=".wav"))
    subprocess.check_call(["sox", str(src), "-r", "48000", str(tmp)])
    data, sr = sf.read(tmp)
    tmp.unlink(missing_ok=True)
    return data, sr

# ──────────────────────────────────────────────────────────────────────────────
def main() -> None:
    ap = argparse.ArgumentParser(description="Convert mono WAV IR → C header")
    ap.add_argument("wav", help="Path to mono WAV impulse-response")
    ap.add_argument("-o", "--output", help="Header file to write (default: <wav>.h)")
    ap.add_argument("--name", default="ir_data",
                    help="Base symbol name (default: ir_data  → kIrDataLen / kIrData)")
    ap.add_argument("--section", default=".qspi",
                    help='Linker section attribute (default: ".qspi")')
    ap.add_argument("--gain_db", type=float, default=-3.0,
                    help="Normalise level in dB (default: −3)")
    ap.add_argument("--reverse", action="store_true",
                    help="Reverse taps for direct FIR convolution")
    args = ap.parse_args()

    wav_path = Path(args.wav)
    if not wav_path.is_file():
        sys.exit(f"✖ File not found: {wav_path}")

    try:
        data, sr = sf.read(wav_path)
    except Exception as e:  # file locked, corrupt, etc.
        sys.exit(f"✖ Could not open WAV: {e}")

    if data.ndim != 1:
        sys.exit("✖ WAV must be mono (1 channel)")

    if sr != 48000:
        print(f"• Resampling {sr} Hz → 48 kHz using SoX …")
        data, sr = resample_to_48k(wav_path)

    # gain & optional reverse
    gain = 10 ** (args.gain_db / 20.0)
    data = (data * gain).astype(np.float32)
    if args.reverse:
        data = data[::-1]

    # build symbols
    base = args.name[0].upper() + args.name[1:]
    sym_len, sym_data = f"k{base}Len", f"k{base}Data"

    header_text = (
        "#pragma once\n#include <cstddef>\n\n"
        f"__attribute__((section(\"{args.section}\")))\n"
        f"const size_t {sym_len} = {len(data)};\n"
        f"const float  {sym_data}[{sym_len}] = {{\n"
        + textwrap.fill(", ".join(f"{x:.8f}f" for x in data), width=78)
        + "\n};\n"
    )

    out_path = Path(args.output) if args.output else wav_path.with_suffix(".h")
    out_path.write_text(header_text)
    print(f"✔ Wrote header: {out_path.resolve()}")

# ──────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    main()





# python wav2c.py "D:\Projects\ZekEng-Intern\DaisyExamples\pod\Convolution\mono files\tunnel_entrance_f_1way_mono.wav" --output ir_tunnel.h --name ir_tunnel --reverse

