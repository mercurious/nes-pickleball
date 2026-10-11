#!/bin/sh
# Runs every test/*.lua in Mesen's test runner against build/pickleball.nes. MESEN=path to the Mesen binary.
cd "$(dirname "$0")/.." || exit 1
MESEN=${MESEN:-$HOME/nes-pickleball-tools/MesenCE/bin/linux-arm64/Release/Mesen}
ROM=${ROM:-build/pickleball.nes}
export PB_TESTLIB="$PWD/test/lib.lua"
fail=0
for t in test/*.lua; do
  [ "$t" = test/lib.lua ] && continue
  "$MESEN" --testRunner "$ROM" "$t" --timeout=${TIMEOUT:-120} > "build/$(basename "$t" .lua).log" 2>&1
  rc=$?
  echo "$t: exit $rc"; [ "$rc" = 0 ] || fail=1
done
exit $fail
