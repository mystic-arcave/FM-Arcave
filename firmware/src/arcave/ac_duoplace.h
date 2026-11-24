/* SPDX-License-Identifier: GPL-3.0-only */
/* UNDERSONG's places (Ben, 2026-11-24: ECHO STEPS and NOTE RALLY as places rather than game screens, each with an
 * opening that tells a story, starting somewhere else and ending where the game is played). Included by ac_duo.h.
 *
 *   ECHO STEPS   a cave of chasms on the way to the Echo's family. Whoever leads plays a path across in one bar: each
 *                note raises a stepping stone where it falls in the bar (left to right, across the gap) and as high as
 *                its key's colour (the four colours, low to high); their rock hops across on them, the Echo riding on
 *                its head. The stones sink to glimmers, the Echo counts 4, 3, 2, 1, and the others play the path back:
 *                each note right brings its stone up under them, a wrong one makes a wobbly grey stone. Then on to the
 *                next chasm, the next one leading. After the sixth, the Echo is home with its family.
 *   NOTE RALLY   the floor of the giant's hall, the Undersong asleep behind it: four mossy cushions each instead of
 *                lanes, the one the note will land on glowing. Each keep-up lights a crystal on its head and its smile
 *                grows; a drop makes it stir (a rumble, an eye opening a crack) and the note floats back up to be served.
 *   the openings   a little story each, played on the story's card in place of its page of words; PLAY skips it.
 *                (Drawn from the previews in docs/undersong-proposals.) */

/* ---------------------------------------------------------------- shared */
/* (kept out of line: inlined, the compiler folds an opening's times into large negative constants that the build's
   check takes for addresses in the chip's ROM) */
__attribute__((noinline)) static int32_t dw_ramp(int32_t t, int32_t a, int32_t b) { return ease8(iclamp((t - a) * 256 / imax(1, b - a), 0, 256)); }
static int32_t dw_hop(int32_t t, int32_t at, int32_t len) { return t >= at && t < at + len ? (t - at) * 256 / len : 0; }
static col_t dw_mix(col_t a, col_t b, int32_t f)
{
    int32_t r = ((a >> 11) & 31) + ((((b >> 11) & 31) - ((a >> 11) & 31)) * f >> 8);
    int32_t g = ((a >> 5) & 63) + ((((b >> 5) & 63) - ((a >> 5) & 63)) * f >> 8);
    int32_t bl = (a & 31) + (((b & 31) - (a & 31)) * f >> 8);
    return (col_t)(r << 11 | g << 5 | bl);
}
/* a player's rock: hop 0..256 */
static void dw_rock(int32_t x, int32_t y, int seat, int32_t z, int32_t ang, int32_t hop, uint32_t tm)
{
    int32_t up = hop > 0 ? (isin(hop * 128) * 12 * z / 256) >> 14 : 0;
    du_body(x, y - up, seat, z, ang, (tm + (uint32_t)seat * 1300u) % 3100u < 150u ? 30 : 256);
}
/* the opening's end: the game's name and its chapter */
static void dw_title(int32_t t, int32_t t0, int mission, col_t c)
{
    int32_t a = iclamp((t - t0) * 256 / 300, 0, 256), y = 70 + 30 * (256 - dw_ramp(t, t0, t0 + 500)) / 256;
    char b[8];
    if (t < t0) return;
    galpha(a * 3 / 4); grect(0, y - 14, AC_W, 64, th->bg); galpha(a);
    ghl(20, 220, y - 12, c); ghl(20, 220, y + 48, c);
    gfit(120, y, 228, DU_NAME[mission], c, 4);
    b[0] = (char)('1' + du_chap(mission)); b[1] = ' '; b[2] = '/'; b[3] = ' '; b[4] = '4'; b[5] = 0;
    gtextc(120, y + 36, b, th->mute, 1);
    galpha(256);
}
static void dw_skip(uint32_t tm)                 /* the opening's foot: PLAY skips it */
{
    galpha(110 + ipulse(tm, 1500) * 80 / 256);
    gfoot(238, TR(T_DU_SKIPOPEN), th->mute, th->bg, 1);
    galpha(256);
}
static int dw_at(int32_t t0, int32_t t1, int32_t at) { return t0 < at && t1 >= at; }   /* (a moment of an opening, just passed) */

/* ---------------------------------------------------------------- ECHO STEPS: the cave, its chasms and ledges */
#define EW_LY 150                                /* the ledges' top */
#define EW_GW 110                                /* a chasm's width: one bar */
#define EW_SP 190                                /* from one chasm to the next */
static const int16_t EW_LV[4] = {156, 141, 126, 111};   /* a stone's height for each colour of the keys, low to high */
static int32_t ew_cam;                           /* the world's x at the screen's left */
static int32_t ew_cz = 192;                      /* the Echo's size (256: as drawn; in the game, smaller than a rock) */
static int32_t ew_gl(int g) { return g * EW_SP + 60; }   /* chasm g's left edge (its right: + EW_GW) */
static int32_t ew_bx(int g, int slot) { return ew_gl(g) + 9 + slot * (EW_GW - 18) / 7; }   /* where in the bar, across it */
static void ew_mushroom(int32_t x, int32_t y, int32_t h, col_t c, uint32_t tm, int k)
{
    int32_t g = 120 + ipulse(tm + (uint32_t)k * 700u, 2600) * 100 / 256;
    galpha(30 * g / 256); gdisc(x, y - h, h + 6, c); galpha(256);
    grect(x - 1 - h / 10, y - h, 3 + h / 5, h, AC_RGB(180, 200, 210));
    gellf(x, y - h, h * 2 / 3 + 3, h / 2 + 2, c, 1);
    galpha(g); gp(x - h / 4, y - h - h / 4, th->text); gp(x + h / 5, y - h - h / 3, th->text); galpha(256);
}
static void ew_tufts(int32_t x0, int32_t x1, int32_t y, int k)
{
    int32_t x;
    for (x = imax(x0, -10) + 3; x < imin(x1, 250) - 2; x += 5 + (int32_t)(ihash((uint32_t)(x + ew_cam + k * 977)) % 5u)) {
        int32_t h = 2 + (int32_t)(ihash((uint32_t)(x + ew_cam) * 3u + (uint32_t)k) % 4u);
        gline(x, y, x - 1, y - h, th->moss); gline(x + 1, y, x + 2, y - h + 1, th->moss);
    }
}
/* a ledge from x0 to x1 (screen), its cliffs going down into the dark */
static void ew_ledge(int32_t x0, int32_t x1, int k)
{
    int32_t xy[16], xx;
    if (x1 < -10 || x0 > 250) return;
    x0 = imax(x0, -60); x1 = imin(x1, 300);
    xy[0] = x0; xy[1] = EW_LY; xy[2] = x1; xy[3] = EW_LY; xy[4] = x1 + 3; xy[5] = EW_LY + 16; xy[6] = x1 - 5; xy[7] = EW_LY + 44;
    xy[8] = x1 + 1; xy[9] = 240; xy[10] = x0 - 1; xy[11] = 240; xy[12] = x0 + 5; xy[13] = EW_LY + 50; xy[14] = x0 - 3; xy[15] = EW_LY + 20;
    gpoly(xy, 8, AC_RGB(16, 26, 34));
    galpha(120); gpolyline(xy, 8, 1, AC_RGB(40, 70, 80)); galpha(256);
    for (xx = x0 + 6; xx < x1 - 6; xx += 17) {   /* pebbles in the rock, flecks of crystal */
        uint32_t h = ihash((uint32_t)(xx + ew_cam) * 7u);
        int32_t py = EW_LY + 10 + (int32_t)(h % 70u);
        if (xx < -20 || xx > 260) continue;
        gellf(xx + (int32_t)(h >> 8) % 8, py, 3 + (int32_t)(h >> 12) % 4, 2, AC_RGB(26, 40, 50), 0);
        if (h % 5u == 0u) { galpha(160); gp(xx + 4, py + 9, th->caps[(h >> 16) % 5u]); gp(xx + 5, py + 8, th->text); galpha(256); }
    }
    ghl(x0, x1 + 1, EW_LY, th->moss); ghl(x0 + 1, x1, EW_LY + 1, AC_RGB(14, 80, 86));
    ew_tufts(x0, x1, EW_LY, k);
}
/* the cave: the dark, stalactites far off, glow-worms on the roof, mist rising out of the deep */
static void ew_back(uint32_t tm)
{
    int32_t k, y;
    for (y = 0; y < 240; y += 8) grect(0, y, 240, 8, dw_mix(AC_RGB(2, 4, 12), AC_RGB(8, 18, 28), y * 256 / 240));
    for (k = 0; k < 9; k++) {                    /* stalactites, drifting by slower than the ledges */
        int32_t x = ((k * 67 - ew_cam / 3) % 300 + 300) % 300 - 30, h = 26 + (k * 37) % 44, xy[6];
        xy[0] = x - 12 - k % 3 * 3; xy[1] = 0; xy[2] = x + 12; xy[3] = 0; xy[4] = x + 2; xy[5] = h;
        gpoly(xy, 3, AC_RGB(12, 22, 34));
    }
    for (k = 0; k < 28; k++) {                   /* glow-worms */
        int32_t x = (((int32_t)(ihash((uint32_t)k * 13u) % 520u) - ew_cam / 2) % 260 + 260) % 260 - 10, y2 = 3 + (int32_t)(ihash((uint32_t)k * 7u) % 34u);
        galpha(60 + ipulse(tm + (uint32_t)k * 377u, 1900 + (uint32_t)k * 31u) * 196 / 256);
        gp(x, y2, k % 3 ? th->caps[3] : th->caps[2]); if (k % 4 == 0) gp(x + 1, y2, th->caps[3]);
    }
    for (k = 0; k < 5; k++) { galpha(12 + k * 6); grect(0, 196 + k * 9, 240, 9, th->caps[2]); }   /* the mist */
    for (k = 0; k < 10; k++) {                   /* motes rising out of it */
        int32_t z = (int32_t)((tm / 7u + (uint32_t)k * 97u) % 160u), x = (((int32_t)(ihash((uint32_t)k * 31u) % 300u) - ew_cam) % 260 + 260) % 260 - 10;
        galpha(140 - z * 140 / 160); gp(x, 236 - z, th->caps[3]);
    }
    galpha(256);
}
/* a stepping stone at (x, y) of colour lane: rise 0..256 (coming up out of the deep), kind 0 solid, 1 a glimmer
 * (sunk: only where it was), 2 wobbly (a wrong note) */
static void ew_stone(int32_t x, int32_t y, int lane, int32_t rise, int kind, uint32_t tm)
{
    col_t c = kind == 2 ? AC_RGB(120, 120, 130) : GH_ZONE[lane & 3];
    int32_t xy[6], dy = (256 - rise) * 22 / 256, wob = kind == 2 ? (tsin(tm, 90, 0) >> 13) : 0;
    uint32_t a0 = ga;
    if (kind == 1) {                             /* a glimmer: a faint outline, twinkling */
        ga = a0 * (uint32_t)(40 + ipulse(tm + (uint32_t)x * 50u, 900) * 60 / 256) >> 8;
        garc(x, y, 8, 3, 0, 65536, c, 0); gp(x - 3, y - 3, th->text); gp(x + 4, y - 5, c);
        ga = a0;
        return;
    }
    y += dy;
    ga = a0 * (uint32_t)(rise * 60 / 256) >> 8; gdisc(x, y + 2, 13, c); ga = a0 * (uint32_t)rise >> 8;
    xy[0] = x - 8; xy[1] = y + wob; xy[2] = x + 8; xy[3] = y - wob; xy[4] = x + 1; xy[5] = y + 10;
    gpoly(xy, 3, dw_mix(c, AC_RGB(0, 0, 0), 150));
    gellf(x, y, 8, 3, c, 0); ghl(x - 5, x + 3, y - 2, th->text);
    if (kind == 2) gline(x - 2, y - 2, x + 1, y + 5, th->bg);
    ga = a0;
}
/* the Echo: a small fluffy creature with long ears, feet at (x, y). sing 0..256 (its mouth open), perk 0..256 (ears
 * up, else drooping), look -1..1, sad: a tear */
static void ew_critter(int32_t x, int32_t y, int32_t sing, int32_t perk, int look, int sad, uint32_t tm)
{
#define EZ(v) ((v) * ew_cz / 256)
    col_t fur = AC_RGB(206, 190, 255), fur2 = AC_RGB(150, 128, 226), inner = AC_RGB(255, 150, 206), eye = AC_RGB(255, 255, 255), pu = AC_RGB(24, 16, 48);
    int32_t cy = y - EZ(10) + (tsin(tm, 30, 0) >> 13), k, xy[6], blink = (tm % 3400u) < 120u;
    uint32_t a0 = ga;
    ga = a0 * 36u >> 8; gdisc(x, cy, EZ(19), fur); ga = a0;
    for (k = -1; k <= 1; k += 2) {               /* the ears */
        int32_t tx = x + k * EZ(8 + (256 - perk) * 13 / 256), ty = cy - EZ(25) + EZ((256 - perk) * 16 / 256);
        xy[0] = x + k; xy[1] = cy - EZ(6); xy[2] = x + k * EZ(9); xy[3] = cy - EZ(3); xy[4] = tx; xy[5] = ty; gpoly(xy, 3, fur2);
        xy[0] = x + k * 2; xy[1] = cy - EZ(6); xy[2] = x + k * EZ(7); xy[3] = cy - EZ(4); xy[4] = (tx * 3 + x) / 4 + k; xy[5] = (ty * 3 + cy) / 4; gpoly(xy, 3, inner);
    }
    for (k = 0; k < 12; k++) { int32_t a = k * 5461; gdisc(x + (icos(a) * EZ(10) >> 14), cy + (isin(a) * EZ(9) >> 14), 2, fur); }   /* fluff */
    gellf(x, cy, EZ(10), EZ(9), fur, 0);
    gellf(x, cy + 3, EZ(6), EZ(4), AC_RGB(238, 230, 255), 0);
    gellf(x - 4, y, 2, 1, fur2, 0); gellf(x + 4, y, 2, 1, fur2, 0);
    if (blink) { ghl(x - 5 + look, x - 1 + look, cy - 2, pu); ghl(x + 2 + look, x + 6 + look, cy - 2, pu); }
    else for (k = -1; k <= 1; k += 2) {
        int32_t ex = x + k * 3 + look;
        gdisc(ex, cy - 2, 2, eye); gp(ex + (look > 0) - (look < 0), cy - 1, pu); gp(ex + (look > 0) - (look < 0), cy - 2, pu);
    }
    ga = a0 * 110u >> 8; gp(x - 6, cy + 1, inner); gp(x + 6, cy + 1, inner); ga = a0;   /* cheeks */
    if (sing > 0) gellf(x + look, cy + 3, 1, 1 + sing / 256, pu, 0);
    else { gp(x - 1 + look, cy + 3, pu); gp(x + look, cy + 4, pu); gp(x + 1 + look, cy + 3, pu); }
    if (sad) { int32_t q = (int32_t)(tm % 900u); ga = a0 * 220u >> 8; gp(x - 4 + look, cy + 1 + q / 120, AC_RGB(120, 200, 255)); gp(x - 4 + look, cy + 2 + q / 120, AC_RGB(120, 200, 255)); ga = a0; }
#undef EZ
}
/* a sound: a note rising and fading from (x, y), age q ms, small for the Echo */
static void ew_note(int32_t x, int32_t y, int32_t q, col_t c, int small)
{
    if (q < 0 || q > 900) return;
    galpha(256 - q * 256 / 900);
    gtextc(x + (tsin((uint32_t)q, 60, 0) >> 12), y - q / 30, "\xE2\x99\xAA", c, small ? 1 : 2);
    galpha(256);
}
static void ew_bubble(int32_t x, int32_t y, int n, int32_t a)   /* the Echo's count, in a bubble: n */
{
    char b[2];
    b[0] = (char)('0' + n); b[1] = 0;
    galpha(a);
    gellf(x, y, 12, 11, th->text, 0); gellf(x, y, 11, 10, AC_RGB(10, 14, 26), 0);
    gtextc(x + 1, y - 7, b, th->caps[0], 2);
    galpha(256);
}

/* ---------------------------------------------------------------- ECHO STEPS: the game, there */
/* where each rock goes in round r: a list of moves (from the last place to this one, starting at t, taking dur ticks,
 * a hop or a roll). t counts from the round's start: 0 the call, 384 the count, 768 the echo, 1152 the count on */
typedef struct { int32_t t, x, y, dur; uint8_t hop; } ew_wp_t;
static int ew_echoer(int seat, int r)            /* a seat's place among those who echo in round r; -1: it leads */
{
    int c = r % du.np, s, j = 0;
    if (seat == c) return -1;
    for (s = 0; s < seat; s++) if (s != c) j++;
    return j;
}
static int32_t ew_home(int seat, int r) { int j = ew_echoer(seat, r); return ew_gl(r) - (j < 0 ? 10 : 28 + 18 * j); }
static int ew_wps(int seat, int r, ew_wp_t *w)
{
    int n = 0, i, j = ew_echoer(seat, r);
    uint32_t base = (uint32_t)(r * 64 + 32);
    int32_t gl = ew_gl(r);
#define WP(T, X, Y, D, H) do { w[n].t = (T); w[n].x = (X); w[n].y = (Y); w[n].dur = (D); w[n].hop = (uint8_t)(H); n++; } while (0)
    WP(-99999, ew_home(seat, r), EW_LY, 1, 0);
    if (j >= 0) WP(420, gl - 10 - 16 * j, EW_LY, 120, 0);   /* (to the edge, to wait) */
    for (i = 0; i < du.n && n < 11; i++) {       /* the stones: the one who leads on each as it was made; the others on
                                                    the ones they played right */
        const du_note_t *e = &du_n[i];
        int slot;
        if (e->step < base || e->step >= base + 16u) continue;
        slot = (int)(e->step - base) / 2;
        if (j < 0 && !e->hid) WP(slot * 48 - 12, ew_bx(r, slot), EW_LV[e->lane & 3], 32, 1);
        if (j >= 0 && (e->st[seat] == 1u || e->st[seat] == 2u)) WP(768 + slot * 48 - 6, ew_bx(r, slot) + 4 * j, EW_LV[e->lane & 3], 32, 1);
    }
    if (j < 0) { WP(384, gl + EW_GW + 12, EW_LY, 40, 1); WP(440, gl + EW_GW + 48, EW_LY, 120, 0); }
    else WP(1152, gl + EW_GW + 12 + 16 * j, EW_LY, 40, 1);
    if (r + 1 < DU_ECHO_R) WP(1200, ew_home(seat, r + 1), EW_LY, 240, 0);
#undef WP
    return n;
}
/* a rock at t on its moves: world x, its middle's y */
static void ew_at(const ew_wp_t *w, int n, int32_t t, int32_t *x, int32_t *y)
{
    int i = 0, k;
    int32_t f;
    for (k = 1; k < n; k++) if (t >= w[k].t) i = k;
    if (i == 0) { *x = w[0].x; *y = w[0].y - 11; return; }
    f = iclamp((t - w[i].t) * 256 / imax(w[i].dur, 1), 0, 256);
    *x = w[i - 1].x + (w[i].x - w[i - 1].x) * (w[i].hop ? f : ease8(f)) / 256;
    *y = w[i - 1].y + (w[i].y - w[i - 1].y) * f / 256 - 11 - (w[i].hop ? (isin(f * 128) * 16) >> 14 : 0);
}
/* a key pressed during the echo, judged wrong: no stone of that player's in that colour right there */
static int ew_wrong(int i)
{
    int k;
    for (k = 0; k < du.n; k++) {
        const du_note_t *e = &du_n[k];
        if ((e->own >> du_wp[i].seat & 1u) && e->lane == du_wp[i].lane && (e->st[du_wp[i].seat] == 1u || e->st[du_wp[i].seat] == 2u)
            && iabs((int32_t)e->step * 24 - du_wp[i].t) <= DU_WIN) return 0;
    }
    return 1;
}
static void ew_family(int32_t x, int home, int32_t q, uint32_t tm)   /* the Echo's family on the last ledge (world x) */
{
    int k;
    for (k = 0; k < 3; k++) {
        int32_t fx = x + k * 15 - ew_cam;
        if (fx < -20 || fx > 260) continue;
        ew_critter(fx, EW_LY - (home && q >= 0 ? dw_hop(q % 520, 0, 300) * 6 / 256 : 0), home ? 200 : 0, home ? 256 : 120, -1, 0, tm + (uint32_t)k * 900u);
        if (!home && (tm + (uint32_t)k * 700u) % 2600u < 900u) ew_note(fx - 4, EW_LY - 22, (int32_t)((tm + (uint32_t)k * 700u) % 2600u), AC_RGB(206, 190, 255), 1);
    }
}
static void du_echo_world(int32_t now)
{
    uint32_t tm = fm1_ms;
    int me = du.seat, r = now < 0 ? 0 : imin(now / DU_ECHO_LEN, DU_ECHO_R - 1), c = r % du.np, nc = (r + 1) % du.np, s, k, i;
    int last = r == DU_ECHO_R - 1, ph, b, mine, next;
    int32_t q = now - r * DU_ECHO_LEN, x[3], y[3], ex, ey, perch = ew_gl(r) + EW_GW + 66, pulse, look = 1, sing = 0;
    uint32_t base = (uint32_t)(r * 64 + 32);
    ew_wp_t w[14];
    ph = q < 0 ? -1 : q / 384; b = q < 0 ? 0 : q % 384 / 96;
    pulse = q < 0 ? 0 : (96 - q % 96) * (96 - q % 96) * 256 / 9216;
    mine = (ph == 0 && c == me) || (ph == 2 && c != me);
    next = (ph == 1 && c != me) || (ph == 3 && !last && nc == me) || (ph < 0 && c == me);
    ew_cz = 128;
    ew_cam = ew_gl(r) - 50 + (q >= 1152 ? (last ? 100 : EW_SP) * dw_ramp(q, 1170, 1420) / 256 : 0);
    ew_back(tm);
    for (k = 0; k <= DU_ECHO_R; k++) {           /* the ledges between the chasms (the first and the last go on) */
        int32_t x0 = k ? ew_gl(k) - (EW_SP - EW_GW) : -400, x1 = k < DU_ECHO_R ? ew_gl(k) : ew_gl(k) + 400;
        ew_ledge(x0 - ew_cam, x1 - ew_cam, k);
        if (k) ew_mushroom(x0 + 40 - ew_cam, EW_LY, 6 + k % 3 * 2, th->caps[k % 5], tm, k);
    }
    ew_family(ew_gl(DU_ECHO_R) + 40, last && q >= 1300, q - 1300, tm);
    /* the stones: rising as the path is played, sinking to glimmers, back up under each note played right */
    for (i = 0; i < du.n; i++) {
        const du_note_t *e = &du_n[i];
        int slot, hit = 0;
        int32_t sx, sy, at;
        if (e->step < base || e->step >= base + 16u || q < 0) continue;
        slot = (int)(e->step - base) / 2;
        sx = ew_bx(r, slot) - ew_cam; sy = EW_LV[e->lane & 3];
        for (s = 0; s < du.np; s++) if ((e->own >> s & 1u) && (e->st[s] == 1u || e->st[s] == 2u)) hit = 1;
        at = e->hid ? 384 : slot * 48 - 12;
        if (q < 480) { if (q >= at) ew_stone(sx, sy, e->lane, imin(256, (q - at) * 256 / 24), 0, tm); continue; }
        if (q < 768 || !hit || q < 768 + slot * 48 - 12) {
            int32_t f = iclamp((q - 480) * 256 / 192, 0, 256);
            if (f < 256) ew_stone(sx, sy + f * 26 / 256, e->lane, 256 - f, 0, tm);
            if (f > 100) { galpha((f - 100) * 256 / 156); ew_stone(sx, sy, e->lane, 256, 1, tm); galpha(256); }
            continue;
        }
        at = 768 + slot * 48 - 12;
        ew_stone(sx, sy, e->lane, imin(256, (q - at) * 256 / 24), 0, tm);
        if (q - at < 72) { galpha(256 - (q - at) * 256 / 72); gcirc(sx, sy, 10 + (q - at) / 5, th->text); galpha(256); sing = 220; }
    }
    for (i = 0; i < du_wpn; i++) {               /* a wrong note in the echo: a wobbly grey stone, a while */
        int32_t age = now - du_wp[i].t, slot = iclamp((du_wp[i].t - r * DU_ECHO_LEN - 768 + 24) / 48, 0, 7);
        if (age < 40 || age > 420 || !ew_wrong(i)) continue;
        galpha(age > 300 ? (420 - age) * 256 / 120 : 256);
        ew_stone(ew_bx(r, slot) - ew_cam + (du_wp[i].seat != me ? 3 : 0), EW_LV[du_wp[i].lane & 3] + 3, du_wp[i].lane, 256, 2, tm);
        galpha(256);
    }
    /* the rocks, each on its moves; a ring beating round each one whose turn it is (or is next) */
    for (s = 0; s < du.np && s < 3; s++) { int n = ew_wps(s, r, w); ew_at(w, n, q, &x[s], &y[s]); }
    for (k = 0; k < du.np && k < 3; k++) {
        int sd = (k + me + 1) % du.np;           /* (yours last: in front) */
        int turn = (ph == 0 && sd == c) || (ph == 2 && sd != c), nx = (ph == 1 && sd != c) || (ph == 3 && !last && sd == nc) || (ph < 0 && sd == c);
        if ((turn || nx) && pulse) {
            galpha(pulse * (turn ? 220 : 140) / 256);
            gcirc(x[sd] - ew_cam, y[sd], 14 + (256 - pulse) * 10 / 256, DU_COL[sd]);
            galpha(256);
        }
        dw_rock(x[sd] - ew_cam, y[sd], sd, sd == me ? 230 : 210, x[sd] * 1159, 0, tm);
    }
    /* the Echo: riding on the head of whoever leads; off to the far side to count and watch; onto the next one's head;
       home to its family after the last */
    ex = x[c]; ey = y[c] - 11;
    if (q >= 400) {
        int32_t f = dw_ramp(q, 400, 470);
        ex = x[c] + (perch - x[c]) * f / 256; ey = (y[c] - 11) + (EW_LY - (y[c] - 11)) * f / 256 - (isin(f * 128) * 18 >> 14);
        look = -1;
    }
    if (q >= 1180) {
        int32_t f = dw_ramp(q, 1180, 1260), tx = last ? ew_gl(DU_ECHO_R) + 22 : x[nc], ty = last ? EW_LY : y[nc] - 11;
        ex = perch + (tx - perch) * f / 256; ey = EW_LY + (ty - EW_LY) * f / 256 - (isin(f * 128) * 18 >> 14);
        look = 1;
    }
    if ((ph == 1 || (ph == 3 && !last)) && q % 96 < 40) sing = 200;   /* (counting) */
    ew_critter(ex - ew_cam, ey, sing, 256, look, 0, tm);
    if (last && q >= 1300) for (k = 0; k < 4; k++) ew_note(ex - ew_cam - 6 + k * 8, ey - 18, (int32_t)((q - 1300 - k * 30) * 6), th->caps[k], 1);
    if ((ph == 1 || (ph == 3 && !last)) && q % 96 < 80) ew_bubble(ex - ew_cam - (ex - ew_cam > 200 ? 20 : -20), ey - 28, 4 - b, q % 96 < 60 ? 256 : (80 - q % 96) * 256 / 20);
    /* what it is now, said large; the rounds and the bar's beats at the top; YOUR TURN */
    if (ph >= 0) {
        int id = ph == 0 ? (c == me ? T_DU_CALL : T_DU_WATCH) : ph == 1 ? (next ? T_DU_GETREADY : T_DU_NEXT_FR)
               : ph == 2 ? (mine ? T_DU_ECHO : T_DU_LISTEN) : last ? -1 : nc == me ? T_DU_NEXT_YOU : T_DU_NEXT_FR;
        int who = ph == 0 ? c : ph == 3 ? nc : mine || next ? me : (c + 1) % du.np;
        if (id >= 0) du_say(44, TR(id), next ? ((q / 48) & 1 ? th->text : DU_COL[me]) : DU_COL[who]);
        for (i = 0; i < DU_ECHO_R; i++) { if (i <= r) grect(6 + i * 9, 8, 6, 6, i == r ? th->text : th->mute); else gbox(6 + i * 9, 8, 6, 6, th->mute); }
        for (i = 0; i < 4; i++) {
            int32_t bx = 87 + i * 22;
            col_t cc = DU_COL[ph == 0 || ph == 3 ? (ph ? nc : c) : (mine || next) ? me : (c + 1) % du.np];
            if (i == b) gdisc(bx, 11, 5 + pulse * 3 / 256, cc); else if (i < b) gdisc(bx, 11, 4, cc); else gcirc(bx, 11, 4, th->mute);
        }
        if (mine && q % 384 < 120) { galpha(230 - q % 384 * 230 / 120); gfit(120, 92, 232, TR(T_TU_TURN), DU_COL[me], 4); galpha(256); }
    }
    du_kb();
    ew_cz = 192;
}

/* ---------------------------------------------------------------- ECHO STEPS: the opening */
static void ec_intro(int32_t t, uint32_t tm)
{
    int32_t px, tx, cx, cy, perk, sing = 0, look = -1, sad = 0, k, pz = 0, ride = 0;
    ew_cam = -440 + 410 * dw_ramp(t, 7200, 10400) / 256;
    ew_back(tm);
    {   /* beyond the chasm, far off: a wall of rock, darker, where the family waits */
        int32_t q[12];
        q[0] = 176 - ew_cam; q[1] = 240; q[2] = 170 - ew_cam; q[3] = 120; q[4] = 184 - ew_cam; q[5] = 70; q[6] = 212 - ew_cam; q[7] = 44;
        q[8] = 320 - ew_cam; q[9] = 30; q[10] = 320 - ew_cam; q[11] = 240;
        gpoly(q, 6, AC_RGB(10, 16, 26));
    }
    ew_ledge(-700 - ew_cam, 56 - ew_cam, 0);     /* the grotto's floor, on to the chasm's edge */
    ew_ledge(196 - ew_cam, 296 - ew_cam, 1);
    ew_mushroom(-400 - ew_cam, EW_LY, 22, th->caps[1], tm, 1); ew_mushroom(-382 - ew_cam, EW_LY, 12, th->caps[3], tm, 2);
    ew_mushroom(-130 - ew_cam, EW_LY, 14, th->caps[4], tm, 3); ew_mushroom(-80 - ew_cam, EW_LY, 8, th->caps[0], tm, 4);
    ew_mushroom(-200 - ew_cam, EW_LY, 10, th->caps[2], tm, 6);
    for (k = 0; k < 7; k++) {                    /* fireflies in the grotto */
        int32_t fx = -420 + k * 50 + (tsin(tm, 7 + (uint32_t)k, k * 9000) >> 10) - ew_cam, fy = 70 + k * 9 % 40 + (tsin(tm, 11, k * 7000) >> 11);
        galpha(100 + ipulse(tm + (uint32_t)k * 500u, 1300) * 150 / 256); gdisc(fx, fy, 1, AC_RGB(255, 230, 120)); galpha(40); gdisc(fx, fy, 4, AC_RGB(255, 230, 120));
    }
    galpha(256);
    for (k = 0; k < 2; k++) {                    /* the broken bridge: its posts, its ropes hanging into the dark */
        int32_t bx = (k ? 204 : 48) - ew_cam, d = k ? -1 : 1, j;
        grect(bx - 2, EW_LY - 18, 4, 18, AC_RGB(110, 80, 56)); grect(bx - 3, EW_LY - 20, 6, 3, AC_RGB(140, 104, 70));
        for (j = 0; j < 2; j++) {
            int32_t sw = tsin(tm, 12, k * 20000 + j * 9000) >> 12;
            gline(bx, EW_LY - 16 + j * 6, bx + d * 6 + sw, EW_LY + 30 + j * 14, AC_RGB(150, 120, 80));
            grect(bx + d * 4 + sw, EW_LY + 24 + j * 14, 6, 2, AC_RGB(110, 80, 56));
        }
    }
    for (k = 0; k < 6; k++) {                    /* the Echo's family across the chasm: eyes in the dark, calling */
        static const int16_t FX[6] = {188, 200, 212, 194, 222, 206}, FY[6] = {128, 116, 134, 98, 110, 84};
        int32_t fx = FX[k] - ew_cam, fy = FY[k], bl = (tm + (uint32_t)k * 911u) % 2900u < 130u;
        if (fx > 250) continue;
        galpha(60); gdisc(fx, fy, 6, AC_RGB(206, 190, 255)); galpha(256);
        if (!bl) { gdisc(fx - 2, fy, 1, th->text); gdisc(fx + 2, fy, 1, th->text); }
        if (t > 8400 && (k == 1 || k == 3)) ew_note(fx - 4, fy - 8, (int32_t)((tm + (uint32_t)k * 400u) % 1400u), AC_RGB(206, 190, 255), 1);
    }
    /* the two rocks: rolling in, flopping down to rest, on to the edge */
    px = -480 + 160 * dw_ramp(t, 0, 2200) / 256;
    tx = -520 + 170 * dw_ramp(t, 150, 2350) / 256;
    if (t >= 4400) px += 34 * dw_ramp(t, 4400, 5000) / 256;   /* Pink goes to look */
    if (t >= 7600) { px += (20 - (-286)) * dw_ramp(t, 7600, 10200) / 256; tx += (-10 - (-350)) * dw_ramp(t, 7800, 10400) / 256; }
    {
        int32_t hop0 = dw_hop(t, 11300, 300) + dw_hop(t, 2700, 220), hop1 = dw_hop(t, 11420, 300) + dw_hop(t, 3700, 250);
        int32_t ang0 = t < 2200 ? -(t * 40) : t < 7600 ? 0 : t < 10200 ? -(t - 7600) * 30 : 0, ang1 = t < 2350 ? -(t * 40) : t < 7800 ? 0 : t < 10400 ? -(t - 7800) * 30 : 0;
        if (t >= 10600 && t < 11300) { ang0 = -5000; ang1 = 5000; }   /* a look at each other */
        if (t >= 2650 && t < 2900) pz = 1;
        dw_rock(tx - ew_cam, EW_LY - 11, 1, 210, ang1, hop1, tm);
        dw_rock(px - ew_cam, EW_LY - 11 + pz * 2, 0, 210, ang0, hop0, tm);
        if (t >= 2650 && t < 3300) {             /* ACHOO: a puff */
            int32_t q = t - 2650, sx = px - ew_cam + 16;
            galpha(256 - q * 256 / 650);
            for (k = 0; k < 9; k++) gdisc(sx + 2 + (q / 18) * (k % 3 + 1) / 3 + k % 3 * 3, EW_LY - 16 + (k / 3 - 1) * (2 + q / 90), q < 300 ? 1 : 0, th->text);
            galpha(256);
        }
        if (t >= 3600 && t < 4400) { ew_note(tx - ew_cam + 8, EW_LY - 26, t - 3600, DU_COL[1], 0); ew_note(tx - ew_cam + 16, EW_LY - 30, t - 3850, DU_COL[1], 0); }
    }
    /* the Echo: behind its mushroom, a sneeze back, two notes back, peeking, out; sad; to the edge; onto Pink */
    cx = -222; cy = EW_LY; perk = 256;
    if (t >= 3000 && t < 3500) {                 /* a tiny sneeze back from behind the mushroom */
        int32_t q = t - 3000;
        galpha(256 - q * 256 / 500); for (k = 0; k < 3; k++) gdisc(-236 - ew_cam - q / 40 - k * 3, EW_LY - 26 + k % 2 * 3, 2, th->text); galpha(256);
    }
    if (t >= 4300 && t < 5100) { ew_note(-236 - ew_cam, EW_LY - 30, t - 4300, AC_RGB(206, 190, 255), 1); ew_note(-230 - ew_cam, EW_LY - 34, t - 4550, AC_RGB(206, 190, 255), 1); }
    if (t >= 4800) {
        int32_t peek = dw_ramp(t, 4800, 5400), out = dw_ramp(t, 5800, 6300);
        cx = -222 + (-258 + 222) * out / 256;
        cy = EW_LY + (256 - peek) * 26 / 256 * (256 - out) / 256;
        cy -= (isin(out * 128) * 14) >> 14;
        look = t < 6500 ? -1 : 1;
        sing = (t >= 4300 && t < 4700) ? 200 : 0;
        if (t >= 6500 && t < 10400) { perk = 256 - dw_ramp(t, 6600, 7000) * 3 / 4; sad = t < 9000; }
        if (t >= 7600) cx += (44 - (-258)) * dw_ramp(t, 7600, 10000) / 256;
        if (t >= 10400) { perk = 64; look = 1; }
        if (t >= 11700) {                        /* up onto Pink's head */
            int32_t f = dw_ramp(t, 11700, 12200);
            cx = 44 + (20 - 44) * f / 256; cy = EW_LY - 22 * f / 256 - (isin(f * 128) * 20 >> 14);
            perk = 64 + 192 * dw_ramp(t, 12200, 12500) / 256;
            if (t >= 12200) { cy = EW_LY - 22; ride = 1; }
        }
        if (t < 5800) gclip_y1 = EW_LY + 1;      /* (peeking up out of the moss behind the mushroom) */
        ew_critter(cx - ew_cam, cy, sing, perk, look, sad, tm);
        gclip_y1 = AC_H;
    }
    ew_mushroom(-236 - ew_cam, EW_LY, 26, th->caps[0], tm, 5);   /* (the mushroom it hides behind, in front of it) */
    if (ride && t > 12500) for (k = 0; k < 3; k++) ew_note(192 + k * 10 - ew_cam, 100 + k * 6, t - 12500 - k * 200, AC_RGB(206, 190, 255), 1);
    gzoom(150 - 90 * dw_ramp(t, 7000, 9600) / 256, 140, 430 - 174 * dw_ramp(t, 7000, 9600) / 256);   /* close in the grotto,
                                                    pulling back to show the chasm */
    dw_title(t, 12900, DM_ECHO, th->caps[0]);
    if (t < 12900) dw_skip(tm);
}
static void ec_sounds(int32_t t0, int32_t t1)    /* the opening's sounds, as their moments pass */
{
    int k;
    if (dw_at(t0, t1, 2650)) { sfx(76, 0, 160, W_NOISE, 1400, 2, 30); sfx(64, 0, 90, W_SQUARE, 500, 1, 0); }   /* ACHOO */
    if (dw_at(t0, t1, 3000)) sfx(96, 0, 70, W_NOISE, 600, 1, 20);        /* a tiny one back */
    if (dw_at(t0, t1, 3600)) sfx(60, 0, 260, W_TRI, 2000, 20, 80);
    if (dw_at(t0, t1, 3850)) sfx(64, 0, 300, W_TRI, 2000, 20, 80);
    if (dw_at(t0, t1, 4300)) sfx(84, 0, 220, W_SINE, 1000, 10, 100);     /* and back, tiny */
    if (dw_at(t0, t1, 4550)) sfx(88, 0, 260, W_SINE, 1000, 10, 100);
    for (k = 0; k < 3; k++) if (dw_at(t0, t1, 8400 + k * 1400)) { sfx(79, 0, 220, W_SINE, 700, 10, 140); sfx(83, 160, 260, W_SINE, 600, 10, 140); }   /* the family calls */
    if (dw_at(t0, t1, 11700)) sfx(84, 0, 80, W_SQUARE, 500, 1, 0);
    if (dw_at(t0, t1, 12500)) { sfx(79, 0, 200, W_SINE, 900, 5, 140); sfx(84, 120, 200, W_SINE, 900, 5, 140); sfx(91, 240, 400, W_SINE, 900, 5, 140); }
    if (dw_at(t0, t1, 12900)) snd_ui(79);
}

/* ---------------------------------------------------------------- NOTE RALLY: the giant's hall */
#define RH_FL 200                                /* the floor */
#define RH_ON 185                                /* a rock on a cushion: its middle */
#define RH_HI 171                                /* the note, just over a rock */
/* the giant's face, behind the floor. smile -256 (a frown) .. 256; eye 0 shut .. 256 open; lit: crystals on its head
 * lit (0..5); light 0..256 (how much of it can be seen); sh: the hall shaking */
static void rh_giant(int32_t smile, int32_t eye, int lit, int32_t light, int32_t sh, uint32_t tm)
{
    col_t head = dw_mix(AC_RGB(4, 6, 12), AC_RGB(24, 32, 50), light), rim = dw_mix(AC_RGB(4, 6, 12), AC_RGB(70, 92, 124), light);
    col_t line = dw_mix(AC_RGB(4, 6, 12), AC_RGB(150, 170, 205), light);
    int32_t cx = 120 + sh, cy = 120, k, xy[8], breathe = tsin(tm, 4, 0) >> 12;
    gellf(cx, cy + breathe, 114, 98, head, 0);
    garc(cx, cy + breathe, 114, 98, 32768, 65536, rim, 1);
    for (k = 0; k < 16; k++) {                   /* moss over its crown */
        int32_t a = 34000 + k * 1900, x = cx + (icos(a) * 108 >> 14), y = cy + breathe + (isin(a) * 92 >> 14);
        galpha(light); gdisc(x, y, 5 + (int32_t)(ihash((uint32_t)k) % 4u), dw_mix(AC_RGB(4, 6, 12), th->moss, 200)); galpha(256);
    }
    for (k = 0; k < 5; k++) {                    /* its crystals: one lit for each time the note is kept up */
        int32_t a = 40000 + k * 4200, x = cx + (icos(a) * 96 >> 14), y = cy + breathe + (isin(a) * 82 >> 14) - 4;
        col_t c = k < lit ? th->caps[k] : dw_mix(AC_RGB(4, 6, 12), AC_RGB(60, 70, 96), light);
        if (k < lit) { galpha(70 + ipulse(tm + (uint32_t)k * 200u, 900) * 60 / 256); gdisc(x, y - 4, 10, c); galpha(256); }
        xy[0] = x; xy[1] = y - 14; xy[2] = x + 5; xy[3] = y - 4; xy[4] = x; xy[5] = y + 2; xy[6] = x - 5; xy[7] = y - 4;
        gpoly(xy, 4, c);
    }
    for (k = -1; k <= 1; k += 2) {               /* its eyes */
        int32_t ex = cx + k * 42, ey = cy - 6 + breathe;
        if (eye > 10) {
            int32_t h = 8 * eye / 256 + 1;
            gellf(ex, ey, 15, h, dw_mix(AC_RGB(4, 6, 12), AC_RGB(255, 236, 170), light), 0);
            gdisc(ex, ey, imin(h, 5), AC_RGB(30, 20, 10));
            garc(ex, ey, 16, h + 1, 32768, 65536, line, 1);
        } else garc(ex, ey, 15, 6, 0, 32768, line, 1);   /* shut: asleep */
        if (smile < -60) gline2(ex - 14 * k, ey - 14 + (-smile) / 64, ex + 4 * k, ey - 18, line);   /* a frown's brows */
    }
    gellf(cx, cy + 22 + breathe, 15, 11, dw_mix(head, AC_RGB(255, 255, 255), 30), 0);   /* its nose */
    gdisc(cx - 5, cy + 26 + breathe, 2, AC_RGB(4, 6, 12)); gdisc(cx + 5, cy + 26 + breathe, 2, AC_RGB(4, 6, 12));
    if (smile >= 0) garc(cx, cy + 44 + breathe, 28, 2 + smile * 12 / 256, 0, 32768, line, 1);
    else garc(cx, cy + 52 + breathe, 22, 2 - smile * 8 / 256, 32768, 65536, line, 1);
    if (smile > 120) { galpha((smile - 120) * 110 / 136); gellf(cx - 66, cy + 28, 12, 6, AC_RGB(255, 120, 170), 0); gellf(cx + 66, cy + 28, 12, 6, AC_RGB(255, 120, 170), 0); galpha(256); }
}
static void rh_snore(int32_t a, uint32_t tm)     /* asleep: a bubble at its nose, and Z's drifting up */
{
    int32_t q = (int32_t)(tm % 2600u), r = q < 1600 ? 3 + q * 9 / 1600 : 12 - (q - 1600) * 9 / 1000, k;
    galpha(a * 120 / 256); gcirc(132, 150, r, AC_RGB(160, 210, 255)); galpha(a * 60 / 256); gdisc(132, 150, r, AC_RGB(160, 210, 255));
    for (k = 0; k < 3; k++) {
        int32_t z = (int32_t)((tm / 3u + (uint32_t)k * 333u) % 1000u);
        galpha(a * (256 - z * 256 / 1000) / 256);
        gtext(150 + z * 30 / 1000 + k * 4, 120 - z * 60 / 1000, "Z", AC_RGB(160, 210, 255), 1 + (z > 500));
    }
    galpha(256);
}
static void rh_cushion(int32_t x, int32_t rx, col_t f, int32_t glow)   /* moss, two little flowers in its side's colour */
{
    if (glow) { galpha(glow); gellf(x, RH_FL, rx + 4, rx / 2 + 1, f, 0); galpha(256); }
    gellf(x, RH_FL + 2, rx, rx / 2, AC_RGB(18, 84, 80), 1); garc(x, RH_FL + 2, rx, rx / 2, 32768, 65536, th->moss, 0);
    gp(x - rx / 2, RH_FL - 3, f); gp(x + rx * 2 / 5, RH_FL - 2, f); gp(x - rx / 2, RH_FL - 4, th->text);
}
static void rh_floor(int32_t sh)
{
    int k;
    grect(0, RH_FL, 240, 240 - RH_FL, AC_RGB(12, 16, 26));
    ghl(0, 240, RH_FL, AC_RGB(50, 60, 84));
    for (k = 0; k < 6; k++) ghl(0, 240, RH_FL + 8 + k * 7, AC_RGB(16, 22, 34));
    for (k = 0; k < 12; k++) gline(k * 24 + sh - 20, RH_FL + 1, k * 30 - 60 + sh, 240, AC_RGB(18, 24, 36));
}
static void rh_note(int32_t x, int32_t y, uint32_t tm)   /* the note: a glowing ball with a note in it */
{
    galpha(60 + ipulse(tm, 500) * 50 / 256); gdisc(x, y, 13, AC_RGB(255, 220, 120)); galpha(256);
    gdisc(x, y, 7, AC_RGB(255, 226, 140)); gcirc(x, y, 7, AC_RGB(255, 255, 230));
    gtextc(x, y - 4, "\xE2\x99\xAA", AC_RGB(120, 70, 10), 1);
}
static void rh_hall(int32_t smile, int32_t eye, int lit, int32_t sh, uint32_t tm)
{
    int k;
    gclear(AC_RGB(2, 3, 8));
    for (k = 0; k < 4; k++) {                    /* pillars far behind */
        int32_t x = 10 + k * 73 + sh / 2;
        grect(x, 0, 12, RH_FL, AC_RGB(8, 10, 18)); ghl(x - 3, x + 15, 8, AC_RGB(14, 18, 30));
    }
    rh_giant(smile, eye, lit, 256, sh, tm);
}

/* ---------------------------------------------------------------- NOTE RALLY: the game, there */
static void du_rally_hall(int32_t now)
{
    uint32_t tm = fm1_ms;
    int me = du.seat, s, k, lit = imin(du.rally, 5);
    int32_t q = now - du_rdrop_t, sh = 0, smile = 40 + lit * 36, eye = 0, rx = du.np == 3u ? 8 : 12;
    char b[8];
    if (du_rdrop_t > -100000 && q >= 0 && q < 384) {   /* dropped: it stirs, a rumble, an eye opening a crack, a frown */
        if (q < 160) sh = (tsin(tm, 400, 0) >> 13) * (160 - q) / 160;
        eye = q < 48 ? q * 120 / 48 : q < 200 ? 120 : (384 - q) * 120 / 184;
        smile = q < 200 ? -200 : -200 + (smile + 200) * (q - 200) / 184;
    }
    rh_hall(smile, eye, lit, sh, tm);
    rh_floor(sh);
    for (s = 0; s < du.np; s++) for (k = 0; k < 4; k++) {   /* the cushions, the one it will land on glowing */
        int land = du.bmode && du.bown == s && du.bto == k;
        rh_cushion(du_rh_x(s, k) + sh, rx, DU_COL[s], land ? (s == me ? 110 + ipulse(tm, 300) * 120 / 256 : 80) : 0);
    }
    if (q >= 0 && q < 160) for (k = 0; k < 7; k++) {   /* pebbles from the roof */
        int32_t z = (q * 4 + k * 37) % 200;
        gdisc(20 + k * 33 + (int32_t)(ihash((uint32_t)k) % 20u), z, 1 + k % 2, AC_RGB(90, 100, 120));
    }
    gbox(70, 2, 100, 5, th->mute);               /* how much of the song is left */
    grect(72, 4, 96 * iclamp(du.end_tick - now, 0, du.end_tick) / imax(du.end_tick, 1), 1, th->pad);
    for (s = 0; s < du.np; s++) if (s != me) dw_rock((du.rx[s] >> 8) + sh, RH_ON, s, 210, (du.rx[s] >> 8) * 1159, du.hop[s] > 0 ? (240 - du.hop[s]) * 256 / 240 : 0, tm);
    dw_rock((du.rx[me] >> 8) + sh, RH_ON, me, 230, (du.rx[me] >> 8) * 1159, du.hop[me] > 0 ? (240 - du.hop[me]) * 256 / 240 : 0, tm);
    if (du.bmode) {                              /* on its way: up and over, from where it was sent to where it lands */
        int from = (du.bown + du.np - 1) % du.np;
        int32_t f = iclamp((now - du.bt0) * 256 / imax(du.bt1 - du.bt0, 1), 0, 256), x0 = du_rh_x(from, du.bfrom), x1 = du_rh_x(du.bown, du.bto);
        int32_t x = x0 + (x1 - x0) * f / 256, y = RH_HI - (isin(f * 128) * (RH_HI - 44) >> 14);
        if (f > 160) { galpha((f - 160) * (du.bown == me ? 220 : 120) / 96); gcirc(x1 + sh, RH_HI, 9 + (256 - f) / 6, th->text); galpha(256); }
        rh_note(x + sh, y, tm);
    } else {                                     /* waiting to be served: over the rock that has it (just dropped: floating
                                                    back up from where it fell) */
        int sv = du.bown;
        int32_t x = (du.rx[sv] >> 8) + sh, y = RH_ON - 26 + (tsin(tm, 50, 0) >> 12);
        if (q >= 0 && q < 192) { int32_t f = dw_ramp(q, 0, 192); x = du_rdrop_x + (x - du_rdrop_x) * f / 256; y = RH_FL - 8 + (y - RH_FL + 8) * f / 256; }
        rh_note(x, y, tm);
        if (sv == me && now >= du.bready) { galpha(150 + ipulse(tm, 900) * 106 / 256); gfit(120, 108, 232, TR(T_DU_SERVE), DU_COL[me], ts(1, 2)); galpha(256); }
    }
    if (du.rally) { b[0] = 'x'; num_str(b + 1, du.rally); gtext(6, 6, b, du.rally >= 4 ? th->pad : th->text, 2); }
    du_kb();
}

/* ---------------------------------------------------------------- NOTE RALLY: the opening */
static const int16_t RH_PX[8] = {18, 44, 70, 96, 144, 170, 196, 222};   /* (the opening's cushions: Pink's, then Teal's) */
static void ra_corridor(int32_t t, uint32_t tm)
{
    int k;
    int32_t open = dw_ramp(t, 3500, 4300), xs;
    gclear(AC_RGB(6, 8, 16));
    for (k = 0; k < 6; k++) {                    /* carved rings along the wall */
        int32_t x = 20 + k * 52;
        galpha(50); garc(x, 96, 18, 18, 0, 65536, th->caps[k % 5], 0); garc(x, 96, 11, 11, 0, 65536, th->caps[(k + 2) % 5], 0); galpha(256);
        if (k % 2 == 0) { galpha(90 + ipulse(tm + (uint32_t)k * 300u, 1700) * 120 / 256); gdisc(x + 26, 70, 3, th->caps[(k + 3) % 5]); galpha(40); gdisc(x + 26, 70, 9, th->caps[(k + 3) % 5]); galpha(256); }
    }
    grect(0, 170, 240, 70, AC_RGB(14, 18, 28)); ghl(0, 240, 170, AC_RGB(60, 70, 96));
    for (k = 0; k < 9; k++) ghl(k * 28, k * 28 + 20, 180 + (k % 3) * 12, AC_RGB(20, 26, 38));
    grect(128, 40, 90, 130, AC_RGB(2, 2, 6));    /* the doorway: dark behind the door */
    if (t > 4300) { galpha(imin(256, (t - 4300) / 3)); gtext(166, 90 - (t - 4300) / 40, "Z", AC_RGB(160, 210, 255), 1); galpha(256); }
    xs = 172 + 70 * open / 256;                  /* the door: five rings, lit one by one; it rolls aside */
    gdisc(xs, 110, 46, AC_RGB(30, 34, 48)); gcirc(xs, 110, 46, AC_RGB(80, 90, 116));
    for (k = 0; k < 5; k++) {
        int32_t on = t >= 2500 + k * 200;
        galpha(on ? 256 : 70); gcirc(xs, 110, 40 - k * 8, th->caps[k]); gcirc(xs, 110, 39 - k * 8, th->caps[k]); galpha(256);
        if (on && t < 2900 + k * 200) { galpha(120); gdisc(xs, 110, 40 - k * 8, th->caps[k]); galpha(256); }
    }
    garc(xs, 110, 46, 46, 0, 65536, AC_RGB(80, 90, 116), 0);
    grect(120, 30, 10, 140, AC_RGB(40, 46, 62)); grect(218, 30, 10, 140, AC_RGB(40, 46, 62)); grect(118, 26, 112, 10, AC_RGB(40, 46, 62));   /* its frame */
    {   /* the rocks: rolling up, Pink's bump, into the dark */
        int32_t p0 = -40 + 140 * dw_ramp(t, 0, 2000) / 256, p1 = -80 + 150 * dw_ramp(t, 200, 2200) / 256, in = dw_ramp(t, 4500, 5400);
        if (t >= 2100 && t < 2400) p0 += 6 * dw_hop(t, 2100, 300) / 256;
        p0 += 90 * in / 256; p1 += 90 * in / 256;
        dw_rock(p1, 160, 1, 220, -(t < 2200 ? t : 2200) * 40 - (t > 4500 ? (t - 4500) * 40 : 0), 0, tm);
        dw_rock(p0, 160, 0, 220, -(t < 2000 ? t : 2000) * 40 - (t > 4500 ? (t - 4500) * 40 : 0), dw_hop(t, 2100, 260), tm);
    }
}
static void ra_intro(int32_t t, uint32_t tm)
{
    int32_t q, light, sh = 0, smile = 40, eye = 0, nx = 120, ny = 0, note = 0, k, px, tx, hp, ht, look = 0, rumble;
    if (t < 5600) {                              /* the corridor, the door, in */
        ra_corridor(t, tm);
        gzoom(130, 150, 340);
        if (t > 5000) { galpha((t - 5000) * 256 / 600); grect(0, 0, AC_W, AC_H, AC_RGB(0, 0, 0)); galpha(256); }
        dw_skip(tm);
        return;
    }
    q = t - 5600; light = dw_ramp(q, 300, 2600);
    px = -20 + 60 * dw_ramp(q, 1800, 2800) / 256; tx = -44 + 60 * dw_ramp(q, 2000, 3000) / 256;
    rumble = q >= 5200 && q < 6800 ? 256 - iabs(q - 6000) * 256 / 800 : 0;
    if (q >= 2900) {                             /* the snore: a note floats out, up; it smiles */
        int32_t f = dw_ramp(q, 2900, 4400);
        note = 1; nx = 120 + (tsin(tm, 14, 0) >> 11); ny = 168 - 118 * f / 256;
        smile = 40 + 180 * dw_ramp(q, 3200, 4200) / 256;
    }
    if (q >= 4600) {                             /* it sinks: the giant frowns, the hall rumbles, an eye opens a crack */
        int32_t f = dw_ramp(q, 4600, 6400);
        nx = 120 + (tsin(tm, 14, 0) >> 11) - 60 * f / 256; ny = 50 + 100 * f / 256;
        smile = 220 - 480 * dw_ramp(q, 4800, 5600) / 256;
        eye = 110 * dw_ramp(q, 5400, 6000) / 256;
        sh = rumble ? (tsin(tm, 400, 0) >> 13) * rumble / 256 : 0;
    }
    if (q >= 5600) {                             /* Pink dashes under it; it lands on Pink's head */
        px += (RH_PX[2] - 40) * dw_ramp(q, 5600, 6300) / 256;
        if (q >= 6400) { nx = RH_PX[2] + (tsin(tm, 50, 0) >> 12); ny = RH_ON - 20 + (tsin(tm, 70, 0) >> 13); }
    }
    if (q >= 6800) { smile = -260 + 460 * dw_ramp(q, 6800, 7600) / 256; eye = 110 - 110 * dw_ramp(q, 6800, 7400) / 256; }
    if (q >= 7400) { tx += (RH_PX[5] - 16) * dw_ramp(q, 7600, 8600) / 256; look = 1; }   /* Teal: to the other side */
    if (q >= 3000 && q < 4600) look = -1;        /* (looking up at it) */
    hp = q >= 5150 && q < 5450 ? dw_hop(q, 5150, 300) : 0;
    ht = q >= 5250 && q < 5550 ? dw_hop(q, 5250, 300) : 0;
    rh_hall(smile, eye, 0, sh, tm);
    if (light < 256) { galpha(256 - light); grect(0, 0, AC_W, RH_FL, AC_RGB(2, 3, 8)); galpha(256); }
    if (eye < 10 && q < 4600) rh_snore(256, tm);
    rh_floor(sh);
    for (k = 0; k < 8; k++) rh_cushion(RH_PX[k] + sh, 12, DU_COL[k < 4 ? 0 : 1], 0);
    for (k = 0; k < 7 && rumble; k++) {          /* pebbles from the roof */
        int32_t z = (int32_t)((tm / 2u + (uint32_t)k * 143u) % 260u);
        gdisc(20 + k * 33 + (int32_t)(ihash((uint32_t)k) % 20u), z * RH_FL / 260, 1 + k % 2, AC_RGB(90, 100, 120));
    }
    dw_rock(tx + sh, RH_ON, 1, 210, look ? (look < 0 ? -2000 : 3000) : 0, ht, tm);
    dw_rock(px + sh, RH_ON, 0, 210, q >= 5600 && q < 6300 ? -(q - 5600) * 60 : look < 0 ? -2000 : 0, hp, tm);
    if (note) rh_note(nx + sh, ny, tm);
    if (q >= 2900 && q < 3600) { galpha(256 - (q - 2900) * 256 / 700); gcirc(120, 164, 6 + (q - 2900) / 20, AC_RGB(255, 226, 140)); galpha(256); }
    if (q < 900) { galpha(256 - q * 256 / 900); grect(0, 0, AC_W, AC_H, AC_RGB(0, 0, 0)); galpha(256); }
    gzoom(120, 112, 256 + 200 * (256 - dw_ramp(q, 0, 2600)) / 256);   /* (close on its sleeping eye, pulling back) */
    dw_title(t, 14000, DM_DRUM, th->caps[3]);
    if (t < 14000) dw_skip(tm);
}
static void ra_sounds(int32_t t0, int32_t t1)
{
    int k;
    if (dw_at(t0, t1, 2100)) sfx(40, 0, 160, W_TRI, 2600, 1, 0);         /* the bump */
    for (k = 0; k < 5; k++) if (dw_at(t0, t1, 2500 + k * 200)) sfx(67 + k * 3, 0, 300, W_SINE, 1100, 5, 140);   /* ring by ring it lights */
    if (dw_at(t0, t1, 3500)) sfx(33, 0, 800, W_NOISE, 1500, 60, 0);      /* it rolls aside */
    for (k = 0; k < 2; k++) if (dw_at(t0, t1, 5600 + 700 + k * 2600)) sfx(31, 0, 1300, W_SAW, 900, 500, 0);   /* the snore */
    if (dw_at(t0, t1, 5600 + 2900)) sfx(79, 0, 900, W_SINE, 1600, 30, 180);   /* a note floats out */
    if (dw_at(t0, t1, 5600 + 4700)) { sfx(74, 0, 300, W_SINE, 1000, 10, 100); sfx(67, 250, 400, W_SINE, 1000, 10, 100); }   /* and sinks */
    if (dw_at(t0, t1, 5600 + 5200)) sfx(26, 0, 1500, W_NOISE, 2400, 200, 0);   /* the hall rumbles */
    if (dw_at(t0, t1, 5600 + 6400)) sfx(86, 0, 120, W_TRI, 1400, 1, 60);  /* on Pink's head */
    if (dw_at(t0, t1, 14000)) snd_ui(79);
}

/* ---------------------------------------------------------------- POTION CHOIR: the opening */
/* (Ben, 2026-11-24: an opening that ends with how it is played.) Down the well in a bucket, the two rocks peering over
 * its rim, to the deep choir: six rocks on a ledge that have lost their voices (one tries; its note cracks and falls).
 * A drop falls from a crystal in the roof and is a potion over Teal, who cannot tell who needs it. Pink can: a gleam
 * on one of them, in Pink's eyes alone. Pink calls out what it looks like, a bit at a time; Teal rolls there and pours,
 * and it sings. Then the lesson, a step at a time (ac_potlesson.h). The choir is the lesson's own (PL_SEED). */
#define PO_FL 204                                /* the cavern's floor, the choir's ledge */
#define PO_LG 150
#define PO_NEED 5                                /* the one who needs it, along the ledge */
#define PO_B 2800                                /* the cavern, after the well */
#define PL_SEED 77u
static int32_t po_cx(int k) { return 25 + k * 38; }
/* the bucket, its bottom's middle at (x, y), on its rope from the top; with the two rocks in it, peering over the rim */
static void po_bucket(int32_t x, int32_t y, int full, int32_t look, uint32_t tm)
{
    int32_t xy[8];
    gline(x, 0, x, y - 34, AC_RGB(150, 120, 80)); gline(x + 1, 0, x + 1, y - 34, AC_RGB(110, 86, 56));
    garc(x, y - 18, 19, 16, 32768, 65536, th->mute, 0);
    if (full) {
        dw_rock(x - 8, y - 22 + (tsin(tm, 50, 0) >> 13), 0, 170, look * 2500, 0, tm);
        dw_rock(x + 9, y - 21 + (tsin(tm, 50, 9000) >> 13), 1, 170, look * 2500, 0, tm);
    }
    xy[0] = x - 19; xy[1] = y - 18; xy[2] = x + 19; xy[3] = y - 18; xy[4] = x + 14; xy[5] = y + 6; xy[6] = x - 14; xy[7] = y + 6;
    gpoly(xy, 4, AC_RGB(92, 62, 40)); gpolyline(xy, 4, 1, AC_RGB(150, 110, 70));
    ghl(x - 18, x + 19, y - 11, AC_RGB(60, 40, 26)); ghl(x - 16, x + 17, y - 1, AC_RGB(60, 40, 26));
}
/* the well: its bricks going by, d px down, the glow of the deep coming up under it */
static void po_well(int32_t t, uint32_t tm)
{
    int32_t d = 560 * dw_ramp(t, 0, 2700) / 256, y, x, k, deep = iclamp(d * 256 / 560, 0, 256);
    gclear(dw_mix(AC_RGB(6, 8, 16), AC_RGB(2, 3, 8), deep));
    for (k = 0; k < 6; k++) {                    /* the deep's colours, rising up the shaft */
        int32_t r = 40 + (int32_t)((tm / 40u + (uint32_t)k * 46u) % 276u);
        galpha(imax(0, 70 - r / 5) * deep / 256);
        garc(120, 268, r, r * 3 / 4, 32768, 65536, th->caps[k % 5], 0);
    }
    galpha(256);
    if (d < 120) { galpha(200 - d * 200 / 120); gellf(120, -10 - d, 80, 30, AC_RGB(255, 236, 190), 0); galpha(256); }   /* the Arcave's light, up and away */
    for (y = -12 - ((d % 12) + 12) % 12; y < 240; y += 12) {   /* the bricks: two walls, every other row half a brick along */
        int32_t row = (y + d) / 12, off = (row & 1) ? 8 : 0;
        col_t c = dw_mix(AC_RGB(58, 64, 84), AC_RGB(22, 26, 38), iclamp((y + d) * 256 / 800, 0, 256));
        for (x = -16 + off; x < 240; x += 16) {
            uint32_t h = ihash((uint32_t)(row * 31 + x));
            if (x > 36 && x + 15 < 204) continue;
            grect(x, y, 15, 11, dw_mix(c, AC_RGB(10, 12, 20), (int32_t)(h % 70u)));
            if (h % 13u == 0u) { galpha(150 + ipulse(tm + h % 900u, 1400) * 100 / 256); gp(x + 4 + (int32_t)(h >> 8) % 7, y + 4, th->caps[(h >> 12) % 5u]); galpha(256); }
            if (h % 7u == 0u && (x == 36 - (36 - off) % 16 || x + 15 >= 204)) ghl(x, x + 15, y, th->moss);
        }
    }
    galpha(150); gline(52, 0, 52, 240, AC_RGB(10, 12, 20)); gline(188, 0, 188, 240, AC_RGB(10, 12, 20)); galpha(256);
    po_bucket(120 + (tsin(tm, 36, 0) >> 12), 132, 1, t > 900 && t < 1900 ? 1 : 0, tm);
}
/* a sparkle, a four-pointed star of size s */
static void po_star(int32_t x, int32_t y, int32_t s, col_t c)
{
    gline(x - s, y, x + s, y, c); gline(x, y - s, x, y + s, c); gdisc(x, y, imax(0, s / 3), c);
}
static void po_intro(int32_t t, uint32_t tm)
{
    int32_t q = t - PO_B, k, px, tx, bx, by, pang = 0, tang = 0, phop = 0, thop = 0, sing = q >= 7400;
    int need;
    __typeof__(du) keep = du;
    if (t < PO_B) {                              /* down the well */
        po_well(t, tm);
        if (t > 2300) { galpha((t - 2300) * 256 / 500); grect(0, 0, AC_W, AC_H, AC_RGB(0, 0, 0)); galpha(256); }
        dw_skip(tm);
        return;
    }
    du_pot_round(PL_SEED, 0);
    need = PO_NEED;
    du_bg(tm);
    {   /* the roof: rock, stalactites, the crystal the drop falls from (over where Teal will stand) */
        int32_t xy[6];
        grect(0, 0, AC_W, 14, AC_RGB(12, 16, 26));
        for (k = 0; k < 9; k++) {
            int32_t x = 8 + k * 29, h = 12 + (k * 37) % 22;
            xy[0] = x - 10; xy[1] = 13; xy[2] = x + 10; xy[3] = 13; xy[4] = x + 1; xy[5] = 13 + h;
            gpoly(xy, 3, AC_RGB(12, 16, 26));
        }
        xy[0] = 155; xy[1] = 12; xy[2] = 165; xy[3] = 12; xy[4] = 160; xy[5] = 34;
        gpoly(xy, 3, th->caps[1]); galpha(60 + ipulse(tm, 1800) * 100 / 256); gdisc(160, 24, 9, th->caps[1]); galpha(256);
    }
    {   /* the ledge the choir stands on, the floor in front */
        int32_t xy[8];
        xy[0] = 0; xy[1] = PO_LG; xy[2] = 240; xy[3] = PO_LG; xy[4] = 240; xy[5] = PO_LG + 12; xy[6] = 0; xy[7] = PO_LG + 14;
        gpoly(xy, 4, AC_RGB(20, 28, 40)); ghl(0, 240, PO_LG, th->moss);
        grect(0, PO_FL, AC_W, AC_H - PO_FL, AC_RGB(16, 22, 32)); ghl(0, 240, PO_FL, th->moss);
        for (k = 0; k < 30; k++) { int32_t x = (int32_t)(ihash((uint32_t)k * 5u) % 240u); gline(x, PO_FL, x - 1, PO_FL - 2 - k % 3, th->moss); }
    }
    for (k = 0; k < 6; k++) {                    /* the deep choir, silent, their colours gone; the one who needs it in its
                                                    colours in Pink's gleam, and at the end, singing */
        int32_t x = po_cx(k), y = PO_LG - 16, up = 0;
        if (k == need && sing) {
            int32_t f = dw_ramp(q, 7400, 7900);
            up = (isin(((q - 7400) % 600) * 32768 / 600) * 3) >> 14;
            galpha(50 * f / 256 + ipulse(tm, 900) * 30 / 256); gdisc(x, y, 28, th->caps[du_ch[k].col % 5u]); galpha(256);
            du_char(x, y - up, &du_ch[k]);
            continue;
        }
        du_char_m(x, y, &du_ch[k], k == need && q >= 3900 ? DC_ALL : DC_ALL & ~DC_COL);
        galpha(60 + ipulse(tm + (uint32_t)k * 300u, 2200) * 60 / 256); gtext(x + 6, y - 32, "...", th->mute, 1);
        galpha(256);
    }
    if (q >= 1600 && q < 2900) {                 /* one tries to sing: the note cracks and falls */
        int32_t e = q - 1600, x = po_cx(2) + 10, y = PO_LG - 50 + (e > 500 ? (e - 500) * (e - 500) / 2500 : -e / 60);
        galpha(e > 1000 ? (1300 - e) * 256 / 300 : 256);
        gtext(x, y, "\xE2\x99\xAA", th->mute, 2);
        if (e > 450) { gline(x + 2, y + 2, x + 7, y + 9, th->bg); gline(x + 3, y + 2, x + 8, y + 9, th->bg); }
        galpha(256);
    }
    if (sing) for (k = 0; k < 6; k++) {          /* its song */
        int32_t z = (int32_t)((tm / 3u + (uint32_t)k * 170u) % 1000u), f = dw_ramp(q, 7400, 7800);
        galpha(f * (256 - z * 256 / 1000) / 256);
        gtext(po_cx(need) - 18 + (int32_t)(ihash((uint32_t)k * 5u) % 30u), PO_LG - 40 - z * 90 / 1000, "\xE2\x99\xAA", th->caps[k % 5], 1);
    }
    galpha(256);
    /* the bucket: down, the two hop out; the rocks: Pink stays, Teal goes to the one Pink calls */
    by = PO_FL - 6 - (256 - dw_ramp(q, 0, 700)) * 260 / 256 - 300 * dw_ramp(q, 1500, 3200) / 256;   /* (and up again, empty) */
    bx = 120;
    po_bucket(bx, by, q < 800, 0, tm);
    px = 80; tx = 160;
    {
        int32_t f = dw_ramp(q, 800, 1300), g = dw_ramp(q, 860, 1360);
        if (q < 1360) {
            px = bx - 8 + (80 - bx + 8) * f / 256; tx = bx + 9 + (160 - bx - 9) * g / 256;
        }
        if (q >= 800) {
            int32_t py = PO_FL - 13 - (isin(f * 128) * 30 >> 14) - (PO_FL - 11 - (by - 22)) * (256 - f) / 256;
            int32_t ty = PO_FL - 13 - (isin(g * 128) * 30 >> 14) - (PO_FL - 11 - (by - 21)) * (256 - g) / 256;
            if (q >= 6000) tx += (po_cx(need) - 6 - 160) * dw_ramp(q, 6000, 6700) / 256;
            tang = q >= 6000 && q < 6700 ? -(q - 6000) * 40 : q >= 3400 && q < 5600 ? (((q - 3400) / 500) & 1 ? 3000 : -3000) : 0;
            pang = q >= 3900 && q < 6400 ? -2500 : 0;
            phop = dw_hop(q, 3900, 260) + dw_hop(q, 7800, 300) + dw_hop(q, 8400, 300);
            thop = dw_hop(q, 5800, 260) + dw_hop(q, 6800, 300) + dw_hop(q, 8100, 300) + dw_hop(q, 8700, 300);
            dw_rock(tx, ty, 1, 250, tang, thop, tm);
            dw_rock(px, py, 0, 250, pang, phop, tm);
        }
    }
    if (q >= 2700 && q < 3200) {                 /* the drop, from the crystal to Teal */
        int32_t e = q - 2700, y = 34 + e * e * (PO_FL - 40 - 34) / 250000;
        gdisc(160, y, 2, DU_COL[1]); gcirc(160, y, 3, th->text);
    }
    if (q >= 3200 && q < 7400) {                 /* the potion, over Teal; at the one that needs it, tipped out */
        int32_t bxx = tx, byy = PO_FL - 36 + (tsin(tm, 60, 0) >> 12) - (thop ? (isin(thop * 128) * 12 >> 14) : 0), a = 256;
        if (q < 3500) { galpha((q - 3200) * 256 / 300); gcirc(160, PO_FL - 36, 4 + (q - 3200) / 20, th->text); galpha(256); }
        if (q >= 6800) { int32_t f = dw_ramp(q, 6800, 7050); bxx = tx + (po_cx(need) - tx) * f / 256; byy = byy + (PO_LG - 62 - byy) * f / 256; }
        if (q >= 7200) a = 256 - (q - 7200) * 256 / 200;
        galpha(a / 4); gdisc(bxx, byy - 4, 11, DU_COL[1]); galpha(a); du_bottle(bxx, byy, DU_COL[1]); galpha(256);
        if (q >= 7000) for (k = 0; k < 4; k++) {   /* poured: drops down onto its head */
            int32_t e = q - 7000 - k * 70, y = byy + 6 + e * e / 900;
            if (e > 0 && y < PO_LG - 30) gdisc(po_cx(need) + (k & 1) * 2 - 1, y, 1, DU_COL[1]);
        }
    }
    if (q >= 3400 && q < 5800) {                 /* Teal cannot tell who needs it */
        galpha(150 + ipulse(tm, 800) * 106 / 256); gtext(tx + 12, PO_FL - 40, "?", th->text, 2); galpha(256);
    }
    if (q >= 5800 && q < 6400) { gtext(tx + 12, PO_FL - 42, "!", th->text, 2); }
    if (q >= 3900 && q < 7000) {                 /* Pink can: a gleam on it, in Pink's eyes alone */
        int32_t a = imin(256, (q - 3900) * 256 / 400) * (q > 6600 ? (7000 - q) : 400) / 400, xy[6], x = po_cx(need), y = PO_LG - 16;
        xy[0] = px + 4; xy[1] = PO_FL - 16; xy[2] = x - 18; xy[3] = y - 24; xy[4] = x + 4; xy[5] = y + 22;
        galpha(a * 50 / 256); gpoly(xy, 3, DU_COL[0]);
        galpha(a); gbox(x - 20, y - 24, 40, 44, DU_COL[0]);
        for (k = 0; k < 3; k++) {
            int32_t ph = ipulse(tm + (uint32_t)k * 333u, 1000);
            po_star(x - 14 + k * 14, y - 28 + (k & 1) * 6, 1 + ph * 3 / 256, th->text);
        }
        galpha(256);
        if (q < 4300) gtext(px + 10, PO_FL - 42, "!", DU_COL[0], 2);
    }
    if (q >= 4600 && q < 6600) {                 /* Pink calls out what it looks like, a bit at a time */
        static const uint8_t ORD[5] = {DC_SHAPE, DC_EYES, DC_COL, DC_TOP, DC_MOUTH};
        int32_t a = imin(256, (q - 4600) * 256 / 200) * (q > 6400 ? (6600 - q) : 200) / 200, show = 0, xy[6];
        for (k = 0; k < 5; k++) if (q >= 4800 + k * 220) show |= ORD[k];
        galpha(a);
        xy[0] = 96; xy[1] = 86; xy[2] = 110; xy[3] = 86; xy[4] = px + 2; xy[5] = 104; gpoly(xy, 3, th->text);
        gellf(104, 56, 34, 32, th->text, 0); gellf(104, 56, 32, 30, th->bg, 0);
        du_char_m(104, 62, &du_ch[need], show);
        galpha(256);
    }
    du = keep;
    if (q < 500) { galpha(256 - q * 256 / 500); grect(0, 0, AC_W, AC_H, AC_RGB(0, 0, 0)); galpha(256); }
    dw_title(t, PO_B + 8900, DM_PIPE, th->caps[2]);
    if (t < PO_B + 8900) dw_skip(tm);
}
static void po_sounds(int32_t t0, int32_t t1)
{
    int k;
    for (k = 0; k < 3; k++) if (dw_at(t0, t1, 200 + k * 800)) sfx(43 + k, 0, 260, W_SAW, 500, 60, 0);   /* the rope creaks */
    if (dw_at(t0, t1, PO_B + 650)) sfx(36, 0, 180, W_TRI, 2600, 1, 0);   /* down */
    if (dw_at(t0, t1, PO_B + 800)) sfx(72, 0, 90, W_SQUARE, 500, 1, 0);
    if (dw_at(t0, t1, PO_B + 860)) sfx(76, 0, 90, W_SQUARE, 500, 1, 0);
    if (dw_at(t0, t1, PO_B + 1600)) sfx(72, 0, 420, W_SINE, 1400, 10, 120);   /* one tries; it cracks */
    if (dw_at(t0, t1, PO_B + 2050)) { sfx(73, 0, 300, W_SAW, 2000, 2, 20); sfx(66, 0, 300, W_SQUARE, 1200, 2, 20); }
    if (dw_at(t0, t1, PO_B + 2700)) sfx(96, 0, 220, W_SINE, 1300, 1, 160);   /* a drop */
    if (dw_at(t0, t1, PO_B + 3200)) { sfx(91, 0, 150, W_SINE, 1300, 1, 160); sfx(98, 90, 220, W_SINE, 1100, 1, 160); }
    if (dw_at(t0, t1, PO_B + 3400)) { sfx(67, 0, 120, W_TRI, 1300, 5, 0); sfx(71, 140, 180, W_TRI, 1300, 5, 0); }   /* ? */
    if (dw_at(t0, t1, PO_B + 3900)) sfx(84, 0, 100, W_SQUARE, 600, 1, 0);   /* ! */
    for (k = 0; k < 5; k++) if (dw_at(t0, t1, PO_B + 4800 + k * 220)) sfx(76 + k * 2, 0, 120, W_TRI, 1500, 3, 60);   /* Pink calls */
    if (dw_at(t0, t1, PO_B + 5800)) sfx(86, 0, 100, W_SQUARE, 600, 1, 0);
    if (dw_at(t0, t1, PO_B + 6000)) sfx(40, 0, 600, W_NOISE, 700, 100, 0);   /* Teal rolls there */
    if (dw_at(t0, t1, PO_B + 6800)) sfx(76, 0, 90, W_SQUARE, 500, 1, 0);
    if (dw_at(t0, t1, PO_B + 7000)) sfx(62, 0, 400, W_NOISE, 900, 20, 40);   /* poured */
    if (dw_at(t0, t1, PO_B + 7400)) {            /* it sings */
        sfx(72, 0, 300, W_SINE, 1600, 10, 180); sfx(76, 250, 300, W_SINE, 1600, 10, 180);
        sfx(79, 500, 300, W_SINE, 1600, 10, 180); sfx(84, 750, 700, W_SINE, 1600, 10, 180);
    }
    if (dw_at(t0, t1, PO_B + 8900)) snd_ui(79);
}
