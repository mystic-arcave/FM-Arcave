#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FM-ARCAVE on this computer (tests/host.c): a script presses keys and turns knobs, out come the screens and the sound.
#   tests/run_arcave.sh NAME [STORY]     runs tests/scripts/NAME.txt -> build/host/NAME-<ms>.ppm, NAME.wav, NAME.png
#   tests/run_arcave.sh NAME STORY SCENE  STORY 0-4: start that far into the story (4: the jukebox mended);
#                                        SCENE 1 the rings, 2 the dish, 3-5 a machine, 6 the race back, 7 the room by the door, 8 the ledge behind the start, 9 the cave at AT=X,Y, 10 UNDERSONG: straight there
#   PYTHON=/path/to/python3              a Python with Pillow, for the contact sheet (build/host/NAME.png)
# macOS: Mach-O refuses the ELF section names (".pool"), so they are defined away on the host.
set -e
cd "$(dirname "$0")/.."
N="$1"; mkdir -p build/host
D=; [ -n "$2" ] && D="-DAC_TEST_STORY=$2"
[ -n "$3" ] && D="$D -DAC_TEST_SCENE=$3"
[ -n "$CATCH" ] && D="$D -DAC_TEST_CATCH=1"       # CATCH=1: NIGHT BLOOM catches every spore
[ -n "$TSIZE" ] && D="$D -DAC_TEST_TSIZE=$TSIZE"   # TSIZE=0|1|2: the text size (default: medium, 1)
[ -n "$DUO" ] && D="$D -DAC_TEST_DUO=$DUO"       # DUO=0..4: UNDERSONG with a friend who never misses, that many chapters in
[ -n "$AT" ] && D="$D -DAC_TEST_X=${AT%,*} -DAC_TEST_Y=${AT#*,}"   # AT=X,Y: with SCENE 9, Rock starts in the cave there
cc -O2 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-const-variable -Wno-empty-body -DDINGHY_HOST \
   '-Dsection(x)=' -DAPP_VER='"dev"' $D -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "build/host/$N" firmware/src/fm_arcave.c
rm -f build/host/"$N"-*.ppm
"build/host/$N" "build/host/$N" "tests/scripts/$N.txt" | grep -v 'screen ->' || true
"${PYTHON:-python3}" tools/sheet.py "build/host/$N.png" $(ls build/host/"$N"-*.ppm | sort -t- -k3 -n) 2>/dev/null || echo "(no contact sheet: Pillow missing)"
