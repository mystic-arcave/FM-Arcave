/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant): JIANT's firmware/src/bt.c and bt_hw.c (V10.12 .. V10.46),
 * which this file follows (the boot guard, the address, the start with the interrupts off, the MIDI input) */
/* Bluetooth LE MIDI for a Dinghy firmware (FM-ARCAVE's port): the FM-1 as a BLE-MIDI peripheral that advertises
 * "FM-ARCAVE XXXX" (XXXX from its own address, so two FM-1s differ). A computer or phone connects to it (and to
 * others beside it) and gets what midi_send sends, and what it writes comes out of midi_get as source 3. Channel
 * messages and F8 FA FB FC only: no SysEx (the installer uses USB).
 *
 * What the app calls (the same three exist as stand-ins where there is no radio: the SMK-37 here, tests/host.c,
 * web/emu/web_host.c):
 *   bt_set(on)    from the main loop. The app keeps ON / OFF in its own save (default OFF) and calls this once at its
 *                 start with what it saved, and again when the setting changes. Calling it again with the same value
 *                 does nothing.
 *   bt_state()    BT_OFF; BT_STARTING (asked for: the radio starts 3 s after power-on, or the FM-1 is about to restart,
 *                 see below); BT_ADV (advertising); BT_CONN (a central connected); BT_FAILED; BT_NONE (no radio in
 *                 this build)
 *   bt_name()     the advertised name; "FM-ARCAVE" alone until the address exists (made at the first ON)
 *
 * The supplies. The radio needs the FM-1's supplies set as the stock firmware sets them (hal/fm1_power.h: the DC-DC
 * for the 1.3 V rail, VDDIO, the core rails). Every firmware that runs this radio does that once at power-on, before
 * the sound, the screen and USB start, and so does this one: only when Bluetooth was ON when the FM-1 was last
 * used (the platform's own record, below), so an FM-1 that never turns Bluetooth on boots exactly as before. The
 * first time ON is chosen in a run that booted without them, the FM-1 therefore RESTARTS ITSELF 3 s later (the app
 * has that long to write its save; the screen says so for a moment), comes up with the supplies set, and the app's
 * bt_set(1) at its start then starts the radio. It restarts once only: if that did not help, BT_FAILED.
 *
 * The radio's start (bt_radio_start, firmware/bt/bt_radio.c): from the main loop, 3 s or more after power-on, the
 * interrupts off for about 0.3 s (the radio calibrates itself): the sound is silent for that long, once.
 *
 * The boot guard. A word in .noinit (bt_crumb: kept across a reset, lost at power-off) says what the radio was doing:
 *   BT_CR_PW     the supplies being set, and the 2 s after
 *   BT_CR_START  the radio's start-up running
 *   BT_CR_RUN    the radio running
 * A reset in PW or START (a hang: the watchdog; a crash), or a crash or a watchdog reset in RUN, and the next boot
 * leaves the supplies and the radio alone, writes OFF into the platform's record and reports BT_FAILED. So a radio
 * that hangs or crashes costs one reset, never a loop. A start-up that returns unfinished is BT_FAILED too, with no
 * second try before the next power-on (the radio is half set up).
 *   BT_FAILED stays until the app calls bt_set(0), which gives BT_OFF. The app's bt_set(1) at its start is ignored
 *   while BT_FAILED: it should show the failure, turn its own saved setting OFF (else the next power-on asks for the
 *   radio again), and let the player choose ON to try again.
 *
 * The platform's record: blob NBLOB - 1 (one flash sector, store.h; an app must not use that blob), under a magic of
 * its own: whether Bluetooth was last ON, and the static random address (made once, at the first ON). The app's own
 * save is not touched. Without a working flash there is no Bluetooth (BT_FAILED). */
/* (FELUCCA_BT: platform.c. The FM-1 only: the SMK-37's flash map and supplies are not this port's) */
enum { BT_OFF, BT_STARTING, BT_ADV, BT_CONN, BT_FAILED, BT_NONE };
#if !FELUCCA_BT
static int bt_state(void) { return BT_NONE; }
static void bt_set(int on) { (void)on; }
static const char *bt_name(void) { return ""; }
static uint32_t bt_tx_room(void) { return 64u; }
#else
#ifndef BT_HOST_TEST                            /* (tests/bt_guard_test.c: this file on a computer, the FM-1 stood in for) */
#include "fm1_power.h"
#define BT_NOINIT __attribute__((section(".noinit.zbt")))   /* (after every other .noinit, app.ld: none of those moves) */
#endif
#include "../bt/bt_midi.h"

struct bt_oq bt_oq;                             /* midi_send's copy for Bluetooth (bt_radio.c takes it) */
int bt_radio_start(const uint8_t addr[6]);      /* firmware/bt/bt_radio.c */
void bt_radio_enable(int on);
uint32_t bt_radio_status(void);

#define BT_REC_AT (BLOBS_AT + (NBLOB - 1u) * 0x1000u)
#define BT_REC_MAGIC 0x31544241u                /* "ABT1" */
static struct { uint8_t on, rsv, addr[6]; } bt_rec;
enum { BT_CR_START = 0xB7570001u, BT_CR_RUN = 0xB7570002u, BT_CR_PW = 0xB7570003u, BT_CR_RB = 0xB7570004u };
static uint32_t bt_crumb BT_NOINIT;             /* BT_CR_PW / START / RUN */
static uint32_t bt_rb BT_NOINIT;                /* BT_CR_RB: restarted for the supplies */
static uint32_t bt_crash_seen BT_NOINIT;        /* fm1_crash.count at the last boot */
enum { BT_HW_NONE, BT_HW_RUN, BT_HW_FAIL };
static uint8_t bt_st, bt_want, bt_hw, bt_pw, bt_pw_tried;   /* BT_*; asked for; BT_HW_*; the supplies set; .. tried */
static uint32_t bt_rb_at;                       /* fm1_ms the FM-1 restarts at (0: no restart due) */
static char bt_nm[16];

static int bt_rec_save(void) { return area_save(BT_REC_AT, 1u, BT_REC_MAGIC, &bt_rec, sizeof bt_rec); }
static int bt_has_addr(void)
{
    uint32_t i, z = 0;
    for (i = 0; i < 6u; i++)
        z |= bt_rec.addr[i];
    return z != 0u;
}
/* the static random address, once (Core Vol 6 Part B 1.3.2.1: the two top bits 1): from the moment ON was chosen,
 * the battery's reading and the time since power-on, as JIANT's */
static void bt_addr_make(void)
{
    uint32_t i, h = fm1_ticks() * 2654435761u ^ (uint32_t)fm1_adc_read(FM1_ADC_BATT) << 7 ^ fm1_ms * 40503u;
    for (i = 0; i < 6u; i++) {
        h ^= h << 13;
        h ^= h >> 17;
        h ^= h << 5 ^ fm1_ticks();
        bt_rec.addr[i] = (uint8_t)(h >> 9);
    }
    bt_rec.addr[5] |= 0xC0u;
    bt_rec.addr[0] |= 1u;                       /* (never all zero: bt_has_addr) */
}

/* a BLE-MIDI message received (bt_radio.c ble_app_midi_in: channel messages, the clock, START CONTINUE STOP) into the
 * MIDI input ring, marked cable 2 (midi_get: source 3). The BLE interrupts (priority 2) are preempted by TIMER5, the
 * ring's other producer (USB, TRS): the interrupts off around it. Full: dropped, as USB's and the jack's */
void bt_midi_rx(uint32_t pkt)
{
    uint32_t ie = fm1_icfg() & 0x200u;
    fm1_irq_off();
    if (mi_w - mi_r < MQ) {
        midi_in_q[mi_w % MQ] = (pkt & ~0xF0u) | 0x20u;
        RING_PUBLISH();
        mi_w++;
    }
    if (ie)
        fm1_irq_on();
}
static void bt_midi_tx(uint32_t pkt) { bt_oq_put(&bt_oq, pkt); }   /* (midi_send; nobody listening: dropped) */
/* how many more messages midi_send can give Bluetooth now (BT_OQ while nobody listens: they are dropped anyway) */
static uint32_t bt_tx_room(void) { return bt_oq.on ? BT_OQ - (bt_oq.w - bt_oq.r) : BT_OQ; }

/* power-on (fm1_cstart, before the screen, the sound and USB): the guard, then the supplies if Bluetooth was ON */
static void bt_boot(void)
{
    uint32_t p3 = fm1_boot.p3_rst, crashed, f;
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))      /* a power-on: .noinit is noise, or what a short power cut kept */
        bt_crumb = bt_rb = bt_crash_seen = 0;
    crashed = fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != bt_crash_seen;
    bt_crash_seen = fm1_crash.magic == FM1_CRASH_MAGIC ? fm1_crash.count : 0u;
    if (!flash_ok || area_load(BT_REC_AT, SECTOR_MAX, BT_REC_MAGIC, &bt_rec, sizeof bt_rec))
        memset(&bt_rec, 0, sizeof bt_rec);
    if (bt_crumb == BT_CR_PW || bt_crumb == BT_CR_START || (bt_crumb == BT_CR_RUN && (crashed || (p3 & 4u)))) {
        bt_crumb = 0;
        bt_st = BT_FAILED;
        if (bt_rec.on) {
            bt_rec.on = 0;
            bt_rec_save();
        }
        return;
    }
    bt_crumb = 0;
    if (bt_rec.on && !bootguard.failed) {       /* (not after a boot that failed: the fewest things first) */
        bt_crumb = BT_CR_PW;
        bt_pw_tried = 1;
        fm1_wdt_feed();
        f = irq_save();
        bt_pw = (uint8_t)fm1_power_init();
        irq_restore(f);
    }
}

static void bt_start(void)
{
    uint32_t f;
    int rc;
    lcd_sync();                                 /* (no picture on its way to the screen while the interrupts are off) */
    fm1_wdt_feed();
    bt_crumb = BT_CR_START;
    f = irq_save();
    memset(abuf, 0, sizeof abuf);               /* (the audio's DMA keeps playing it: silence) */
    rc = bt_radio_start(bt_rec.addr);
    irq_restore(f);
    fm1_wdt_feed();
    if (rc) {                                   /* the start-up returned unfinished: OFF, not again before power-off */
        bt_crumb = 0;
        bt_hw = BT_HW_FAIL;
        bt_st = BT_FAILED;
        bt_rec.on = 0;
        bt_rec_save();
        return;
    }
    bt_crumb = BT_CR_RUN;
    bt_rb = 0;
    bt_hw = BT_HW_RUN;
    bt_st = BT_ADV;
}

static int bt_state(void) { return bt_st; }
static void bt_set(int on)
{
    on = on != 0;
    if (on ? bt_want && bt_st != BT_OFF && bt_st != BT_FAILED : !bt_want && bt_st == BT_OFF)
        return;                                 /* (as it is already) */
    if (!on) {
        bt_want = 0;
        bt_rb_at = 0;
        if (bt_hw == BT_HW_RUN) {
            bt_radio_enable(0);
            bt_crumb = 0;
        }
        if (bt_rec.on) {
            bt_rec.on = 0;
            bt_rec_save();
        }
        bt_st = BT_OFF;
        return;
    }
    if (bt_st == BT_FAILED)                     /* (until bt_set(0)) */
        return;
    bt_want = 1;
    if (bt_hw == BT_HW_FAIL || !flash_ok) {
        bt_st = BT_FAILED;
        return;
    }
    if (!bt_rec.on || !bt_has_addr()) {         /* ON kept (the supplies at the next power-on), the address made */
        if (!bt_has_addr())
            bt_addr_make();
        bt_rec.on = 1;
        if (bt_rec_save()) {
            bt_rec.on = 0;
            bt_st = BT_FAILED;
            return;
        }
    }
    if (bt_hw == BT_HW_RUN) {                   /* set up already: advertising again */
        bt_radio_enable(1);
        bt_crumb = BT_CR_RUN;
        bt_st = BT_ADV;
        return;
    }
    if (!bt_pw) {
        if (bt_pw_tried || bt_rb == BT_CR_RB) { /* the supplies did not take, or a restart did not bring them */
            bt_st = BT_FAILED;
            return;
        }
        bt_rb_at = (fm1_ms + 3000u) | 1u;       /* restart, once the app has written its save */
    }
    bt_st = BT_STARTING;
}
static const char *bt_name(void)
{
    static const char B[] = "FM-ARCAVE", HEX[] = "0123456789ABCDEF";
    uint32_t n;
    for (n = 0; n < sizeof B - 1u; n++)
        bt_nm[n] = B[n];
    if (bt_has_addr()) {                        /* (as the stack makes it: ble_host.c, BLE_NAME_HEX) */
        bt_nm[n++] = ' ';
        bt_nm[n++] = HEX[bt_rec.addr[1] >> 4];
        bt_nm[n++] = HEX[bt_rec.addr[1] & 15u];
        bt_nm[n++] = HEX[bt_rec.addr[0] >> 4];
        bt_nm[n++] = HEX[bt_rec.addr[0] & 15u];
    }
    bt_nm[n] = 0;
    return bt_nm;
}

/* the main loop (platform_poll) */
static void bt_tick(void)
{
    if (bt_crumb == BT_CR_PW && fm1_ms >= 2000u)
        bt_crumb = 0;                           /* (the supplies held through the start of everything else) */
    if (bt_rb_at && (int32_t)(fm1_ms - bt_rb_at) >= 0) {
        bt_rb_at = 0;
        bt_rb = BT_CR_RB;
        fm1_audio_stop();
        lcd_sync();
        lcd_fill(0, 0, 240, 240, C_BLACK);
        draw_text_box(0, 92, 240, &FONT_S, "BLUETOOTH", C_WHITE, 1);
        draw_text_box(0, 124, 240, &FONT_S, "RESTARTING", C_WHITE, 1);
        usb_detach();
        fm1_delay_ms(700);
        fm1_wdt_feed();
        bootguard_clear(&bootguard);            /* (meant: not a failed boot) */
        fm1_reboot();
    }
    if (bt_want && bt_st == BT_STARTING && bt_hw == BT_HW_NONE && bt_pw && fm1_ms > 3000u)
        bt_start();
    if (bt_want && bt_hw == BT_HW_RUN)
        bt_st = bt_radio_status() & 1u ? BT_CONN : BT_ADV;
}
#endif
