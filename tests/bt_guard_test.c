/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of Bluetooth's rules in the platform (firmware/src/bt.c), the FM-1 stood in for: the record in flash, the
 * supplies only at a power-on that follows ON, the one restart, the radio's start 3 s in, the boot guard (a hang or a
 * crash costs one reset, never a loop), BT_FAILED until bt_set(0), the name, the MIDI ring's marks. Nothing here
 * touches a radio: what the radio itself does only an FM-1 can show. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#define BT_HOST_TEST 1
#define FELUCCA_BT 1
#define BT_NOINIT
#define RING_PUBLISH() ((void)0)
/* ---- the platform, stood in for */
#define NBLOB 8u
#define BLOBS_AT 0x000F4000u
#define SECTOR_MAX (4096u - 12u)
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
static uint8_t flash_ok = 1;
static uint32_t fm1_ms, ticks, irq_on = 1, icfg = 0x200u;
static struct { uint8_t p3_rst; } fm1_boot;
#define FM1_CRASH_MAGIC 0x43525348u
static struct { uint32_t magic, count; } fm1_crash;
typedef struct { uint32_t magic, failed, pending; } bootguard_t;
static bootguard_t bootguard;
static void bootguard_clear(bootguard_t *b) { b->failed = b->pending = 0; }
static int32_t abuf[16];
enum { FM1_ADC_BATT = 1, C_BLACK = 0, C_WHITE = 1 };
static int FONT_S;
static uint32_t fm1_ticks(void) { return ticks += 977u; }
static int32_t fm1_adc_read(int ch) { (void)ch; return 600; }
static uint32_t fm1_icfg(void) { return icfg; }
static void fm1_irq_off(void) { irq_on = 0; }
static void fm1_irq_on(void) { irq_on = 1; }
static uint32_t irq_save(void) { irq_on = 0; return 0; }
static void irq_restore(uint32_t f) { (void)f; irq_on = 1; }
static void fm1_wdt_feed(void) {}
static void fm1_audio_stop(void) {}
static void lcd_sync(void) {}
static void lcd_fill(int x, int y, int w, int h, int c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
static void draw_text_box(int x, int y, int w, const int *f, const char *s, int c, int a) { (void)x, (void)y, (void)w, (void)f, (void)s, (void)c, (void)a; }
static void usb_detach(void) {}
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
/* the flash sector of the record, and counters of everything that matters */
static uint8_t sector[64], sector_ok, save_fails;
static uint32_t n_save, n_pw, n_start, n_reboot, pw_ret = 1, start_ret, en_last = 9, status;
static uint8_t start_addr[6], start_irq;
static jmp_buf rebooted;
static int area_load(uint32_t at, uint32_t max, uint32_t magic, void *p, uint32_t n)
{
    (void)max;
    if (at != BLOBS_AT + 7u * 0x1000u || magic != 0x31544241u || !sector_ok)
        return -1;
    memcpy(p, sector, n);
    return 0;
}
static int area_save(uint32_t at, uint32_t sectors, uint32_t magic, const void *p, uint32_t n)
{
    (void)sectors;
    if (at != BLOBS_AT + 7u * 0x1000u || magic != 0x31544241u || save_fails)
        return -1;
    memcpy(sector, p, n);
    sector_ok = 1;
    n_save++;
    return 0;
}
static int fm1_power_init(void) { n_pw++; return (int)pw_ret; }
static void fm1_reboot(void) { n_reboot++; longjmp(rebooted, 1); }
int bt_radio_start(const uint8_t addr[6]) { n_start++; memcpy(start_addr, addr, 6); start_irq = (uint8_t)irq_on; return (int)start_ret; }
void bt_radio_enable(int on) { en_last = (uint32_t)on; }
uint32_t bt_radio_status(void) { return status; }

#include "../firmware/src/bt.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-108s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}
/* a reset: RAM gone except .noinit (the three words); p3: 1 a power-on (.noinit is then whatever it was: the test
 * leaves it, as a short power cut does), 4 the watchdog, 0x40 a restart */
static void reset(uint8_t p3)
{
    bt_st = bt_want = bt_hw = bt_pw = bt_pw_tried = 0;
    bt_rb_at = 0;
    memset(&bt_rec, 0xEE, sizeof bt_rec);
    memset(&bt_oq, 0, sizeof bt_oq);
    fm1_ms = 0;
    fm1_boot.p3_rst = p3;
    status = 0;
    en_last = 9;
    bt_boot();
}
static void run_to(uint32_t ms) { for (; fm1_ms < ms; fm1_ms += 5u) bt_tick(); bt_tick(); }

int main(void)
{
    uint32_t i;
    bt_crumb = 0x12345678u; bt_rb = 0x9ABCDEF0u; bt_crash_seen = 77;        /* (RAM noise) */
    reset(1);
    check("a new FM-1, power-on: OFF, the supplies left alone, nothing written", bt_state() == BT_OFF && !n_pw && !n_save && !bt_crumb);
    check("the name before any address: FM-ARCAVE alone", !strcmp(bt_name(), "FM-ARCAVE"));
    bt_set(0);
    run_to(10000);
    check("bt_set(0) on an FM-1 that is OFF: nothing happens, ever", bt_state() == BT_OFF && !n_save && !n_pw && !n_start && !n_reboot);

    /* ---- the first ON: the record, then one restart */
    bt_set(1);
    check("the first ON: STARTING, ON and an address kept (static random: top two bits set), no radio yet",
          bt_state() == BT_STARTING && n_save == 1 && sector[0] == 1 && (sector[7] & 0xC0u) == 0xC0u && !n_start && !n_pw);
    {
        char want[16];
        snprintf(want, sizeof want, "FM-ARCAVE %02X%02X", sector[3], sector[2]);
        check("the name: FM-ARCAVE and the address's last four hex digits, as the stack advertises it", !strcmp(bt_name(), want));
    }
    bt_set(1);
    check("ON again: nothing more written", n_save == 1);
    if (!setjmp(rebooted)) {
        run_to(fm1_ms + 2900u);
        check("2.9 s later: still running (the app is writing its save)", !n_reboot);
        run_to(fm1_ms + 200u);
        check("the restart never came", 0);
    }
    check("3 s after ON: the FM-1 restarts itself, marked as restarted, not counted as a failed boot",
          n_reboot == 1 && bt_rb == BT_CR_RB && !bootguard.pending && !n_start);
    reset(0x40);
    check("after the restart: the supplies set once, before anything else runs; the guard word says so", n_pw == 1 && bt_crumb == BT_CR_PW && bt_state() == BT_OFF);
    bt_set(1);                                                             /* (the app, at its start, with what it saved) */
    run_to(1990);
    check("the app's ON at its start: STARTING, no radio in the first seconds", bt_state() == BT_STARTING && !n_start && bt_crumb == BT_CR_PW);
    run_to(2990);
    check("2 s up: the supplies held, the guard word cleared", !bt_crumb && !n_start);
    run_to(3010);
    check("3 s up: the radio started, once, with the interrupts off and the kept address: ADVERTISING",
          n_start == 1 && !start_irq && !memcmp(start_addr, sector + 2, 6) && bt_state() == BT_ADV && bt_crumb == BT_CR_RUN && !bt_rb && n_reboot == 1);
    status = 3; run_to(fm1_ms + 20u);
    check("a central connects: CONN", bt_state() == BT_CONN);
    status = 0; run_to(fm1_ms + 20u);
    check("it leaves: ADVERTISING", bt_state() == BT_ADV);

    /* ---- MIDI */
    bt_midi_tx(0x00403C09u);
    check("midi_send with nobody listening: dropped", bt_oq.w == bt_oq.r);
    bt_oq.on = 1; bt_midi_tx(0x00403C09u);
    check("midi_send while a central listens: in Bluetooth's ring", bt_oq.w - bt_oq.r == 1 && bt_oq.q[bt_oq.r % BT_OQ] == 0x00403C09u);
    mi_w = mi_r = 0; bt_midi_rx(0x7F6EBF0Bu);
    check("a message from Bluetooth: into the input ring marked cable 2 (midi_get: 3), the interrupts back on", mi_w == 1 && midi_in_q[0] == 0x7F6EBF2Bu && irq_on);
    icfg = 0; irq_on = 0; bt_midi_rx(0x7F6EBF0Bu); icfg = 0x200u;
    check("... taken with the interrupts off: they stay off", mi_w == 2 && !irq_on);
    for (i = 0; i < 100u; i++) bt_midi_rx(0x7F6EBF0Bu);
    check("the input ring full: the rest dropped, nothing written past it", mi_w - mi_r == MQ);

    /* ---- OFF and ON again in one run */
    bt_set(0);
    check("OFF: the link closed, OFF kept, the guard word cleared", bt_state() == BT_OFF && en_last == 0 && sector[0] == 0 && !bt_crumb && n_save == 2);
    run_to(fm1_ms + 5000u);
    check("... and it stays off", bt_state() == BT_OFF && n_start == 1);
    bt_set(1);
    check("ON again in the same run: advertising at once, no second start-up, no restart", bt_state() == BT_ADV && en_last == 1 && n_start == 1 && sector[0] == 1 && bt_crumb == BT_CR_RUN);
    run_to(fm1_ms + 4000u);
    check("... no restart follows", n_reboot == 1);

    /* ---- the next power-on with ON kept: no restart */
    reset(1);
    bt_set(1); run_to(3010);
    check("power-on with ON kept: the supplies at boot, the radio 3 s in, no restart", n_pw == 2 && n_start == 2 && n_reboot == 1 && bt_state() == BT_ADV);

    /* ---- a clean restart while it runs (an update): not a failure */
    reset(0x40);
    bt_set(1); run_to(3010);
    check("a clean restart while the radio ran (an update): Bluetooth comes back", n_pw == 3 && n_start == 3 && bt_state() == BT_ADV);

    /* ---- the boot guard */
    reset(4);                                                              /* (the watchdog, the radio running) */
    check("the watchdog while the radio ran: FAILED, OFF kept, the supplies left alone", bt_state() == BT_FAILED && sector[0] == 0 && n_pw == 3 && !bt_crumb);
    bt_set(1); run_to(8000);
    check("the app's ON at its start while FAILED: ignored (no radio, no restart)", bt_state() == BT_FAILED && n_start == 3 && n_reboot == 1 && sector[0] == 0);
    reset(1);
    check("the power-on after: OFF (the app shows its own setting; nothing starts by itself)", bt_state() == BT_OFF && n_pw == 3);
    bt_set(1);
    if (!setjmp(rebooted))
        run_to(3100);
    reset(0x40); bt_set(1); run_to(2990);
    bt_crumb = BT_CR_START;                                                /* (the start-up hangs: the watchdog) */
    reset(4);
    check("a hang in the radio's start-up: one reset, then FAILED, OFF kept", bt_state() == BT_FAILED && sector[0] == 0 && n_reboot == 2);
    bt_set(0);
    check("bt_set(0) clears FAILED: OFF", bt_state() == BT_OFF);
    bt_set(1); run_to(9000);
    check("ON again in that run (already restarted once): FAILED at once, no second restart, no radio", bt_state() == BT_FAILED && n_reboot == 2);
    reset(4);
    check("... and no reset loop: the next boot leaves everything alone", bt_state() != BT_STARTING && n_reboot == 2);

    reset(1); bt_set(1);
    if (!setjmp(rebooted))
        run_to(3100);
    bt_crumb = BT_CR_PW;                                                   /* (a hang while the supplies were being set) */
    reset(4);
    check("a hang while the supplies were set: FAILED, OFF kept, not tried again at that boot", bt_state() == BT_FAILED && sector[0] == 0);

    reset(1);
    fm1_crash.magic = FM1_CRASH_MAGIC; fm1_crash.count = 1;
    sector[0] = 1; bt_crumb = BT_CR_RUN;
    reset(0x40);                                                           /* (the crash screen's restart) */
    check("a crash while the radio ran: FAILED, OFF kept", bt_state() == BT_FAILED && sector[0] == 0);
    sector[0] = 1; bt_crumb = BT_CR_RUN;
    reset(0x40);
    check("the same crash record at a later clean restart: not counted again", bt_state() == BT_OFF);

    sector[0] = 1; bt_crumb = BT_CR_START; bt_rb = BT_CR_RB;
    { uint32_t pw0 = n_pw; reset(1);
      check("a power cut in the middle of a start (RAM kept its words): a power-on forgets them", bt_state() == BT_OFF && n_pw == pw0 + 1 && !bt_rb); }

    /* ---- failures that are not hangs */
    start_ret = 1; bt_set(1); run_to(3010);
    check("the radio's start-up returns unfinished: FAILED, OFF kept", bt_state() == BT_FAILED && sector[0] == 0 && !bt_crumb);
    { uint32_t st0 = n_start; bt_set(0); bt_set(1); run_to(fm1_ms + 5000u);
      check("... OFF then ON in that run: FAILED, the half set up radio is not started twice", bt_state() == BT_FAILED && n_start == st0); }
    start_ret = 0;

    reset(1); sector_ok = 0; save_fails = 1;
    { uint32_t rb0 = n_reboot; bt_set(1); run_to(6000);
      check("the record cannot be written: FAILED, no restart", bt_state() == BT_FAILED && n_reboot == rb0); }
    save_fails = 0;
    reset(1); flash_ok = 0;
    { uint32_t rb0 = n_reboot; bt_set(1); run_to(6000);
      check("no working flash: FAILED, no restart", bt_state() == BT_FAILED && n_reboot == rb0); }
    flash_ok = 1;

    sector_ok = 1; sector[0] = 1; pw_ret = 0;
    reset(1);
    { uint32_t rb0 = n_reboot, st0 = n_start; bt_set(1); run_to(6000);
      check("the supplies do not read back as set: FAILED, no radio, no restart", bt_state() == BT_FAILED && n_reboot == rb0 && n_start == st0); }
    pw_ret = 1;

    sector[0] = 1; bootguard.failed = 1;
    { uint32_t pw0 = n_pw; reset(0x40);
      check("after a boot that failed (the platform's own guard): the supplies are left alone", n_pw == pw0); }
    bootguard.failed = 0;

    puts(fails ? "BT guard: FAILED" : "BT guard: all passed");
    return fails != 0;
}
