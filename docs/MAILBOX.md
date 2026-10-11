# The mailbox: the ROM's contract with a sidecar (v1)

Sixteen bytes at the top of the NES's internal RAM, `$07F0-$07FF`, mirrored by the game every frame and read
or written by anything that can reach the emulator's memory: RetroArch's network commands
(`READ_CORE_MEMORY 7f0 16` with fceumm, `READ_CORE_RAM 7f0 16` with nestopia; `WRITE_CORE_MEMORY` /
`WRITE_CORE_RAM` to write), Mesen's Lua (`emu.read(0x7F0, emu.memType.nesMemory)`), a flash cart's debugger.
The full RAM map with every other variable is generated into `docs/RAM_MAP.md` by the build.

| address | name | who writes | meaning |
|---|---|---|---|
| $07F0 | MB_MAGIC | ROM | $5B while a match is running (0 on the title and menu) |
| $07F1 | MB_VERSION | ROM | 1 |
| $07F2 | MB_STATE | ROM | 0 PRESERVE, 1 CALL (waiting for the score call), 2 SERVE_DROP (the ball drops from the server's hand), 3 FLIGHT (the serve in the air), 4 RALLY, 5 DEAD (the umpire's call shows), 6 GAMEOVER |
| $07F3 | MB_SCORE_A | ROM | team A's points (team A is the near court: slots 0 and 1) |
| $07F4 | MB_SCORE_B | ROM | team B's points (the far court: slots 2 and 3) |
| $07F5 | MB_SERVER | ROM | bit 0 the serving team, bits 1-2 the server number (1 or 2), bits 3-4 the serving player's slot |
| $07F6 | MB_MODE | ROM | bits 0-2 the mode (0 1P singles, 1 1P doubles, 2 2P doubles, 3 2P singles, 4 exhibition), bits 4-6 the level 1-5 |
| $07F7 | MB_EVENT_N | ROM | increments on every event; poll this |
| $07F8 | MB_EVENT | ROM | the last event's code (below) |
| $07F9 | MB_EVENT_ARG | ROM | the slot (0-3) or team (0-1) the event concerns |
| $07FA | MB_CALL | both | the serve gate. The ROM writes 1 when it wants the score called; a sidecar that has announced itself (by writing $A5 here when it starts) sees 1, speaks, and writes 2; the ROM then serves (and writes 0... it keeps $A5 while a sidecar is present). Without a sidecar the ROM proceeds after 90 frames; with one, after 240 frames at most. |
| $07FB | MB_DIRECTIVE | sidecar | an order for the computer partner of a human (1P DOUBLES): 0 its own call, 1 it takes the middle, 2 it leaves the middle, 3 stays back, 4 comes up to the line, 5 covers the other half, 6 plays soft (patient), 7 attacks every ball above the net |
| $07FC | MB_CHATTER | sidecar | bit 0 chatter wanted, bit 1 the human calls the score (reserved for v0.0.2) |
| $07FD | MB_BUBBLE | sidecar | a phrase code: the ROM shows it as a speech bubble beside the player in MB_BUBBLE_SLOT for 45 frames, then clears it and this byte. 1 MINE! 2 YOURS! 3 SWITCH! 4 GOT IT 5 BOUNCE IT 6 NICE! 7 SORRY 8 OUT! 9 GO! 10 BACK! 11 UP! 12 MIDDLE! 13 STAY! 14 NO! 15 GOOD 16 HELP! |
| $07FE | MB_BUBBLE_SLOT | sidecar | the slot 0-3 (written together with MB_BUBBLE, in one write of two bytes) |
| $07FF | MB_RESERVED | tests | the rally-result hook: 1 or 2 ends the live rally at once with team A or B losing (a "passed ball"), for the rules oracle. A sidecar never writes it. |

## Events (MB_EVENT)

| code | event | arg |
|---|---|---|
| 1 | SCORE_CALL | the server's slot; the call is "A-B-n" (serving team's score first; no n in singles) |
| 2 | SERVE | slot |
| 3 | RETURN | slot (the second shot of the rally) |
| 4 | THIRD_SHOT | slot |
| 5 DINK, 6 DRIVE, 7 LOB, 8 VOLLEY, 9 SMASH | the kind of the shot just hit | slot |
| 10 | BOUNCE_IT | the slot that says it (the ball is going long) |
| 11 MINE, 12 YOURS, 13 SWITCH | team talk | slot |
| 14 | POINT_WON | team |
| 15 FAULT_OUT, 16 FAULT_NET, 17 FAULT_LET (replay), 18 FAULT_SHORT (a serve into the kitchen), 19 FAULT_WRONG_COURT, 20 FAULT_FOOT (a serve not hit before it bounced), 21 FAULT_TWO_BOUNCE, 22 FAULT_DOUBLE_BOUNCE, 23 FAULT_KITCHEN, 24 FAULT_BODY, 25 FAULT_NET_TOUCH | the umpire's call | the team at fault |
| 26 | SIDE_OUT | the team that serves next |
| 27 | SECOND_SERVER | the new server's slot |
| 28 | GAME_OVER | the winning team |
| 29 | NICE_SHOT | slot |
| 30 | LONG_RALLY | the rally's length (every 10 shots) |
| 31 | PASSED | the team that let the ball by (it bounced on their side and went out) |

## Slots and sides

Slot 0 is player 1 (the human in every mode but exhibition), slot 1 their partner, slots 2 and 3 the far team;
in singles only 0 and 2 play. Team A's "right" is the screen's right; team B's "right" is the screen's left.
Positions are in `docs/RAM_MAP.md` (`px`, `py`: four 16-bit words each, 8.8 fixed point; the ball `bx`, `by`,
`bz` and its velocities `vx`, `vy`, `vz`).

## Stability

Mailbox v1 is frozen with v0.0.1. Additions bump MB_VERSION and keep every meaning above. The ROM never waits
on the mailbox except at the serve gate, treats any value outside the ranges above as "do nothing", and plays
identically when nothing writes to it. `sidecar/stub.py` exercises the whole contract from a desktop RetroArch.
