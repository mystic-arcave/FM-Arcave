#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FM-ARCAVE's tests, none of which needs an FM-1. From the repo root:
#   sh tests/run_arcave_tests.sh
# 1. the game on the host: every script in tests/scripts plays through (title to the cave, the mending, each machine)
# 2. the browser build in Node (when build/emu exists): it loads with no imports, draws, sounds, follows a Link beacon
# 3. the update path (when build/fm-arcave.fwsc exists): Dinghy's own tests of the M-UPGRADE entry and the loader
# (tests/run_tests.sh is Dinghy's suite for its sample app and the SMK-37; FM-ARCAVE builds for the FM-1 only.)
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
fail=0
run() { echo "== $1"; shift; "$@" || { echo "   FAILED"; fail=1; }; }
play() { tests/run_arcave.sh "$@" > "$OUT/$1.log" 2>&1 && grep -q '^sound' "$OUT/$1.log"; }

run "the intro: title, wake, roll, pads, the last stretch" play intro
run "power on: OCT- and OCT+ breathe three times from \"change language\", the words and PLAY on time beside them, then OCT dark" sh -c 'tests/run_arcave.sh octpulse > build/host/octpulse.log 2>&1 && grep -q "  3000 ms  lights: breathing 0000, PLAY 0" build/host/octpulse.log && grep -q "  4000 ms  lights: breathing 0003, PLAY 0" build/host/octpulse.log && grep -q "  9500 ms  lights: breathing 0003, PLAY 2" build/host/octpulse.log && grep -q " 10600 ms  lights: breathing 0000, PLAY 2" build/host/octpulse.log'
run "a language chosen over the title: no OCT breaths after it, PLAY as before" sh -c 'tests/run_arcave.sh octpulse2 > build/host/octpulse2.log 2>&1 && grep -q " 7000 ms  lights: breathing 0000, PLAY 0" build/host/octpulse2.log && grep -q " 8600 ms  lights: breathing 0000, PLAY 2" build/host/octpulse2.log'
run "the course, standing still: at a minute and a half \"keep going\" at the right edge, gone after five seconds" play keepgoing
run "the title in each language" play lang
run "the controls box by the start, HOME: the controls lesson, each step shown then done by the player (roll, hop, big hop, the jump onto the bounce mushroom; the other grip only shown), ALL DONE" play controls
run "the PLAY button drawn as the green button in the wake line" play playbtn
run "Turkish: the title, the tip fading as the wake text comes in, the controls sheet" play turkish
run "the hollow behind the start: hidden by a rock face until Rock is on the ledge and rolls in" play hollow 0 8
run "the settings, line by line" play menu
run "Dutch: the title, the wake text, the controls sheet, the settings, ABOUT" play dutch
run "Dutch: into the cave, the Hat, the rings, the dish, the jukebox" play dutchstory 1
run "into the cave, the Hat, the jukebox, the settings" play story 1
run "the jukebox past the Hat: its name at the screen's edge, Rock settling in its dip, PLAY: the rings" sh -c 'tests/run_arcave.sh juke 2 > build/host/juke.log 2>&1 && grep -q "^sound" build/host/juke.log'
run "JUICE WORKS: HOME, the lesson played through (a glass, pouring to the ticket's line, serving, moving a glass)" play help 4 4
run "HOME in a machine: PLAY skips a step (the language stays); left alone, a step is shown again; a knob during the replay goes straight to the turn" play helpknob 4 4
run "JUICE WORKS: ORDER UP! at the start, the order sliding in; a hint after 5 s for the keys, then the knob, then serving" play juicehelp 4 4
run "REC in a game: the box, the game held under it; PLAY on again; REC twice: back to the Arcave" play recask 4 4
run "CRYSTAL ROAD: HOME, the lesson on its own song, on the beat (a low crystal, missed then caught; a high then a low; the miss only shown; KNOB 1 and ALGO)" play helprun 4 3
run "NIGHT BLOOM: HOME, the lesson played through (KNOB 1's petal onto its spore; each on its colour only shown, a key goes on; SELECT's ring, ALGORITHM's ring)" play helpbloom 4 5
run "a new song at the mended jukebox: a long slide, effects, their page, kept (and saved)" sh -c 'tests/run_arcave.sh newsong 4 2 > build/host/newsong.log 2>&1 && grep -q "settings 0 saved" build/host/newsong.log'
run "the rings, stuck: a minute (the note, HOME breathing), the hint page and back, two minutes (HOME held three seconds: the rings glide into line, the light)" play hint 2 1
run "the rings lock as they come into line (the foot shows which), a locked knob does nothing, the light" play lock 2 1
run "the four rings, the dish, a song kept, the power back (and saved)" sh -c 'tests/run_arcave.sh mend 2 1 > build/host/mend.log 2>&1 && grep -q "settings 0 saved" build/host/mend.log'
run "the settings: skip to the Mystic Arcave, OCT+ confirms (and saved)" sh -c 'tests/run_arcave.sh skip > build/host/skip.log 2>&1 && grep -q "settings 0 saved" build/host/skip.log'
run "the lit cave, a tune changed" play lit 4
run "the mended jukebox: its name only in front of it, the offer of a new song only after Rock has left it once" env AT=522,380 sh -c 'tests/run_arcave.sh jukeback 4 9 > build/host/jukeback.log 2>&1 && grep -q "^sound" build/host/jukeback.log'
run "the machines up close, lit: a crystal, a juice press, a flower" env AT=740,330 sh -c 'tests/run_arcave.sh cabinets 4 9 > build/host/cabinets.log 2>&1 && grep -q "^sound" build/host/cabinets.log'
run "the ring secret: only a big hop at full roll off the high right ledge reaches it" env AT=850,167 sh -c 'tests/run_arcave.sh ringjump 4 9 > build/host/ringjump.log 2>&1 && grep -q "cave secret 2 found" build/host/ringjump.log'
run "the keyboard in the cave: every key its note, lowest key lowest, in each tune's own key; with LINK on, the shared key" env AT=466,274 sh -c 'tests/run_arcave.sh keys 4 9 > build/host/keys.log 2>&1 && grep -q "key 0 plays 48 (tune 1)" build/host/keys.log && grep -q "key 13 plays 62 (tune 2)" build/host/keys.log && grep -q "key 0 plays 50 (tune 3)" build/host/keys.log && grep -q "key 13 plays 69 (tune 4)" build/host/keys.log && grep -q "key 26 plays 79 (tune 5)" build/host/keys.log && grep -q "1[0-9]* ms  key 0 plays 45 (tune 3)" build/host/keys.log'
run "the battery: its level in the settings' corner; low, flashing in the corner on any screen" env AT=466,274 sh -c 'tests/run_arcave.sh batt 4 9 > build/host/batt.log 2>&1 && grep -q "^sound" build/host/batt.log'
run "a booster, a secret high up, then GLO held: the save wiped (and saved)" sh -c 'tests/run_arcave.sh boost 4 > build/host/boost.log 2>&1 && grep -q "settings 0 saved" build/host/boost.log'
run "the room up on the left, its door, outside: over the start line" play door 4 7
run "the race back: cheering rock friends where the signs stood" play racecrowd 4 6
run "the race back, start to finish (and the time saved)" sh -c 'tests/run_arcave.sh race 4 6 > build/host/race.log 2>&1 && grep -q "settings 0 saved" build/host/race.log'
run "CRYSTAL ROAD, every note caught: the start card, SPORE DRIFT in PEBBLE to its end (ROCK and CRYSTAL open), KNOB 1 to CRYSTAL, HOME, EDIT: every song (and saved)" env CATCH=1 sh -c 'tests/run_arcave.sh road 4 3 > build/host/road.log 2>&1 && grep -q "CRYSTAL ROAD tune 1 mode 2: 23 notes" build/host/road.log && grep -q "settings 0 saved (220 B)" build/host/road.log'
run "CRYSTAL ROAD: all six songs, each its own road, SELECT turning the song" env CATCH=1 sh -c 'tests/run_arcave.sh roadsongs 4 3 > build/host/roadsongs.log 2>&1 && grep -q "CRYSTAL ROAD tune 0 mode 0" build/host/roadsongs.log'
run "CRYSTAL ROAD's start card plays the song as a perfect player would, its keys lighting as each note plays" sh -c 'tests/run_arcave.sh roaddemo 4 3 > build/host/roaddemo.log 2>&1 && grep -q "demo: keys lit 00FE000 (tune 1, PEBBLE)" build/host/roaddemo.log'
run "CRYSTAL ROAD, nobody playing: PEBBLE still reaches its end, a key on the result card replays it to practise, then ROCK: the hearts go, GAME OVER" sh -c 'tests/run_arcave.sh road2 4 3 > build/host/road2.log 2>&1 && grep -q "CRYSTAL ROAD tune 1 mode 1" build/host/road2.log && grep -q "PEBBLE) practice" build/host/road2.log'
run "JUICE WORKS: a drink poured and served, a glass moved, a spill" env CATCH=1 sh -c 'tests/run_arcave.sh table 4 4 > build/host/table.log 2>&1 && grep -q "^sound" build/host/table.log'
run "FX a dozen times; child ear protection (and saved)" sh -c 'tests/run_arcave.sh fx 4 > build/host/fx.log 2>&1 && grep -q "settings 0 saved" build/host/fx.log'
run "NIGHT BLOOM" play bloom 4 5
run "NIGHT BLOOM, every spore caught: SELECT, ALGORITHM, then KNOB 2 at fifty-eight, each with its arrow" env CATCH=1 sh -c 'tests/run_arcave.sh bloom4 4 5 > build/host/bloom4.log 2>&1 && grep -q "^sound" build/host/bloom4.log'
run "the small text size" env TSIZE=0 sh -c 'tests/run_arcave.sh story 1 > build/host/story0.log 2>&1 && grep -q "^sound" build/host/story0.log'
run "the large text size" env TSIZE=2 sh -c 'tests/run_arcave.sh story 1 > build/host/story2.log 2>&1 && grep -q "^sound" build/host/story2.log'
run "the well in the cave floor: an old well with a bucket; PLAY: Rock in the bucket, a knob lowers him, the rope snaps, down; REC comes back up" env AT=246,380 sh -c 'tests/run_arcave.sh well 4 9 > build/host/well.log 2>&1 && grep -q "MIDI out BF 110" build/host/well.log && grep -q "MIDI out BF 117 9" build/host/well.log'
run "UNDERSONG with a friend who never misses: the story's four chapters, the finale, the four games' scores, ROCK-OFF (and saved)" env DUO=0 CATCH=1 sh -c 'tests/run_arcave.sh duo 4 10 > build/host/duo.log 2>&1 && grep -q "UNDERSONG NOTE RALLY: 5900 of 3600, 3 stars, chapters 4" build/host/duo.log && grep -q "UNDERSONG ROCK-OFF: 11400 of 11400" build/host/duo.log && grep -q "settings 0 saved (220 B)" build/host/duo.log'
run "UNDERSONG for three, beside two friends who never miss: the five games for three, each with its own score" env NP=3 DUO=0 CATCH=1 sh -c 'tests/run_arcave.sh trio 4 10 > build/host/trio.log 2>&1 && grep -q "UNDERSONG FOR THREE ECHO STEPS: 9600 of 9600, 3 stars, chapters 3" build/host/trio.log && grep -q "UNDERSONG FOR THREE TWIN ROAD: 14000 of 14000" build/host/trio.log && grep -q "UNDERSONG FOR THREE POTION CHOIR: 1200 of 1200" build/host/trio.log && grep -q "UNDERSONG FOR THREE ROCK-OFF: 11400 of 11400" build/host/trio.log'
run "UNDERSONG, nobody playing beside a friend who never misses: POTION CHOIR is not cleared, the story stays at its first chapter" env DUO=0 sh -c 'tests/run_arcave.sh duoidle 4 10 > build/host/duoidle.log 2>&1 && grep -q "UNDERSONG POTION CHOIR: .* 0 stars, chapters 0" build/host/duoidle.log'
run "UNDERSONG, the story told: TWIN ROAD's card turned to another song, and that one is played and scored" env DUO=4 CATCH=1 sh -c 'tests/run_arcave.sh duosong 4 10 > build/host/duosong.log 2>&1 && grep -q "UNDERSONG TWIN ROAD on MYCELIUM NIGHTS: 11600 of 11600, 3 stars" build/host/duosong.log'
run "POTION CHOIR by hand: the friend pours, a knob and a key pour yours (the wrong rock), one round runs out of time" env DUO=0 sh -c 'tests/run_arcave.sh duopot 4 10 > build/host/duopot.log 2>&1 && grep -q "UNDERSONG POTION CHOIR: 600 of 1200, 0 stars, chapters 0" build/host/duopot.log'
run "NOTE RALLY by hand: a key serves, the note falls where Rock is not, it is served again" env DUO=3 sh -c 'tests/run_arcave.sh duorally 4 10 > build/host/duorally.log 2>&1 && grep -q "UNDERSONG NOTE RALLY: 400 of 3600, 0 stars, chapters 3" build/host/duorally.log'
if [ -f build/emu-catch/fm-arcave.wasm ]; then run "UNDERSONG between browser builds wired to each other: seats, one clock, the story, ROCK-OFF, REC, a friend leaving; then three: all five games for three, one going dark" node tests/duo_test.mjs; fi
if [ -f build/emu-catch/fm-arcave.wasm ]; then run "UNDERSONG online: three browser builds through the lobby server (server/node.mjs) and the Online page's code, 80 ms each way: a lobby made, joined, refused; two games for two, a broken connection, ECHO STEPS for three, one leaving" node tests/online_test.mjs; fi
if [ -f build/emu/fm-arcave.wasm ]; then run "the browser build in Node" node tests/emu_test.mjs; else echo "-- no build/emu: web/emu/build.sh first for the browser test"; fi
if [ -f build/fm-arcave.fwsc ]; then
    CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
    export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
    $CC -o "$OUT/ota_test" tests/ota_test.c
    run "M-UPGRADE entry: the running firmware takes the package" "$OUT/ota_test" build/fm-arcave.fwsc
    head -c 60000 build/fm-arcave.bin > "$OUT/old_app.bin"
    python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
    $CC -o "$OUT/ldr_test" tests/ldr_test.c
    run "update loader: another app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/fm-arcave.fwsc
else echo "-- no build/fm-arcave.fwsc: ./build.sh first for the update-path tests"; fi
[ "$fail" = 0 ] && echo "all passed" || { echo "some tests failed"; exit 1; }
