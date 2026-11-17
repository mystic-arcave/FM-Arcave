#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The browser emulator: firmware/src/arcave through web/emu/web_host.c -> build/emu (fm-arcave.wasm and the page).
#   web/emu/build.sh [VERSION]        needs emcc (Emscripten) on the PATH, or EMCC=/path/to/emcc
set -e
cd "$(dirname "$0")/../.."
V="${1:-dev}"
mkdir -p build/emu
python3 tools/gen_tables.py >/dev/null
python3 tools/gen_course.py >/dev/null
"${EMCC:-emcc}" -O2 -std=gnu99 -w "-DAPP_VER=\"$V\"" '-Dsection(x)=' -Ifirmware/gen -Ifirmware/src/arcave --no-entry \
    -sSTANDALONE_WASM -sSTACK_SIZE=262144 -sINITIAL_MEMORY=4194304 -sFILESYSTEM=0 -o build/emu/fm-arcave.wasm web/emu/web_host.c
sed -e "s/@VERSION@/$V/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js web/emu/panel.js build/emu/
echo "emu: build/emu ($(wc -c < build/emu/fm-arcave.wasm | tr -d ' ') B wasm, $V)"
