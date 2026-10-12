# Pently (vendored)

The music and sound-effect engine by Damian Yerrick, from https://github.com/pinobatch/pently at commit
`8035576362c0257b9300fa2cc0f0ee7f905d5def` (2026-09-22), under the zlib licence in `LICENSE`. These are the
files the cartridge needs, plus the two documents that describe the score language and the API.

Altered for NES Pickleball (zlib clause 2):

- `pentlymusic.s` includes `pentlybss.inc` from the include path (the build generates it into `build/`) instead
  of `../obj/nes/pentlybss.inc`.

The configuration is the cartridge's own, `src/pentlyconfig.inc`; the score is `audio/pickleball.pently`.
