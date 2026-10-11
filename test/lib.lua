-- NES Pickleball test library for Mesen's test runner. test/run.sh prepends this to each test (the sandbox has
-- no dofile). T.run(body, name) drives the body as a coroutine, one resume per frame; T.step(n) runs n frames
-- with the current input; T.press(table) sets the input (e.g. {a=true}); T.rd/T.wr read and write RAM.
T = {}
T.failures = 0
T.frames = 0
T.input = {}
local mem = emu.memType.nesMemory

function T.rd(addr) return emu.read(addr, mem) end
function T.rd16(addr) return emu.read(addr, mem) + 256 * emu.read(addr + 1, mem) end
function T.wr(addr, v) emu.write(addr, v, mem) end
function T.press(buttons) T.input = buttons or {} end

function T.step(n)
  local target = T.frames + (n or 1)
  while T.frames < target do coroutine.yield() end
end

function T.tap(button, hold)
  local t = {}; t[button] = true
  T.press(t); T.step(hold or 2); T.press({}); T.step(2)
end

function T.check(cond, msg)
  if not cond then
    T.failures = T.failures + 1
    print("FAIL: " .. tostring(msg))
  end
  return cond
end

function T.done(name)
  local code = (T.failures == 0) and 0 or 1
  print(string.format("%s: %d failures, %d frames", name, T.failures, T.frames))
  emu.stop(code)
end

function T.run(body, name)
  local co = coroutine.create(function() body(); T.done(name) end)
  local function onFrame()
    T.frames = T.frames + 1
    if coroutine.status(co) ~= "dead" then
      local ok, err = coroutine.resume(co)
      if not ok then print("ERROR: " .. tostring(err)); emu.stop(2) end
    end
  end
  local function onPoll()                 -- setInput only works from the inputPolled callback
    emu.setInput(T.input, 0)
  end
  emu.addEventCallback(onFrame, emu.eventType.endFrame)
  emu.addEventCallback(onPoll, emu.eventType.inputPolled)
end

-- the RAM map of this build (generated into the concatenated test by test/run.sh as RAM = {...})
function T.sym(name) return RAM[name] end
