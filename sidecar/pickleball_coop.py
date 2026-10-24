#!/usr/bin/env python3
# The teammate add-on (v0.0.2): runs beside RetroArch and talks to the cartridge through the mailbox
# (docs/MAILBOX.md), nothing else. Two modes:
#   coop   1 PLAYER DOUBLES: you play slot 0, the cartridge's computer partner plays slot 1. The sidecar gives every
#          player a voice: the server calls the score (through the serve gate), the players chatter ("mine", "bounce
#          it", "sorry"), the umpire calls the faults. Between points a language model makes the partner's call
#          (MIDDLE/YOURS, UP/BACK, SWITCH, PATIENT/ATTACK) from the score, the last point and its record in that
#          situation; the call becomes MB_DIRECTIVE and a spoken line. Hold R1 and talk to your partner (talk.py):
#          "I've got the middle", "stay back", "come up", "switch", "be patient", "attack", "hold up" / "go",
#          "your call", "what's the score"; anything else goes to the model, which answers in a few words.
#   watch  EXHIBITION: four voices and the umpire call a computer match; the sidecar starts each game itself.
# The sidecar steers the title menu to the mode (and the level, PB_LEVEL) when the cartridge shows it; in coop it
# leaves START to you. It answers every serve gate it sees, whatever the mode, once it has announced itself.
# Usage: pickleball_coop.py SECONDS [log.jsonl] [--mode coop|watch] [--level 1-5] [--mem]
#   --mem: READ/WRITE_CORE_MEMORY (the handheld's fceumm); default READ/WRITE_CORE_RAM (nestopia on the desktop)
# Env: PB_MODE, PB_LEVEL, PB_VOICES (voices.py), PB_CACHE (the WAV cache, default build/voices), PB_TTS, PB_AUDIO,
#   PB_RECORD (the record across games, JSON), PB_MODEL=0 (no model: the partner plays its own game),
#   PB_TALK=0 (no R1), PB_STANDIN=1 (a reflex plays slot 0, to test coop without a person), VS_PORT (llama-server,
#   default 8099), AG_APP (autogamer's app directory: its talk.py, llm.py and hud.py are used when it is there),
#   AG_MODEL_LABEL, TL_CALIB (the mean think time is saved there), TL_TTS=0 (captions only: no voices at all).
import json, os, random, re, sys, threading, time, urllib.request
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
if os.environ.get("AG_APP") and os.path.isdir(os.environ["AG_APP"]): sys.path.insert(1, os.environ["AG_APP"])   # its talk, llm, hud; our ra, pad
from ra import RA
from pad import Pad
import voices as V

def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default
SECS = float(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else 864000
LOGF = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else None
MODE = opt("--mode", os.environ.get("PB_MODE", "coop")); WATCH = MODE == "watch"
LEVEL = opt("--level", os.environ.get("PB_LEVEL", "3" if WATCH else "")); LEVEL = int(LEVEL) if LEVEL else None
MEM = "--mem" in sys.argv
RD, WR = ("READ_CORE_MEMORY", "WRITE_CORE_MEMORY") if MEM else ("READ_CORE_RAM", "WRITE_CORE_RAM")
CACHE = os.environ.get("PB_CACHE", os.path.join(HERE, "..", "build", "voices"))
RECORD = os.environ.get("PB_RECORD"); USE_MODEL = os.environ.get("PB_MODEL", "1") != "0" and not WATCH
TALK = os.environ.get("PB_TALK", "1") != "0" and not WATCH; STANDIN = os.environ.get("PB_STANDIN") == "1"
LLM = "http://127.0.0.1:%s" % os.environ.get("VS_PORT", "8099"); MODEL = os.environ.get("AG_MODEL_LABEL", "model")
CALIB = os.environ.get("TL_CALIB"); SPEAK = os.environ.get("TL_TTS", "1") != "0"
MB = 0x7F0
ST = ["PRESERVE", "CALL", "SERVE_DROP", "FLIGHT", "RALLY", "DEAD", "GAMEOVER"]
EV = ("NONE SCORE_CALL SERVE RETURN THIRD_SHOT DINK DRIVE LOB VOLLEY SMASH BOUNCE_IT MINE YOURS SWITCH POINT_WON "
      "FAULT_OUT FAULT_NET FAULT_LET FAULT_SHORT FAULT_WRONG_COURT FAULT_FOOT FAULT_TWO_BOUNCE FAULT_DOUBLE_BOUNCE "
      "FAULT_KITCHEN FAULT_BODY FAULT_NET_TOUCH SIDE_OUT SECOND_SERVER GAME_OVER NICE_SHOT LONG_RALLY PASSED").split()
SHOTS = {"DINK": "dink", "DRIVE": "drive", "LOB": "lob", "VOLLEY": "volley", "SMASH": "smash", "SERVE": "serve",
         "RETURN": "return", "THIRD_SHOT": "third shot"}
# what a fault event means: the umpire's word (a cache key) and how to tell the model
FAULTS = {"FAULT_OUT": ("out", "hit it out"), "FAULT_NET": ("net", "hit the net"), "FAULT_LET": ("let", "a let serve, replayed"),
          "FAULT_SHORT": ("short", "served short, into the kitchen"), "FAULT_WRONG_COURT": ("wrongcourt", "served into the wrong court"),
          "FAULT_FOOT": ("fault", "faulted the serve"), "FAULT_TWO_BOUNCE": ("twobounce", "volleyed before the two bounces"),
          "FAULT_DOUBLE_BOUNCE": ("point", "let it bounce twice"), "FAULT_KITCHEN": ("kitchen", "volleyed from the kitchen"),
          "FAULT_BODY": ("fault", "were hit by the ball"), "FAULT_NET_TOUCH": ("nettouch", "touched the net"),
          "PASSED": ("out", "let a long ball go")}
# the eight directives: MB_DIRECTIVE, the model's choice text, the cache key of the partner's line, the bubble
DIRECTIVES = [(1, "MIDDLE: you take the middle, player 1 covers the sideline", "d1", 12),
              (2, "YOURS: player 1 takes the middle, you cover your sideline", "d2", 2),
              (4, "UP: get up to the kitchen line as soon as you can", "d4", 11),
              (3, "BACK: stay back on the third shot, come in later", "d3", 10),
              (5, "SWITCH: cover the other half (after a lob)", "d5", 3),
              (6, "PATIENT: dink and reset, no speed-ups", "d6", 13),
              (7, "ATTACK: speed up every ball above the net", "d7", 9)]
DNAME = {0: "OWN CALL", 1: "MIDDLE", 2: "YOURS", 3: "BACK", 4: "UP", 5: "SWITCH", 6: "PATIENT", 7: "ATTACK"}
DKEY = {d: k for d, _, k, _ in DIRECTIVES}; DBUB = {d: b for d, _, _, b in DIRECTIVES}
SYSTEM = ("You are the computer partner of a human player in a doubles pickleball video game (NES, 1985 rules: side-out "
          "scoring, first to 11 by two, the two-bounce rule, no volleys from the kitchen). Your player runs and hits "
          "by itself; between points you make one partner's call, which stands until your next: MIDDLE or YOURS (who "
          "covers the middle), UP or BACK (rush the line, or stay back on the third shot), SWITCH (cover the other "
          "half), PATIENT or ATTACK (dink and reset, or speed up). Player 1 is a person: help them win and keep it "
          "friendly. Their orders stand until they change them, so follow them. Each turn you get the score, who "
          "serves, how the last point went, your record with each call in this situation, and lettered choices. "
          "Reply exactly as: PICK <letter>")
# what player 1 says (hold R1): the first pattern that matches is the order (the narrow ones first)
ORDERS = [
    ("score", r"\b(what'?s|what is|say|call|give me) the score\b|\bscore\?"),
    ("yourcall", r"\b(your call|you decide|up to you|do your thing|your choice|play (it )?your way|you'?re free|you choose)\b"),
    ("mine", r"\b(i'?ve got|i have|i'?ll take|i got|i take|give me|leave me|let me have)\b.{0,12}\bmiddle\b|\bmiddle'?s mine\b|\bmiddle is mine\b"),
    ("middle", r"\b(you|u)\b.{0,8}\b(take|got|have|cover|get)\b.{0,8}\bmiddle\b|\btake the middle\b|\bcover the middle\b|\bmiddle'?s yours\b|^\W*middle\b"),
    ("back", r"\b(stay|hang|get|drop|fall|hold|keep) back\b|\bstay (home|deep)\b|\bback off\b|\bdon'?t (come|rush|go) (in|up)\b"),
    ("up", r"\b(come|get|move|go|let'?s go|push|rush) (up|in|forward|to the (line|kitchen|net))\b|\bto the (line|net|kitchen)\b|\bcome on up\b"),
    ("switch", r"\b(switch|swap|change sides|other side|cross)\b"),
    ("patient", r"\b(be patient|patient|slow (it )?down|dink|soft|reset|take it easy|easy|calm|don'?t (speed|rush|attack))\b"),
    ("attack", r"\b(attack|aggressive|speed (it )?up|hit (it )?hard|drive|smash|go for it|punish|kill it|bang)\b"),
    ("hold", r"\b(hold up|hold on|wait|stop|freeze|hang on|time ?out)\b"),
    ("go", r"\b(go|okay go|let'?s go|play|resume|carry on|continue)\b"),
]
# an order: the directive it sets (None: no change) and the partner's spoken answer (cache keys)
OBEY = {"mine": (2, ["gotit", "d2"]), "middle": (1, ["gotit", "d1"]), "back": (3, ["gotit", "d3"]), "up": (4, ["gotit", "d4"]),
        "switch": (5, ["gotit", "d5"]), "patient": (6, ["gotit", "d6"]), "attack": (7, ["gotit", "d7"]),
        "hold": (None, ["holding"]), "go": (None, ["go"]), "yourcall": (None, ["yourcall"])}
FALLBACK_SAY = ["Let's just play.", "Next point.", "I'm with you, partner.", "Okay. Let's get this one."]

def log(d):
    d = dict(t=round(time.time() - t0, 1), **d); line = json.dumps(d)
    print(line, flush=True)
    if LOGF:
        with open(LOGF, "a") as f: f.write(line + "\n")

def save_mean(path, xs):
    if not path or len(xs) < 3: return
    try:
        with open(path + ".tmp", "w") as f: f.write("%.2f\n" % (sum(xs) / len(xs)))
        os.replace(path + ".tmp", path)
    except OSError: pass

def _chat_plain(url, messages, max_tokens, temperature, timeout):
    """llama-server's OpenAI endpoint without autogamer's streaming client (standalone runs)"""
    body = {"messages": messages, "max_tokens": max_tokens, "temperature": temperature, "cache_prompt": True}
    req = urllib.request.Request(url + "/v1/chat/completions", json.dumps(body).encode(), {"Content-Type": "application/json"})
    r = json.loads(urllib.request.urlopen(req, timeout=timeout).read())
    return r["choices"][0]["message"]["content"].strip(), r.get("timings", {})

try: import llm; chat = lambda m, n, t: llm.chat(LLM, m, n, t, 120, cache=True)
except ImportError: chat = lambda m, n, t: _chat_plain(LLM, m, n, t, 120)

def ask(text, reply=False, temp=0.3):
    if reply: text += "\nReply as: PICK <letter>, then a new line: SAY <a few words back to player 1>"
    return chat([{"role": "system", "content": SYSTEM}, {"role": "user", "content": text}], 40 if reply else 6, temp)[0]

def faithful(line, allowed_numbers, game_over):
    """the model's words to player 1 keep to the facts: no numbers but the score's, no game or match won unless it is"""
    l = line.lower(); words = {w: i for i, w in enumerate(V.NUM)}
    nums = {int(x) for x in re.findall(r"\b\d+\b", l)} | {words[w] for w in re.findall(r"\b[a-z-]+\b", l) if w in words}
    if nums - set(allowed_numbers): return False
    if not game_over and re.search(r"\b(won|win|wins|winning|lost|lose|game over|match|set)\b.{0,12}\b(game|match|set)\b|\bgame over\b", l): return False
    return 1 <= len(l.split()) <= 24                       # "Thanks!" is an answer

class Mailbox:
    """one read of the sixteen bytes, decoded"""
    __slots__ = ("raw", "magic", "state", "a", "b", "srv", "mode", "level", "n", "ev", "arg", "call", "directive", "bubble")
    def __init__(self, raw):
        self.raw = raw; self.magic = raw[0]; self.state = raw[2]; self.a, self.b = raw[3], raw[4]; self.srv = raw[5]
        self.mode = raw[6] & 7; self.level = (raw[6] >> 4) & 7; self.n = raw[7]; self.ev = raw[8]; self.arg = raw[9]
        self.call = raw[10]; self.directive = raw[11]; self.bubble = raw[13]
    def serving(self): return self.srv & 1
    def server_num(self): return (self.srv >> 1) & 3
    def server_slot(self): return (self.srv >> 3) & 3
    def doubles(self): return self.mode in (1, 2, 4)
    def valid(self):
        """a match's mailbox (a read before the cartridge has booted, or of a cleared RAM, is not one)"""
        return self.magic == 0x5B and self.raw[1] == 1 and self.state <= 6 and self.mode <= 4 and 1 <= self.level <= 5
    def fresh_title(self): return self.magic == 0 and self.raw[1] == 0
    def live(self): return self.valid()

def score_words(mb):
    """the call: the serving team's score, the receiving team's, the server number in doubles"""
    s, r = (mb.a, mb.b) if mb.serving() == 0 else (mb.b, mb.a)
    return [str(s), str(r)] + ([str(mb.server_num())] if mb.doubles() else [])

def team_of(slot): return 0 if slot < 2 else 1

class Game:
    """what the sidecar knows of the match in progress: for the chatter, the model's question and the record"""
    def __init__(self):
        self.shots = []; self.rally = 0; self.last_point = None; self.point_start = None; self.situation = None
        self.hitter = None; self.over = False; self.games_seen = 0

if __name__ == "__main__":
    ra = RA(timeout=0.3); pad = Pad(user=0)
    voice = V.voices_from_env(); UMP = voice[4]
    t0 = time.time(); speaker = V.Speaker(CACHE, ra=ra, log=log); speaker.start()
    for i in range(120):
        if ra.cmd("VERSION"): break
        time.sleep(0.5)
    else: sys.exit("pickleball_coop: no RetroArch on UDP 55355")
    if "PAUSED" in (ra.cmd("GET_STATUS") or ""): ra.cmd("PAUSE_TOGGLE", reply=False)
    if SPEAK and V.tts_up():
        threading.Thread(target=V.ensure, args=(CACHE, voice), kwargs=dict(log=lambda s: log(dict(cache=s))), daemon=True).start()
    elif SPEAK: log(dict(note="no tts-server on %s: captions only" % V.TTS)); SPEAK = False
    ra.poke(MB + 10, [0xA5], WR)                              # I am here: the cartridge waits for my score calls
    log(dict(start=MODE, level=LEVEL, model=USE_MODEL and MODEL, talk=TALK, voices=voice, speak=SPEAK, standin=STANDIN))

    def say(slot, keys=None, text=None, show=None, bubble=None, on_done=None, priority=0):
        """a line in a slot's voice (4: the umpire); the caption names the speaker; a bubble over the player"""
        v = voice[slot] if slot < 4 else UMP
        who = "Umpire" if slot == 4 else ("You" if slot == 0 and not WATCH else "Partner" if slot == 1 and not WATCH else v.capitalize())
        words = text if text is not None else " ".join(V.PHRASES.get(k, k) for k in keys or [])
        cap = show if show is not None else "%s: %s" % (who, words)
        if not SPEAK:
            if cap: ra.cmd("SHOW_MSG " + cap, reply=False)
            if on_done: on_done()
            ok = True
        else: ok = speaker.say(v, keys, text, cap, on_done, priority)
        if ok and bubble and slot < 4: ra.poke(MB + 13, [bubble, slot], WR)
        return ok

    # ---- the serve gate and the chatter
    cooldown = {}                                              # slot -> when it may speak again
    def chatter(slot, key, bubble=None, gap=1.2):
        if time.time() < cooldown.get(slot, 0): return False
        if slot == 0 and not WATCH: return False               # the human does not talk for themselves
        if say(slot, [key], bubble=bubble): cooldown[slot] = time.time() + gap; return True
        return False

    gate = dict(open=False, since=0.0, released=0)
    def release():
        if gate["open"]:
            ra.poke(MB + 10, [2], WR); gate["open"] = False; gate["released"] += 1
            log(dict(gate="released", after_s=round(time.time() - gate["since"], 2)))

    # ---- the partner's calls (coop, 1 PLAYER DOUBLES)
    hist = {}
    if RECORD:
        try: hist = json.load(open(RECORD))
        except (OSError, ValueError): hist = {}
    def save_hist():
        if not RECORD: return
        try:
            os.makedirs(os.path.dirname(os.path.abspath(RECORD)), exist_ok=True)
            with open(RECORD + ".tmp", "w") as f: json.dump(hist, f)
            os.replace(RECORD + ".tmp", RECORD)
        except OSError: pass
    def situation(mb):
        us, them = mb.a, mb.b
        return "%s:%s" % ("serving" if mb.serving() == 0 else "receiving", "ahead" if us > them else "behind" if us < them else "level")
    def bump(sit, directive, won):
        if sit is None: return
        r = hist.setdefault(sit, {}).setdefault(str(directive), dict(n=0, won=0, lost=0))
        r["n"] += 1; r["won" if won else "lost"] += 1
    def record_text(sit):
        parts = []
        for d in range(8):
            r = hist.get(sit, {}).get(str(d))
            if r and r["n"]: parts.append("%s %d points, won %d" % (DNAME[d], r["n"], r["won"]))
        return "; ".join(parts) or "nothing yet"

    standing = dict(directive=0, by="model", since=time.time())  # the call in force and who made it
    order = dict(directive=None, said="", said_t=0.0, hold=False)  # player 1's standing order
    told = []; heard = []; pending = []; thinks = []; worker = [None]
    def busy(): return worker[0] is not None and worker[0].is_alive()

    def set_directive(d, by, spoken_keys=None, show=None):
        if d != standing["directive"] or by != standing["by"]:
            ra.poke(MB + 11, [d], WR); standing.update(directive=d, by=by, since=time.time())
        if spoken_keys: say(1, spoken_keys, show=show, bubble=DBUB.get(d) if d else None, priority=1)

    def facts(mb, g, choices, chatty):
        us, them = mb.a, mb.b; serving = mb.serving() == 0
        lines = ["Doubles, game to 11. Score: you %d, they %d. %s%s." % (us, them, "You serve" if serving else "They serve",
                 " (server %d of 2)" % mb.server_num() if serving else "")]
        if g.last_point:
            lp = g.last_point
            lines.append("Last point: %s, %d shots%s." % (lp["text"], lp["rally"], (": " + ", ".join(lp["shots"][-8:])) if lp["shots"] else ""))
        if not chatty:
            lines.append("Your standing call: %s." % DNAME[standing["directive"]])
            lines.append("Your record in this situation (%s): %s." % (g.situation or situation(mb), record_text(g.situation or situation(mb))))
        if order["directive"] is not None:
            lines.append('Player 1\'s standing order: %s (they said "%s" %d s ago). Keep to it.' % (DNAME[order["directive"]], order["said"], time.time() - order["said_t"]))
        if told: lines.append('Player 1 just said to you: "%s"' % " / ".join(told))
        return "\n".join(lines + ["%s) %s" % ("ABCDEFGH"[i], c) for i, c in enumerate(choices)])

    def ask_now(mb, g):
        chatty = bool(told)
        keys = [d for d, *_ in DIRECTIVES] + ([standing["directive"]] if chatty and standing["directive"] else [])
        texts = {d: t for d, t, *_ in DIRECTIVES}; texts[standing["directive"]] = "KEEP your standing call (%s)" % DNAME[standing["directive"]]
        if order["directive"] is not None: keys = [order["directive"]]             # their order stands: no other choice
        random.shuffle(keys); choices = [texts.get(k, DNAME[k]) for k in keys]
        box = dict(keys=keys, t=time.time(), told=list(told), mb=mb, numbers={mb.a, mb.b, mb.server_num(), g.last_point["rally"] if g.last_point else 0})
        text = facts(mb, g, choices, chatty); told.clear(); pending.append(box)
        def think(text=text, box=box):
            t1 = time.time()
            try: box["ans"] = ask(text, reply=bool(box["told"]))
            except Exception as ex: box["ans"] = "ERROR %s" % ex
            box["dt"] = time.time() - t1
            if not box["told"]: thinks.append(box["dt"]); save_mean(CALIB, thinks[1:][-50:])
        worker[0] = threading.Thread(target=think, daemon=True); worker[0].start()
        log(dict(ask="chat" if chatty else "call", choices=len(keys), order=order["directive"]))

    def take_answer(b, mb, g):
        m = re.search(r"PICK\s*\(?([A-H])\)?", b["ans"]); idx = "ABCDEFGH".index(m.group(1)) if m else -1
        pick = b["keys"][idx] if 0 <= idx < len(b["keys"]) else b["keys"][0]
        back = [l for l in b["ans"].splitlines()[1:] if l.strip()] if b["told"] else []
        spoken = re.sub(r"^\W*SAY\W*", "", back[0]).strip().strip('"')[:120] if back else None
        rejected = None
        if spoken and not faithful(spoken, b["numbers"] | {11}, g.over):
            rejected = spoken
            try: again = ask(facts(mb, g, [DNAME[k] for k in b["keys"]], True) + '\nPlayer 1 said: "%s"' % " / ".join(b["told"]), reply=True, temp=0.1)
            except Exception: again = ""
            lines = [l for l in again.splitlines()[1:] if l.strip()]
            spoken = re.sub(r"^\W*SAY\W*", "", lines[0]).strip().strip('"')[:120] if lines else ""
            if not spoken or not faithful(spoken, b["numbers"] | {11}, g.over): spoken = random.choice(FALLBACK_SAY)
        if order["directive"] is not None: pick = order["directive"]            # player 1's order stands
        changed = pick != standing["directive"]
        if order["hold"]: changed = False
        if changed: set_directive(pick, "model", None if spoken else [DKEY.get(pick, "ok")])
        if spoken: say(1, text=spoken, show="Partner: %s" % spoken, priority=1)
        log(dict(call=DNAME.get(pick), changed=changed, parsed=bool(m), think_s=round(b["dt"], 2), answer=b["ans"][:200],
                 told=b["told"] or None, rejected=rejected))

    def obey(words, mb, g):
        """an order from player 1: carry it out and answer; None when it is not an order (the model answers)"""
        w = words.lower().replace("’", "'")
        key = next((k for k, pat in ORDERS if re.search(pat, w)), None)
        if key is None: told.append(words[:120]); return None
        now = time.time()
        if key == "score":
            keys = score_words(mb) if mb.live() else ["0", "0", "2"]
            say(1, keys, show="Partner: %s" % ", ".join(V.PHRASES[k] for k in keys), priority=1); return "score"
        if key == "yourcall": order.update(directive=None, hold=False); say(1, OBEY[key][1], priority=1); return key
        if key == "hold": order["hold"] = True; say(1, OBEY[key][1], bubble=13, priority=1); return key
        if key == "go": order["hold"] = False; say(1, OBEY[key][1], bubble=9, priority=1); return key
        d, keys = OBEY[key]
        order.update(directive=d, said=words[:60], said_t=now, hold=False)
        set_directive(d, "player1", keys, show='You: "%s"  Partner: %s' % (words[:40], V.PHRASES[keys[-1]]))
        return key

    if TALK:
        try:
            import talk
            talk.Talk(lambda: heard.append(("listen",)), lambda w, secs, why="": heard.append(("text", w, secs, why))).start()
            log(dict(talk="on", asr=talk.ASR))
        except ImportError: log(dict(talk="off: no talk.py (set AG_APP to autogamer's app directory)")); TALK = False

    # ---- the title menu: the mode and the level, by reading the cartridge's variables and tapping the pad
    menu = dict(done=False, t=0.0, taps=0)
    WANT_MODE = 4 if WATCH else 1
    def steer_menu():
        if menu["done"] or time.time() - menu["t"] < 0.35 or time.time() - idle[1] < 1.0: return   # never into a live match
        menu["t"] = time.time()
        r = ra.ram(0x480, 0x44, RD)                            # mode, level, ... cursor ($04C3)
        if r is None: return
        mode, level, cursor = r[0], r[1], r[0x43]
        toward = lambda line: pad.tap("select" if (line - cursor) % 5 <= (cursor - line) % 5 else "up", 0.08)   # the fewer taps
        if mode != WANT_MODE:
            if cursor != 0: toward(0)
            else: pad.tap("right" if (WANT_MODE - mode) % 5 <= 2 else "left", 0.08)
        elif LEVEL and level != LEVEL:
            if cursor != 1: toward(1)
            else: pad.tap("right" if (LEVEL - level) % 5 <= 2 else "left", 0.08)
        elif WATCH: pad.tap("start", 0.12); menu["done"] = True; log(dict(menu="start", level=level))
        else:
            menu["done"] = True; log(dict(menu="set", mode=mode, level=level))
            ra.cmd("SHOW_MSG 1 PLAYER DOUBLES is set. Pick a level if you like, then START. Hold R1 to talk to your partner.", reply=False)
        menu["taps"] += 1
        if menu["taps"] > 80: menu["done"] = True; log(dict(menu="gave up", mode=mode, level=level, cursor=cursor))

    # ---- a reflex for slot 0 (tests without a person): serves, runs to the ball, swings (test/ra_play.py's rules)
    try: SYM = json.load(open(os.path.join(HERE, "..", "build", "ramsyms.json")))
    except OSError: SYM = None
    def standin_step(mb):
        if not SYM: return
        zp = ra.ram(0, 0x40, RD)
        if zp is None: return
        A = lambda n: SYM[n]["addr"]; u16 = lambda i: zp[i] | (zp[i + 1] << 8)
        s16 = lambda i: (u16(i) - 65536) if u16(i) > 32767 else u16(i)
        bx, by, bz = u16(A("bx")) / 256, u16(A("by")) / 256, s16(A("bz")) / 256
        vy = s16(A("vy")) / 256; px, py = u16(A("px")) / 256, u16(A("py")) / 256
        live, bounce, hitter, shots = zp[A("ball_live")], zp[A("bounce_side")], zp[A("last_hitter")], zp[A("shots")]
        st = mb.state; buttons = set()
        if st == 2:
            if mb.server_slot() == 0 and 3 <= bz <= 12 and not menu.get("served"): pad.tap("a", 0.05); menu["served"] = True
            return
        menu["served"] = False
        if st in (3, 4):
            tx = bx - 8
            if tx < px - 1.5: buttons.add("left")
            elif tx > px + 1.5: buttons.add("right")
            ty = by + 4 if bounce else (160 if abs(vy) < 1.5 else py)
            if by > 136:
                if ty < py - 2: buttons.add("up")
                elif ty > py + 2: buttons.add("down")
            lead = py - by
            if live and by > 136 and -2 <= lead <= 10 and abs(bx - px) <= 16 and hitter >= 2 and not (shots in (1, 2) and bounce == 0):
                buttons.add("a" if bz >= 10 else "b")
        pad.hold(buttons)

    g = Game(); prev = None; last_n = None; was_live = False; hist_t = time.time(); gate_deadline = 0.0
    partner_mode = False; polls = 0; told_mode = 0.0
    idle = [None, 0.0, -1, 0.0, 0.0]        # the mailbox's last key and when it changed; the title counter, when read, when it moved
    try:
        while time.time() - t0 < SECS:
            raw = ra.ram(MB, 16, RD)
            if raw is None: time.sleep(0.05); continue
            mb = Mailbox(raw); polls += 1
            # The v0.0.1 cartridge leaves MB_MAGIC at $5B on the title after a game or a demo (the contract says 0),
            # with the mailbox stale. The title's own idle counter ($04C7, two bytes) moves every frame there and
            # nowhere else: the title is up when it moved more recently than the mailbox did.
            key = (mb.magic, mb.n, mb.state, mb.a, mb.b, mb.call)
            if key != idle[0]: idle[0] = key; idle[1] = time.time()
            if time.time() - idle[3] > 0.4:
                looked = idle[3]; idle[3] = time.time(); r = ra.ram(0x4C7, 2, RD)
                moved = r is not None and (r[0] | (r[1] << 8)) != idle[2]
                if r is not None: idle[2] = r[0] | (r[1] << 8)
                if idle[1] > looked: idle[4] = 0.0            # the mailbox changed since the last look: a match
                elif moved: idle[4] = time.time()             # the counter alone moved: the title
            on_title = mb.fresh_title() or idle[4] > idle[1]
            if not mb.valid() and not mb.fresh_title(): time.sleep(0.1); continue      # not booted yet, or garbage
            if on_title:
                if was_live:
                    was_live = False; menu["done"] = False; menu["taps"] = 0; pad.release_all()
                    log(dict(title="back", games=g.games_seen)); save_hist()
                steer_menu(); time.sleep(0.1); continue
            if not was_live:                                   # a match begins
                was_live = True; prev = None; last_n = mb.n; g = Game(); g.situation = situation(mb); menu["done"] = True
                partner_mode = mb.mode == 1 and not WATCH
                log(dict(match="start", mode=mb.mode, level=mb.level, partner=partner_mode))
                if USE_MODEL and not partner_mode and mb.mode != 4 and time.time() - told_mode > 60:
                    told_mode = time.time(); ra.cmd("SHOW_MSG Your partner talks in 1 PLAYER DOUBLES; this mode gets the voices only.", reply=False)
                if partner_mode: set_directive(order["directive"] or 0, "player1" if order["directive"] else "model")

            # the serve gate: the server calls the score, then the serve is released
            if mb.call == 1 and not gate["open"]:
                gate.update(open=True, since=time.time()); gate_deadline = time.time() + 3.7   # the cartridge serves at 4.0
                keys = score_words(mb); slot = mb.server_slot()
                say(slot, keys, show="%s calls: %s" % ("You" if slot == 0 and not WATCH else "Partner" if slot == 1 and not WATCH else voice[slot].capitalize(),
                                                        ", ".join(V.PHRASES[k] for k in keys)), on_done=release, priority=2)
                log(dict(call_score=keys, slot=slot, state=ST[mb.state]))
            elif gate["open"] and (mb.call != 1 or time.time() > gate_deadline):
                if mb.call == 1: release(); log(dict(gate="deadline"))
                else: gate["open"] = False

            # events
            if mb.n != last_n:
                missed = (mb.n - (last_n if last_n is not None else mb.n)) % 256 - 1
                last_n = mb.n; ev = EV[mb.ev] if mb.ev < len(EV) else str(mb.ev); arg = mb.arg
                if missed > 0: log(dict(missed_events=missed, then=ev))
                if ev in SHOTS:
                    g.shots.append("%s by %s" % (SHOTS[ev], "you" if arg == 0 and not WATCH else "your partner" if arg == 1 and not WATCH else "them" if arg >= 2 else "a teammate"))
                    g.rally += 1; g.hitter = arg
                elif ev == "MINE": chatter(arg, "mine")                    # the cartridge draws these two bubbles itself
                elif ev == "BOUNCE_IT": chatter(arg, "bounce")
                elif ev == "YOURS": chatter(arg, "yours", 2)
                elif ev == "SWITCH": chatter(arg, "switch", 3)
                elif ev == "NICE_SHOT":
                    partner = arg ^ 1
                    if partner != 0 or WATCH: chatter(partner, "nice", 6)
                elif ev == "LONG_RALLY": chatter(4, "rally", gap=20)
                elif ev in FAULTS:
                    word, meaning = FAULTS[ev]; team = arg & 1
                    who = g.hitter if g.hitter is not None and team_of(g.hitter) == team else (mb.server_slot() if team_of(mb.server_slot()) == team else team * 2)
                    we_lost = team == 0 and ev != "PASSED"
                    if ev == "PASSED": we_lost = team != 0     # the team that let it go wins the rally
                    g.last_point = dict(text="%s %s" % (("you" if who == 0 and not WATCH else "your partner" if who == 1 and not WATCH else "they") if ev != "PASSED" else ("they" if team else "you"), meaning)
                                        if ev != "FAULT_LET" else "a let, replayed", rally=g.rally, shots=list(g.shots), won=not we_lost, fault=ev)
                    if ev != "FAULT_LET": say(4, [word], priority=1)
                    if ev not in ("FAULT_LET", "PASSED", "FAULT_DOUBLE_BOUNCE") and (who != 0 or WATCH):
                        threading.Timer(1.1, chatter, args=(who, "sorry", 7)).start()
                    log(dict(event=ev, team=team, who=who, rally=g.rally))
                elif ev == "GAME_OVER":
                    g.over = True; g.games_seen += 1; say(4, ["game"], priority=1)
                    for s in (0, 1) if arg == 0 else (2, 3):
                        if s != 0 or WATCH: threading.Timer(1.5 + 0.6 * (s & 1), chatter, args=(s, "nice" if s & 1 else "yes", 6)).start()
                    if partner_mode and g.last_point and g.situation: bump(g.situation, standing["directive"], g.last_point["won"]); save_hist()
                    log(dict(event=ev, winner=arg, score=(mb.a, mb.b)))
                elif ev == "SCORE_CALL":
                    # the point before this call: who won it (the serving team scored) and whether the serve changed
                    if prev is not None and g.last_point is not None and g.last_point.get("fault") != "FAULT_LET":
                        scored = (mb.a, mb.b) != (prev.a, prev.b)
                        won = (prev.serving() == 0) if scored else (prev.serving() != 0)
                        g.last_point["won"] = won
                        if prev.serving() != mb.serving(): threading.Timer(0.2, chatter, args=(4, "sideout"), kwargs=dict(gap=0.5)).start()
                        elif prev.server_num() != mb.server_num() and mb.doubles(): threading.Timer(0.2, chatter, args=(4, "secondserver"), kwargs=dict(gap=0.5)).start()
                        if partner_mode: bump(g.situation, standing["directive"], won)
                        log(dict(point=("won" if won else "lost"), last=g.last_point["text"], rally=g.last_point["rally"], under=DNAME[standing["directive"]], situation=g.situation))
                    g.situation = situation(mb); g.shots = []; g.rally = 0; g.hitter = None
                    if partner_mode and USE_MODEL and not busy() and not order["hold"] and (order["directive"] is None or told):
                        ask_now(mb, g)
                else: log(dict(event=ev, arg=arg))

            # what player 1 said (R1)
            while heard:
                e = heard.pop(0)
                if e[0] == "listen": ra.cmd("SHOW_MSG Your partner is listening... (let go of R1)", reply=False); continue
                _, words, secs, why = e
                if not words:
                    if why: ra.cmd("SHOW_MSG " + why, reply=False)
                    log(dict(heard="", why=why)); continue
                key = obey(words, mb, g)
                log(dict(heard=words, asr_s=round(secs, 2), order=key))
                if key is None:
                    ra.cmd('SHOW_MSG You: "%s"' % words[:70], reply=False)
                    if USE_MODEL and partner_mode and not busy(): ask_now(mb, g)
            for b in [b for b in pending if "ans" in b]:
                pending.remove(b); take_answer(b, mb, g)

            if STANDIN and not WATCH: standin_step(mb)
            if time.time() - hist_t > 60: save_hist(); hist_t = time.time()
            prev = mb
            time.sleep(0.006)                                     # with the UDP round trip, 40-50 polls a second
    finally:
        pad.release_all(); save_hist()
        if gate["open"]: release()
        ra.poke(MB + 10, [0], WR)                               # not announced any more: the cartridge calls its own
        log(dict(end=True, polls=polls, gates=gate["released"], spoken=speaker.spoken, dropped=speaker.dropped,
                 missing_wavs=len(speaker.missing), mean_think_s=round(sum(thinks) / max(len(thinks), 1), 1)))
