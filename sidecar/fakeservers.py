#!/usr/bin/env python3
# Stand-ins for llamatalk's three servers, for testing the sidecar on a machine without the models (the laptop):
#   tts-server on 8093:    /health, /v1/audio/voices, /v1/audio/speech (a tone 0.25 s a word, pitched by voice)
#   llama-server on 8099:  /health, /v1/chat/completions (streamed or not): answers PICK <letter> after FAKE_THINK s,
#                          the letter of the choice whose text starts with FAKE_PICK (default UP), else A; a question
#                          that carries "Player 1 just said" also gets a SAY line (FAKE_SAY)
#   llama-server on 8091:  /health, /v1/audio/transcriptions: the words in the file FAKE_ASR_FILE (default
#                          /dev/shm/pickleball-fake-asr.txt), else "what's the score"
# Every request is appended to FAKE_LOG (default build/fakeservers.jsonl) for the tests to read.
# Usage: fakeservers.py [SECONDS]      env: FAKE_THINK (default 1.5), FAKE_PICK, FAKE_SAY, FAKE_ASR_FILE, FAKE_LOG
import json, math, os, re, struct, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
THINK = float(os.environ.get("FAKE_THINK", "1.5")); PICK = os.environ.get("FAKE_PICK", "UP")
SAY = os.environ.get("FAKE_SAY", "You've got this, partner. Score is ours to take.")
ASR_FILE = os.environ.get("FAKE_ASR_FILE", "/dev/shm/pickleball-fake-asr.txt")
LOG = os.environ.get("FAKE_LOG", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "fakeservers.jsonl"))
VOICES = ["alba", "jean", "marius", "cosette", "george", "anna", "bill_boerst"]
lock = threading.Lock()

def log(d):
    with lock:
        os.makedirs(os.path.dirname(LOG), exist_ok=True)
        with open(LOG, "a") as f: f.write(json.dumps(dict(t=round(time.time(), 2), **d)) + "\n")

def tone(secs, hz):
    n = int(24000 * secs); return b"".join(struct.pack("<h", int(6000 * math.sin(2 * math.pi * hz * i / 24000) * min(1, (n - i) / 2400, i / 600))) for i in range(n))

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body, ctype="application/json"):
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)
    def body(self): return self.rfile.read(int(self.headers.get("Content-Length", 0)))
    def do_GET(self):
        if self.path == "/health": return self.send(200, b'{"status":"ok"}')
        if self.path == "/v1/audio/voices": return self.send(200, json.dumps({"voices": VOICES}).encode())
        self.send(404, b"{}")
    def do_POST(self):
        b = self.body(); port = self.server.server_address[1]
        if self.path == "/v1/audio/speech":
            r = json.loads(b or b"{}"); words = len(r.get("input", "").split()); v = r.get("voice", "")
            log(dict(server="tts", text=r.get("input"), voice=v))
            return self.send(200, tone(0.25 * max(words, 1) + 0.1, 200 + 40 * (VOICES.index(v) if v in VOICES else 0)), "audio/pcm")
        if self.path == "/v1/audio/transcriptions":
            try: text = open(ASR_FILE).read().strip()
            except OSError: text = "what's the score"
            log(dict(server="asr", text=text, bytes=len(b))); time.sleep(0.4)
            return self.send(200, json.dumps({"text": "language English<asr_text>" + text}).encode())
        if self.path == "/v1/chat/completions":
            r = json.loads(b); user = r["messages"][-1]["content"]; user = user if isinstance(user, str) else json.dumps(user)
            choices = re.findall(r"^([A-H])\) (.*)$", user, re.M); letter = "A"
            for l, text in choices:
                if text.upper().startswith(PICK.upper()): letter = l
            answer = "PICK %s" % letter + ("\nSAY %s" % SAY if "Player 1 just said" in user else "")
            log(dict(server="llm", pick=letter, say="Player 1 just said" in user, prompt_chars=len(user), stream=bool(r.get("stream"))))
            time.sleep(THINK)
            if not r.get("stream"):
                return self.send(200, json.dumps({"choices": [{"message": {"role": "assistant", "content": answer}}], "timings": {"predicted_per_second": 9.0}}).encode())
            self.send_response(200); self.send_header("Content-Type", "text/event-stream"); self.end_headers()
            self.wfile.write(("data: " + json.dumps({"prompt_progress": {"total": 300, "processed": 300}, "choices": []}) + "\n\n").encode())
            for tok in re.findall(r"\S+\s*|\n", answer):
                self.wfile.write(("data: " + json.dumps({"choices": [{"delta": {"content": tok}}]}) + "\n\n").encode()); self.wfile.flush(); time.sleep(0.02)
            self.wfile.write(("data: " + json.dumps({"choices": [{"delta": {}}], "timings": {"predicted_per_second": 9.0}}) + "\n\ndata: [DONE]\n\n").encode())
            return
        self.send(404, b"{}")

if __name__ == "__main__":
    T = float(sys.argv[1]) if len(sys.argv) > 1 else 3600
    servers = [ThreadingHTTPServer(("127.0.0.1", p), H) for p in (8093, 8099, 8091)]
    for s in servers: threading.Thread(target=s.serve_forever, daemon=True).start()
    print("fakeservers: tts 8093, llm 8099, asr 8091 for %.0f s" % T, flush=True)
    time.sleep(T)
