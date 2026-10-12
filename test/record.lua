-- The listening rig: plays the cartridge and records what Mesen's mixer outputs to a WAV (needs the
-- emu.startWaveRecording call from tools/mesen-wave.patch). Every game event is printed with the frame it
-- happened on, counted from the start of the recording, so tools/loudness.py can cut and measure each sound.
-- PB_MODE: 4 exhibition (default), 0 1P singles; PB_SECS: seconds to record (default 40); PB_LA/PB_LB: levels;
-- PB_STYLE: how a hit sounds (src/sound.h: 0 sample over synth, 1 sample, 2 synth).
-- REC (prepended by test/run.sh's REC=path) is the WAV's path.
local SECS = PB.SECS or 40
local MODE = PB.MODE or 4
local mb = T.sym("mb_magic")
local function mbx(i) return T.rd(mb + i) end
T.run(function()
  T.step(30)
  if PB.STYLE then T.wr(T.sym("snd_style"), PB.STYLE) end
  local f0 = T.frames
  emu.startWaveRecording(REC or (OUT .. "/rec.wav"))
  print(string.format("REC start frame %d", f0))
  T.step(30)
  if MODE == 4 then T.tap("left", 3) end              -- the menu wraps from 1 PLAYER SINGLES to EXHIBITION
  T.tap("start", 3)
  T.step(2)
  if PB.LA then T.wr(T.sym("team_level"), PB.LA) end
  if PB.LB then T.wr(T.sym("team_level") + 1, PB.LB) end
  print(string.format("EV %d 0 0 start", T.frames - f0))
  local last_n = mbx(7)
  while T.frames - f0 < SECS * 60 do
    T.step(1)
    local n = mbx(7)
    if n ~= last_n then
      last_n = n
      print(string.format("EV %d %d %d", T.frames - f0, mbx(8), mbx(9)))
    end
  end
  emu.stopWaveRecording()
  print(string.format("REC stop frame %d", T.frames))
end, "record")
