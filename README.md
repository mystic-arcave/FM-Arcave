# FM-ARCAVE

**The Mystic Arcave on the M-VAVE FM-1: a music game you play with the knobs and the keys.**

<p align="center"><img src="docs/screen.png" width="240" alt="FM-ARCAVE's title: a wizard's hat with one eye over the name"></p>

Wake up Rock, roll down to the cave, and mend the jukebox with a song of your own. Then the Arcave is yours: three
machines, a music room of four instruments and a mixer, a well that leads down to a game for two or three, and a
jukebox on the ALGORITHM knob. Friends can join in from the next room or the other side of the world.

## Contents

1. [Preface](#preface)
2. [Where to find it](#where-to-find-it)
3. [Status](#status)
4. [The controls everywhere](#the-controls-everywhere)
5. [The game](#the-game)
   - [The opening roll](#the-opening-roll)
   - [The cave](#the-cave)
   - [The jukebox](#the-jukebox)
   - [Free play and the machines](#free-play-and-the-machines)
   - [The music room](#the-music-room)
   - [UNDERSONG](#undersong)
   - [Tickets, upgrades and the profile](#tickets-upgrades-and-the-profile)
6. [Playing together](#playing-together)
7. [Mystic machines and instruments: the slots](#mystic-machines-and-instruments-the-slots)
8. [Your save](#your-save)
9. [The website and the browser FM-1](#the-website-and-the-browser-fm-1)
10. [Installing it on an FM-1](#installing-it-on-an-fm-1)
11. [Building it](#building-it)
12. [How the code is laid out](#how-the-code-is-laid-out)
13. [Tests](#tests)
14. [Releasing](#releasing)
15. [Documents](#documents)
16. [Credits and licence](#credits-and-licence)

## Preface

FM-ARCAVE is made by one person, me, working with a great deal of Claude Code: I decide what the game is, play it
and say what is wrong with it, and Claude writes most of the code, the tests and the pages from that. The world and
its ideas (Rock, the Hat, the cave of arcade machines, music you play by moving through a place) come from **Mystic
Arcave**, the art installation game project I started in the fall of 2024. This firmware carries that world onto a
small synth you can hold in your hands.

## Where to find it

| | |
| --- | --- |
| **The page** | <https://mystic-arcave.github.io/FM-Arcave/>: what the game is, with pictures and the newest version |
| **Play it in the browser** | [the emulator](https://mystic-arcave.github.io/FM-Arcave/emu/): the firmware's own code built for the web, the FM-1 drawn around it |
| **Install it on an FM-1** | [the installer](https://mystic-arcave.github.io/FM-Arcave/install/), over USB from Chrome or Edge |
| **Mystic machines** | [the MACHINES page](https://mystic-arcave.github.io/FM-Arcave/machines/): which machine stands on each ledge, and which instrument in each place of the music room |
| **Back up your save** | [the SAVES page](https://mystic-arcave.github.io/FM-Arcave/save/) |
| **Play together** | [the Link page](https://mystic-arcave.github.io/FM-Arcave/link/) (LOCAL: FM-1s on one computer; ONLINE: a lobby code shared with friends) |
| **Releases** | [GitHub releases](https://github.com/mystic-arcave/FM-Arcave/releases): the `.fwsc` package, with a build attestation |

## Status

**Version 0.29.** Releases up to here have been installed and played on real FM-1s (the newer FM-1, the one with
M-VAVE printed under the screen), UNDERSONG among them on two FM-1s at once. The newest parts have so far been tested
in the browser build and in scripted runs (see [Tests](#tests)), not yet on an FM-1:

- machines and instruments loaded into the FM-1's slots, and running from its flash;
- the music room with friends (up to four), watching a friend's instrument and swapping it;
- UNDERSONG's ONE VOICE, and the opening roll's rescue hop;
- the SAVES page and the spare copy of the save against a real FM-1.

Installing is at your own risk. The way back is any other firmware's installer, or OCT- held at power-on (the USB
rescue).

## The controls everywhere

| | |
| --- | --- |
| **Knobs** | roll Rock (any of the seven); in a machine or an instrument they are its controls |
| **Keys** | jump: the bottom row a hop, the top row a big one. In the cave and NIGHT BLOOM every key plays its note in the tune's key |
| **GLO** | the settings, over the paused game: the player's profile, a teleport, language, text size, child ear protection, look, LINK, Bluetooth, about, replay the intro, skip to the Mystic Arcave, UNDERSONG's story again, the version, and last, erasing everything (PLAY held three seconds on that line, and nowhere else). In there PLAY or OCT+ confirms. The FM-1's battery level is in their corner; a low battery flashes there on any screen |
| **OCT- / OCT+** | the language: English, German, Danish, French, Spanish, Turkish and Dutch (in the music room they are the octave) |
| **HOME** | how it is played: the controls on the course, a lesson in each machine and instrument |
| **EDIT** | the upgrades (outside the music room) |
| **FX** | one of a dozen effects, picture and sound |
| **REC** | in the middle of any game, a box: REC again leaves for the Arcave, PLAY plays on |

## The game

### The opening roll

The title; as "change language" comes up under it, OCT- and OCT+ breathe three times, carrying on into "press PLAY to
wake up Rock" as PLAY breathes. Then Rock rolls down a long hill to the cave, and the hill is a piece of music: 28
bars laid along the course, from FIRST LIGHT through THE VALLEY and THE POOLS to THE CLIMB. Where Rock is sets the
chord; how well he rolls (his speed, the launch pads, spores, jelly caps and star rings he takes) brings the layers in,
from slow notes to the full melody and its sixteenths. The keys play the chord's notes and a jumping key adds a run up
the scale; the pictures follow the music (the hill's rim in the chord's colour, mushrooms popping on the arpeggio, the
melody as stars joined across the sky).

The opening is smooth: two long downhill runs into ramps to build up speed, then the bumps come back gently. If Rock
cannot make a hill, every key lights and any key hops him over its crest. HOME on the course plays the controls as a
loop of little scenes: a line drawing of the FM-1 with its own screen playing a tiny copy of the game, and drawn hands
doing each control in turn. Behind the start a rock face hides the third of the game's three secrets.

### The cave

A platforming hub with no music, only a low hum and the sounds Rock makes. Both ends of the floor are ramps with a
booster; springy caps and the boosters reach what is high up. Someone calls from off the screen: the Hat, a wizard's
hat with no body. The jukebox that powered the three arcade machines has gone out.

- **The gift shop.** The unmarked left booster throws Rock up to a ledge and a room with a stall under a striped
  awning, its keeper asleep behind the counter until Rock comes. PLAY there opens the upgrades. Past it is the door
  back outside: the way out is through the gift shop, and through it the course becomes a **race back** to where the
  game began, with a start line, a clock, the pads and boosters turned round and crowds of rock friends cheering. The
  best time is kept with the scores.
- **The scores board** hangs on the small ledge high on the right wall: the best scores and the time played. Beside
  it, a tunnel leads down to the **music room**.
- **The well** appears, a surprise, once the jukebox is mended: UNDERSONG.
- **Secrets:** a row of chimes, a ring, and the hollow behind the start of the course.

### The jukebox

Four rings of glass, each with a crack: KNOB 1, 2, 3, 4 turn them until the light gets out. Two neighbouring cracks
in line lock together, and a locked ring stays put. Stuck a minute, and HOME gives a hint; stuck two, and holding HOME
three seconds glides the rings into line. Then the dish: slide a hand along the keys to spin it at the hand's speed,
hold a speed for eight beats, and that is your tempo. KNOB 1-4 set the song's echo, reverb, distortion and chorus.
REC, and two bars of keys are your first song. The power comes back with it; PLAY at the jukebox records the song
again the same way.

### Free play and the machines

ALGORITHM changes the tune, each in its own key (SPORE DRIFT, CRYSTAL STEPS, MYCELIUM NIGHTS, OPAL TIDE, ROLL ON, and
YOUR SONG), and the keyboard plays along in it. Each machine stands in a slight dip that holds a slow Rock. In a
machine PLAY starts, HOME teaches it (an interactive lesson: WATCH, then YOUR TURN with the real controls, then a tick),
REC goes back to the Arcave. Every round ends with a strip of tickets.

| | |
| --- | --- |
| **CRYSTAL ROAD** | a song's melody comes down a road as crystals and the keys play it, Rock leaping to wherever a key is pressed. Each jukebox tune has a melody written for it; SELECT, PRESETS or ALGORITHM picks the song, KNOB 1-4 the difficulty (PEBBLE, then ROCK, then CRYSTAL), and the start card plays the song as a perfect player would. Every song keeps a best score and up to three crystals in each difficulty |
| **JUICE WORKS** | four taps of mushroom juice on KNOB 1-4; glasses put down, moved and served with the keys; tickets show the layered drink to pour. Its lesson opens at the shop's front, where a rock orders a drink from Rock the barista |
| **NIGHT BLOOM** | KNOB 1, then SELECT, then ALGORITHM each turn a petal, and KNOB 2 joins once fifty spores are caught; catch each spore on its own colour |
| **GLOW CAPS** | the first machine that is not built in: loaded from the MACHINES page onto a ledge ([slots](#mystic-machines-and-instruments-the-slots)) |

### The music room

Down the tunnel from the cave's high right ledge, through a crystal door: the inside of a great geode. Rock rolls about
it (two crystal slides, springy caps, a booster, bubbles off the pool that carry him up to seven chimes, a floor that
is a marimba), and four instruments stand along it with THE POOL, the mixer, in the middle. Every instrument is
silent by itself: a sound comes only when something in it meets something else, so each is a little world you set up
and listen to. Each has a button of its own while Rock is loose in the room:

| Button | | |
| --- | --- | --- |
| **FX** | **ROOTS** | sixteen roots hang from the roof, a pod at each tip (a shaker, a woodblock, a bell, a drum, a snare); the wind swings them and they knock together |
| **SCL** | **THE GEODE** | a turning shell with stones loose inside; bare rock clicks, crystal rings, mushrooms throw back an acid bass, moss is noise, ooze plays a chord; a plectrum keeps time |
| **ENV** | **THE POOL** | the mixer: each instrument is a stone afloat, near the front loud, left and right are left and right; the current, dives, the loop, the echo, the tempo |
| **LFO** | **MUSHROOM RACK** | spore pods bounce along a row of sixteen caps, each landing an acid bass note; the cap's kind is the bounce |
| **EDIT** | **CRYSTAL SEAM** | sixteen crystals grow out of a slope; full-grown and stressed one breaks, rings, and throws shards that set off others |

Inside an instrument the bottom row of keys is its sixteen parts, KNOB 1-4 its laws, PRESETS and the layouts ready-made
lines, REC records onto the room's loop, SAVE keeps the whole room, HOME goes back to the room. Anywhere in the room:

- **GLO held: THE QUICK MIX**, gone when GLO is let go. Four bottom keys for each instrument (mute, solo, stop, wipe its
  loop), KNOB 1-4 the levels, PLAY stops or starts everything. **FX, SCL, ENV, LFO or EDIT goes straight to that
  instrument**, from the floor or from another instrument.
- **A tap on GLO:** the settings, whose first line here is MUSIC ROOM: Rock's sounds, the metronome, the count-in,
  the volume, whether the instruments play when Rock comes in, the lessons, playing together, and I HEAR.
- **HOME held: HOW IT WORKS**, the lessons by topic: each instrument's own, and what LFO, ENV, FX, ARP, SCL, the
  presets, REC and the quick mix do. The first time an instrument is opened its whole lesson plays.

The four places are slots: each holds its own instrument or one loaded from the MACHINES page (THE SPRING is the first:
vents, bubbles, lily pads and a frog).

### UNDERSONG

An old well in the Arcave, a bucket on its rope. PLAY and Rock hops into the bucket; a knob lowers him until the rope
snaps and down he goes, into a waiting screen until a friend's FM-1 is found ([Playing together](#playing-together)).
Each player sees the other's rock, keys and dial, all on one clock.

The story, four chapters played together, each with a little story of its own as it opens:

| | |
| --- | --- |
| **POTION CHOIR** | rocks, no two alike, in another order on each screen: one sees who needs the potion and describes them, the other pours. Six, then eight, then twelve rocks. It has an animated lesson |
| **TWIN ROAD** | a song split between the two on an oncoming road, each player with their own note shapes, the big notes both at once |
| **ECHO STEPS** | a cave of chasms: one plays a path across in a bar, each note a stepping stone; the Echo, a small long-eared creature, counts 4, 3, 2, 1 and the other plays it back |
| **NOTE RALLY** | the floor of the giant's hall: one note kept in the air over mossy cushions, a knob rolls you under it, a key sends it back on the beat |

Once the story is told the four can be played again for the best score, TWIN ROAD on any of five songs. **ROCK-OFF**
is the one game against each other. **ONE VOICE** (new in 0.29, for two): the two players are one creature, a Hum,
on its way home down a tunnel; one holds a key for how high it floats, the other turns a knob for its shape (round,
square, spiky), and both must be right as each crystal wall arrives. After eight walls they swap jobs.

With three FM-1s in the well, the story and ROCK-OFF are played for three: ECHO STEPS' other two echo together, TWIN
ROAD is split three ways, POTION CHOIR's two tellers each see half of the rock, NOTE RALLY's note goes round the three,
ROCK-OFF is all against all. Anyone who leaves stops the game with a message that says who.

### Tickets, upgrades and the profile

Every round on a machine (and every game of UNDERSONG) ends with a strip of tickets: more the better it went.
**EDIT** spends them: stones, hats, faces and trails for Rock, and for each of the keyboard's four colours a light, a
spark, a voice and a way of jumping (how high and far a jump goes stays the same for everyone). What is under the
cursor is tried on before it is bought; the Hat takes the tickets. The settings' first line is the **player's
profile**: the first time, NAME YOUR ROCK (a letter on every key, the knobs to move along the name, FX for a rock name
off the dice), after that a card with Rock as he is worn, his tickets, the upgrades and the high scores. Everything
that can be unlocked is one line of a table: `firmware/src/arcave/ac_prof.h` says how to add to it.

## Playing together

FM-1s find each other through the **Link page**, which relays MIDI between them: by USB or Bluetooth to one computer
(LOCAL), or through a lobby server to friends elsewhere (ONLINE: one makes a lobby, the others type its six-letter
code). Emulator windows in one browser find each other by themselves. The protocol is in [docs/LINK.md](docs/LINK.md).

- **LINK** (the settings): tempo, beat and bar the same on every FM-1, each keeping its own scene and tune.
- **UNDERSONG:** two or three players in the well.
- **The music room:** up to four, an instrument each. Whoever has TOGETHER on (it is, unless turned off) and is in
  their music room is in. The room is the first one's: the others get all of it sent over (TUNING IN, a few seconds),
  and their own room is kept as it was for the next time they are alone (SAVE in a friend's room keeps it as one's
  own). Every FM-1 plays the whole room itself on one shared clock, and only what the players change is sent. I HEAR:
  MY OWN has each FM-1 sound only its own player's instrument, for one room with four loudspeakers.
  - **One player an instrument.** A friend's instrument does not answer the others.
  - **Watching.** GLO held and a friend's instrument's button goes onto it anyway, to watch: "WATCHING" and their name,
    and they are told who is watching them; a strip on both screens says so while it lasts. Nothing there answers the
    one watching but HOME.
  - **Swapping.** OCT- and OCT+ pressed together by both of them, at about the same time, and they swap: the one
    watching plays, the one playing watches. When the player leaves, it is the watcher's.
  - **The same instruments.** Every FM-1 needs the same instrument in each place; one with other instruments loaded
    steps out ("OTHER INSTRUMENTS") and plays its own room while that one is there.

## Mystic machines and instruments: the slots

As presets go into a synth, machines and instruments go into the FM-1's slots. The [MACHINES
page](https://mystic-arcave.github.io/FM-Arcave/machines/) chooses which machine stands on each of the Arcave's three
ledges and which instrument stands in each of the music room's four places, and loads ones the firmware does not
carry, by USB or Bluetooth (or into the browser's game). The FM-1 has seven slots of 40 KiB in its flash, one for each
ledge and each place; a new install has the built-in ones (CRYSTAL ROAD, JUICE WORKS, NIGHT BLOOM; ROOTS, THE GEODE,
MUSHROOM RACK, CRYSTAL SEAM), which always stay in the firmware to go back to. Each keeps its own scores, layout and
settings in the save, whichever stands there.

A machine or an instrument is one C file against a small API (`firmware/src/arcave/mm.h`, `mi.h`): no globals, the
firmware's drawing and sound through a table of functions. `tools/mm_build.py` makes the files the page loads
(`.mmach`, `.minst`). [docs/MACHINES.md](docs/MACHINES.md) tells how to write one; GLOW CAPS
(`firmware/machines/`) and THE SPRING (`firmware/instruments/`) are the examples.

## Your save

The FM-1 keeps one save: the story, the scores, the music room, the profile, UNDERSONG's chapters, what stands in
each slot. It also keeps a spare copy, made at each power-on, which is used if a save is cut short by the power going.

The [SAVES page](https://mystic-arcave.github.io/FM-Arcave/save/) backs a save up: an FM-1's (USB or Bluetooth) or the
browser game's, into a list kept in the browser and to a file. PUT BACK puts one onto the same FM-1 or another, or
into the browser's game; what was there is copied first. A save from an older version is brought up to date.

## The website and the browser FM-1

The site (`web/site`, `web/install`, `web/link`, `web/emu`) is rebuilt from `main` on every push. The emulator is the
firmware itself compiled to WebAssembly, the FM-1 drawn around it; it keeps its own save in the browser.

- **PLAY WITH MY FM-1** (USB or Bluetooth): a real FM-1 becomes the controller and the game shows on the computer,
  from the FM-1's own save, which keeps what you play.
- **COMPUTER KEYS:** the computer's two bottom rows of letters are the FM-1's two rows of keys, every key and button
  labelled with its own.
- **BIG SCREEN:** the game fills the window.

## Installing it on an FM-1

The [installer](https://mystic-arcave.github.io/FM-Arcave/install/) updates an FM-1 over USB from Chrome or Edge,
with the newest release. The package's identity is `FM-1_960`. To go back, use any other firmware's installer, or
hold OCT- at power-on for the USB rescue.

## Building it

The tools: Python 3 with Pillow, Emscripten's `emcc` for the browser build, Node for the tests, and Docker for the
FM-1 build (JieLi's toolchain runs in a container; [docs/MAKE-IT-YOURS.md](docs/MAKE-IT-YOURS.md)).

```
python3 tools/gen_tables.py && python3 tools/gen_course.py   # the tables (already in firmware/gen)
tests/run_arcave.sh intro            # the game on this computer: build/host/intro.png (screens), intro.wav
web/emu/build.sh 0.29                # the browser build
./build.sh                           # the FM-1 package build/fm-arcave.fwsc
python3 tools/mm_build.py            # the loadable machines and instruments (build/machines)
python3 tools/make_site.py 0.29 build/fm-arcave.fwsc    # the whole site in build/site
sh tests/run_arcave_tests.sh         # every test; none needs an FM-1
```

The chip has no floating point and no 64-bit division, so the game is whole numbers throughout
([docs/RULES.md](docs/RULES.md)). The picture is drawn into RAM and only the rows that changed go to the screen.
Space is tight: `./build.sh` fails if the image passes 581,564 bytes (0.29 is about 578 KB) or RAM passes 96 KiB.

## How the code is laid out

```
firmware/src/arcave/     the game, one unity build (arcave.c includes the rest), everything static
  ac_base.h              drawing, font, input           ac_synth.h   voices, filter, echo, the clock
  ac_music.h             the tunes                      ac_text.h    seven languages
  ac_intro.h             the opening roll               ac_rollmus.h the roll's music
  ac_tutor.h, ac_mtutor.h   the controls shown, the machines' lessons
  ac_cave.h              the cave, the well, the gift shop, the tunnel
  ac_juke.h              the rings and the dish         ac_mach.h    the three machines
  ac_room.h              the music room's shared controls, sounds and loop
  ac_roots.h, ac_geode.h, ac_rack.h, ac_seam.h   the four instruments
  ac_hall.h              the room itself, THE POOL, the quick mix, the room's settings
  ac_rtutor.h            HOW IT WORKS                   ac_roomnet.h the music room with friends
  ac_duo.h               UNDERSONG; ac_duoplace.h its places, ac_potlesson.h POTION CHOIR's lesson, ac_onevoice.h ONE VOICE
  ac_prof.h, ac_deco.h, ac_shop.h   the profile, Rock dressed, the upgrades
  ac_sync.h              LINK                           ac_remote.h  PLAY WITH MY FM-1
  ac_slots.h, mm.h, mi.h, ac_mmrun.h, ac_mirun.h   the slots: the bank, the APIs, loaded ones running
  ac_menu.h              the settings
firmware/machines/, instruments/   the loadable ones (GLOW CAPS, THE SPRING)
firmware/gen/            tables made by tools/ (the course, the curves)
firmware/src/fm_arcave.c the FM-1 app: Dinghy's platform under the game
firmware/src/, hal/      Dinghy's platform (updates, USB rescue, MIDI, audio, keys, screen, storage); dinghy.c is its sample app
firmware/bt/, src/bt.c   Bluetooth LE MIDI: the stack and the radio's driver
web/emu/                 the browser FM-1 (web_host.c, worklet.js, panel.js); remote.js PLAY WITH MY FM-1, saves.js the saves, machines.js the slots
web/site/, install/, link/   the pages (link/undersong.js the relay, lobby.js the online lobby, ble.js Bluetooth)
server/                  the online lobby (a Cloudflare Worker; node.mjs runs it locally)
tools/                   gen_course.py (the opening's land), gen_tables.py, mm_build.py, make_site.py, build.py
tests/                   host.c (the game on a computer), scripts/ (scripted playthroughs), *_test.mjs (browser builds together)
```

## Tests

`sh tests/run_arcave_tests.sh` runs everything, about twenty minutes, and needs no FM-1:

- **scripted playthroughs** (`tests/scripts/*.txt`, run by `tests/run_arcave.sh NAME STORY SCENE`): the game built for
  the computer, fed knobs, keys and buttons on a timeline; each writes its log, screens and sound to `build/host/`
  and the suite checks the log's lines. Switches such as `DUO=`, `NP=3`, `MOD=` and `INS=` set the scene up;
  `tests/run_arcave.sh`'s head lists them;
- **browser builds together** (`tests/*_test.mjs`): several WebAssembly builds wired through the Link page's own relay
  code: UNDERSONG for two and three, the music room for four (with a delay as online), the online lobby, PLAY WITH MY
  FM-1 by USB and Bluetooth, the slots, the saves;
- **the FM-1 build's own checks**: the image's size, RAM, code that must run from RAM, register access only in `hal/`.

## Releasing

Pushing a tag `vX.Y` builds the package, runs the tests and publishes a release with a build attestation
(`.github/workflows/release.yml`). Every push to `main` publishes the site with the emulator
(`.github/workflows/pages.yml`, its version from the newest tag; after tagging a commit already on `main`, run that
workflow again). The online lobby is deployed separately: `npx wrangler deploy` in `server/`.

## Documents

| | |
| --- | --- |
| [docs/LINK.md](docs/LINK.md) | every message between FM-1s, the Link pages, Bluetooth, PLAY WITH MY FM-1 |
| [docs/MACHINES.md](docs/MACHINES.md) | the slots, and how to write a machine or an instrument |
| [docs/RULES.md](docs/RULES.md) | the chip's habits |
| [docs/API.md](docs/API.md) | what Dinghy's platform gives an app |
| [docs/MAKE-IT-YOURS.md](docs/MAKE-IT-YOURS.md) | the toolchain, a release, the Firmware Hub |
| [docs/SMK37.md](docs/SMK37.md) | Dinghy on the SMK-37 keyboard |
| [LICENSING.md](LICENSING.md) | who made what |

## Credits and licence

FM-ARCAVE is by benjamuels (Mystic Arcave), from the Mystic Arcave installation, written with Claude Code. It is
built on [Dinghy](https://github.com/zvenson/dinghy) by zvenson, which carries the platform of
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments) as
[SLOOP](https://github.com/isod89/sloop-fm1) (isod89) has it. The rings and the dish are after UMBRA OBSCURA.
Everything is GPL-3.0-only; [LICENSING.md](LICENSING.md) says who made what.

FM-ARCAVE is not made or endorsed by M-VAVE.
