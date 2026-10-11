-- NES Pickleball test library for Mesen's test runner: run frames, press buttons, read RAM, assert, finish.
-- Usage in a test:  local T = dofile(...)  -- see test/run.sh, which passes the path through a global.
local T = {}
T.failures = 0
T.frames = 0
T.input = {}
local mem = emu.memType.nesMemory

function T.rd(addr) return emu.read(addr, mem) end
function T.rd16(addr) return emu.read(addr, mem) + 256 * emu.read(addr + 1, mem) end
function T.wr(addr, v) emu.write(addr, v, mem) end

function T.press(buttons)   -- buttons: a table like {a=true, left=true}; applied every frame until changed
  T.input = buttons or {}
end

function T.step(n)          -- run n frames, applying the current input (via the inputPolled hook below)
  local target = T.frames + (n or 1)
  while T.frames < target do
    emu.resume()
    coroutine.yield()
  end
end

function T.check(cond, msg)
  if not cond then
    T.failures = T.failures + 1
    emu.log("FAIL: " .. tostring(msg))
    print("FAIL: " .. tostring(msg))
  else
    emu.log("ok: " .. tostring(msg))
  end
end

function T.done(name)
  local code = (T.failures == 0) and 0 or 1
  emu.log(string.format("%s: %d failures, %d frames", name, T.failures, T.frames))
  print(string.format("%s: %d failures, %d frames", name, T.failures, T.frames))
  emu.stop(code)
end

-- the driver: a coroutine runs the test body; every frame end we count and apply the input
function T.run(body, name)
  local co = coroutine.create(function() body(); T.done(name) end)
  local function onFrame()
    T.frames = T.frames + 1
    emu.setInput(T.input, 0)
    if coroutine.status(co) ~= "dead" then
      local ok, err = coroutine.resume(co)
      if not ok then emu.log("ERROR: " .. tostring(err)); print("ERROR: " .. tostring(err)); emu.stop(2) end
    end
  end
  emu.addEventCallback(onFrame, emu.eventType.endFrame)
end
return T
