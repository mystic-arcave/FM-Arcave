# Mystic machines and instruments: the slots, and writing one

The Arcave has three ledges with a machine on each. The firmware carries three machines (CRYSTAL ROAD, JUICE WORKS,
NIGHT BLOOM), and a new install shows them as ever. Each ledge also has a **slot**: 40 KiB of the FM-1's data flash for
one machine that is *loaded*, not built in. The site's [MACHINES page](https://mystic-arcave.github.io/FM-Arcave/machines/)
chooses what stands on each ledge and sends loaded machines to an FM-1, the way presets go into a synth. Nothing in
the game itself chooses machines, and a loaded machine costs the firmware image nothing.

The music room is the same. It has four places; the firmware carries an instrument for each (ROOTS, THE GEODE, MUSHROOM
RACK, CRYSTAL SEAM), and each place has a slot of 40 KiB for one **loaded instrument** that stands there instead when
the page says so. THE SPRING is the first.

> **Where this stands.** Everything here runs in the tests and in the browser build, where every loadable machine and
> instrument is also built in. On an FM-1 what is loaded runs from its flash; that path is built with the FM-1's own toolchain and
> follows what Umbra Live's plate bank does on the same hardware, but it has **not yet been tried on an FM-1**.

## For players

- **MACHINES page, MY FM-1** (USB or Bluetooth): the FM-1 shows its browser screen while the page reads its ledges.
  Pick a machine for a ledge. One of the firmware's own is there at once; a loaded one is sent into that ledge's slot
  (a few seconds by USB), checked by the FM-1 byte for byte, and stands there from then on. DONE lets the FM-1 go: it
  starts again with its ledges as you left them.
- **THE BROWSER'S GAME** has ledges of its own, kept in the browser.
- A ledge is never empty. If a loaded machine is damaged, made for a newer FM-ARCAVE, or its slot is emptied, the
  ledge shows the firmware's own machine again. If the FM-1 ever restarts while inside a loaded machine, loaded
  machines rest until it has been switched off and on, so a machine that hangs cannot keep the FM-1 from starting.
- A loaded machine has the same frame as the others: a start card, PLAY, the score and hearts, tickets after a game,
  REC to leave. HOME shows the machine's own words on how it is played. Its best score is kept in your save; which
  machine stands where is in the save too, the loaded machines themselves are not ([SAVES](https://mystic-arcave.github.io/FM-Arcave/save/)).
- **The music room** is on the same page, under the ledges: pick an instrument for a place. A loaded instrument is an
  instrument like the others: the same knobs and buttons, EDIT, the loop, the mixer, its own lesson the first time
  and its topics under HOME held. The firmware's instrument of that place and the loaded one each keep their own
  layout and settings in the save, so swapping back and forth loses neither; only that place's part of the loop is
  wiped, because it is made of the other one's sounds.
- **Playing together** needs the same instruments in the same places on every FM-1, since each plays the whole room
  itself. An FM-1 that comes into a room where a friend with other instruments already is gets the line OTHER
  INSTRUMENTS and has its own room until that friend leaves; friends with the same loaded instruments share a room
  as ever.

## How it is built

| Part | Where |
| --- | --- |
| What a loaded machine is, and everything it may use | `firmware/src/arcave/mm.h` |
| What a loaded instrument is, and everything it may use | `firmware/src/arcave/mi.h` |
| The ledges' slots and the room's places', the bank, the checks, the page's commands | `firmware/src/arcave/ac_slots.h` |
| A loaded machine being played (the firmware's functions as it gets them) | `firmware/src/arcave/ac_mmrun.h` |
| A loaded instrument in a place of the room | `firmware/src/arcave/ac_mirun.h`; which stands where, and both kept in the save: `room_kit`, `room_unpack` in `ac_hall.h`; friends with other instruments: `rn_kit` in `ac_roomnet.h` |
| The bank in flash, run in place; the mark that makes loaded machines rest | `firmware/src/platform.c` (`bank_*`), `firmware/hal/fm1_guard.h` |
| The machines | `firmware/machines/*.c` (GLOW CAPS: `glowcaps.c`) |
| The instruments | `firmware/instruments/*.c` (THE SPRING: `spring.c`) |
| Their files and the catalogue | `tools/mm_build.py` -> `build/machines/` |
| The page's side | `web/emu/machines.js`, `web/site/machines/index.html` |
| Tests | `tests/machines_test.mjs`, `tests/room_test.mjs` (its last part), `tests/scripts/glowcaps.txt` (`MOD=1 tests/run_arcave.sh glowcaps 4 3`), `tests/scripts/spring.txt` (`INS=2 tests/run_arcave.sh spring 4 11`) |

**The bank** is 280 KiB of the FM-1's data area from flash `0x97000`: seven slots of `0xA000`, the ledges' three and
then the room's four places', ending at `0xDD000` (the first 128 KiB are the platform's slot area, the rest lies
beyond it in the same data area, which FM-ARCAVE uses for nothing else). The CPU sees it at `0x02093000` through the
plain XIP window. The firmware erases and writes it by the sector and runs what is loaded where it lies; the
instruction-fetch guard is widened to the bank's end (`FM1_CODE_BANK_END`). Every call into loaded code is marked, so
an FM-1 that restarts inside it runs nothing loaded until it has been switched off and on.

**A machine file** (`.mmach`; an instrument's is `.minst`, the same in every way but its first word) is the machine as slot 0 holds it, followed by the list of places in it that hold an
address. The tool finds them by linking the machine twice, two slots apart: every 32-bit value that moved by exactly
that distance and points into the machine is one (at any halfword: pi32v2 loads a constant with a 48-bit instruction).
The page adds the slot's distance to each and writes the machine's size and CRC-32 into its header before sending.

**Before a machine is run** the firmware checks its first word, its length, the CRC-32 of all of it, the API it was
built for, the RAM it asks for, and that every address in its header lies inside its own slot (which also refuses a
machine that was not moved to that slot).

**The link** is PLAY WITH MY FM-1's (`ac_remote.h`, protocol 3), control changes on MIDI channel 16:

| | | |
| --- | --- | --- |
| to the FM-1 | CC 60 k | what the next save-shaped transfer is: 0 a save, 1 + bank slot a piece of what is being loaded into that slot (the ledges' slots 0..2, the places' 3..6) |
| | CC 61 v | 0: tell the bank's state. 16 + slot: empty the slot, a machine is coming. 32 + slot: that was all, check it. 48 + 4 x slot + w: the ledge shows the firmware's machine w (0..2), or (3) its loaded one. 64 + slot: empty the slot. For a place of the room: 80 + place an instrument is coming, 84 + place that was all, 88 + place empty its slot, 96 + 2 x place + w the firmware's own instrument (0) or the loaded one (1) |
| from it | CC 62 r | the answer: 0 done, 1 not now, 2 damaged, 3 the flash would not, 4 whole but not for this firmware |
| | CC 63 1 | the next save-shaped transfer from the FM-1 is the bank's state (`mb_info`) |

A machine goes over in pieces of 1 KiB, each answered (CC 79) before the next.

## Writing a machine

Start from `firmware/machines/glowcaps.c`. A machine is one C file:

```c
#include "../src/arcave/mm.h"
typedef struct { ... } my_t;                      /* all of its RAM */
static void my_start(void *st, mm_ctx_t *c, const mm_api_t *a) { ... }   /* a game begins; st is all zero */
static void my_tick(void *st, mm_ctx_t *c, const mm_api_t *a)  { ... }   /* a frame of the game */
static void my_draw(void *st, mm_ctx_t *c, const mm_api_t *a)  { ... }   /* its picture, under the cards too */
MM_MACHINE(mine) = { MM_MAGIC, MM_API, sizeof(my_t), 17, 1, 0, 0,   /* its number (16 is GLOW CAPS), its version */
    2, 5, 40, 0,                                 /* its colour (0..4), hearts at the start, a ticket per 40 points */
    "MY MACHINE", MY_HELP, MY_SIGN, my_start, my_tick, my_draw };
```

The rules, all of them checked by the build or by the firmware:

- **No variables of its own.** No `static` or global that is not `const`: its RAM is `st`, as many bytes as the header
  says (up to 4096), zero when its ledge is walked up to and when a game starts. Tables and texts are `static const`.
- **Nothing of the firmware's by name.** Everything comes through `a` (drawing, sound, numbers, `a->over()` when the
  game is lost) and `c` (the clocks, keys, knobs and buttons, the score and lives, the lights it wants). `mm.h` lists
  them all. No C library, no floating point, no 64-bit division.
- **The firmware owns** HOME, PLAY, REC and GLO, the cards, the score and hearts on the screen, the tickets.
- **On the beat.** `c->beat_q8` is the jukebox's clock, 256 a beat; it stands still while the game is held.
- **Help in seven languages**, `help[0]` English; a language left 0 shows the English.
- **A sign**, sixteen rows of sixteen dots, for its cabinet in the cave.

Then:

1. Add it to `MM_TWINS` in `ac_slots.h` (so the browser game and the tests have it) and to `MACHINES` in
   `tools/mm_build.py` (the catalogue: its number, name, version, a line about it).
2. Play it on this computer: `MOD=1 tests/run_arcave.sh NAME 4 3` with a script in `tests/scripts/`, or in the browser
   build (`web/emu/build.sh`, then the MACHINES page's THE BROWSER'S GAME).
3. `python3 tools/mm_build.py` makes `build/machines/NAME.mmach` and the catalogue (it needs the FM-1 toolchain for a
   file an FM-1 takes; without it the file is for the browser game only). It refuses a machine that has variables of
   its own, calls something outside itself, or holds an address the page could not move.

**The API only grows.** A machine built for `MM_API` n runs on every firmware whose `MM_API` is n or more: new
functions go at the end of `mm_api_t`, new facts at the end of `mm_ctx_t`, nothing is moved or removed.

Machine numbers: 0 to 2 are the firmware's own; 16 GLOW CAPS; take the next free one for yours.

## Writing an instrument

Start from `firmware/instruments/spring.c`, and read the head of `mi.h` and of `ac_room.h` first: an instrument of the
room is a small physical world of 16 parts, one to each key of the bottom row, stepped a hundred times a second in
whole numbers, and a sound comes only when something in that world meets something else. The firmware gives every
instrument the same controls and keeps, saves and shares its layout and its laws; the instrument brings

- **names**: its own, a short one and two letters; KNOB 1-4's laws, SELECT's, SEQ's device and its four, the kinds of
  thing a part can be (eight at most, the first is "nothing"), PRESETS, the ready-made layouts (`algos[0]` is YOUR OWN)
- **its world**: `init` (set up from the layout, also after every change of it) and `step` (10 ms of it). The world is
  `st`, as many bytes as the header says (up to 2048), zero before `init` is first called. `c->r` is the instrument as
  the room keeps it (the parts `p[16]`, the knobs, `run`, `seq`, `sel`, `preset`); `c->k[4]` are the four laws as the
  world feels them, swing included; `c->tq8` is the room's clock and `a->cross(per, off, &i)` says when a line of it
  went by, so things can land on the beat
- **its picture**: `draw` (the whole screen between the head and the strip; `c->edit` when EDIT has it open) and
  `small` (as it stands in the room, about 60 wide)
- **its keys**: `key` (bottom row) and `top` (top row); `play` when PLAY is pressed
- **its sounds**: `voice`, up to sixteen of its own, each built from `a->vo` (an oscillator), `a->flt` and `a->glide`,
  or one of the firmware's with `a->std`. `ksnd[kind]` says which voice a part of each kind makes when it is struck
  (`a->strike(part, vel)`: its flash, its sound, onto the loop when REC is on); `a->hit` plays a voice that is no part's
- **its lesson**: steps (a caption in seven languages each, 0 for the English), one function that plays the instrument
  while a step is up, and the topics HOME held lists

```c
#include "../src/arcave/mi.h"
typedef struct { ... } my_t;                                              /* its world */
static void my_init(void *st, mi_ctx_t *c, const mi_api_t *a) { ... }
static void my_step(void *st, mi_ctx_t *c, const mi_api_t *a) { ... }    /* 10 ms */
...
MI_INSTRUMENT(mine) = { MI_MAGIC, MI_API, sizeof(my_t), 17, 1, 0, 0,     /* its number (16 is THE SPRING), its version */
    "MY INSTRUMENT", "MINE", "MI", ... };                                 /* mi_head_t, in order */
```

The rules are a machine's (no variables of its own, nothing of the firmware's by name, no C library), and so are the
steps: add it to `MI_TWINS` in `ac_slots.h` and to `INSTRUMENTS` in `tools/mm_build.py`; play it with
`INS=2 tests/run_arcave.sh NAME 4 11` (it stands in THE GEODE's place, SCL opens it) or in the browser build;
`python3 tools/mm_build.py` makes `build/machines/NAME.minst`.

Change an instrument's kinds, knobs' ranges or layouts and it is another instrument to friends playing together (its
version is part of what an FM-1 says it has), so give it a new version.

Instrument numbers: 0 to 3 are the firmware's own, by place; 16 THE SPRING; take the next free one for yours.
