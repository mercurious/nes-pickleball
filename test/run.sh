#!/bin/sh
# Runs the Lua tests in Mesen's test runner against build/pickleball.nes, headless. Each test is the library plus
# the RAM map plus the test body, concatenated (the Lua sandbox has no dofile). MESEN=path; TESTS="boot oracle".
cd "$(dirname "$0")/.." || exit 1
MESEN=${MESEN:-$HOME/nes-pickleball-tools/MesenCE/bin/linux-arm64/Release/linux-arm64/publish/Mesen}
ROM=${ROM:-build/pickleball.nes}
export SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy
[ -f ~/.config/MesenCE/settings.json ] || { mkdir -p ~/.config/MesenCE; echo "{}" > ~/.config/MesenCE/settings.json; }
mkdir -p build/tests
python3 - <<'PY'
import json
s = json.load(open("build/ramsyms.json"))
import os
pb = ", ".join("%s=%s" % (k[3:], v) for k, v in os.environ.items() if k.startswith("PB_") and v.isdigit())
rec = os.environ.get("REC", "")
open("build/tests/ram.lua", "w").write("RAM = {" + ", ".join('["%s"]=%d' % (k, v["addr"]) for k, v in s.items()) + "}\nPB = {" + pb + "}\n"
                                       + ("REC = %s\n" % json.dumps(os.path.abspath(rec)) if rec else "")
                                       + "OUT = %s\n" % json.dumps(os.path.abspath("build/tests")))
PY
fail=0
for name in ${TESTS:-boot oracle doubles voice cues}; do
  cat build/tests/ram.lua test/lib.lua "test/$name.lua" > "build/tests/$name.lua"
  timeout ${TIMEOUT:-900} "$MESEN" --testRunner "$ROM" "build/tests/$name.lua" --timeout=${MESEN_TIMEOUT:-800} > "build/tests/$name.log" 2>&1
  rc=$?
  echo "$name: exit $rc  $(grep -E 'failures|oracle:' build/tests/$name.log | tail -1)"
  grep -E "^(FAIL|ERROR)" "build/tests/$name.log" | head -5
  [ "$rc" = 0 ] || fail=1
  # a test that recorded the mixer asks for its loudness to be checked: "LOUDNESS <tools/loudness.py options>"
  args=$(grep '^LOUDNESS ' "build/tests/$name.log" | head -1 | cut -d' ' -f2-)
  if [ -n "$args" ]; then
    python3 tools/loudness.py "${REC:-build/tests/$name.wav}" "build/tests/$name.log" $args > "build/tests/$name.loud" || fail=1
    grep -E "per event|^  |^FAIL" "build/tests/$name.loud"
  fi
done
exit $fail
