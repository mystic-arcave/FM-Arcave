#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# A local test build of the browser emulator, never published: build/emu-local. Down the well a CPU player 2 is always
# waiting (AC_TEST_DUO: a friend who never misses, ready when you are), and PLAY WITH MY FM-1 takes the FM-1 as a
# controller only: the save it sends is left unread and none is sent back, so nothing played here reaches it.
#   web/emu/build_local.sh            needs emcc on the PATH, or EMCC=/path/to/emcc; then serve build/emu-local
set -e
cd "$(dirname "$0")/../.."
O=build/emu-local
mkdir -p "$O"
python3 tools/gen_tables.py >/dev/null
python3 tools/gen_course.py >/dev/null
"${EMCC:-emcc}" -O2 -std=gnu99 -w '-DAPP_VER="local"' -DAC_TEST_DUO=0 '-Dsection(x)=' -Ifirmware/gen -Ifirmware/src/arcave --no-entry \
    -sSTANDALONE_WASM -sSTACK_SIZE=262144 -sINITIAL_MEMORY=4194304 -sFILESYSTEM=0 -o "$O/fm-arcave.wasm" web/emu/web_host.c
sed -e "s/@VERSION@/local/g" -e "s/@CTRLONLY@/1/g" -e "s|\.\./site\.css|site.css|" web/emu/index.html > "$O/index.html"
cp web/emu/worklet.js web/emu/panel.js web/emu/remote.js web/emu/saves.js web/emu/machines.js web/link/ble.js web/site/site.css "$O/"
echo "emu-local: $O ($(wc -c < "$O/fm-arcave.wasm" | tr -d ' ') B wasm): a CPU player 2, the FM-1 a controller only"
