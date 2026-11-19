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
run "power on: after the title OCT- and OCT+ breathe three times, then go dark, and PLAY breathes with its words" sh -c 'tests/run_arcave.sh octpulse > build/host/octpulse.log 2>&1 && grep -q " 9500 ms  lights: breathing 0003, PLAY 0" build/host/octpulse.log && grep -q " 13600 ms  lights: breathing 0000, PLAY 2" build/host/octpulse.log'
run "a language chosen over the title: no OCT breaths after it, PLAY as before" sh -c 'tests/run_arcave.sh octpulse2 > build/host/octpulse2.log 2>&1 && grep -q " 7000 ms  lights: breathing 0000, PLAY 0" build/host/octpulse2.log && grep -q " 8600 ms  lights: breathing 0000, PLAY 2" build/host/octpulse2.log'
run "the course, standing still: at a minute and a half \"keep going\" at the right edge, gone after five seconds" play keepgoing
run "the title in each language" play lang
run "the controls box by the start, HOME: the controls shown, a full loop (roll, hop, big hop, the jump onto the bounce mushroom, the other grip)" play controls
run "the PLAY button drawn as the green button in the wake line" play playbtn
run "Turkish: the title, the tip fading as the wake text comes in, the controls sheet" play turkish
run "the hollow behind the start: hidden by a rock face until Rock is on the ledge and rolls in" play hollow 0 8
run "the settings, line by line" play menu
run "into the cave, the Hat, the jukebox, the settings" play story 1
run "the jukebox past the Hat: its name at the screen's edge, Rock settling in its dip, PLAY: the rings" sh -c 'tests/run_arcave.sh juke 2 > build/host/juke.log 2>&1 && grep -q "^sound" build/host/juke.log'
run "JUICE WORKS: HOME shows how it is played (a glass, the tap, the ticket's line, serving, moving a glass)" play help 4 4
run "MYSTIC RUN: HOME shows how it is played (a hop, a high hop, a block's button, the tempo)" play helprun 4 3
run "NIGHT BLOOM: HOME shows how it is played (KNOB 1's petal, catching, SELECT's ring, ALGORITHM's ring, an arrow to each)" play helpbloom 4 5
run "a new song at the mended jukebox: a long slide, effects, their page, kept (and saved)" sh -c 'tests/run_arcave.sh newsong 4 2 > build/host/newsong.log 2>&1 && grep -q "settings 0 saved" build/host/newsong.log'
run "the rings, stuck: a minute (the note, HOME breathing), the hint page and back, two minutes (HOME held three seconds: the rings glide into line, the light)" play hint 2 1
run "the rings lock as they come into line (the foot shows which), a locked knob does nothing, the light" play lock 2 1
run "the four rings, the dish, a song kept, the power back (and saved)" sh -c 'tests/run_arcave.sh mend 2 1 > build/host/mend.log 2>&1 && grep -q "settings 0 saved" build/host/mend.log'
run "the settings: skip to the Mystic Arcave, OCT+ confirms (and saved)" sh -c 'tests/run_arcave.sh skip > build/host/skip.log 2>&1 && grep -q "settings 0 saved" build/host/skip.log'
run "the lit cave, a tune changed" play lit 4
run "the mended jukebox: its name only in front of it, the offer of a new song only after Rock has left it once" env AT=522,380 sh -c 'tests/run_arcave.sh jukeback 4 9 > build/host/jukeback.log 2>&1 && grep -q "^sound" build/host/jukeback.log'
run "the machines up close, lit: a crystal, a juice press, a flower" env AT=740,330 sh -c 'tests/run_arcave.sh cabinets 4 9 > build/host/cabinets.log 2>&1 && grep -q "^sound" build/host/cabinets.log'
run "the battery: its level in the settings' corner; low, flashing in the corner on any screen" env AT=466,274 sh -c 'tests/run_arcave.sh batt 4 9 > build/host/batt.log 2>&1 && grep -q "^sound" build/host/batt.log'
run "a booster, a secret high up, then GLO held: the save wiped (and saved)" sh -c 'tests/run_arcave.sh boost 4 > build/host/boost.log 2>&1 && grep -q "settings 0 saved" build/host/boost.log'
run "the room up on the left, its door, outside: over the start line" play door 4 7
run "the race back: cheering rock friends where the signs stood" play racecrowd 4 6
run "the race back, start to finish (and the time saved)" sh -c 'tests/run_arcave.sh race 4 6 > build/host/race.log 2>&1 && grep -q "settings 0 saved" build/host/race.log'
run "MYSTIC RUN" play run 4 3
run "JUICE WORKS: a drink poured and served, a glass moved, a spill" env CATCH=1 sh -c 'tests/run_arcave.sh table 4 4 > build/host/table.log 2>&1 && grep -q "^sound" build/host/table.log'
run "MYSTIC RUN a long way in: the start card, the controls, blocks and their buttons, EDIT: the scores" env CATCH=1 sh -c 'tests/run_arcave.sh run2 4 3 > build/host/run2.log 2>&1 && grep -q "^sound" build/host/run2.log'
run "FX a dozen times; child ear protection (and saved)" sh -c 'tests/run_arcave.sh fx 4 > build/host/fx.log 2>&1 && grep -q "settings 0 saved" build/host/fx.log'
run "NIGHT BLOOM" play bloom 4 5
run "NIGHT BLOOM, every spore caught: SELECT, ALGORITHM, then KNOB 2 at fifty, each with its arrow" env CATCH=1 sh -c 'tests/run_arcave.sh bloom4 4 5 > build/host/bloom4.log 2>&1 && grep -q "^sound" build/host/bloom4.log'
run "the small text size" env TSIZE=0 sh -c 'tests/run_arcave.sh story 1 > build/host/story0.log 2>&1 && grep -q "^sound" build/host/story0.log'
run "the large text size" env TSIZE=2 sh -c 'tests/run_arcave.sh story 1 > build/host/story2.log 2>&1 && grep -q "^sound" build/host/story2.log'
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
