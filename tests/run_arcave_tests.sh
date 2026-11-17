#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FM ARCAVE's tests, none of which needs an FM-1. From the repo root:
#   sh tests/run_arcave_tests.sh
# 1. the game on the host: every script in tests/scripts plays through (title to the cave, the mending, each machine)
# 2. the browser build in Node (when build/emu exists): it loads with no imports, draws, sounds, follows a Link beacon
# 3. the update path (when build/fm-arcave.fwsc exists): Dinghy's own tests of the M-UPGRADE entry and the loader
# (tests/run_tests.sh is Dinghy's suite for its sample app and the SMK-37; FM ARCAVE builds for the FM-1 only.)
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
fail=0
run() { echo "== $1"; shift; "$@" || { echo "   FAILED"; fail=1; }; }
play() { tests/run_arcave.sh "$@" > "$OUT/$1.log" 2>&1 && grep -q '^sound' "$OUT/$1.log"; }

run "the intro: title, wake, roll, pads, the last stretch" play intro
run "the title in each language" play lang
run "the controls box by the start, HOME: the controls sheet" play controls
run "the settings, line by line" play menu
run "into the cave, the Hat, the jukebox, the settings" play story 1
run "the four rings, the dish, a song kept, the power back (and saved)" sh -c 'tests/run_arcave.sh mend 2 1 > build/host/mend.log 2>&1 && grep -q "settings 0 saved" build/host/mend.log'
run "the settings: skip to the Mystic Arcave, OCT+ confirms (and saved)" sh -c 'tests/run_arcave.sh skip > build/host/skip.log 2>&1 && grep -q "settings 0 saved" build/host/skip.log'
run "the lit cave, a tune changed" play lit 4
run "a booster, a secret high up, then GLO held: the save wiped (and saved)" sh -c 'tests/run_arcave.sh boost 4 > build/host/boost.log 2>&1 && grep -q "settings 0 saved" build/host/boost.log'
run "the room up on the left, its door, outside: over the start line" play door 4 7
run "the race back, start to finish (and the time saved)" sh -c 'tests/run_arcave.sh race 4 6 > build/host/race.log 2>&1 && grep -q "settings 0 saved" build/host/race.log'
run "MYSTIC RUN" play run 4 3
run "THE LIGHT TABLE" play table 4 4
run "MYSTIC RUN a long way in: the start card, the controls, blocks and their buttons, EDIT: the scores" env CATCH=1 sh -c 'tests/run_arcave.sh run2 4 3 > build/host/run2.log 2>&1 && grep -q "^sound" build/host/run2.log'
run "FX a dozen times; child ear protection (and saved)" sh -c 'tests/run_arcave.sh fx 4 > build/host/fx.log 2>&1 && grep -q "settings 0 saved" build/host/fx.log'
run "NIGHT BLOOM" play bloom 4 5
run "NIGHT BLOOM, every spore caught: the rings join one by one" env CATCH=1 sh -c 'tests/run_arcave.sh bloom4 4 5 > build/host/bloom4.log 2>&1 && grep -q "^sound" build/host/bloom4.log'
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
