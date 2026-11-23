/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant): JIANT's firmware/bt/bt_seg.c (V10.12 .. V10.35), from which
 * this file is taken; FM-ARCAVE's changes are listed below */
/* The Bluetooth unit: Optimist's BLE stack (Henri David, w0ts/optimist; the hardware-tested copy in
 * charlesvestal/fm1-base bt/ble, GPL-3.0-only OR MPL-2.0, used here under GPL-3.0-only) with the WL82 radio drivers
 * (hal/fm1_ble.h, hal/fm1_ble_rf.h: Melodee's versions, Kerem Kilic, GPL-3.0-only; hal/fm1_rf_cal.h and
 * hal/ble_rf_tables_v15.h from felucca-bsp, Apache-2.0), as a translation unit of its own, so that none of the stack's
 * names meets the platform's or the game's (those are one unit: src/fm_arcave.c). tools/build.py compiles it to
 * build/bt.o and links it with the app.
 *
 * FM-ARCAVE's changes to JIANT's file: linked into the app's own image (JIANT keeps it in a segment of its own at
 * flash 0xEA000, outside its full app slot; FM-ARCAVE's slot has the room), so no segment header, no CRC, no plain XIP
 * window; the name "FM-ARCAVE XXXX" (XXXX from the address: two FM-1s differ); PROX's RSSI ring and AIR's scanner
 * left out; the functions renamed bt_radio_*.
 *
 * Peripheral only: advertising as "FM-ARCAVE XXXX" with the BLE-MIDI service (connectable), no scanner, no central;
 * Just Works pairing with bonding when a central asks (FELUCCA_BLE_BOND), the link encrypted then. The radio is
 * calibrated by its own measurement at the first start (fm1_rf_cal.h: records 106 / 107 / 108 the SDK board's
 * constants, 187 measured, ~160 ms), as Melodee, fm1-base and JIANT do on units whose stock calibration is gone.
 *
 * What the app (firmware/src/bt.c) calls:
 *   bt_radio_start(addr)  the first ON: the vectors (IRQ 45 / 29 at priority 2, below the audio and TIMER5), the
 *                         radio's start-up and calibration, the stack, advertising. The caller has the interrupts off
 *                         and the audio buffer silent. 0: running; 1: the start-up did not run to its end
 *   bt_radio_enable(on)   advertising on again / off (and any link closed); the radio stays set up until power-off
 *   bt_radio_status()     bit 0 a central connected, bit 1 its MIDI notifications on, [15:8] connections since start
 * MIDI both ways (bt_midi.h): what a central writes, decoded by the stack (ble_midi.c), goes into the app's MIDI
 * input ring (bt_midi_accept, then the app's bt_midi_rx: src/bt.c); what midi_send gives (the app's bt_oq, filled by
 * the main loop while a central listens) goes out in the notifications the stack packs each connection event
 * (ble_att.c att_midi_out), stamped with this unit's millisecond count. Received timestamps are not used (the
 * messages are taken as they come). */
#include <stdint.h>
#define FM1_IRQ_EXTERN 1                       /* (hal/fm1_irq.h: the crash record is the app's) */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define BLE_BARRIER() RING_PUBLISH()
#define BLE_DEVICE_NAME "FM-ARCAVE"
#define BLE_NAME_HEX 1                         /* " XXXX" after it: the address's last four hex digits (src/bt.c bt_name) */
#define BLE_LL_MAX_OCTETS 27u                  /* stock's PDU size, as Melodee and JIANT (radio-tested) */
#define BLE_DIAG 0                             /* (no counters, no console: as Melodee) */
#define FELUCCA_BLE_BOND 1                     /* Just Works pairing with bonding, LL encryption: a central that asks to
                                                * pair (Windows, some Macs and Androids) pairs, no PIN */
#define BLE_LTK_DERIVE 1                       /* .. the bond's key made again from EDIV / Rand (ble_smp.c ble_smp_ltk_of):
                                                * nothing kept, every bonded central back after a power-off */
#include "fm1_cc.h"
#include "fm1_time.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "ble/ble_stack.c"

static uint8_t bs_conns, bs_linked;

#include "bt_midi.h"
extern struct bt_oq bt_oq;                     /* (src/bt.c: midi_send's copy) */
void bt_midi_rx(uint32_t pkt);                 /* (src/bt.c: into the MIDI input ring) */

static uint32_t bs_t, bs_ms, bs_frac;          /* milliseconds from TIMER4's ticks (24 a microsecond) */
static uint32_t bs_now(void)
{
    uint32_t t = fm1_ticks(), d = t - bs_t + bs_frac;
    bs_t = t;
    bs_ms += d / 24000u;
    bs_frac = d % 24000u;
    return bs_ms;
}
BLE_API int ble_app_midi_peek(uint32_t *pkt, uint32_t *t_ms)
{
    uint32_t due, now;
    if (!bt_oq_peek(&bt_oq, pkt, &due))
        return 0;
    now = bs_now();
    if (due) {                                 /* (JIANT V10.33) a message with a time of its own: not before it,
                                                * stamped with it (FM-ARCAVE's midi_send gives none: always at once) */
        int32_t d = (int32_t)(fm1_ticks() - due);
        if (d < 0)
            return 0;
        now -= (uint32_t)d / 24000u;
    }
    *t_ms = now;
    return 1;
}
BLE_API void ble_app_midi_pop(void) { bt_oq_pop(&bt_oq); }
/* bonding: the key is made again from EDIV / Rand (BLE_LTK_DERIVE), the central's identity is not needed (no central
 * role): nothing to keep */
BLE_API void ble_app_bond(const uint8_t rand[8], uint16_t ediv, const uint8_t ltk[16]) { (void)rand, (void)ediv, (void)ltk; }
BLE_API void ble_app_peer_id(const uint8_t irk[16], const uint8_t addr[6], uint8_t addr_rand) { (void)irk, (void)addr, (void)addr_rand; }
BLE_API void ble_app_midi_in(uint32_t pkt, uint16_t ts, uint16_t last_ts)
{
    (void)ts, (void)last_ts;                   /* (taken as it comes) */
    if ((pkt = bt_midi_accept(pkt)) != 0u)
        bt_midi_rx(pkt);
}
BLE_API void ble_app_state(void)
{
    uint8_t c = (uint8_t)ble_connected(), on = (uint8_t)ble_midi_ready();
    if (c && !bs_linked && bs_conns < 255u)
        bs_conns++;
    bs_linked = c;
    if (on && !bt_oq.on)
        bt_oq.r = bt_oq.w;                     /* (a new listener: nothing stale) */
    bt_oq.on = on;
}

/* the SDK board's values for the three constant records (crystal load, PA bias, Wi-Fi per-rate gains: zero keeps the
 * defaults); 187 is measured (fm1_rf_cal_on), as fm1-base's bt_own.c, Melodee's midi_ble.c and JIANT's bt_seg.c */
static const struct ble_rf_trims BS_TRIMS = {{0x0B, 0x0B}, {1, 7, 4, 7, 11, 1, 7}, {0}, {0}};

int bt_radio_start(const uint8_t addr[6])
{
    fm1_ble_crumb_boot();                      /* (Melodee's .noinit breadcrumb: the last start's, then from zero) */
    fm1_ble_step(FM1_BLE_STEP_STACK_INIT);
    ble_init(addr, 1);
    fm1_guard_unlock_top();                    /* (the vectors are in the protected top of RAM) */
    fm1_ble_irq_attach(BLE_HW_IRQ_PRIO);       /* the two vectors, masked until the radio runs */
    fm1_ble_irqs_hold(1);
    fm1_guard_lock_top();
    fm1_rf_cal_on = 1;                         /* (our own measurement, no stored 187) */
    ble_hw_wl82_start(&BS_TRIMS);              /* the radio's start-up, its calibration (the crystal ramp from RAM) */
    if (fm1_ble_rf_stat.section != FM1_RF_SECT_DONE)
        return 1;                              /* (the start-up did not run to its end: no advertising, held) */
    fm1_ble_step(FM1_BLE_STEP_ENABLE);
    ble_enable(1);
    fm1_ble_step(FM1_BLE_STEP_RUNNING);
    fm1_ble_irqs_hold(0);                      /* (taken once the caller turns the interrupts back on) */
    return 0;
}

void bt_radio_enable(int on)
{
    fm1_ble_irqs_hold(1);
    fm1_ble_step(on ? FM1_BLE_STEP_ENABLE : FM1_BLE_STEP_SET_OFF);
    if (!on)
        ble_enable(0);                         /* off: the advertising stops, a link is closed */
    ble_enable(on != 0);
    if (!on)
        bs_linked = bt_oq.on = 0;
    fm1_ble_irqs_hold(0);
}

uint32_t bt_radio_status(void)
{
    return (uint32_t)ble_connected() | (uint32_t)ble_midi_ready() << 1 | (uint32_t)bs_conns << 8;
}
