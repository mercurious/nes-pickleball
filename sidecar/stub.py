#!/usr/bin/env python3
"""The desktop sidecar stub: the whole v0.0.2 contract against a running RetroArch, with a printed "voice".
It announces itself in the mailbox, answers every score call (prints it, releases the serve), speaks every
event as a line, shows speech bubbles over the players, and takes partner directives from a file.
Usage: stub.py [SECONDS] [--mem]   (--mem: READ/WRITE_CORE_MEMORY, the handheld's fceumm; default the
achievement map READ/WRITE_CORE_RAM, nestopia on the desktop). Directives: echo 3 > build/directive"""
import json, os, sys, time
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from ra import RA
SECS = float(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else 120
MEM = "--mem" in sys.argv
RD, WR = ("READ_CORE_MEMORY", "WRITE_CORE_MEMORY") if MEM else ("READ_CORE_RAM", "WRITE_CORE_RAM")
MB = 0x7F0
NUM = "zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen nineteen twenty twenty-one".split()
EV = ("NONE SCORE_CALL SERVE RETURN THIRD_SHOT DINK DRIVE LOB VOLLEY SMASH BOUNCE_IT MINE YOURS SWITCH POINT_WON "
      "FAULT_OUT FAULT_NET FAULT_LET FAULT_SHORT FAULT_WRONG_COURT FAULT_FOOT FAULT_TWO_BOUNCE FAULT_DOUBLE_BOUNCE "
      "FAULT_KITCHEN FAULT_BODY FAULT_NET_TOUCH SIDE_OUT SECOND_SERVER GAME_OVER NICE_SHOT LONG_RALLY PASSED").split()
LINES = {"MINE": "Mine!", "YOURS": "Yours!", "BOUNCE_IT": "Bounce it!", "SWITCH": "Switch!", "NICE_SHOT": "Nice!",
         "FAULT_KITCHEN": "Kitchen!", "FAULT_TWO_BOUNCE": "Let it bounce!", "SIDE_OUT": "Side out.",
         "SECOND_SERVER": "Second server.", "GAME_OVER": "Game!", "LONG_RALLY": "What a rally!", "FAULT_OUT": "Out!"}
BUBBLE = {"MINE": 1, "YOURS": 2, "SWITCH": 3, "BOUNCE_IT": 5, "NICE_SHOT": 6}
VOICE = ["alba", "jean", "marius", "cosette"]        # one Pocket TTS voice per slot (v0.0.2 renders them)

ra = RA(timeout=0.3)
def say(slot, text, bubble=None):
    print("%6.1fs [%s] %s" % (time.time() - t0, VOICE[slot & 3] if slot < 4 else "umpire", text), flush=True)
    if bubble: ra.poke(MB + 13, [bubble, slot & 3], WR)      # MB_BUBBLE, MB_BUBBLE_SLOT (one write, two bytes)

for i in range(60):
    if ra.cmd("VERSION"): break
    time.sleep(0.5)
t0 = time.time(); last_n = None; calls = 0; dirfile = os.path.join(HERE, "..", "build", "directive")
ra.poke(MB + 10, [0xA5], WR)                                 # I am here: the ROM waits for my call
print("stub: announced (mb_call = A5)")
while time.time() - t0 < SECS:
    mb = ra.ram(MB, 16, RD)
    if mb is None or mb[0] != 0x5B: time.sleep(0.1); continue
    if mb[10] == 1:                                          # the serve gate: call the score, then release
        a, b, srv = mb[3], mb[4], mb[5]
        serving, num, slot = srv & 1, (srv >> 1) & 3, (srv >> 3) & 3
        s_score, r_score = (a, b) if serving == 0 else (b, a)
        doubles = (mb[6] & 7) in (1, 2, 4)
        text = "%s, %s%s" % (NUM[s_score], NUM[r_score], (", %s" % NUM[num]) if doubles else "")
        say(slot, text + ".")
        time.sleep(0.6)                                      # a cached phrase would play here
        ra.poke(MB + 10, [2], WR); calls += 1
    if mb[7] != last_n:
        last_n = mb[7]
        ev = EV[mb[8]] if mb[8] < len(EV) else str(mb[8])
        if ev in LINES: say(mb[9], LINES[ev], BUBBLE.get(ev))
    try:
        d = int(open(dirfile).read().strip() or "0"); os.remove(dirfile)
        ra.poke(MB + 11, [d], WR); say(1, "Got it.", 4)
    except (OSError, ValueError): pass
    time.sleep(0.03)
ra.poke(MB + 10, [0], WR)
print("stub: %d score calls answered" % calls)
