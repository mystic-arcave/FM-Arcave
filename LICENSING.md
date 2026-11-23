# Licensing

FM-ARCAVE is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The sample app `firmware/src/dinghy.c`, the wiring `platform.c`, the host mode `tests/host.c`, the platform's docs (`docs/API.md`, `RULES.md`, `MAKE-IT-YOURS.md`, `SMK37.md`) | zvenson ([Dinghy](https://github.com/zvenson/dinghy)) |
| The game: `firmware/src/arcave/`, `firmware/src/fm_arcave.c`, `firmware/gen/arcave_*.h`, `tools/gen_tables.py`, `gen_course.py`, `make_site.py`, `sheet.py`, `site_shots.sh`, `web/emu/`, `web/site/`, `web/install/`, `web/link/`, `tests/run_arcave*.sh`, `tests/emu_test.mjs`, `tests/scripts/` (all but `midi.txt`, `smk37.txt`), `docs/LINK.md` | Copyright (C) 2026 benjamuels (FM-ARCAVE, from the Mystic Arcave) |
| The rings and the dish (`ac_juke.h`): the rules are those of the lamp and the ring in UMBRA OBSCURA | benjamuels |
| The update protocol in the browser: `web/lib/fm1ota.js`, `fm1pkg.js` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as JIANT / UMBRA LIVE carry them |
| The voices' sine (`ac_synth.h` `sy_sine`) | after `dinghy.c` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |
| Optimist's Bluetooth LE stack by Henri David (hdavid, <https://github.com/w0ts/optimist>), the hardware-tested copy in fm1-base by Charles Vestal (<https://github.com/charlesvestal/fm1-base>, `bt/ble`), as JIANT FM carries it (<https://github.com/juanjiant-bit/JIANT>: with its RSSI hook, unused here, and its pairing key made again instead of kept, `BLE_LTK_DERIVE`). The peripheral's files only: the scanner and the central role (`ble_scan.c`, `ble_ll_central.c`, `ble_hw_wl82_central.c`, `ble_central.c`, `ble_gattc.c`, `ble_smp_init.c`) are left out. Used under the GPL-3.0-only option of its dual licence, headers kept. With it the host test of its BLE-MIDI codec | GPL-3.0-only OR MPL-2.0 | `firmware/bt/ble/`, `tests/ble_midi_test.c`, `LICENSES/MPL-2.0.txt` |
| Melodee by Kerem Kilic (Ellic Studio, <https://github.com/keremimo/melodee>): the radio drivers as Melodee carries them (Optimist's, with Melodee's start-up replay), and the supplies' set-up the radio needs, as JIANT carries them | GPL-3.0-only | `firmware/hal/fm1_ble.h`, `firmware/hal/fm1_ble_rf.h`, `firmware/hal/fm1_power.h` |
| JIANT FM by Juan Ignacio Antonucci (Juan Jiant, <https://github.com/juanjiant-bit/JIANT>): the Bluetooth unit and its glue (the start with the interrupts off, the boot guard, the address, MIDI in and out), from which FM-ARCAVE's are taken; its host test of the rings | GPL-3.0-only | `firmware/bt/bt_radio.c` (JIANT's `bt_seg.c`), `firmware/bt/bt_midi.h`, `firmware/src/bt.c` (after JIANT's `bt.c`, `bt_hw.c`), `tests/bt_midi_test.c` |
| The radio's own calibration, JieLi's (the AC79 SDK's `wl_rf_common.a`) rewritten in C by Charles Vestal for felucca-bsp (<https://github.com/jmarianski/felucca-bsp>, `chip/`) | Apache-2.0 | `firmware/hal/fm1_rf_cal.h`, `LICENSES/Apache-2.0.txt` |
| The radio's start-up values captured from M-VAVE's V15 (published under Apache-2.0 in fm1_open) and JieLi's SDK, as Optimist and felucca-bsp carry them | Apache-2.0 | `firmware/hal/ble_rf_tables_v15.h`, `LICENSES/Apache-2.0.txt` |

## Radio

Dinghy never switches the FM-1's radio on. FM-ARCAVE does, only while Bluetooth is ON (the default is OFF): a
Bluetooth LE MIDI peripheral on Optimist's own stack, no JieLi library, linked into the app (`firmware/bt/`,
`firmware/src/bt.c`). The stack's files are offered by their author under GPL-3.0-only OR MPL-2.0; FM-ARCAVE, and
every package built from it, is GPL-3.0-only as a whole (the Apache-2.0 radio files above are one-way compatible with
it). Whoever passes on a firmware built from this is responsible for what their radio transmits where they are.

## The logo

`assets/logo.svg` / `logo.png` are Dinghy's (the designburg signet: Copyright (C) designburg, all rights reserved, not
under the GPL). They mark Dinghy itself and are kept here only beside its docs; FM-ARCAVE does not use them.

## Fonts on the pages

The site asks Google Fonts for Silkscreen and Instrument Sans (both SIL OFL 1.1); no font file is in this tree. The
game's own pixel font is drawn in `tools/gen_tables.py`.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. FM-ARCAVE is not made or endorsed by them.
