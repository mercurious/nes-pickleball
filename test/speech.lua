-- The cartridge's voice: with no sidecar, the score call plays on the DMC channel before the first serve.
T.run(function()
  T.step(60); T.tap("start", 3); T.step(5)
  T.check(T.rd(T.sym("state")) == 1, "waiting at the score call")
  local dmc, frames = 0, 0
  while T.rd(T.sym("state")) == 1 and frames < 300 do
    T.step(1); frames = frames + 1
    if (T.rd(0x4015) & 0x10) ~= 0 then dmc = dmc + 1 end
  end
  print(string.format("speech: DMC active on %d of %d call frames", dmc, frames))
  T.check(dmc > 20, "the DMC channel played during the call")
  T.check(T.rd(T.sym("state")) >= 2, "the call ended and the serve began")
  T.check(frames >= 75 and frames < 240, "the call took about 90 to 240 frames, took " .. frames)
end, "speech")
