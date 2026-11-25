/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE in a browser: a fake FM-1 for WebAssembly, as tests/host.c is one for a computer. The game
 * (firmware/src/arcave/arcave.c) runs unchanged on it. The page's audio worklet asks for sound (web_render);
 * every 44.1 frames of it are a millisecond of the FM-1's time, in which the game's loop runs once.
 * Built by web/emu/build.sh (emcc, no JS glue, no imports). */
#include <stdint.h>
#define EXPORT __attribute__((used, visibility("default")))
#ifndef APP_VER
#define APP_VER "dev"
#endif
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL = 3, BTN_ENV = 4, BTN_LFO = 5, BTN_EDIT = 6,
       BTN_GLO = 7, BTN_HOME = 8, BTN_SAVE = 9, BTN_ARP = 10, BTN_SEQ = 11, BTN_PLAY = 12, BTN_REC = 13 };
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_KNOB1 = 2, ENC_KNOB2 = 3, ENC_KNOB3 = 4, ENC_KNOB4 = 5, ENC_PRESETS = 6 };
#define KEY_LED(k) (14u + (k))

static volatile uint32_t fm1_ms;
static int32_t batt_raw = -1;                    /* (no battery in a browser: the game shows none) */
static struct { uint32_t notes, buttons; } fm1_in;
static uint32_t w_edges, w_rel, w_note_edges;
static int32_t w_enc[8];
static uint32_t w_led, w_led_keys, w_dim_keys;
static uint32_t fm1_input_edges(uint32_t *released) { uint32_t b = w_edges; *released = w_rel; w_edges = 0; w_rel = 0; return b; }
static uint32_t fm1_input_note_edges(void) { uint32_t b = w_note_edges; w_note_edges = 0; return b; }
static int32_t fm1_enc_take(uint32_t e) { int32_t v = w_enc[e & 7u]; w_enc[e & 7u] = 0; return v; }
static void fm1_led_key(uint32_t id, int on)
{
    if (id < 14u) { if (on) w_led |= 1u << id; else w_led &= ~(1u << id); }
    else if (id < 41u) { if (on) w_led_keys |= 1u << (id - 14u); else w_led_keys &= ~(1u << (id - 14u)); }
}
static uint32_t w_play_led;                     /* PLAY's green LED: 0 dark, 1 lit, 2 breathing */
static void fm1_play_led(uint32_t mode) { w_play_led = mode; }
static uint32_t w_breathe;                      /* breathing buttons, bit = BTN_... */
static void fm1_led_key_breathe(uint32_t id, int on) { if (on) w_breathe |= 1u << id; else w_breathe &= ~(1u << id); }
static uint32_t w_br_t0;                        /* a breath is 2.25 s, as panel.js draws it (from when a light starts) */
static void fm1_breath_sync(void) { w_br_t0 = fm1_ms; }
static uint32_t fm1_breaths(void) { return (fm1_ms - w_br_t0) / 2250u; }
static void fm1_led_key_glow(uint32_t id, int on)
{
    if (id >= 14u && id < 41u) { if (on) w_dim_keys |= 1u << (id - 14u); else w_dim_keys &= ~(1u << (id - 14u)); }
}

/* the screen: 240 x 240 RGB565 as the page reads it */
static uint16_t w_fb[240 * 240];
static uint32_t w_draws;
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) { uint16_t c = px[j * w + i]; w_fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8); }
    w_draws++;
}

/* MIDI: in from the page (Web MIDI), out to it */
static uint8_t w_mi[256][3], w_mo[256][3];
static uint32_t w_mi_w, w_mi_r, w_mo_w, w_mo_r;
/* Bluetooth: none in a browser (the page itself speaks to FM-1s: web/link/ble.js) */
enum { BT_OFF, BT_STARTING, BT_ADV, BT_CONN, BT_FAILED, BT_NONE };
static int bt_state(void) { return BT_NONE; }
static void bt_set(int on) { (void)on; }
static const char *bt_name(void) { return ""; }
static uint32_t w_bt_room = 64u;                /* (tests/remote_ble_test.mjs plays a Bluetooth link: its room, set by web_bt_room) */
static uint32_t bt_tx_room(void) { return w_bt_room; }
static int midi_get(uint8_t *st, uint8_t *d1, uint8_t *d2)
{
    if (w_mi_r == w_mi_w) return 0;
    *st = w_mi[w_mi_r & 255u][0]; *d1 = w_mi[w_mi_r & 255u][1]; *d2 = w_mi[w_mi_r & 255u][2]; w_mi_r++;
    return 1;
}
static void midi_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    w_mo[w_mo_w & 255u][0] = (uint8_t)st; w_mo[w_mo_w & 255u][1] = (uint8_t)d1; w_mo[w_mo_w & 255u][2] = (uint8_t)d2; w_mo_w++;
}

/* the settings: a block the page keeps in the browser's storage; as large as the FM-1's (one 4 KiB sector less its header) */
static uint8_t w_store[4096];
static uint32_t w_store_n, w_store_gen;
static uint8_t w_store_hit;                     /* a save was read: the SAVES page asks (web_about) */
static int settings_load(void *p, uint32_t n)
{
    uint32_t i;
    if (!w_store_n || w_store_n != n) return -1;
    for (i = 0; i < n; i++) ((uint8_t *)p)[i] = w_store[i];
    w_store_hit = 1;
    return 0;
}
/* the FM-1's spare copy of its save (arcave.c, blob 6): here so that the tests see it made and used. The page does not
 * keep it from one visit to the next: a browser's save is not cut short by the power going */
static uint8_t w_spare[4096];
static uint32_t w_spare_n;
static int blob_load(uint32_t b, void *p, uint32_t n)
{
    uint32_t i;
    if (b != 6u || !w_spare_n || w_spare_n != n) return -1;
    for (i = 0; i < n; i++) ((uint8_t *)p)[i] = w_spare[i];
    return 0;
}
static int blob_save(uint32_t b, const void *p, uint32_t n)
{
    uint32_t i;
    if (b != 6u || n > sizeof w_spare) return -1;
    for (i = 0; i < n; i++) w_spare[i] = ((const uint8_t *)p)[i];
    w_spare_n = n;
    return 0;
}
static int settings_save(const void *p, uint32_t n)
{
    uint32_t i;
    if (n > sizeof w_store) return -1;
    for (i = 0; i < n; i++) w_store[i] = ((const uint8_t *)p)[i];
    w_store_n = n; w_store_gen++;
    return 0;
}

/* the machine bank (ac_slots.h; platform.c bank_*): bytes the page keeps with the browser's save and hands over before
 * the game starts (web_bank). Nothing runs from it here: every loadable machine is built in, the bank says which */
static uint8_t w_bank[7u * 0xA000u];
static uint32_t w_bank_gen;
static const uint8_t *bank_at(uint32_t off) { return off < sizeof w_bank ? w_bank + off : 0; }
static int bank_erase(uint32_t off)
{
    uint32_t i;
    if (off >= sizeof w_bank || (off & 0xFFFu)) return -1;
    for (i = 0; i < 0x1000u; i++) w_bank[off + i] = 0xFF;
    w_bank_gen++;
    return 0;
}
static int bank_write(uint32_t off, const void *p, uint32_t n)
{
    uint32_t i;
    if (off > sizeof w_bank || n > sizeof w_bank - off) return -1;
    for (i = 0; i < n; i++) w_bank[off + i] &= ((const uint8_t *)p)[i];
    w_bank_gen++;
    return 0;
}
static int bank_runs(void) { return 1; }
static void bank_mark(int in) { (void)in; }

#include "../../firmware/src/arcave/arcave.c"

static int32_t w_blk[2 * 256];
static float w_l[1024], w_r[1024];
static uint32_t w_have, w_pos, w_owed;

EXPORT void web_boot(void) { fm1_ms = 1; arcave_frame(); }
#ifdef AC_TEST_DUO
EXPORT const char *web_duo_hint(void) { return du_hint(); }   /* (the local test build: what the CPU friend says) */
#endif
EXPORT float *web_out_l(void) { return w_l; }
EXPORT float *web_out_r(void) { return w_r; }
/* n frames (at most 1024) of sound, and the FM-1's time that goes with them */
EXPORT void web_render(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n && i < 1024u; i++) {
        if (w_pos >= w_have) {
            uint32_t k;
            for (k = 0; k < 512u; k++) w_blk[k] = 0;
            arcave_audio(w_blk, 256);
            w_have = 256; w_pos = 0;
        }
        w_l[i] = (float)(w_blk[2u * w_pos] > 32767 ? 32767 : w_blk[2u * w_pos] < -32767 ? -32767 : w_blk[2u * w_pos]) / 32768.0f;
        w_r[i] = (float)(w_blk[2u * w_pos + 1u] > 32767 ? 32767 : w_blk[2u * w_pos + 1u] < -32767 ? -32767 : w_blk[2u * w_pos + 1u]) / 32768.0f;
        w_pos++;
        w_owed += 10u;
        if (w_owed >= 441u) { w_owed -= 441u; fm1_ms++; arcave_poll(); arcave_frame(); }
    }
}
EXPORT uint16_t *web_screen(void) { return w_fb; }
EXPORT uint32_t web_screen_draws(void) { return w_draws; }
EXPORT void web_buttons(uint32_t mask)
{
    w_edges |= mask & ~fm1_in.buttons; w_rel |= fm1_in.buttons & ~mask;
    fm1_in.buttons = mask;
}
EXPORT void web_keys(uint32_t mask) { w_note_edges |= mask & ~fm1_in.notes; fm1_in.notes = mask; }
EXPORT void web_enc(uint32_t e, int32_t n) { w_enc[e & 7u] += n; }
/* BIG SCREEN on or off: the text one size smaller while it is on (ac_base.h, tsize_down) */
EXPORT void web_big(uint32_t on) { tsize_down = on ? 1u : 0u; fb_all = 1; }
EXPORT void web_bt_room(uint32_t n) { w_bt_room = n; }
EXPORT void web_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    w_mi[w_mi_w & 255u][0] = (uint8_t)st; w_mi[w_mi_w & 255u][1] = (uint8_t)d1; w_mi[w_mi_w & 255u][2] = (uint8_t)d2; w_mi_w++;
}
EXPORT int32_t web_midi_out(void)               /* the next message out: st | d1 << 8 | d2 << 16, or -1 */
{
    int32_t v;
    if (w_mo_r == w_mo_w) return -1;
    v = w_mo[w_mo_r & 255u][0] | w_mo[w_mo_r & 255u][1] << 8 | w_mo[w_mo_r & 255u][2] << 16; w_mo_r++;
    return v;
}
EXPORT uint32_t web_lit_buttons(void) { return w_led | (w_play_led == 1u ? 1u << 14 : 0u) | (w_play_led == 2u ? 1u << 15 : 0u); }   /* bit 14: PLAY green; 15: it breathes */
EXPORT uint32_t web_lit_keys(void) { return w_led_keys; }
EXPORT uint32_t web_breath_buttons(void) { return w_breathe; }
EXPORT uint32_t web_dim_keys(void) { return w_dim_keys; }
EXPORT uint8_t *web_store(void) { return w_store; }
EXPORT uint32_t web_store_len(void) { return w_store_n; }
EXPORT void web_store_set(uint32_t n) { w_store_n = n <= sizeof w_store ? n : 0; }
EXPORT uint32_t web_store_gen(void) { return w_store_gen; }
EXPORT uint32_t web_clock_q8(void) { return tr_now_q8(); }
EXPORT uint32_t web_bpm_q8(void) { return tr_bpm_q8; }
/* UNDERSONG (ac_duo.h), for the tests and the page: its state, seat << 4, a friend there << 5, the game << 8, chapters
 * cleared << 12, result written << 16, stars << 17; and the score as this FM-1 has it */
EXPORT uint32_t web_duo(void) { return du.state | (uint32_t)(du.seat & 3u) << 4 | (uint32_t)(du_peer() ? 1 : 0) << 6 | (uint32_t)du.mission << 8 | (uint32_t)duo_prog << 12 | (uint32_t)du.latched << 16 | (uint32_t)du.stars << 17 | (uint32_t)du.np << 20; }
EXPORT int32_t web_duo_score(int who) { du_score(); return who < 0 ? du.score : du.sc[who & 3]; }
EXPORT uint32_t web_tick(void) { return tr_tick; }
/* the music room with friends (ac_roomnet.h), for the tests: seat, how many << 3, tuning << 6, the station sat on << 8,
 * what is open << 12 (255: Rock loose), a friend's room << 20, the room's number << 21; a sum of everything that is the
 * same on every FM-1 (the shared bytes, and the loop whatever its order); one of those bytes */
EXPORT uint32_t web_room(void) { return rn.seat | (uint32_t)rn.np << 3 | (uint32_t)rn.tuning << 6 | (uint32_t)rn.st << 8 | (uint32_t)rm.focus << 12 | (uint32_t)rn.guest << 20 | (uint32_t)rn.tag << 21 | (uint32_t)rn.vw << 28; }
EXPORT void web_to_room(void) { if (scene != SC_ROOM) { to_cave(0); story = 4; scene = SC_ROOM; room_enter(); } }   /* (tests: straight into the music room) */
EXPORT uint32_t web_room_sum(void)
{
    uint32_t h = 2166136261u, l = 0;
    int i;
    for (i = 0; i < RN_N; i++) h = (h ^ *rn_byte(i)) * 16777619u;
    for (i = 0; i < rm.loop_n; i++) l += ihash((uint32_t)rm.loop[i].t * 131u + rm.loop[i].ik) ^ ((uint32_t)rm.loop[i].deg << 16 | (uint32_t)rm.loop[i].vel << 8 | rm.loop[i].sq);
    return h ^ l ^ (uint32_t)rm.loop_n << 24;
}
EXPORT uint32_t web_room_byte(uint32_t i) { return i < (uint32_t)RN_N ? *rn_byte((int)i) : i == 9000u ? rm.loop_n : i == 9001u ? (uint32_t)RI_REP : i == 9002u ? (uint32_t)RN_N : 0u; }
/* the remote (ac_remote.h): the size of this build's save, which the page compares with an FM-1's; and whether this
 * build is itself playing an FM-1's part (the tests) */
_Static_assert(sizeof svr <= sizeof w_store, "the game's save outgrew the page's store: raise w_store");
EXPORT uint32_t web_save_size(void) { return sizeof svr; }
EXPORT uint32_t web_save_part(uint32_t i) { return i == 0u ? (uint32_t)__builtin_offsetof(__typeof__(svr), mach) : i == 1u ? (uint32_t)__builtin_offsetof(__typeof__(svr), ins) : 0u; }   /* (tests: where the ledges' slots and the room's places' are in the save) */
/* the game saved now, not in a second and a half (or, for the time played, a quarter of an hour): the page asks before
 * letting an FM-1 go, so what was just played is in the save the FM-1 keeps */
EXPORT void web_save_now(void) { save_dirty = 0; save_now(); }
/* what the save this build started from holds, for the SAVES page (web/emu/saves.js): 0 one was read at all, 1 the
 * seconds played, 2 the tickets, 3 CRYSTAL ROAD's crystals, 4 UNDERSONG's chapters, 5 it was the spare that was read,
 * 16.. the rock's name */
EXPORT uint32_t web_about(uint32_t i)
{
    uint32_t k, n = 0;
    if (i == 0u) return w_store_hit | save_spared;
    if (i == 1u) return play_s;
    if (i == 2u) return pf.tickets;
    if (i == 3u) { for (k = 0; k < JUKE_N; k++) n += (gh_gem[k] & 3u) + (gh_gem[k] >> 2 & 3u) + (gh_gem[k] >> 4 & 3u); return n; }
    if (i == 4u) return duo_prog;
    if (i == 5u) return save_spared;
    if (i >= 16u && i < 16u + PF_NAME_MAX) return (pf.flags & PF_NAMED) ? (uint8_t)pf.name[i - 16u] : 0u;
    return 0;
}
/* the machine bank: its bytes, their length, how often it was written (the page keeps it when that changes); a
 * ledge's slot (ms.slot | the bank slot's state << 8 | the machine's number << 16); the machines' frame (which
 * machine | its state << 4 | in a machine << 8 | the slot being played << 12); a command for the bank as the page's
 * link would give it, done at once (the MYSTIC MACHINES page, for the browser's own game: it empties a slot with
 * 16 + slot, writes the machine into web_bank() itself, then 32 + slot has it checked); straight up to a ledge */
EXPORT uint8_t *web_bank(void) { return w_bank; }
EXPORT uint32_t web_bank_len(void) { return sizeof w_bank; }
EXPORT uint32_t web_bank_gen(void) { return w_bank_gen; }
EXPORT uint8_t *web_bank_info(void) { mb_info(); return mb_infob; }   /* (the bank's state, as the link tells it: 16 + 32 a slot) */
/* a place of the music room: is_.slot | its bank slot's state << 8 | its instrument's number << 16 | the loaded one stands there << 31 */
EXPORT uint32_t web_place(uint32_t j) { return j < MI_NSLOT ? is_.slot[j] | (uint32_t)mb.st[MM_NSLOT + j] << 8 | (uint32_t)mb.id[MM_NSLOT + j] << 16 | (uint32_t)mi_on((int)j) << 31 : 0u; }
/* (tests) the room: instrument j opened as its button does; a part's kind | size << 8 | note << 16 | quality << 24; its loop's hits */
EXPORT void web_room_open(uint32_t j) { if (scene == SC_ROOM && j < RI_N) { rm.focus = (uint8_t)j; rh.zoom = 0; rh.closing = 0; room_seen |= (uint8_t)(1u << j); } }
EXPORT uint32_t web_room_part(uint32_t j, uint32_t s) { const rpart_t *q = &ri[j & 3u].p[s & 15u]; return q->what | (uint32_t)q->size << 8 | (uint32_t)q->note << 16 | (uint32_t)q->qual << 24; }
EXPORT uint32_t web_room_loop(uint32_t j) { uint32_t n = 0, i; for (i = 0; i < rm.loop_n; i++) n += (uint32_t)(rm.loop[i].ik >> 4) == j; return n; }
EXPORT uint32_t web_room_k(uint32_t j, uint32_t i) { return ri[j & 3u].k[i & 3u] | (uint32_t)ri[j & 3u].algo << 8 | (uint32_t)RD[j & 3u]->nkind << 16 | (uint32_t)rm.pulse[j & 3u] << 24; }
EXPORT uint32_t web_ledge(uint32_t i) { return i < MM_NSLOT ? ms.slot[i] | (uint32_t)mb.st[i] << 8 | (uint32_t)mb.id[i] << 16 : 0u; }
EXPORT uint32_t web_mach(void) { return mc.which | (uint32_t)mc.state << 4 | (uint32_t)(scene == SC_MACH) << 8 | (uint32_t)mb.cur << 12 | (uint32_t)mb.help << 16; }
EXPORT int32_t web_mach_score(void) { return mc.score; }
EXPORT int32_t web_bank_cmd(uint32_t v)
{
    int r = mb_cmd(v);
    while (r < 0 && mbw.ask) { mb_poll(); if (!mbw.ask) r = 0; }
    return r;
}
EXPORT void web_to_ledge(uint32_t i) { if (i < MM_NSLOT) { to_cave(0); story = 4; scene = SC_MACH; ledge_go((int)i); } }
EXPORT uint8_t *web_spare(void) { return w_spare; }
EXPORT uint32_t web_spare_len(void) { return w_spare_n; }
EXPORT void web_spare_set(uint32_t n) { w_spare_n = n <= sizeof w_spare ? n : 0; }
EXPORT uint32_t web_remote(void) { return fmr.on | (uint32_t)fmr.msg << 1 | (uint32_t)fmr.tx_st << 4; }
EXPORT uint32_t web_scene(void) { return scene | (uint32_t)story << 8 | (uint32_t)lang << 16 | (uint32_t)lk.state << 24; }
