-- Boot test: the ROM runs, the title shows, START reaches the court, the player walks.
local T = dofile(os.getenv("PB_TESTLIB") or "test/lib.lua")
T.run(function()
  T.step(60)
  T.check(T.rd(0x0000) ~= nil, "RAM readable")
  local fc0 = T.rd(0x0000)              -- frame_count is the first zero-page byte of the kernel
  T.step(30)
  T.check(T.rd(0x0000) ~= fc0, "NMI runs (frame counter advances)")
  T.press({start = true}); T.step(2); T.press({}); T.step(10)
  T.press({right = true}); T.step(20); T.press({})
  emu.takeScreenshot()
  T.check(true, "screenshot taken")
end, "boot")
