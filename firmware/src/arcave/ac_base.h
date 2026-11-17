/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE: the ground everything stands on. Integer maths, a 240 x 240 picture in RAM that is compared with
 * the last one sent (only what changed goes to the screen), drawing, the pixel font, the panel read once a frame.
 *
 * The platform under it (Dinghy's platform.c on the FM-1, tests/host.c on a computer, web/emu/web_host.c in a
 * browser) supplies: fm1_ms, fm1_in, fm1_input_edges, fm1_input_note_edges, fm1_enc_take, fm1_led_key,
 * fm1_led_key_glow, lcd_blit, lcd_sync, midi_get, midi_send, settings_load, settings_save. */
#include "arcave_tab.h"

#define AC_W 240
#define AC_H 240
#define TAU16 65536                              /* angles: 65536 = a full turn */
#define Q8(x) ((int32_t)((x) * 256))
#define AC_POOL __attribute__((section(".pool")))

typedef uint16_t col_t;
#define AC_RGB(r, g, b) ((col_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define HEXC(h) AC_RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)

/* ---------------------------------------------------------------- maths */
static int32_t isin(int32_t a)                   /* a: 65536 a turn -> Q14 */
{
    uint32_t u = (uint32_t)a & 0xFFFFu, q = u >> 14, p = u & 16383u, i, f;
    int32_t v;
    if (q & 1u) p = 16384u - p;                  /* the way back down */
    i = p >> 6; f = p & 63u;
    v = i >= 256u ? AC_SIN[256] : AC_SIN[i] + (((AC_SIN[i + 1] - AC_SIN[i]) * (int32_t)f) >> 6);
    return (q & 2u) ? -v : v;
}
static int32_t icos(int32_t a) { return isin(a + 16384); }
#define RAD16(r_q10) ((int32_t)(((int64_t)(r_q10) * 10430) >> 10))   /* radians (Q10) -> angle units */

static uint32_t isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}
static uint32_t ihash(uint32_t x)                /* 0..65535, the same every time for the same x */
{
    x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
    return x & 0xFFFFu;
}
static uint32_t ac_rng_s = 0x1234ABCDu;
static uint32_t ac_rnd(void) { ac_rng_s ^= ac_rng_s << 13; ac_rng_s ^= ac_rng_s >> 17; ac_rng_s ^= ac_rng_s << 5; return ac_rng_s; }
static int32_t iclamp(int32_t v, int32_t a, int32_t b) { return v < a ? a : v > b ? b : v; }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }
static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t smooth8(int32_t a, int32_t b, int32_t x)   /* smoothstep, 0..256 */
{
    int32_t t;
    if (b == a) return x >= b ? 256 : 0;
    t = iclamp(((x - a) * 256) / (b - a), 0, 256);
    return (t * t * (768 - 2 * t)) >> 16;
}
static int32_t ease8(int32_t t)                  /* 0..256 -> 0..256, slow at both ends */
{
    t = iclamp(t, 0, 256);
    return t < 128 ? (2 * t * t) >> 8 : 256 - ((2 * (256 - t) * (256 - t)) >> 8);
}
static int32_t ipulse(uint32_t ms, uint32_t period_ms)   /* 0..256, a slow sine */
{
    return 128 + (isin((int32_t)((ms % period_ms) * 65536u / period_ms)) >> 7);
}

/* ---------------------------------------------------------------- the picture */
static col_t fb_px[AC_W * AC_H] AC_POOL;         /* what the game draws */
static col_t fb_prev[AC_W * AC_H] AC_POOL;       /* what the screen shows */
#define AC_TXROWS 32
static col_t fb_tx[2][AC_W * AC_TXROWS] AC_POOL; /* a band on its way to the screen (two: one may still be going out) */
static uint8_t fb_txi, fb_all = 1;
static uint32_t ga = 32;                         /* the ink's strength for everything drawn: 0..32 */
static int32_t gclip_y0 = 0, gclip_y1 = AC_H;

static inline col_t mix565(col_t d, col_t s, uint32_t a)
{
    uint32_t dd = (d | (uint32_t)d << 16) & 0x07E0F81Fu, ss = (s | (uint32_t)s << 16) & 0x07E0F81Fu;
    dd = (dd + (((ss - dd) * a) >> 5)) & 0x07E0F81Fu;
    return (col_t)(dd | dd >> 16);
}
static void galpha(int32_t a256) { ga = (uint32_t)iclamp((a256 + 4) >> 3, 0, 32); }

static void gclear(col_t c)
{
    uint32_t i;
    for (i = 0; i < AC_W * AC_H; i++) fb_px[i] = c;
}
static inline void gp(int32_t x, int32_t y, col_t c)
{
    if ((uint32_t)x < AC_W && y >= gclip_y0 && y < gclip_y1) {
        col_t *p = &fb_px[y * AC_W + x];
        *p = ga >= 32u ? c : mix565(*p, c, ga);
    }
}
static void ghl(int32_t x0, int32_t x1, int32_t y, col_t c)   /* x0 .. x1 - 1 */
{
    col_t *p;
    if (y < gclip_y0 || y >= gclip_y1) return;
    if (x0 < 0) x0 = 0;
    if (x1 > AC_W) x1 = AC_W;
    if (x0 >= x1 || !ga) return;
    p = &fb_px[y * AC_W + x0];
    if (ga >= 32u) for (; x0 < x1; x0++) *p++ = c;
    else for (; x0 < x1; x0++, p++) *p = mix565(*p, c, ga);
}
static void grect(int32_t x, int32_t y, int32_t w, int32_t h, col_t c)
{
    int32_t j;
    for (j = 0; j < h; j++) ghl(x, x + w, y + j, c);
}
static void gbox(int32_t x, int32_t y, int32_t w, int32_t h, col_t c)   /* an outline */
{
    ghl(x, x + w, y, c); ghl(x, x + w, y + h - 1, c);
    grect(x, y + 1, 1, h - 2, c); grect(x + w - 1, y + 1, 1, h - 2, c);
}
static void gline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, col_t c)
{
    int32_t dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy, n = 1200;
    if ((x0 < -400 && x1 < -400) || (x0 > 640 && x1 > 640) || (y0 < -400 && y1 < -400) || (y0 > 640 && y1 > 640)) return;
    while (n--) {
        int32_t e2 = 2 * err;
        gp(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void gline2(int32_t x0, int32_t y0, int32_t x1, int32_t y1, col_t c)   /* two pixels wide */
{
    gline(x0, y0, x1, y1, c);
    if (iabs(x1 - x0) > iabs(y1 - y0)) gline(x0, y0 + 1, x1, y1 + 1, c);
    else gline(x0 + 1, y0, x1 + 1, y1, c);
}
static void gdisc(int32_t cx, int32_t cy, int32_t r, col_t c)
{
    int32_t y;
    if (r <= 0) { if (r == 0) gp(cx, cy, c); return; }
    for (y = -r; y <= r; y++) {
        int32_t w = (int32_t)isqrt((uint32_t)(r * r - y * y));
        ghl(cx - w, cx + w + 1, cy + y, c);
    }
}
/* an ellipse, filled; part: 0 whole, 1 the upper half only */
static void gellf(int32_t cx, int32_t cy, int32_t rx, int32_t ry, col_t c, int part)
{
    int32_t y;
    if (rx <= 0 || ry <= 0) return;
    for (y = -ry; y <= (part ? 0 : ry); y++) {
        int32_t w = (int32_t)(isqrt((uint32_t)((ry * ry - y * y) * 256)) * (uint32_t)rx / (uint32_t)(ry * 16));
        ghl(cx - w, cx + w + 1, cy + y, c);
    }
}
/* an arc of an ellipse as short lines: angles in turn units, a0 < a1 */
static void garc(int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t a0, int32_t a1, col_t c, int thick)
{
    int32_t n = iclamp((imax(rx, ry) * iabs(a1 - a0)) / (65536 * 2 / 3) + 4, 4, 48), k, px = 0, py = 0;
    for (k = 0; k <= n; k++) {
        int32_t a = a0 + (a1 - a0) * k / n, x = cx + ((rx * icos(a) + 8192) >> 14), y = cy + ((ry * isin(a) + 8192) >> 14);
        if (k) { if (thick) gline2(px, py, x, y, c); else gline(px, py, x, y, c); }
        px = x; py = y;
    }
}
static void gcirc(int32_t cx, int32_t cy, int32_t r, col_t c) { garc(cx, cy, r, r, 0, 65536, c, 0); }

/* a filled shape with up to 48 corners (even-odd): the hat, the rock, the mountain */
#define GPOLY_MAX 48
static void gpoly(const int32_t *xy, int32_t n, col_t c)
{
    int32_t y0 = 100000, y1 = -100000, i, y;
    if (n < 3 || n > GPOLY_MAX) return;
    for (i = 0; i < n; i++) { y0 = imin(y0, xy[2 * i + 1]); y1 = imax(y1, xy[2 * i + 1]); }
    y0 = imax(y0, gclip_y0); y1 = imin(y1, gclip_y1 - 1);
    for (y = y0; y <= y1; y++) {
        int32_t xs[GPOLY_MAX], m = 0, j, k;
        for (i = 0, j = n - 1; i < n; j = i++) {
            int32_t ya = xy[2 * i + 1], yb = xy[2 * j + 1];
            if ((ya <= y && yb > y) || (yb <= y && ya > y))
                xs[m++] = xy[2 * i] + (xy[2 * j] - xy[2 * i]) * (y - ya) / (yb - ya);
        }
        for (i = 1; i < m; i++) {                /* few crossings: an insertion sort */
            int32_t v = xs[i];
            for (k = i; k > 0 && xs[k - 1] > v; k--) xs[k] = xs[k - 1];
            xs[k] = v;
        }
        for (i = 0; i + 1 < m; i += 2) ghl(xs[i], xs[i + 1] + 1, y, c);
    }
}
static void gpolyline(const int32_t *xy, int32_t n, int closed, col_t c)
{
    int32_t i;
    for (i = 0; i + 1 < n; i++) gline(xy[2 * i], xy[2 * i + 1], xy[2 * i + 2], xy[2 * i + 3], c);
    if (closed && n > 2) gline(xy[2 * n - 2], xy[2 * n - 1], xy[0], xy[1], c);
}
/* the whole picture toward black: f 0 (as it is) .. 256 (black), in eight steps as the screen would show a fade */
static void gfade(int32_t f)
{
    uint32_t i, a = (uint32_t)iclamp((f + 16) >> 5, 0, 8) * 4u;
    if (!a) return;
    if (a >= 32u) { gclear(0); return; }
    for (i = 0; i < AC_W * AC_H; i++) if (fb_px[i]) fb_px[i] = mix565(fb_px[i], 0, a);
}
/* a veil over the picture: every pixel a part of the way to c (the pause menu's glass) */
static void gveil(col_t c, uint32_t a32)
{
    uint32_t i;
    for (i = 0; i < AC_W * AC_H; i++) fb_px[i] = mix565(fb_px[i], c, a32);
}

/* ---------------------------------------------------------------- the font */
static uint32_t utf8_next(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t c = *p++;
    if (c >= 0xE0u && p[0] && p[1]) { c = (c & 15u) << 12 | (p[0] & 63u) << 6 | (p[1] & 63u); p += 2; }
    else if (c >= 0xC0u && p[0]) { c = (c & 31u) << 6 | (p[0] & 63u); p++; }
    *s = (const char *)p;
    return c;
}
static uint32_t fnt_glyph(uint32_t cp)
{
    uint32_t lo = 0, hi = FNT_N;
    if (cp >= 'a' && cp <= 'z') cp -= 32u;
    else if (cp >= 0xE0u && cp <= 0xFEu && cp != 0xF7u) cp -= 32u;   /* à -> À ... */
    else if (cp == 0x153u) cp = 0x152u;          /* œ */
    while (lo < hi) {
        uint32_t m = (lo + hi) >> 1;
        if (FNT_CP[m] == cp) return m;
        if (FNT_CP[m] < cp) lo = m + 1u; else hi = m;
    }
    return fnt_glyph('?');
}
static int32_t gtextw(const char *s, int32_t sc)
{
    int32_t w = 0;
    while (*s) w += (FNT_W[fnt_glyph(utf8_next(&s))] + 1) * sc;
    return w ? w - sc : 0;
}
/* text with its capitals' top at y (accents reach 2 * sc above); returns the x after it */
static int32_t gtext(int32_t x, int32_t y, const char *s, col_t c, int32_t sc)
{
    while (*s) {
        uint32_t g = fnt_glyph(utf8_next(&s)), r, b;
        for (r = 0; r < 8u; r++) {
            uint32_t row = FNT_ROW[g][r];
            for (b = 0; row; b++, row = (row << 1) & 0xFFu)
                if (row & 0x80u) {
                    if (sc == 1) gp(x + (int32_t)b, y + (int32_t)r - 2, c);
                    else grect(x + (int32_t)b * sc, y + ((int32_t)r - 2) * sc, sc, sc, c);
                }
        }
        x += (FNT_W[g] + 1) * sc;
    }
    return x;
}
static void gtextc(int32_t cx, int32_t y, const char *s, col_t c, int32_t sc) { gtext(cx - gtextw(s, sc) / 2, y, s, c, sc); }
static void gtextr(int32_t xr, int32_t y, const char *s, col_t c, int32_t sc) { gtext(xr - gtextw(s, sc), y, s, c, sc); }

/* text broken into lines of at most w pixels, at most `shown` characters of it (a typewriter; -1: all).
 * align 0 left, 1 centre. Returns the number of lines; *total gets the character count. '\n' breaks a line. */
static int32_t gtext_wrap(int32_t x, int32_t y, int32_t w, const char *s, col_t c, int32_t sc, int32_t lh,
                          int32_t shown, int align, int32_t *total, int draw)
{
    int32_t lines = 0, count = 0;
    while (*s) {
        const char *p = s, *brk = 0, *end;
        int32_t lw = 0, n = 0, nb = 0;
        char buf[64];
        while (*p && *p != '\n') {
            const char *q = p;
            uint32_t cp = utf8_next(&q);
            int32_t gw = (FNT_W[fnt_glyph(cp)] + 1) * sc;
            if (lw + gw - sc > w && brk) break;
            if (cp == ' ') brk = p;
            lw += gw;
            p = q;
        }
        end = (*p && *p != '\n' && brk) ? brk : p;
        for (p = s; p < end && n < 60; ) {       /* this line, cut at what is shown so far */
            const char *q = p;
            utf8_next(&q);
            if (shown >= 0 && count + nb >= shown) break;
            while (p < q) buf[n++] = *p++;
            nb++;
        }
        buf[n] = 0;
        for (p = s; p < end; ) { utf8_next(&p); count++; }
        if (draw && n) {
            if (align == 1) gtextc(x + w / 2, y + lines * lh, buf, c, sc);
            else gtext(x, y + lines * lh, buf, c, sc);
        }
        lines++;
        s = end;
        while (*s == ' ') s++;
        if (*s == '\n') s++;
    }
    if (total) *total = count;
    return lines;
}

/* ---------------------------------------------------------------- to the screen: only the rows and columns that changed */
static void ac_present(void)
{
    int32_t y = 0;
    while (y < AC_H) {
        int32_t x0 = AC_W, x1 = -1, y0, rows = 0, r;
        col_t *tx;
        /* find the next changed row */
        for (; y < AC_H; y++) {
            const col_t *a = &fb_px[y * AC_W], *b = &fb_prev[y * AC_W];
            int32_t i;
            if (fb_all) { x0 = 0; x1 = AC_W - 1; break; }
            for (i = 0; i < AC_W && a[i] == b[i]; i++) ;
            if (i < AC_W) { int32_t j; for (j = AC_W - 1; a[j] == b[j]; j--) ; x0 = i; x1 = j; break; }
        }
        if (y >= AC_H) break;
        y0 = y;
        /* the band: following changed rows, the span the widest of them */
        for (rows = 1, y++; rows < AC_TXROWS && y < AC_H; rows++, y++) {
            const col_t *a = &fb_px[y * AC_W], *b = &fb_prev[y * AC_W];
            int32_t i, j;
            if (fb_all) continue;
            for (i = 0; i < AC_W && a[i] == b[i]; i++) ;
            if (i >= AC_W) break;
            for (j = AC_W - 1; a[j] == b[j]; j--) ;
            if (i < x0) x0 = i;
            if (j > x1) x1 = j;
        }
        fb_txi ^= 1u;
        tx = fb_tx[fb_txi];                      /* the other buffer may still be going out */
        for (r = 0; r < rows; r++) {
            const col_t *a = &fb_px[(y0 + r) * AC_W + x0];
            col_t *b = &fb_prev[(y0 + r) * AC_W + x0], *o = &tx[r * (x1 - x0 + 1)];
            int32_t i;
            for (i = 0; i <= x1 - x0; i++) { col_t v = a[i]; b[i] = v; o[i] = (col_t)(v >> 8 | v << 8); }
        }
        lcd_blit((uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0 + 1), (uint32_t)rows, tx);
    }
    fb_all = 0;
}

/* ---------------------------------------------------------------- the panel, read once a frame */
#define AC_NKEY 27
static const uint8_t KEY_IS_SHARP[AC_NKEY] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0};
static struct {
    uint32_t keys, kdown, kup;                   /* the 27 keys: held, pressed, let go (bit 0 = F3) */
    uint32_t btn, bdown, bup;                    /* the buttons, bit = BTN_... */
    int32_t enc[8];                              /* detents turned this frame, by ENC_... */
    int32_t any_knob;                            /* all seven together */
    int32_t hikey;                               /* the highest key pressed this frame, -1 none */
} in;
static uint32_t in_prev_keys;

static void ac_input(void)
{
    uint32_t rel = 0, k, e;
    in.keys = fm1_in.notes & ((1u << AC_NKEY) - 1u);
    in.kdown = (in.keys & ~in_prev_keys) | (fm1_input_note_edges() & ((1u << AC_NKEY) - 1u));
    in.kup = in_prev_keys & ~in.keys;
    in_prev_keys = in.keys;
    in.btn = fm1_in.buttons;
    in.bdown = fm1_input_edges(&rel);
    in.bup = rel;
    in.any_knob = 0;
    for (e = 0; e < 7u; e++) { in.enc[e] = fm1_enc_take(e); in.any_knob += in.enc[e]; }
    in.hikey = -1;
    for (k = 0; k < AC_NKEY; k++) if (in.kdown >> k & 1u) in.hikey = (int32_t)k;
}
#define BDOWN(b) (in.bdown >> (b) & 1u)
#define BHELD(b) (in.btn >> (b) & 1u)

/* the lights: set what should be lit each frame; only changes reach the panel */
static uint32_t led_key_want, led_key_dim_want, led_btn_want, led_key_now, led_key_dim_now, led_btn_now;
static void ac_leds_begin(void) { led_key_want = led_key_dim_want = led_btn_want = 0; }
static void ac_leds_end(void)
{
    uint32_t k;
    for (k = 0; k < AC_NKEY; k++) {
        if ((led_key_want ^ led_key_now) >> k & 1u) fm1_led_key(KEY_LED(k), (int)(led_key_want >> k & 1u));
        if ((led_key_dim_want ^ led_key_dim_now) >> k & 1u) fm1_led_key_glow(KEY_LED(k), (int)(led_key_dim_want >> k & 1u));
    }
    for (k = 0; k < 14u; k++)
        if ((led_btn_want ^ led_btn_now) >> k & 1u) fm1_led_key(k, (int)(led_btn_want >> k & 1u));
    led_key_now = led_key_want; led_key_dim_now = led_key_dim_want; led_btn_now = led_btn_want;
}
