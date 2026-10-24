#!/usr/bin/env python3
# The voices of the teammate add-on: one Pocket TTS voice per player slot and one for the umpire, a phrase cache
# rendered once (every word a player says in a match, as a WAV per voice), and a speaker that plays them through
# pacat one line at a time. Lines that are not in the cache (the model's) are synthesized live, with the game's
# volume ducked while they play. Pocket TTS is llamatalk's tts-server (port 8093): a request carries its voice.
# The cache: DIR/<voice>/<key>.wav, 24 kHz s16 mono. Usage as a tool: cache.py (next to this file).
# Env: PB_TTS (default http://127.0.0.1:8093), PB_AUDIO=0 (no pacat: a line "plays" as a sleep of its length),
#   PB_DUCK=0 (never touch RetroArch's volume), XDG_RUNTIME_DIR (pacat's PulseAudio/PipeWire).
import json, os, queue, shutil, subprocess, threading, time, urllib.request, wave
TTS = os.environ.get("PB_TTS", "http://127.0.0.1:8093")
RATE = 24000
ENV = dict(os.environ, XDG_RUNTIME_DIR=os.environ.get("XDG_RUNTIME_DIR", "/run/0-runtime-dir"))
AUDIO = os.environ.get("PB_AUDIO", "1") != "0" and shutil.which("pacat") is not None
DUCK = os.environ.get("PB_DUCK", "1") != "0"
GAP = b"\0" * (RATE * 2 * 90 // 1000)           # 90 ms between the words of a score call

NUM = ("zero one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen "
       "seventeen eighteen nineteen twenty twenty-one").split()
# every phrase a voice may need, by key; the digits by their number ("4" -> "four")
PHRASES = {str(i): w for i, w in enumerate(NUM)}
PHRASES.update({
    # team talk and the umpire's words (the cartridge's own bubbles use the same words)
    "mine": "Mine!", "yours": "Yours!", "switch": "Switch!", "bounce": "Bounce it!", "out": "Out!", "nice": "Nice!",
    "sorry": "Sorry!", "gotit": "Got it.", "kitchen": "Kitchen!", "letitbounce": "Let it bounce!",
    "sideout": "Side out.", "secondserver": "Second server.", "fault": "Fault.", "net": "Net.", "let": "Let. Play it again.",
    "short": "Short.", "wrongcourt": "Wrong court.", "twobounce": "Two bounce.", "nettouch": "Net touch.",
    "game": "Game!", "point": "Point.", "rally": "What a rally!", "good": "Good one.", "yes": "Yes!",
    # the partner's calls (MB_DIRECTIVE 0-7) and its answers to orders
    "d0": "My call. I'll play my own game.", "d1": "I've got the middle.", "d2": "You take the middle.",
    "d3": "I'll stay back.", "d4": "Let's get up to the line.", "d5": "I'll switch with you.",
    "d6": "Let's be patient. Soft hands.", "d7": "I'm attacking everything above the net.",
    "ok": "Okay.", "holding": "Holding. Say go.", "go": "Let's go.", "yourcall": "Okay, my call.",
    "again": "Say that again?", "nomic": "I can't hear you. There's no microphone.", "thinking": "Hang on.",
})
SLOT_VOICES = ("alba", "jean", "marius", "cosette")   # player slots 0-3; the umpire is the fifth voice
UMPIRE_VOICE = "george"
ROLES = ("slot0", "slot1", "slot2", "slot3", "umpire")

def voices_from_env(default=SLOT_VOICES + (UMPIRE_VOICE,)):
    """PB_VOICES=alba,jean,marius,cosette,george: the four slots' voices and the umpire's"""
    v = [x.strip() for x in os.environ.get("PB_VOICES", "").split(",") if x.strip()]
    return tuple(v + list(default[len(v):]))[:5] if v else default

def synth(text, voice, timeout=120):
    """PCM s16 24 kHz mono of the text in the voice, or b"" when the server fails"""
    body = json.dumps({"input": text, "voice": voice, "response_format": "pcm"}).encode()
    req = urllib.request.Request(TTS + "/v1/audio/speech", body, {"Content-Type": "application/json"})
    try: return urllib.request.urlopen(req, timeout=timeout).read()
    except Exception: return b""

def tts_up(timeout=2):
    try: return urllib.request.urlopen(TTS + "/health", timeout=timeout).status == 200
    except Exception: return False

def tts_voices():
    """the voices the server has, or [] when it cannot say"""
    try:
        v = json.loads(urllib.request.urlopen(TTS + "/v1/audio/voices", timeout=10).read())
        if isinstance(v, dict): v = v.get("voices") or v.get("data") or []
        return [x if isinstance(x, str) else x.get("name") or x.get("id") or "" for x in v]
    except Exception: return []

def write_wav(path, pcm):
    tmp = path + ".tmp"
    with wave.open(tmp, "wb") as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(pcm)
    os.replace(tmp, path)

def read_wav(path):
    try:
        with wave.open(path, "rb") as w: return w.readframes(w.getnframes())
    except (OSError, wave.Error): return b""

def ensure(cache_dir, voices, keys=None, log=print, stop=None):
    """render what the cache misses: every PHRASES key (or keys) for every voice. Returns (rendered, failed)."""
    keys = list(keys or PHRASES); done = failed = 0; todo = [(v, k) for v in dict.fromkeys(voices) for k in keys
                                                           if not os.path.exists(os.path.join(cache_dir, v, k + ".wav"))]
    if not todo: return 0, 0
    log("voices: rendering %d phrases into %s" % (len(todo), cache_dir)); t0 = time.time()
    for i, (v, k) in enumerate(todo):
        if stop and stop(): break
        os.makedirs(os.path.join(cache_dir, v), exist_ok=True)
        pcm = synth(PHRASES[k], v)
        if pcm: write_wav(os.path.join(cache_dir, v, k + ".wav"), pcm); done += 1
        else: failed += 1
        if (i + 1) % 20 == 0: log("voices: %d/%d (%.0f s)" % (i + 1, len(todo), time.time() - t0))
    log("voices: %d rendered, %d failed, %.0f s" % (done, failed, time.time() - t0))
    return done, failed

def game_volume(pct):
    """RetroArch's own stream to pct % (its sink input), so a duck never touches the spoken lines (as talk.py)"""
    if not DUCK or not AUDIO: return
    try: out = subprocess.run(["pactl", "list", "sink-inputs"], capture_output=True, text=True, timeout=5, env=ENV).stdout
    except (OSError, subprocess.TimeoutExpired): return
    block = None
    for line in out.splitlines() + ["Sink Input #end"]:
        if line.startswith("Sink Input #"):
            if block and "retroarch" in block[1].lower():
                subprocess.run(["pactl", "set-sink-input-volume", block[0], "%d%%" % pct], capture_output=True, timeout=5, env=ENV)
            block = [line.split("#")[1].strip(), ""]
        elif block: block[1] += line

class Speaker(threading.Thread):
    """one line at a time, in order. say(voice, keys) plays cached phrases (a list of keys, joined with short gaps:
    the score call); say(voice, text=...) synthesizes live (ducking the game). Each line may carry a caption
    (SHOW_MSG through ra), a callback for when its audio has ended (the serve gate), and a priority: chatter
    (priority 0) is dropped when more than one line is already waiting; calls (1) always play."""
    def __init__(self, cache_dir, ra=None, log=None):
        super().__init__(daemon=True); self.dir = cache_dir; self.ra = ra; self.log = log or (lambda *a: None)
        self.q = queue.Queue(); self.busy_until = 0.0; self.spoken = 0; self.dropped = 0; self.missing = set()

    def pcm_of(self, voice, keys):
        parts = []
        for k in keys:
            p = read_wav(os.path.join(self.dir, voice, k + ".wav"))
            if not p:
                self.missing.add((voice, k)); p = synth(PHRASES.get(k, k), voice, timeout=30)   # the cache misses it: live
                if p:
                    try: os.makedirs(os.path.join(self.dir, voice), exist_ok=True); write_wav(os.path.join(self.dir, voice, k + ".wav"), p)
                    except OSError: pass
            if p: parts.append(p)
        return GAP.join(parts)

    def waiting(self): return self.q.qsize()

    def say(self, voice, keys=None, text=None, show=None, on_done=None, priority=0):
        """queue a line; returns False when it was dropped (chatter behind a queue)"""
        if priority == 0 and self.q.qsize() >= 2: self.dropped += 1; return False
        self.q.put((voice, list(keys or []), text, show, on_done)); return True

    def run(self):
        while True:
            voice, keys, text, show, on_done = self.q.get()
            try:
                live = text is not None and not keys
                pcm = synth(text, voice) if live else self.pcm_of(voice, keys)
                if show and self.ra: self.ra.cmd("SHOW_MSG " + show, reply=False)
                secs = len(pcm) / (RATE * 2)
                self.busy_until = time.time() + secs
                if live and pcm: game_volume(35)
                if pcm:
                    if AUDIO:
                        subprocess.run(["pacat", "--raw", "--format=s16le", "--rate=%d" % RATE, "--channels=1",
                                        "--client-name=pickleball"], input=pcm, env=ENV, timeout=secs + 10)
                    else: time.sleep(secs)
                if live and pcm: game_volume(100)
                self.spoken += 1
                self.log(dict(said=text or " ".join(PHRASES.get(k, k) for k in keys), voice=voice, audio_s=round(secs, 2), live=live))
            except Exception as ex:
                self.log(dict(speak_error=str(ex), voice=voice, keys=keys, text=text))
            finally:
                if on_done:
                    try: on_done()
                    except Exception: pass
