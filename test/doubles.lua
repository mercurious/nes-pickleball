-- 1 PLAYER DOUBLES with the human idle for one game: the computer partner (slot 1) must return the serves that
-- come to it. Fails on v0.0.1: team A's "who takes it" only ran from slot 0, which is the human, so the partner
-- never owned a ball (0 returns over 11 serves on 2026-10-11). Not in run.sh's default list until the fix lands.
T.run(function()
  T.step(60)
  T.tap("right", 3)                         -- the mode line: 1 PLAYER DOUBLES
  T.step(5)
  T.tap("start", 3)
  T.step(10)
  T.check(T.rd(T.sym("mode")) == 1, "the menu selected 1 PLAYER DOUBLES")
  local hits, serves, last = {[0]=0, 0, 0, 0}, {[0]=0, 0, 0, 0}, 0
  for f = 1, 20000 do
    T.step(1)
    if T.rd(T.sym("mode")) ~= 1 then break end   -- the game ended: back to the title (and the demo)
    local sh = T.rd(T.sym("shots"))
    if sh ~= last then
      local lh = T.rd(T.sym("last_hitter"))
      if lh < 4 then
        if sh == 1 then serves[lh] = serves[lh] + 1 else hits[lh] = hits[lh] + 1 end
      end
      last = sh
    end
  end
  for s = 0, 3 do print(string.format("slot %d: serves %d, rally hits %d", s, serves[s], hits[s])) end
  T.check(hits[1] > 0, "the computer partner returned at least one ball")
end, "doubles")
