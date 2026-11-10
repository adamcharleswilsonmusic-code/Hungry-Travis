# SPDX-License-Identifier: GPL-3.0-only
"""Prepare local AVFoundation helpers. Does not open an audio or MIDI device."""
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'fm1/build/recorder';out.mkdir(parents=True,exist_ok=True)
cache=out/'module-cache';cache.mkdir(exist_ok=True)
for source,target in [('record_usb_audio.swift','audio'),('export_movie.swift','export')]:
 src=ROOT/'tools'/source;dst=out/target
 if not dst.exists() or src.stat().st_mtime>dst.stat().st_mtime:
  subprocess.run(['swiftc','-module-cache-path',str(cache),str(src),'-o',str(dst)],check=True)
print('FM-1 recording helpers ready')
