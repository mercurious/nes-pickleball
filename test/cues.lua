-- Every cue starts and ends: no channel is left sounding. A real exhibition (rallies, faults, jingles, the voice,
-- applause) runs for PB_SECS (default 150) seconds, and the APU's own state (Mesen's emu.getState) is read:
--  * at every serve, before the serve's own hit, all five channels are silent (v0.0.1 failed this: its effects
--    held their first step until the next one);
--  * whenever the DMC is idle its level is back on the resting level, 64, within 4 frames (every sample starts and
--    ends there, and the driver sets it again when its frame count for a sample runs out, which has up to 3
--    frames' margin; dmc_fixes counts the samples cut off by another, a hit right after a bounce).
local SECS = PB.SECS or 150
local mb = T.sym("mb_magic")
local function mbx(i) return T.rd(mb + i) end
local function sounding(st)
  local function pulse(p) return st[p .. "envelope.lengthCounter.counter"] > 0 and st[p .. "envelope.volume"] > 0
      and st[p .. "realPeriod"] >= 8 end
  local s = {}
  if pulse("apu.square1.") then s[#s + 1] = "pulse 1" end
  if pulse("apu.square2.") then s[#s + 1] = "pulse 2" end
  if st["apu.triangle.lengthCounter.counter"] > 0 and st["apu.triangle.linearCounter"] > 0 then s[#s + 1] = "triangle" end
  if st["apu.noise.envelope.lengthCounter.counter"] > 0 and st["apu.noise.envelope.volume"] > 0 then s[#s + 1] = "noise" end
  if st["apu.dmc.bytesRemaining"] > 0 then s[#s + 1] = "dmc" end
  return s
end
T.run(function()
  T.step(60); T.tap("left", 3); T.tap("start", 3)
  local last_n, serves, quiet, drift, run, f0 = mbx(7), 0, 0, 0, 0, T.frames
  while T.frames - f0 < SECS * 60 do
    T.step(1)
    local n = mbx(7)
    local st = emu.getState()
    if st["apu.dmc.bytesRemaining"] == 0 and st["apu.dmc.bufferEmpty"] and st["apu.dmc.outputLevel"] ~= 64 then
      run = run + 1
      if run == 5 then
        drift = drift + 1
        print(string.format("DMC level %d while idle at frame %d", st["apu.dmc.outputLevel"], T.frames))
      end
    else run = 0 end
    if n ~= last_n then
      last_n = n
      if mbx(8) == 2 then                       -- SERVE: printed before the serve's sounds are processed
        serves = serves + 1
        local s = sounding(T.prev or st)
        if #s == 0 then quiet = quiet + 1
        else print(string.format("serve at frame %d: still sounding: %s", T.frames, table.concat(s, ", "))) end
      end
    end
    T.prev = st
  end
  print(string.format("cues: %d serves, %d with every channel silent; the DMC off its resting level %d times; " ..
        "%d samples cut off by another in %d s", serves, quiet, drift, T.rd(T.sym("dmc_fixes")), SECS))
  T.check(serves >= 5, "at least 5 serves in the run")
  T.check(quiet == serves, "every channel silent before every serve")
  T.check(drift == 0, "the DMC's level never off 64 for more than 4 frames while idle")
end, "cues")
