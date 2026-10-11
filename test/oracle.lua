-- The rules oracle: an exhibition (four computer players) plays many games while a model of Sections 4-6 of the
-- rulebook predicts the score, the serving team, the server number and the server's position after every event.
-- The rally-result hook (mailbox byte $07FF) ends each rally at once with a random loser, so a game takes seconds.
local GAMES = PB.GAMES or 12
local EV = {SCORE_CALL=1, POINT_WON=14, SIDE_OUT=26, SECOND_SERVER=27, GAME_OVER=28, PASSED=31}
local SIDE_X = { [0] = {84, 172}, [1] = {156, 100} }   -- team -> {left, right} screen x (team B's right is our left)

T.run(function()
  local m = { score = {[0]=0, [1]=0}, serving = 0, num = 2, slot = 0, start = {[0]=0, [1]=2}, games = {[0]=0,[1]=0} }
  local seed = 20261011
  local function partner(s) return s ~ 1 end
  local function right_slot(team) local s = m.start[team]; if m.score[team] % 2 == 1 then return partner(s) end; return s end
  local function team(s) return s >> 1 end
  local mb = T.sym("mb_magic")
  local function mbx(i) return T.rd(mb + i) end
  T.step(60)
  T.wr(mb + 10, 0xA5)                     -- announce as a sidecar before the match: the calls are answered below
  T.tap("left", 3)                        -- the menu wraps from 1 PLAYER SINGLES to EXHIBITION
  T.tap("start", 3)
  T.step(10)
  T.check(mbx(0) == 0x5B, "mailbox magic")
  T.check((mbx(6) & 7) == 4, "exhibition mode selected")
  T.check(mbx(10) == 1, "the first serve gate asks for the call (the announcement survived match_init)")
  local last_n = mbx(7)
  local points, games_done, errors = 0, 0, 0
  local frames_cap = 2000000
  while games_done < GAMES and T.frames < frames_cap do
    T.step(1)
    local n = mbx(7)
    if n ~= last_n then
      last_n = n
      local ev, arg = mbx(8), mbx(9)
      local st = T.rd(T.sym("state"))
      if PB.VERBOSE and points < 40 then
        print(string.format("  f%d ev=%d arg=%d n=%d rom %d-%d srv=%d | model %d-%d serving=%d num=%d slot=%d", T.frames, ev, arg, n, mbx(3), mbx(4), mbx(5), m.score[0], m.score[1], m.serving, m.num, m.slot))
      end
      if ev == EV.SCORE_CALL then
        -- the ROM's score, server and position must match the model before every serve
        local ok = true
        ok = T.check(mbx(3) == m.score[0] and mbx(4) == m.score[1], string.format("score %d-%d (model %d-%d) at point %d", mbx(3), mbx(4), m.score[0], m.score[1], points)) and ok
        local srv = mbx(5)
        ok = T.check((srv & 1) == m.serving, "serving team " .. (srv & 1) .. " vs model " .. m.serving) and ok
        ok = T.check(((srv >> 1) & 3) == m.num, "server number " .. ((srv >> 1) & 3) .. " vs model " .. m.num) and ok
        ok = T.check(((srv >> 3) & 3) == m.slot, "server slot " .. ((srv >> 3) & 3) .. " vs model " .. m.slot) and ok
        ok = T.check(arg == m.slot, "SCORE_CALL names the server") and ok
        -- the server stands on the side the team's score says (5.B.3), behind the baseline
        local px = T.rd16(T.sym("px") + 2 * m.slot) >> 8
        local py = T.rd16(T.sym("py") + 2 * m.slot) >> 8
        local right = (right_slot(m.serving) == m.slot)
        local want = SIDE_X[m.serving][right and 2 or 1]
        ok = T.check(px == want, string.format("server x %d, wanted %d (right=%s)", px, want, tostring(right))) and ok
        ok = T.check((m.serving == 0 and py == 208) or (m.serving == 1 and py == 64), "server behind the baseline, y=" .. py) and ok
        if not ok then errors = errors + 1 end
        if points % 25 == 0 then print(string.format("oracle: point %d, game %d, frame %d", points, games_done + 1, T.frames)) end
        -- answer the serve gate (the sidecar's job), then end the rally at once with a random loser
        T.step(2)
        T.check(mbx(10) == 1, "the serve gate asks for the call")
        T.wr(mb + 10, 2)
        T.step(40)
        seed = (seed * 1103515245 + 12345) % 2147483648
        local loser = (seed >> 16) & 1
        T.wr(mb + 15, loser + 1)
        points = points + 1
      elseif ev == EV.PASSED or (ev >= 15 and ev <= 25 and ev ~= 17) then
        -- the model applies the outcome (a fault or a passed ball; a let replays)
        local loser = arg
        if loser ~= m.serving then
          m.score[m.serving] = m.score[m.serving] + 1
        else
          if m.num == 1 then m.num = 2; m.slot = partner(m.slot)
          else m.serving = 1 - m.serving; m.num = 1; m.slot = right_slot(m.serving) end
        end
      elseif ev == EV.POINT_WON then
        T.check(arg == m.serving, "a point goes to the serving team")
      elseif ev == EV.SECOND_SERVER then
        T.check(arg == m.slot, "second server is the partner")
      elseif ev == EV.SIDE_OUT then
        T.check(arg == m.serving, "side out to the other team")
      elseif ev == EV.GAME_OVER then
        local w = arg
        T.check(m.score[w] >= 11 and m.score[w] - m.score[1 - w] >= 2, string.format("game won at %d-%d", m.score[w], m.score[1-w]))
        games_done = games_done + 1
        m.games[w] = m.games[w] + 1
        -- the ROM returns to the title after a game in one-game mode; start the next exhibition
        T.step(290)
        T.wr(mb + 10, 0xA5)
        T.tap("left", 3); T.tap("start", 3); T.step(10)
        m.score[0] = 0; m.score[1] = 0; m.serving = 0; m.num = 2; m.slot = 0
        last_n = mbx(7)
      end
    end
  end
  T.check(games_done >= GAMES, string.format("%d games played (%d points, %d bad calls, %d frames)", games_done, points, errors, T.frames))
  print(string.format("oracle: %d games, %d points, %d frames", games_done, points, T.frames))
end, "oracle")
