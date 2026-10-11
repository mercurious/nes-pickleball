#!/bin/sh
# Kills the desktop RetroArch and starts it again with the ROM (the RESET network command does nothing there).
cd "$(dirname "$0")/.." || exit 1
pkill -x retroarch 2>/dev/null; sleep 1
mkdir -p build/shots
nohup retroarch -L "${CORE:-/usr/lib64/libretro/nestopia_libretro.so}" --appendconfig test/ra.cfg "${1:-build/pickleball.nes}" > build/retroarch.log 2>&1 &
sleep 3
