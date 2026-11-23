#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Bluetooth's tests, on this computer (no radio, no FM-1). From the repo root: sh tests/run_bt_tests.sh
#   ble_midi_test   the stack's BLE-MIDI packet codec (firmware/bt/ble/ble_midi.c)
#   bt_midi_test    what is taken in from a central, and midi_send's ring going out in packets (firmware/bt/bt_midi.h)
#   bt_guard_test   the platform's rules (firmware/src/bt.c): the record, the one restart, the start, the boot guard
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
for t in ble_midi_test bt_midi_test bt_guard_test; do
    ${CC:-cc} -std=c11 -O1 -Wall -Wno-unused-function -fsanitize=undefined -o "$OUT/$t" "tests/$t.c"
    "$OUT/$t" > "$OUT/$t.log" 2>&1 || { cat "$OUT/$t.log"; echo "$t FAILED"; exit 1; }
    tail -1 "$OUT/$t.log"
done
