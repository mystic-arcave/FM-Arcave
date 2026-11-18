/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE in a browser: a fake FM-1 for WebAssembly, as tests/host.c is one for a computer. The game
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

/* the settings: a block the page keeps in the browser's storage */
static uint8_t w_store[512];
static uint32_t w_store_n, w_store_gen;
static int settings_load(void *p, uint32_t n)
{
    uint32_t i;
    if (!w_store_n || w_store_n != n) return -1;
    for (i = 0; i < n; i++) ((uint8_t *)p)[i] = w_store[i];
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

#include "../../firmware/src/arcave/arcave.c"

static int32_t w_blk[2 * 256];
static float w_l[1024], w_r[1024];
static uint32_t w_have, w_pos, w_owed;

EXPORT void web_boot(void) { fm1_ms = 1; arcave_frame(); }
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
EXPORT uint32_t web_scene(void) { return scene | (uint32_t)story << 8 | (uint32_t)lang << 16 | (uint32_t)lk.state << 24; }
