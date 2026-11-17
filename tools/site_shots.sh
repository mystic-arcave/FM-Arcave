#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The pictures on the site (web/site/img), taken from the game itself: the host build plays the test scripts and the
# frames named here are kept, as they are on the FM-1's screen (240 x 240).
#   PYTHON=/path/to/python3 tools/site_shots.sh      (a Python with Pillow)
set -e
cd "$(dirname "$0")/.."
tests/run_arcave.sh intro >/dev/null
tests/run_arcave.sh lang >/dev/null
tests/run_arcave.sh story 1 >/dev/null
tests/run_arcave.sh lit 4 >/dev/null
tests/run_arcave.sh mend 2 1 >/dev/null
tests/run_arcave.sh run 4 3 >/dev/null
CATCH=1 tests/run_arcave.sh table 4 4 >/dev/null
tests/run_arcave.sh bloom 4 5 >/dev/null
mkdir -p web/site/img
"${PYTHON:-python3}" - <<'PY'
from PIL import Image
S = {"title": "lang-4600", "wake": "intro-9200", "course": "intro-24000", "pad": "intro-19000", "psst": "story-3600", "hat": "story-7600",
     "cave": "lit-4300", "jukebox": "lit-7600", "rings": "mend-1500", "dish": "mend-12100", "run": "run-6000",
     "table": "table-7800", "bloom": "bloom-2500", "screen": "intro-1500"}
for name, src in S.items():
    Image.open(f"build/host/{src}.ppm").convert("RGB").save(f"web/site/img/{name}.png", optimize=True)
    print("web/site/img/" + name + ".png <- " + src)
PY
