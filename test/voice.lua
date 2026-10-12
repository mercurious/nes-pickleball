-- The umpire's voice must be heard, not just be busy (v0.0.1's played at -25 dB under a stuck tone, and its test
-- only checked that the DMC channel was active). With no sidecar the cartridge calls the score before the first
-- serve: this records the call and the first rally, and test/run.sh hands the WAV to tools/loudness.py, which
-- fails unless the call's loudest 50 ms reach -32 dBFS and every serve's hit -40 dBFS (a hit measures about -30,
-- a word about -26 at Mesen's default mix).
local mb = T.sym("mb_magic")
local function mbx(i) return T.rd(mb + i) end
T.run(function()
  T.step(60)
  emu.startWaveRecording(OUT .. "/voice.wav")       -- an absolute path: Mesen's working directory is its own
  local f0 = T.frames
  T.tap("start", 3); T.step(2)
  T.check(T.rd(T.sym("state")) == 1, "waiting at the score call")
  print(string.format("EV %d 1 0", T.frames - f0))            -- the call starts with the match
  local frames, last_n = 0, mbx(7)
  while T.rd(T.sym("state")) == 1 and frames < 400 do
    T.step(1); frames = frames + 1
  end
  T.check(frames >= 80 and frames < 240, "the call took 80 to 240 frames, took " .. frames)
  for i = 1, 600 do
    T.step(1)
    local n = mbx(7)
    if n ~= last_n then last_n = n; print(string.format("EV %d %d %d", T.frames - f0, mbx(8), mbx(9))) end
  end
  emu.stopWaveRecording()
  print("LOUDNESS --win 1500 --min 1=-32 2=-40")
end, "voice")
