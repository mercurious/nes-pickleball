-- The soak: real rallies, computer against computer, G games at level LA (team A) vs LB (team B).
-- Reports games won, points, rally lengths, the computer's own faults, and the worst frame's main-loop time.
-- Env: PB_SOAK_GAMES, PB_LA, PB_LB. Fails on a hang (no event for 3000 frames) or a fault storm.
local G = PB.SOAK_GAMES or 4
local LA = PB.LA or 3
local LB = PB.LB or 3
T.run(function()
  local mb = T.sym("mb_magic")
  local function mbx(i) return T.rd(mb + i) end
  local won, points, rallies, faults, cpu_kitchen, cpu_2b, longest = {[0]=0,[1]=0}, 0, 0, 0, 0, 0, 0
  local function start()
    T.step(30); T.tap("left", 3); T.tap("start", 3); T.step(10)
    T.wr(T.sym("team_level"), LA); T.wr(T.sym("team_level") + 1, LB)
  end
  T.step(60); start()
  T.check(mbx(0) == 0x5B and (mbx(6) & 7) == 4, "exhibition running")
  local last_n, idle, games = mbx(7), 0, 0
  local rally = 0
  while games < G do
    T.step(1)
    local n = mbx(7)
    if n == last_n then
      idle = idle + 1
      if idle > 3000 then T.check(false, "no event for 3000 frames (hang?) at state " .. T.rd(T.sym("state"))); break end
    else
      idle = 0; last_n = n
      local ev, arg = mbx(8), mbx(9)
      if ev >= 2 and ev <= 9 then rally = rally + 1 end
      if ev == 21 then cpu_2b = cpu_2b + 1 end
      if ev == 23 then cpu_kitchen = cpu_kitchen + 1 end
      if (ev >= 15 and ev <= 25 and ev ~= 17) or ev == 31 then
        points = points + 1; rallies = rallies + rally; if rally > longest then longest = rally end
        if ev ~= 31 and ev ~= 22 then faults = faults + 1 end
        rally = 0
      end
      if ev == 28 then
        won[arg] = won[arg] + 1; games = games + 1
        print(string.format("game %d: %d-%d to team %d (frames %d)", games, mbx(3), mbx(4), arg, T.frames))
        T.step(300); start(); last_n = mbx(7)
      end
    end
  end
  print(string.format("soak LA=%d LB=%d: games A %d B %d, points %d, mean rally %.1f, longest %d, faults %d, kitchen %d, two-bounce %d, frames %d",
    LA, LB, won[0], won[1], points, points > 0 and rallies / points or 0, longest, faults, cpu_kitchen, cpu_2b, T.frames))
  T.check(points > 0 and (cpu_kitchen + cpu_2b) * 10 <= points, "the computer's kitchen and two-bounce faults under 1 in 10 points")
  T.check(points > 0 and rallies / points >= 3, "mean rally length at least 3 shots")
end, "soak")
