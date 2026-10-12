#!/usr/bin/env python3
"""Writes the screenshots in a test log ("PNG frame hex" lines, test/shots.lua) as OUTDIR/frame.png.
Usage: shots.py build/tests/shots.log OUTDIR"""
import sys, os
os.makedirs(sys.argv[2], exist_ok=True)
for line in open(sys.argv[1]):
    if line.startswith("PNG "):
        _, frame, hx = line.split()
        open(os.path.join(sys.argv[2], "%06d.png" % int(frame)), "wb").write(bytes.fromhex(hx))
