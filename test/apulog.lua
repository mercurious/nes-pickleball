-- A reference recorder for any NES ROM (used on NES Tennis, which is never committed): records the mixer to REC
-- and prints every write to the APU ($4000-$4017) as "W frame addr value", so a sound can be read register by
-- register. The input is a crude player: Start through the menus, then A every 24 frames while walking left and
-- right. PB_SECS: seconds (default 90).
local SECS = PB.SECS or 90
T.run(function()
  emu.addMemoryCallback(function(addr, value)
    print(string.format("W %d %04X %02X", T.frames, addr, value))
  end, emu.callbackType.write, 0x4000, 0x4017)
  emu.startWaveRecording(REC or (OUT .. "/ref.wav"))
  T.step(90)
  for i = 1, 4 do T.tap("start", 4); T.step(60) end
  local dir = "left"
  for f = 1, SECS * 60 - 330, 24 do
    if (f // 24) % 6 == 0 then dir = (dir == "left") and "right" or "left" end
    local t = {a = true}; t[dir] = true
    T.press(t); T.step(3)
    t = {}; t[dir] = true
    T.press(t); T.step(21)
  end
  emu.stopWaveRecording()
end, "apulog")
