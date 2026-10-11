#!/usr/bin/env python3
"""A reflex player over RetroArch for the desktop harness: starts the game, serves, runs to the ball and swings,
logs every mailbox event and the score. Usage: ra_play.py SECONDS [--verb READ_CORE_RAM]  (RetroArch must be running)"""
import json, os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "sidecar"))
from ra import RA
from pad import Pad
SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 60
VERB = "READ_CORE_RAM" if "--verb" not in sys.argv else sys.argv[sys.argv.index("--verb") + 1]
S = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "ramsyms.json")))
A = lambda n: S[n]["addr"]
ra = RA(timeout=0.3); pad = Pad()
ST = ["PRESERVE", "CALL", "SERVE_DROP", "FLIGHT", "RALLY", "DEAD", "GAMEOVER"]
EV = ("NONE SCORE_CALL SERVE RETURN THIRD_SHOT DINK DRIVE LOB VOLLEY SMASH BOUNCE_IT MINE YOURS SWITCH POINT_WON "
      "FAULT_OUT FAULT_NET FAULT_LET FAULT_SHORT FAULT_WRONG_COURT FAULT_FOOT FAULT_TWO_BOUNCE FAULT_DOUBLE_BOUNCE "
      "FAULT_KITCHEN FAULT_BODY FAULT_NET_TOUCH SIDE_OUT SECOND_SERVER GAME_OVER NICE_SHOT LONG_RALLY PASSED").split()

def s16(b, i): v = b[i] | (b[i + 1] << 8); return v - 65536 if v > 32767 else v
def u16(b, i): return b[i] | (b[i + 1] << 8)
def snap():
    zp = ra.ram(0, 0x100, VERB); mb = ra.ram(0x7F0, 16, VERB); bss = ra.ram(0x400, 0x100, VERB)
    if zp is None or mb is None or bss is None: return None
    d = dict(state=zp[A("state")], bx=u16(zp, A("bx")) / 256, by=u16(zp, A("by")) / 256, bz=s16(zp, A("bz")) / 256,
             vy=s16(zp, A("vy")) / 256, px=u16(zp, A("px")) / 256, py=u16(zp, A("py")) / 256, sw=bss[A("sw_t") - 0x400],
             shots=zp[A("shots")], bounce=zp[A("bounce_side")], live=zp[A("ball_live")], hitter=zp[A("last_hitter")],
             score=(mb[3], mb[4]), server=mb[5], ev_n=mb[7], ev=mb[8], ev_arg=mb[9], call=mb[10],
             dbg=list(bss[A("dbg") - 0x400: A("dbg") - 0x400 + 8]), vz=s16(zp, A("vz")) / 256, vx=s16(zp, A("vx")) / 256)
    return d

for i in range(60):
    if ra.cmd("VERSION"): break
    time.sleep(0.5)
else:
    sys.exit("no RetroArch on UDP 55355")
time.sleep(1.0)
t0 = time.time(); last_ev = None; last_state = None; served = False; frames = 0; log = []
# the menu: --mode N (0 singles 1 1P doubles 2 2P doubles 3 2P singles 4 exhibition), --level N, --bo3, --to15
def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default
md = int(opt("--mode", 0)); lv = int(opt("--level", 1))
time.sleep(2.5)
for i in range(md): pad.tap("right", 0.1); time.sleep(0.25)
pad.tap("select", 0.1); time.sleep(0.25)
for i in range(lv - 1): pad.tap("right", 0.1); time.sleep(0.25)
if "--to15" in sys.argv:
    pad.tap("select", 0.08); time.sleep(0.15); pad.tap("right", 0.08); time.sleep(0.15); pad.tap("select", 0.08); time.sleep(0.15)
elif "--bo3" in sys.argv:
    pad.tap("select", 0.08); time.sleep(0.15); pad.tap("select", 0.08); time.sleep(0.15); pad.tap("right", 0.08); time.sleep(0.15)
pad.tap("start", 0.2); time.sleep(0.3)
HUMAN = md != 4
while time.time() - t0 < SECS:
    d = snap()
    if d is None: time.sleep(0.05); continue
    frames += 1
    if d["ev_n"] != last_ev:
        last_ev = d["ev_n"]; msg = "%6.1fs EVENT %s arg=%d score=%s state=%s ball=(%.0f,%.0f,%.1f) v=(%.2f,%.2f,%.2f) dbg=%s" % (time.time() - t0, EV[d["ev"]] if d["ev"] < len(EV) else d["ev"], d["ev_arg"], d["score"], ST[d["state"]], d["bx"], d["by"], d["bz"], d["vx"], d["vy"], d["vz"], d["dbg"]); print(msg, flush=True); log.append(msg)
    if d["state"] != last_state:
        last_state = d["state"]; served = False
    st = d["state"]; buttons = set()
    if not HUMAN: time.sleep(0.03); continue
    if st == 2:   # SERVE_DROP: our serve when the server is slot 0 (near); press A at hip height
        if (d["server"] >> 3) == 0 and 3 <= d["bz"] <= 12 and not served: pad.tap("a", 0.05); served = True
    elif st in (3, 4):
        # run toward where the ball crosses our depth, keep it 8 px to the forehand (right) side; swing when close
        if d["vy"] > 0.1:
            fr = max(0, min(90, (d["py"] - d["by"]) / d["vy"])); tx = d["bx"] + fr * 0 - 8
        else: tx = d["bx"] - 8
        if tx < d["px"] - 1.5: buttons.add("left")
        elif tx > d["px"] + 1.5: buttons.add("right")
        ty = d["by"] + 4 if d["bounce"] else (160 if abs(d["vy"]) < 1.5 else d["py"])
        if d["by"] > 136:
            if ty < d["py"] - 2: buttons.add("up")
            elif ty > d["py"] + 2: buttons.add("down")
        lead = d["py"] - d["by"]
        if d["live"] and d["by"] > 136 and -2 <= lead <= 10 and abs(d["bx"] - d["px"]) <= 16 and d["hitter"] >= 2:
            if not ((d["shots"] in (1, 2)) and d["bounce"] == 0):
                buttons.add("a" if d["bz"] >= 10 else "b")
    pad.hold(buttons)
    if "--shots" in sys.argv and int(time.time() - t0) % 20 == 10 and not getattr(sys, "_shot", 0) == int(time.time() - t0):
        sys._shot = int(time.time() - t0); ra.cmd("SCREENSHOT", reply=False)
    time.sleep(0.016)
pad.release_all()
ra.cmd("SCREENSHOT", reply=False)
print("done: %d polls, final %s" % (frames, snap()))
