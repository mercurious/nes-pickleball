#!/bin/sh
# Runs the ROM in the desktop RetroArch (nestopia on Fedora) with the harness settings. Stop: pkill -x retroarch
cd "$(dirname "$0")/.." || exit 1
mkdir -p build/shots
CORE=${CORE:-/usr/lib64/libretro/nestopia_libretro.so}
exec retroarch -L "$CORE" --appendconfig test/ra.cfg "${1:-build/pickleball.nes}"
