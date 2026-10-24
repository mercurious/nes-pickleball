#!/bin/sh
# The teammate add-on against the ROM in the desktop RetroArch (nestopia, test/ra.cfg), with stand-ins for the model
# servers (sidecar/fakeservers.py: a tone for every spoken line, a canned PICK for every question, canned words for
# R1) and no sound output. No model, no handheld, no microphone: the whole protocol short of the real voices.
#   1. watch: an exhibition at level 3 for WATCH_S (default 70) s: every serve gate answered within the gate's 4 s,
#      the umpire's and the players' lines queued, the cache rendered through the fake TTS
#   2. coop: 1 PLAYER DOUBLES with a reflex stand-in for slot 0 for COOP_S (default 90) s: the model asked between
#      points, its picks written to MB_DIRECTIVE, the record saved; three things "said" with R1 through talk.py's
#      inject file: an order (stay back), a question (the score), and a remark for the model (a SAY line, guarded)
# Needs: retroarch with nestopia (Fedora: dnf install retroarch libretro-nestopia), autogamer's app/ for talk.py
# (AG_APP, default ~/autogamer/app). Everything it starts is killed at the end. Exit 1 when a check fails.
# Usage: test/sidecar_test.sh [watch|coop|all]     (default all)   Output: build/sidecar_test/
cd "$(dirname "$0")/.." || exit 1
WHAT=${1:-all}; O=build/sidecar_test; rm -rf "$O"; mkdir -p "$O" build/shots
export AG_APP=${AG_APP:-$HOME/autogamer/app} PB_AUDIO=0 PB_DUCK=0 PB_CACHE=$O/voices PB_RECORD=$O/pickleball_record.json
export FAKE_LOG=$O/fakeservers.jsonl FAKE_ASR_FILE=$O/asr.txt FAKE_THINK=${FAKE_THINK:-1.2} AG_MIC=none
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
n=0; bad=0
ok() { n=$((n + 1)); if [ "$1" = 0 ]; then echo "ok   $2"; else echo "FAIL $2"; bad=$((bad + 1)); fi; }
count() { c=$(grep -c "$1" "$2" 2>/dev/null); echo "${c:-0}"; }
PIDS=""
cleanup() { for p in $PIDS; do kill "$p" 2>/dev/null; done; pkill -x retroarch 2>/dev/null; rm -f /dev/shm/autogamer/talk.wav; }
trap cleanup EXIT INT TERM; rm -f /dev/shm/autogamer/talk.wav
[ -f build/pickleball.nes ] || make -s || exit 1
python3 sidecar/fakeservers.py 100000 > "$O/fakeservers.out" 2>&1 & PIDS="$PIDS $!"
sleep 0.5
curl -sf http://127.0.0.1:8093/health >/dev/null; ok $? "the fake servers answer"

if [ "$WHAT" = watch ] || [ "$WHAT" = all ]; then
  sh test/ra_restart.sh
  python3 -u sidecar/pickleball_coop.py "${WATCH_S:-70}" "$O/watch.jsonl" --mode watch --level 3 > "$O/watch.out" 2>&1
  L=$O/watch.jsonl
  ok $? "watch: the sidecar ran and ended"
  [ "$(count '"menu": "start"' "$L")" -ge 1 ]; ok $? "watch: the menu was steered to EXHIBITION and started"
  grep -q '"match": "start", "mode": 4, "level": 3' "$L"; ok $? "watch: an exhibition at level 3 began"
  G=$(count '"gate": "released"' "$L"); [ "$G" -ge 4 ]; ok $? "watch: $G serve gates answered (4 or more)"
  [ "$(count '"gate": "deadline"' "$L")" = 0 ]; ok $? "watch: no gate hit the sidecar's deadline"
  python3 - "$L" <<'EOF'; ok $? "watch: every gate released within 3 s (the cartridge waits 4)"
import json, sys
xs = [json.loads(l)["after_s"] for l in open(sys.argv[1]) if '"gate": "released"' in l]
print("      gate latencies: %s" % " ".join("%.1f" % x for x in xs)); sys.exit(0 if xs and max(xs) < 3.0 else 1)
EOF
  [ "$(count '"call_score"' "$L")" -ge 4 ]; ok $? "watch: the score was called in the server's voice"
  [ "$(count '"event": "FAULT\|"event": "PASSED' "$L")" -ge 1 ]; ok $? "watch: the umpire had a fault to call"
  [ "$(count '"said": "Side out.\|"said": "Second server.' "$L")" -ge 1 ]; ok $? "watch: a side out or second server was announced"
  [ "$(grep -c '"server": "tts"' "$FAKE_LOG")" -ge 100 ]; ok $? "watch: the phrase cache was rendered through the TTS ($(grep -c '"server": "tts"' "$FAKE_LOG") requests)"
  [ "$(ls "$PB_CACHE"/*/ 2>/dev/null | grep -c wav)" -ge 250 ]; ok $? "watch: $(ls "$PB_CACHE"/*/ 2>/dev/null | grep -c wav) WAVs in the cache"
  grep -q '"end": true' "$L"; ok $? "watch: a clean end"
  # again, with the rally-result hook ending every rally at once (the serving team scores): a game ends in ~25 s,
  # the title comes back (the cartridge leaves the mailbox stale there), the sidecar sees it and starts the next
  sh test/ra_restart.sh
  python3 - <<'EOF' > "$O/quick.out" 2>&1 &
import sys, time; sys.path.insert(0, "sidecar"); from ra import RA
ra = RA(timeout=0.3); t0 = time.time()
while time.time() - t0 < 75:
    m = ra.ram(0x7F0, 16, "READ_CORE_RAM")
    if m and m[0] == 0x5B and m[2] in (3, 4): ra.poke(0x7FF, [2 - (m[5] & 1)], "WRITE_CORE_RAM")
    time.sleep(0.05)
EOF
  PIDS="$PIDS $!"
  python3 -u sidecar/pickleball_coop.py 75 "$O/again.jsonl" --mode watch --level 3 > "$O/again.out" 2>&1
  L=$O/again.jsonl
  [ "$(count '"event": "GAME_OVER"' "$L")" -ge 1 ]; ok $? "again: a game ended ($(count '"event": "GAME_OVER"' "$L"))"
  [ "$(count '"title": "back"' "$L")" -ge 1 ]; ok $? "again: the title was seen after the game"
  [ "$(count '"match": "start"' "$L")" -ge 2 ]; ok $? "again: the next exhibition was started ($(count '"match": "start"' "$L") matches)"
  grep -q '"said": "Game!"' "$L"; ok $? "again: the umpire called the game"
fi

if [ "$WHAT" = coop ] || [ "$WHAT" = all ]; then
  sh test/ra_restart.sh
  : > "$FAKE_LOG"; echo "stay back" > "$FAKE_ASR_FILE"; mkdir -p /dev/shm/autogamer
  PB_STANDIN=1 PB_LEVEL=1 AG_MODEL_LABEL=FAKE python3 -u sidecar/pickleball_coop.py "${COOP_S:-90}" "$O/coop.jsonl" --mode coop > "$O/coop.out" 2>&1 &
  SC=$!; PIDS="$PIDS $SC"
  L=$O/coop.jsonl
  # the menu is set; the stand-in has no START of its own, so press it as the player would
  for i in $(seq 1 60); do grep -q '"menu": "set"' "$L" 2>/dev/null && break; sleep 0.5; done
  grep -q '"menu": "set", "mode": 1, "level": 1' "$L"; ok $? "coop: the menu was set to 1 PLAYER DOUBLES, level 1, START left to the player"
  sleep 0.5; python3 sidecar/pad.py start 0.2
  for i in $(seq 1 40); do grep -q '"match": "start"' "$L" 2>/dev/null && break; sleep 0.5; done
  grep -q '"match": "start", "mode": 1' "$L"; ok $? "coop: the match began in 1 PLAYER DOUBLES"
  # four things said with R1, 12 s apart: a remark (the model answers), an order, a question, and the model's calls
  # given back (talk.py hears a WAV put at its inject path)
  python3 - <<'EOF'
import struct, wave
w = wave.open("/dev/shm/autogamer/talk.wav.tmp", "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
w.writeframes(struct.pack("<h", 300) * 16000); w.close()
EOF
  sleep 10; echo "nice shot partner" > "$FAKE_ASR_FILE"; cp /dev/shm/autogamer/talk.wav.tmp /dev/shm/autogamer/talk.wav
  sleep 12; echo "stay back" > "$FAKE_ASR_FILE"; cp /dev/shm/autogamer/talk.wav.tmp /dev/shm/autogamer/talk.wav
  sleep 12; echo "what's the score" > "$FAKE_ASR_FILE"; cp /dev/shm/autogamer/talk.wav.tmp /dev/shm/autogamer/talk.wav
  sleep 12; echo "your call" > "$FAKE_ASR_FILE"; cp /dev/shm/autogamer/talk.wav.tmp /dev/shm/autogamer/talk.wav
  wait $SC; ok $? "coop: the sidecar ran and ended"
  G=$(count '"gate": "released"' "$L"); [ "$G" -ge 3 ]; ok $? "coop: $G serve gates answered"
  [ "$(count '"ask": "call"' "$L")" -ge 2 ]; ok $? "coop: the model was asked between points ($(count '"ask": "call"' "$L") times)"
  [ "$(count '"call": "UP", "changed": true' "$L")" -ge 1 ]; ok $? "coop: its pick (UP) became the standing call"
  grep -q '"said": "Let'"'"'s get up to the line.' "$L"; ok $? "coop: the partner said its call"
  [ -s "$PB_RECORD" ] && python3 -c "import json,sys; h=json.load(open('$PB_RECORD')); assert any(v for v in h.values()); print('      record:', json.dumps(h)[:160])"; ok $? "coop: the record across games was written"
  grep -q '"heard": "stay back", "asr_s": [0-9.]*, "order": "back"' "$L"; ok $? "coop: 'stay back' was obeyed as an order"
  grep -q '"said": "Got it. I'"'"'ll stay back.' "$L"; ok $? "coop: the order was acknowledged out loud"
  grep -q '"order": "score"' "$L"; ok $? "coop: 'what's the score' was answered from the mailbox"
  grep -q '"heard": "nice shot partner", "asr_s": [0-9.]*, "order": null' "$L" && grep -q '"ask": "chat"' "$L"; ok $? "coop: the remark went to the model"
  grep -q '"said": "You'"'"'ve got this, partner' "$L" || grep -q '"rejected":' "$L"; ok $? "coop: the model's reply was spoken (or rejected by the guard)"
  grep -q '"order": "yourcall"' "$L"; ok $? "coop: 'your call' handed the calls back to the model"
  python3 - "$L" <<'EOF'; ok $? "coop: while 'stay back' stood, no pick of the model's overrode it"
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
t_order = next((r["t"] for r in rows if r.get("order") == "back"), None)
t_back = next((r["t"] for r in rows if r.get("order") == "yourcall"), 1e9)
within = [r for r in rows if "call" in r and (t_order or 1e9) < r["t"] < t_back]
bad = [r for r in within if r["changed"] and r["call"] != "BACK"]
print("      %d calls while the order stood, %d overrode it" % (len(within), len(bad))); sys.exit(1 if bad or t_order is None else 0)
EOF
  grep -q '"end": true' "$L"; ok $? "coop: a clean end"
fi
echo "$((n - bad))/$n passed"; [ $bad = 0 ]
