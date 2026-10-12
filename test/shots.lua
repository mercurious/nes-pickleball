-- Screenshots for looking at the drawing: an exhibition, a PNG every PB_EVERY frames (default 7) while a player
-- swings, PB_N of them (default 24), written as hex to the log (the test runner's Lua has no file access);
-- tools/shots.py turns the log into PNGs.
local EVERY = PB.EVERY or 7
local N = PB.N or 24
T.run(function()
  T.step(60); T.tap("left", 3); T.tap("start", 3)
  local n, f = 0, 0
  while n < N and f < 20000 do
    T.step(1); f = f + 1
    local sw = 0
    for s = 0, 3 do sw = sw + T.rd(T.sym("sw_t") + s) end
    if sw > 0 and f % EVERY == 0 then
      local png = emu.takeScreenshot()
      print(string.format("PNG %d %s", T.frames, (png:gsub(".", function(c) return string.format("%02x", c:byte()) end))))
      n = n + 1
    end
  end
end, "shots")
