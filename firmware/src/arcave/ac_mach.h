/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's three machines. All three run on the jukebox's clock, so what you do in them lands in the music.
 *   MYSTIC RUN        Rock runs on the beat; a key hops a crystal, the top row hops higher. KNOB 1 is the tempo.
 *   JUICE WORKS       four taps of mushroom juice (KNOB 1-4), glasses put under them, moved and served with the
 *                     keys, tickets that say what to pour: many things to keep going at once.
 *   NIGHT BLOOM       new here: four knobs turn four petals round a flower, one to a ring, a click a twenty-fourth
 *                     of a turn, as the knob itself. Spores drift in; catch each on the petal of its own colour. */

enum { MS_TITLE, MS_PLAY, MS_OVER };
static struct { uint8_t which, state; int32_t t, score, lives, combo, msg_t, flash; uint8_t msg; } mc;
static uint32_t best[3];
static uint8_t save_dirty;

static void mach_msg(int id) { mc.msg = (uint8_t)id; mc.msg_t = 700; }
static void num_str(char *b, int32_t v)
{
    char t[12]; int32_t n = 0, k = 0;
    if (v <= 0) t[n++] = '0';
    while (v > 0 && n < 10) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[k++] = t[--n];
    b[k] = 0;
}
static void mach_hud(void)
{
    char b[16]; int32_t i;
    num_str(b, mc.score);
    gtextr(234, 8, b, th->text, 2);
    for (i = 0; i < imin(mc.lives, 8); i++) gtext(8 + i * 9, 8, "\xE2\x99\xA5", th->caps[0], 1);
    if (mc.msg_t > 0) { galpha(imin(256, mc.msg_t * 256 / 300)); gfit(120, 40 - (700 - mc.msg_t) / 60, 232, TR(mc.msg), mc.msg == T_MISS ? th->caps[0] : th->pad, ts(2, 4)); galpha(256); }
}
/* the start (and the end) of a machine: its name; PLAY starts; HOME shows how it is played; REC goes back */
static void mach_card(const char *name, col_t c)
{
    char b[40]; const char *p;
    int32_t y, a = ts(1, 3), used, n = 0;
    gveil(th->bg, 24);
    used = gfit(120, 14 + 2 * ts(3, 4), 232, name, c, tsize ? 3 : 2);
    y = 14 + 2 * ts(3, 4) + 5 * used + 10;
    if (mc.state == MS_OVER) {                   /* how it went */
        y += gblock(4, y, 232, TR(T_GAMEOVER), th->caps[0], ts(2, 3), 1);
        num_str(b, mc.score); gtextc(120, y + 4, b, th->text, 4); y += 30;
    }
    while (a > 1 && y + gblock_h(232, TR(mc.state == MS_OVER ? T_PLAY_AGAIN : T_PLAY_START), a) + gblock_h(232, TR(T_HOME_CONTROLS), a)
           + (best[mc.which] ? TLH(a) : 0) + 12 > 238 - gblock_h(232, TR(T_REC_BACK), ts(1, 2))) a--;
    galpha(140 + ipulse((uint32_t)mc.t, 1600) * 116 / 256);
    y += gblock(4, y, 232, TR(mc.state == MS_OVER ? T_PLAY_AGAIN : T_PLAY_START), th->pad, a, 1) + 6;
    galpha(256);
    if (best[mc.which]) {
        for (p = TR(T_BEST); *p && n < 24; ) b[n++] = *p++;
        b[n++] = ' '; num_str(b + n, (int32_t)best[mc.which]);
        gfit(120, y + 2 * a, 232, b, th->mute, a);
    }
    y = gfoot(238, TR(T_REC_BACK), th->mute, th->bg, ts(1, 2));   /* at the foot: HOME, and under it REC */
    gfoot(y - 2, TR(T_HOME_CONTROLS), th->text, th->bg, a);
}
static void mach_over(void)
{
    mc.state = MS_OVER; mc.t = 0;
    if ((uint32_t)mc.score > best[mc.which]) { best[mc.which] = (uint32_t)mc.score; save_dirty = 1; }
    sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150);
}

/* ---------------------------------------------------------------- MYSTIC RUN */
static struct { uint32_t t0, done_h, blk_h; int32_t hop_t, hop_h, hop_k; uint32_t seed; uint8_t blk_done; } mr;
/* the buttons a block may ask for (not the ones that mean something everywhere: FX, EDIT, GLO, HOME, PLAY, REC) */
static const uint8_t MR_BTN[6] = {BTN_SEL, BTN_ENV, BTN_LFO, BTN_SAVE, BTN_ARP, BTN_SEQ};
static const char *const MR_BTN_NAME[6] = {"SCL", "ENV", "LFO", "SAVE", "ARP", "SEQ"};
static int mr_blk_btn(uint32_t h, int which)     /* a block's button (its second, for a double) */
{
    int b1 = (int)(ihash(h * 17u + mr.seed) % 6u);
    return which ? (b1 + 1 + (int)(ihash(h * 19u + mr.seed) % 5u)) % 6 : b1;
}
#define MR_HOP 62                                /* ticks in the air */
static int mr_obs(uint32_t h)                    /* what stands at half-beat h: 0 nothing, 1 a crystal, 2 a tall one, 3 a spore,
                                                    4 a block (its button makes it go), 5 a double block (two buttons) */
{
    uint32_t r;
    if (h < 8u) return 0;
    if (h >= 24u && !(h & 7u)) {                 /* every fourth beat may be a block: never two on the screen at once */
        uint32_t rb = ihash(h * 13u + mr.seed);
        if (rb < (h < 72u ? 24000u : 36000u)) return (h >= 88u && !(rb & 3u)) ? 5 : 4;
    }
    r = ihash(h * 7u + mr.seed);
    if (!(h & 1u)) {
        uint32_t dens = 26000u + imin(26000, (int32_t)h * 120);
        if (r < dens / 4u && h > 24u) return 2;
        return r < dens ? 1 : 0;
    }
    return (r < 16000u && mr_obs(h + 1u) == 0) ? 3 : 0;
}
static int32_t mr_height(uint32_t now)           /* how high Rock is, px */
{
    int32_t u = (int32_t)(now - (uint32_t)mr.hop_t);
    if (mr.hop_h == 0 || u < 0 || u >= MR_HOP) return 0;
    return mr.hop_h * 4 * u * (MR_HOP - u) / (MR_HOP * MR_HOP);
}
static void run_tick(void)
{
    uint32_t now = (tr_now_q8() >> 8) - mr.t0, h;
    if (in.enc[ENC_KNOB1] && !tr_follow) tr_bpm_q8 = (uint32_t)iclamp((int32_t)(tr_bpm_q8 >> 8) + in.enc[ENC_KNOB1] * 2, 70, 180) << 8;
    if (in.hikey >= 0 && mr_height(now) < 4) {
        mr.hop_t = (int32_t)now; mr.hop_k = in.hikey; mr.hop_h = 24 + in.hop * 40 / 256;
        sfx(key_note(in.hikey) + 12, 0, 300, W_TRI, 3000, 3, 120);
    }
    for (h = mr.done_h + 1u; h * 48u <= now + 281u; h++) {   /* a block coming into view: its button lights, and pressed, counts */
        int o = mr_obs(h), need, k;
        if (o < 4) continue;
        if (mr.blk_h != h) { mr.blk_h = h; mr.blk_done = 0; }
        need = o == 5 ? 3 : 1;
        for (k = 0; k < 2; k++) {
            int b = MR_BTN[mr_blk_btn(h, k)];
            if (!(need >> k & 1) || (mr.blk_done >> k & 1)) continue;
            led_btn_want |= 1u << b;
            if (BDOWN(b)) {
                mr.blk_done |= (uint8_t)(1u << k); mc.score += 15;
                sfx(67 + 5 * k, 0, 300, W_SQUARE, 1800, 2, 140); sfx(96, 0, 120, W_NOISE, 1200, 1, 100);
                if (mr.blk_done == need) { mc.combo++; mach_msg(T_GOOD); snd_chime(0); }
            }
        }
        break;
    }
    for (h = mr.done_h + 1u; h * 48u <= now; h++) {
        int o = mr_obs(h), ht = mr_height(h * 48u);
        mr.done_h = h;
        if (o >= 4) {                            /* a block reaches Rock: gone, or it hurts */
            if (!(mr.blk_h == h && mr.blk_done == (o == 5 ? 3 : 1))) {
                mc.combo = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
                sfx_ex(41, 0, 400, W_SAW, 4000, 2, 100, 0, 0, 65000);
                if (mc.lives <= 0) { mach_over(); return; }
            }
            continue;
        }
        if (o == 1 || o == 2) {
            if (ht > (o == 1 ? 11 : 30)) {
                int32_t off = iabs((int32_t)(h * 48u) - (mr.hop_t + MR_HOP / 2));
                mc.combo++; mc.score += (off < 9 ? 20 : 10) + imin(mc.combo, 20);
                mach_msg(off < 9 ? T_PERFECT : T_GOOD);
                sfx(72 + SC_PEN[mc.combo % 5], 0, 400, W_SINE, 2600, 3, 150);
            } else {
                mc.combo = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
                sfx_ex(45, 0, 300, W_SAW, 3600, 2, 100, 0, 0, 65100);
                if (mc.lives <= 0) { mach_over(); return; }
            }
        } else if (o == 3 && ht > 16) { mc.score += 5; sfx(84, 0, 300, W_SINE, 2400, 3, 180); }
    }
    {   /* any key for a crystal, the top row for a tall one: the lights say which is next */
        uint32_t nh = (now / 96u + 1u) * 2u;
        int o = mr_obs(nh);
        if (o == 1) led_key_dim_want = (1u << AC_NKEY) - 1u;
        if (o == 2) { int k2; for (k2 = 0; k2 < AC_NKEY; k2++) if (KEY_IS_SHARP[k2]) led_key_want |= 1u << k2; }
    }
    if (now % 96u < 12u) led_btn_want |= 1u << BTN_PLAY;   /* (PLAY's green light, on the beat) */
}
static void run_draw(void)
{
    uint32_t now = (tr_now_q8() >> 8) - mr.t0, h, tm = (uint32_t)mc.t;
    int32_t gy = 178, rx = 64, i, sx, beat = 256 - imin(256, (int32_t)(now % 96u) * 256 / 40);
#define MRX(tk) (rx + ((int32_t)(tk) - (int32_t)now) * 60 / 96)
    sky_draw();
    far_layer((int32_t)now / 6, 0, 256, 120, 0, th->far1, th->fill);
    far_layer((int32_t)now / 3, 0, 256, 142, 1, th->far2, th->fill);
    for (i = 0; i < 6; i++) {                    /* mushrooms that swell on the beat */
        int32_t wx = ((int32_t)(now * 60u / 96u) / 80 + i) * 80, x = wx - (int32_t)(now * 60u / 96u), s = 90 + (int32_t)(ihash((uint32_t)(wx / 80)) >> 10);
        mush_draw(x, gy, s + beat * 16 / 256, th->caps[ihash((uint32_t)(wx / 80 + 3)) * 5u >> 16], th->stem, beat);
    }
    grect(0, gy, 240, 62, th->ground);
    for (i = 0; i < 4; i++) {
        static const uint8_t BD[4] = {4, 9, 15, 23};
        int32_t on = 8 + i * 5, per = on + 5 + i * 4, sh = (int32_t)(now * 60u / 96u);
        galpha(200 - i * 44);
        for (sx = 0; sx < 240; sx++) if ((sx + sh) % per < on) gp(sx, gy + BD[i], th->bands[i]);
    }
    galpha(140 + beat * 116 / 256); ghl(0, 240, gy, th->rim); galpha(256);
    for (h = now / 48u; h < now / 48u + 8u; h++) {
        int o = mr_obs(h), x = MRX(h * 48u);
        if (h <= mr.done_h || !o) continue;
        if (o == 3) { galpha(70); gdisc(x, gy - 34, 5, th->caps[h % 5u]); galpha(256); grect(x - 1, gy - 35, 2, 2, th->caps[h % 5u]); }
        else if (o >= 4) {                       /* a block (or two, stacked), each with its button's name on it */
            int32_t k, need = o == 5 ? 3 : 1, done = mr.blk_h == h ? mr.blk_done : 0, row = 0;
            for (k = 0; k < 2; k++) {
                const char *nm = MR_BTN_NAME[mr_blk_btn(h, k)];
                int32_t by;
                if (!(need >> k & 1) || (done >> k & 1)) continue;
                by = gy - 30 - row * 30; row++;
                grect(x - 26, by, 52, 30, th->panel); gbox(x - 26, by, 52, 30, th->pad); gbox(x - 24, by + 2, 48, 26, th->caps[(h + (uint32_t)k) % 5u]);
                gfit(x, by + 10, 46, nm, th->text, 2);
            }
        }
        else {                                   /* crystals: plain to see at first, then less and less so */
            int32_t hh = o == 1 ? 13 : 30, xy[6], k, vis = iclamp(256 - ((int32_t)h - 8) * 2, 84, 256);
            if (vis > 100) { galpha((vis - 84) * 150 / 172); gdisc(x, gy - hh / 2, hh / 2 + 9, th->caps[h % 5u]); }
            for (k = 0; k < (o == 1 ? 2 : 3); k++) {
                xy[0] = x - 5 + k * 5 - 4; xy[1] = gy; xy[2] = x - 5 + k * 5 + (k - 1); xy[3] = gy - hh + iabs(k - 1) * 5; xy[4] = x - 5 + k * 5 + 4; xy[5] = gy;
                galpha(256); gpoly(xy, 3, th->dark ? 0 : th->bands[k]);
                if (vis > 128) { galpha((vis - 128) * 2); gpoly(xy, 3, th->caps[(h + (uint32_t)k) % 5u]); }
                galpha(vis); gpolyline(xy, 3, 1, vis > 190 ? th->pad : th->caps[(h + (uint32_t)k) % 5u]);
                galpha(256);
            }
        }
    }
    rock_draw(rx, gy - 8 - mr_height(now), 256, (int32_t)(now * 400u), (tm % 3100u) < 150u ? 40 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    if (mc.flash > 0) { galpha(mc.flash * 120 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
    {
        char b[16]; int32_t n, a = ts(1, 2);
        num_str(b, (int32_t)(tr_bpm_q8 >> 8));
        for (n = 0; b[n]; n++) ;
        b[n++] = ' '; b[n++] = 'B'; b[n++] = 'P'; b[n++] = 'M'; b[n] = 0;
        gtext(8, 236 - 5 * a, b, th->mute, a);
    }
#undef MRX
}

/* ---------------------------------------------------------------- JUICE WORKS */
/* Four taps of mushroom juice over a counter with four bays. KNOB 1-4 open and close the taps (three clicks:
 * a trickle, a pour, a gush). The keys under a tap put a glass in its bay; pressed again (as they are let go) the
 * glass is served; held while a key under another tap is pressed, the glass moves there. Tickets hang between the
 * taps: each shows the drink to make, layer by layer, a mark on the glass to a part. Juice on the floor, a glass
 * run over, a wrong drink or a ticket left too long costs a drop of the five. */
#define JW_NTK 3
#define JW_UPX 6                                 /* px to a part, on a glass and on a ticket alike */
#define JW_CAP 6                                 /* parts a glass holds */
#define JW_FLOOR 204
#define JW_X(k) (30 + (k) * 60)
static const col_t JW_COL[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
#define JW_TOL 184                               /* how far off a mark still serves (Q8 parts): nearly three quarters of one */
static const uint16_t JW_RATE[4] = {0, 150, 330, 700};   /* parts (Q8) a second, by how far the tap is open */
typedef struct { uint8_t n, c[3]; int32_t v[3]; } jw_glass_t;   /* layers from the bottom: colour, parts (Q8) */
static struct {
    uint8_t valve[4], has[4], used[4];
    jw_glass_t g[4], out;
    int32_t puddle[4], splash[4], out_t, out_x, out_ok;
    struct { uint8_t on, n, c[3], u[3]; int32_t t, life; } tk[JW_NTK];
    int32_t spawn_t, served;
    uint32_t prev_keys, seed, nseed;
} jw;
static int jw_zone(int key) { return key * 4 / AC_NKEY; }
static int32_t jw_total(const jw_glass_t *g) { return (g->n > 0 ? g->v[0] : 0) + (g->n > 1 ? g->v[1] : 0) + (g->n > 2 ? g->v[2] : 0); }
static void jw_hurt(int k)                       /* a drop lost; k: the bay it happened in, or -1 */
{
    mc.combo = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
    if (k >= 0) jw.splash[k] = 500;
    sfx_ex(43, 0, 400, W_SAW, 3400, 2, 120, 0, 0, 65150);
    if (mc.lives <= 0) mach_over();
}
static void jw_ticket(void)
{
    uint32_t i, r, k;
    for (i = 0; i < JW_NTK; i++) if (!jw.tk[i].on) break;
    if (i == JW_NTK) return;
    r = ihash(jw.seed + jw.nseed * 7u); jw.nseed++;
    jw.tk[i].n = (uint8_t)(jw.served >= 12 && (r & 7u) < 2u ? 3 : jw.served >= 3 && (r & 7u) < 5u ? 2 : 1);
    for (k = 0; k < jw.tk[i].n; k++) {
        uint32_t c = ihash(r + k * 31u) % 4u, u;
        if (k && c == jw.tk[i].c[k - 1]) c = (c + 1u + ihash(r + k * 5u) % 3u) % 4u;   /* a layer is never its neighbour's colour */
        u = 1u + ihash(r + k * 13u + 3u) % (jw.tk[i].n == 1 ? 4u : jw.tk[i].n == 2 ? 3u : 2u);
        jw.tk[i].c[k] = (uint8_t)c; jw.tk[i].u[k] = (uint8_t)u;
    }
    jw.tk[i].life = imax(20000, 42000 - jw.served * 1100) + (jw.tk[i].n - 1) * 9000;
    jw.tk[i].t = jw.tk[i].life; jw.tk[i].on = 1;
    sfx(79, 0, 120, W_SQUARE, 1400, 2, 120); sfx(86, 90, 200, W_SQUARE, 1400, 2, 120);   /* a bell: an order */
}
static void jw_serve(int z)
{
    jw_glass_t *g = &jw.g[z];
    int32_t i, k, best_i = -1, best_e = 0x7FFFFFFF;
    jw.has[z] = 0; jw.out = *g; jw.out_t = 500; jw.out_x = JW_X(z); jw.out_ok = 0;
    if (!g->n) { snd_ui(60); return; }           /* an empty glass back on the shelf: nothing lost */
    for (i = 0; i < JW_NTK; i++) {
        int32_t e = 0;
        if (!jw.tk[i].on || jw.tk[i].n != g->n) continue;
        for (k = 0; k < g->n; k++) {
            if (jw.tk[i].c[k] != g->c[k]) break;
            e = imax(e, iabs(g->v[k] - Q8(jw.tk[i].u[k])));
        }
        if (k == g->n && e <= JW_TOL && e < best_e) { best_e = e; best_i = i; }
    }
    if (best_i < 0) { jw_hurt(z); return; }      /* nobody asked for that */
    mc.combo++; jw.served++; jw.out_ok = 1;
    mc.score += 10 * g->n + (best_e < 64 ? 10 : 0) + jw.tk[best_i].t * 10 / jw.tk[best_i].life + imin(mc.combo, 10);
    mach_msg(best_e < 64 ? T_PERFECT : T_GOOD);
    jw.tk[best_i].on = 0;
    for (k = 0; k < g->n; k++) sfx(72 + SC_PEN[(g->c[k] + k) % 5], (uint32_t)k * 90u, 500, W_SINE, 2600, 3, 170);
    snd_chime(best_e < 64);
}
static void juice_tick(int32_t dt)
{
    uint32_t down = in.kdown, up = jw.prev_keys & ~in.keys, zheld = 0;
    int32_t k, i;
    for (k = 0; k < 4; k++) {                    /* the taps */
        int32_t d = in.enc[ENC_KNOB1 + k];
        if (d) {
            int v = iclamp(jw.valve[k] + d, 0, 3);
            if (v != jw.valve[k]) { jw.valve[k] = (uint8_t)v; sfx(48 + v * 5 + k * 2, 0, 90, W_SQUARE, 1300, 1, 60); }
        }
    }
    for (k = 0; k < AC_NKEY; k++) if (in.keys >> k & 1u) zheld |= 1u << jw_zone(k);
    for (k = 0; k < AC_NKEY; k++)                /* a key let go over a glass that was there when it went down: served */
        if (up >> k & 1u) {
            int z = jw_zone(k);
            if (jw.has[z] && !jw.used[z] && !(zheld >> z & 1u)) { jw_serve(z); jw.used[z] = 1; if (mc.state != MS_PLAY) return; }
        }
    for (k = 0; k < 4; k++) if (!(zheld >> k & 1u)) jw.used[k] = 0;
    for (k = 0; k < AC_NKEY; k++)
        if (down >> k & 1u) {
            int z = jw_zone(k), a = -1;
            if (!jw.has[z]) {
                for (i = 0; i < 4; i++) if (i != z && jw.has[i] && (zheld >> i & 1u)) a = i;
                if (a >= 0) {                    /* held there, pressed here: the glass moves */
                    jw.g[z] = jw.g[a]; jw.has[z] = 1; jw.has[a] = 0; jw.used[a] = 1; jw.used[z] = 1;
                    sfx(76, 0, 120, W_TRI, 2200, 2, 120); sfx(96, 0, 60, W_NOISE, 500, 1, 60);
                } else {                         /* a fresh glass */
                    jw.has[z] = 1; jw.g[z].n = 0; jw.used[z] = 1;
                    sfx(88 + z * 2, 0, 160, W_SINE, 1800, 1, 160);
                }
            }
        }
    jw.prev_keys = in.keys;
    for (k = 0; k < 4; k++) {                    /* what is open, flows */
        int32_t add = (int32_t)JW_RATE[jw.valve[k]] * dt / 1000;
        if (jw.splash[k] > 0) jw.splash[k] -= dt;
        if (!add) { jw.puddle[k] = imax(0, jw.puddle[k] - dt / 8); continue; }
        if (jw.has[k]) {
            jw_glass_t *g = &jw.g[k];
            int32_t was = jw_total(g), now;
            if (!g->n || (g->c[g->n - 1] != k && g->n < 3)) { g->c[g->n] = (uint8_t)k; g->v[g->n] = 0; g->n++; }
            g->v[g->n - 1] += add;
            now = was + add;
            if ((now >> 8) != (was >> 8) && (now >> 8) <= JW_CAP)   /* each part reached, a note higher: the ear can count */
                sfx_ex(60 + SC_PEN[(now >> 8) % 5] + 12 * ((now >> 8) / 5), 0, 220, W_TRI, 2400, 2, 150, 0, (k * 2 - 3) * 25, 0);
            if (now > Q8(JW_CAP) + 80) { jw.has[k] = 0; jw_hurt(k); if (mc.state != MS_PLAY) return; }   /* over the brim: lost */
        } else {
            jw.puddle[k] += add;
            if (jw.puddle[k] > 384) { jw.puddle[k] = 0; jw_hurt(k); if (mc.state != MS_PLAY) return; }
        }
    }
    for (i = 0; i < JW_NTK; i++)                 /* the tickets: each waits only so long */
        if (jw.tk[i].on) {
            jw.tk[i].t -= dt;
#ifdef AC_TEST_CATCH
            if (jw.tk[i].t < 2000) jw.tk[i].t = 2000;
#endif
            if (jw.tk[i].t <= 0) { jw.tk[i].on = 0; jw_hurt(-1); if (mc.state != MS_PLAY) return; }
        }
    jw.spawn_t -= dt;
    if (jw.spawn_t <= 0) { jw_ticket(); jw.spawn_t = imax(3800, 8500 - jw.served * 260); }
    if (jw.out_t > 0) jw.out_t -= dt;
    for (k = 0; k < AC_NKEY; k++) {              /* the keys under a glass are lit, the rest glow */
        if (jw.has[jw_zone(k)]) led_key_want |= 1u << k; else led_key_dim_want |= 1u << k;
    }
}
static void jw_glass_draw(int32_t x, int32_t y, int32_t hw, const jw_glass_t *g, col_t rim)   /* y: its foot */
{
    int32_t k, yy = y - 1, h = JW_CAP * JW_UPX + 4;
    for (k = 0; k < g->n; k++) {
        int32_t lh = imin(g->v[k] * JW_UPX >> 8, yy - (y - h));
        if (lh > 0) { grect(x - hw + 1, yy - lh, hw * 2 - 1, lh, JW_COL[g->c[k]]); yy -= lh; }
    }
    gline(x - hw, y - h, x - hw, y, rim); gline(x + hw, y - h, x + hw, y, rim); ghl(x - hw, x + hw + 1, y, rim);
    for (k = 1; k <= JW_CAP; k++) ghl(x - hw + 1, x - hw + (k % 3 ? 3 : 5), y - 1 - k * JW_UPX, rim);   /* a mark to a part */
}
static void juice_draw(void)
{
    uint32_t tm = (uint32_t)mc.t;
    int32_t k, i;
    gclear(th->bg);
    for (k = 0; k < 10; k++) {                   /* the cellar wall: slow bubbles of the four juices */
        int32_t q = (int32_t)((tm / 14u + (uint32_t)k * 97u) % 260u);
        galpha(26); gdisc(12 + (k * 53) % 220 + (tsin(tm, 9, k * 7000) >> 12), 250 - q, 3 + k % 4, JW_COL[k % 4]);
    }
    galpha(256);
    ghl(0, 240, 16, th->rim);                    /* the main pipe the four tanks hang from */
    for (k = 0; k < 4; k++) {
        int32_t x = JW_X(k), v = jw.valve[k], top = JW_FLOOR;
        col_t c = JW_COL[k];
        char nm[2] = {(char)('1' + k), 0};
        grect(x - 1, 16, 3, 6, th->rim);
        galpha(70 + (tsin(tm, 18, k * 15000) >> 9)); gdisc(x, 36, 24, c);          /* its glow */
        galpha(256);
        gellf(x, 40, 21, 18, th->bg, 1); grect(x - 21, 40, 43, 9, th->bg);         /* the tank: a cap of glass, juice in it */
        galpha(190); gellf(x, 40, 19, 15, c, 1); grect(x - 19, 40, 39, 7, c); galpha(256);
        garc(x, 40, 21, 18, 32768, 65536, th->text, 0); gline(x - 21, 40, x - 21, 48, th->text); gline(x + 21, 40, x + 21, 48, th->text); ghl(x - 21, x + 22, 48, th->text);
        gtextc(x + 1, 33, nm, th->bg, 2);
        grect(x - 3, 49, 7, 11, th->panel); gbox(x - 3, 49, 7, 11, th->text);      /* the tap, and how far it is open */
        for (i = 0; i < 3; i++) grect(x + 8 + i * 5, 52, 3, 6, i < v ? c : th->mute);
        if (jw.has[k]) top = JW_FLOOR - 1 - imin(JW_CAP * JW_UPX + 3, jw_total(&jw.g[k]) * JW_UPX >> 8);
        if (v) {                                 /* the stream */
            int32_t w = v + 1, y;
            for (y = 60; y < top; y += 4) grect(x - w / 2 + ((tsin(tm, 90, y * 900) > 9000) ? 1 : 0), y, w, imin(4, top - y), c);
            galpha(150); gdisc(x, top, 3 + v + (int32_t)((tm / 60u) & 1u), c); galpha(256);
        }
        if (jw.puddle[k] > 0) { galpha(200); gellf(x, JW_FLOOR, 4 + jw.puddle[k] / 22, 2, c, 1); galpha(256); }
        if (jw.has[k]) jw_glass_draw(x, JW_FLOOR, 13, &jw.g[k], th->text);
        else { galpha(70); gbox(x - 13, JW_FLOOR - 4, 27, 4, th->mute); galpha(256); }   /* where a glass goes */
        if (jw.splash[k] > 0)
            for (i = 0; i < 7; i++) {
                int32_t a = 34000 + i * 4700, r = (500 - jw.splash[k]) * 30 / 500 + 4;
                galpha(jw.splash[k] * 256 / 500); grect(x + ((r * icos(a)) >> 14), JW_FLOOR - 6 + ((r * isin(a)) >> 14), 3, 3, c);
            }
        galpha(256);
        grect(x - 24, JW_FLOOR + 10, 49, 3, jw.has[k] ? c : th->mute);             /* the keys under this tap */
        for (i = 0; i < 7; i++) grect(x - 24 + i * 7, JW_FLOOR + 15, 6, 9, (in.keys && jw_zone(in.hikey < 0 ? 0 : in.hikey) == k) ? c : th->panel);
        for (i = 0; i < 7; i++) gbox(x - 24 + i * 7, JW_FLOOR + 15, 6, 9, jw.has[k] ? c : th->mute);
    }
    ghl(0, 240, JW_FLOOR + 1, th->rim); ghl(0, 240, JW_FLOOR + 4, th->mute);
    for (i = 0; i < JW_NTK; i++)                 /* the tickets: the drink as it should stand in the glass */
        if (jw.tk[i].on) {
            int32_t x = 60 + i * 60, left = jw.tk[i].t * 256 / jw.tk[i].life, sway = tsin(tm, 14, i * 20000) >> 13;
            jw_glass_t g;
            g.n = jw.tk[i].n;
            for (k = 0; k < g.n; k++) { g.c[k] = jw.tk[i].c[k]; g.v[k] = Q8(jw.tk[i].u[k]); }
            if (left < 64 && (tm / 160u) & 1u) galpha(120);
            gline(x, 16, x + sway, 78, th->mute);
            grect(x - 13 + sway, 78, 27, 62, th->panel); gbox(x - 13 + sway, 78, 27, 62, left < 64 ? th->caps[0] : th->text);
            grect(x - 11 + sway, 81, 23 * left / 256, 3, left < 64 ? th->caps[0] : left < 128 ? JW_COL[0] : JW_COL[2]);
            jw_glass_draw(x + sway, 135, 8, &g, th->text);
            galpha(256);
        }
    if (jw.out_t > 0) {                          /* a glass on its way out: up and off if it was right, dropped if not */
        int32_t q = 500 - jw.out_t;
        galpha(jw.out_t * 256 / 500);
        jw_glass_draw(jw.out_x, JW_FLOOR + (jw.out_ok ? -q * 40 / 500 : q * 30 / 500), 13, &jw.out, jw.out_ok ? th->pad : th->caps[0]);
        galpha(256);
    }
    if (mc.flash > 0) { galpha(mc.flash * 110 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
}

/* ---------------------------------------------------------------- NIGHT BLOOM */
#define NB_MAX 14
static const uint8_t NB_R[4] = {26, 42, 58, 74};   /* drawn in from the frame: the spores have room to be seen coming */
#define NB_ARC 7000                              /* a petal's half width */
/* a ring, its petal and its spores share one colour no other ring could be taken for: yellow, white, green, blue */
/* the four rings' knobs, in the order they join: two from the right hand's side of the panel, two from the left */
static const uint8_t NB_ENC[4] = {ENC_KNOB1, ENC_SELECT, ENC_KNOB2, ENC_ALGORITHM};
static const char *const NB_KNOB[4] = {"KNOB 1", "SELECT", "KNOB 2", "ALGORITHM"};
static const col_t NB_COL[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
static struct {
    int32_t ang[4], hit[4];
    struct { uint32_t born; int16_t a, curl; uint8_t c, on, past; } sp[NB_MAX];
    struct { int16_t x, y; uint8_t c; } fl[48];
    uint32_t nfl, last_beat, t0;
    int32_t breath, rings, caught, ring_t, rip_t;
} nb;
static void bloom_spawn(uint32_t now, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < NB_MAX; i++)
        if (!nb.sp[i].on) {
            nb.sp[i].on = 1; nb.sp[i].past = 0; nb.sp[i].born = now; nb.sp[i].c = (uint8_t)(ihash(seed) % (uint32_t)nb.rings);
            nb.sp[i].a = (int16_t)ihash(seed + 1u); nb.sp[i].curl = (int16_t)((int32_t)(ihash(seed + 2u) & 0x3FFFu) - 0x2000);
            return;
        }
}
/* a spore's place: it reaches its own ring two beats after it is born, the centre a beat and a half later */
static void bloom_pos(uint32_t i, uint32_t now, int32_t *rad, int32_t *ang)
{
    int32_t age = (int32_t)(now - nb.sp[i].born), ring = NB_R[nb.sp[i].c];
    /* slow at first, then faster: the way in goes with the square of the time, and still meets its ring on the beat */
    *rad = age <= 192 ? 122 - (122 - ring) * age / 192 * age / 192 : ring - (ring - 6) * (age - 192) / 144;
    *ang = nb.sp[i].a + nb.sp[i].curl * age / 192;
}
static void bloom_tick(int32_t dt)
{
    uint32_t now = (tr_now_q8() >> 8) - nb.t0, i, beat2;
    int32_t k, hard = nb.caught >= 30;
    for (k = 0; k < 4; k++) {
        if (k < nb.rings && in.enc[NB_ENC[k]]) nb.ang[k] += in.enc[NB_ENC[k]] * 2731;   /* a click of the knob: a twenty-fourth of a turn */
        if (nb.hit[k] > 0) nb.hit[k] -= dt;
    }
    if (in.kdown && nb.breath >= 1000) {         /* a breath: a ripple goes out and every spore is pushed back a beat */
        nb.breath -= 1000; nb.rip_t = 450;
        for (i = 0; i < NB_MAX; i++) if (nb.sp[i].on && !nb.sp[i].past) nb.sp[i].born = (uint32_t)imin((int32_t)nb.sp[i].born + 96, (int32_t)now);
        sfx(96, 0, 600, W_NOISE, 1400, 80, 220);
    }
    nb.breath = imin(3000, nb.breath + dt / 6);
    if (nb.rip_t > 0) nb.rip_t -= dt;
    if (nb.ring_t > 0) nb.ring_t -= dt;
    /* the pace: one ring, a spore every other beat; two or three, most beats; all four, every beat, then the half beats too */
    beat2 = now / (hard ? 48u : 96u);
    if (beat2 != nb.last_beat) {
        uint32_t r = ihash(beat2 * 5u + nb.t0);
        int go;
        nb.last_beat = beat2;
        if (hard) go = !(beat2 & 1u) || r < (uint32_t)imin(40000, 6000 + (nb.caught - 30) * 700);
        else go = nb.rings == 1 ? !(beat2 & 1u) : nb.rings < 4 ? (!(beat2 & 1u) || r < 30000u) : 1;
        if (go && nb.ring_t < 900) bloom_spawn(beat2 * (hard ? 48u : 96u), beat2 * 11u + nb.t0);   /* (a breather while a new ring arrives) */
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t age;
        if (!nb.sp[i].on) continue;
        age = (int32_t)(now - nb.sp[i].born);
        if (!nb.sp[i].past && age >= 192) {      /* it is at its ring: is the petal there? */
            int32_t rad, ang, d;
            bloom_pos(i, nb.sp[i].born + 192u, &rad, &ang);
            d = (int32_t)(int16_t)(uint16_t)(ang - nb.ang[nb.sp[i].c]);
#ifdef AC_TEST_CATCH                             /* (tests) every spore is caught: the rings arriving one by one */
            d = 0; nb.ang[nb.sp[i].c] = ang;
#endif
            if (iabs(d) < NB_ARC) {
                int32_t c = nb.sp[i].c;
                nb.sp[i].on = 0; mc.combo++; mc.score += 1 + mc.combo / 5; nb.hit[c] = 220; nb.caught++;
                sfx_ex(60 + 12 * (c / 2) + SC_PEN[(c * 2 + ((uint32_t)ang >> 13)) % 5], 0, 700, c & 1 ? W_TRI : W_SINE, 3200, 4, 170, 0, (icos(ang) * 60) >> 14, 0);
                if (nb.nfl < 48u) {              /* a flower opens where the garden has room */
                    uint32_t s = ihash(nb.nfl * 3u + nb.t0), e = nb.nfl % 4u;
                    nb.fl[nb.nfl].x = (int16_t)(e < 2u ? 6 + (int32_t)(s * 228u >> 16) : (e == 2u ? 6 + (int32_t)(s & 15u) : 218 + (int32_t)(s & 15u)));
                    nb.fl[nb.nfl].y = (int16_t)(e == 0u ? 28 + (int32_t)(s & 7u) : e == 1u ? 222 + (int32_t)(s & 7u) : 40 + (int32_t)(s * 170u >> 16));
                    nb.fl[nb.nfl].c = (uint8_t)c; nb.nfl++;
                }
                if (nb.rings < 4 && nb.caught >= nb.rings * 5) {   /* five caught: the next ring, and its knob, join in */
                    nb.rings++; nb.ring_t = 2200; snd_chime(1);
                }
            } else { nb.sp[i].past = 1; mc.combo = 0; }
        }
        if (nb.sp[i].past && age >= 336) {       /* it reached the flower: a petal falls */
            nb.sp[i].on = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
            sfx_ex(43, 0, 400, W_SAW, 3200, 2, 120, 0, 0, 65200);
            if (mc.lives <= 0) { mach_over(); return; }
        }
    }
    led_btn_want |= (now % 96u < 14u) ? 1u << BTN_PLAY : 0u;
    led_key_dim_want = nb.breath >= 1000 ? (1u << AC_NKEY) - 1u : 0u;
    led_key_want |= in.keys;
}
static void bloom_draw(void)
{
    uint32_t now = (tr_now_q8() >> 8) - nb.t0, i, tm = (uint32_t)mc.t;
    int32_t cx = 120, cy = 126, k, beat = 256 - imin(256, (int32_t)(now % 96u) * 256 / 50);
    sky_draw();
    for (i = 0; i < nb.nfl; i++) {               /* the garden so far */
        col_t c = NB_COL[nb.fl[i].c];
        int32_t x = nb.fl[i].x, y = nb.fl[i].y, s = 2 + ((tsin(tm, 20, (int32_t)i * 7000) + 16384) >> 13);
        galpha(60); gdisc(x, y, s + 3, c); galpha(256);
        gp(x - s, y, c); gp(x + s, y, c); gp(x, y - s, c); gp(x, y + s, c); grect(x - 1, y - 1, 3, 3, c); gp(x, y, th->text);
    }
    if (nb.rip_t > 0) {                          /* the breath going out */
        int32_t rr = 12 + (450 - nb.rip_t) * 110 / 450;
        galpha(nb.rip_t * 200 / 450); garc(cx, cy, rr, rr, 0, 65536, th->text, 1); garc(cx, cy, rr - 5, rr - 5, 0, 65536, th->mute, 0); galpha(256);
    }
    for (k = 0; k < nb.rings; k++) {
        col_t c = NB_COL[k];
        int32_t fresh = (k == nb.rings - 1 && nb.ring_t > 0) ? nb.ring_t : 0, on_ = nb.hit[k] > 0 || (fresh && (fresh / 150) % 2);
        galpha(fresh ? 200 : 96); garc(cx, cy, NB_R[k], NB_R[k], 0, 65536, c, 1);
        galpha(256);
        garc(cx, cy, NB_R[k] - 1, NB_R[k] - 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, on_ ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 1, NB_R[k] + 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, on_ ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 3, NB_R[k] + 3, nb.ang[k] - NB_ARC + 900, nb.ang[k] + NB_ARC - 900, c, 1);
        if (nb.hit[k] > 0) { galpha(nb.hit[k]); gdisc(cx + ((NB_R[k] * icos(nb.ang[k])) >> 14), cy + ((NB_R[k] * isin(nb.ang[k])) >> 14), 9, c); galpha(256); }
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t rad, ang, x, y;
        col_t c;
        if (!nb.sp[i].on) continue;
        bloom_pos(i, now, &rad, &ang);
        c = NB_COL[nb.sp[i].c];
        x = cx + ((rad * icos(ang)) >> 14); y = cy + ((rad * isin(ang)) >> 14);
        galpha(nb.sp[i].past ? 50 : 110); gdisc(x, y, 7, c); galpha(256);
        gdisc(x, y, 3, nb.sp[i].past ? th->mute : c);
    }
    for (k = 0; k < 5; k++) {                    /* the flower: a petal for each life */
        int32_t a = k * 13107 + (int32_t)(tm * 2u), on = k < mc.lives;
        galpha(on ? 256 : 50);
        gdisc(cx + (((7 + beat / 64) * icos(a)) >> 14), cy + (((7 + beat / 64) * isin(a)) >> 14), 4, on ? th->caps[0] : th->mute);
    }
    galpha(256); gdisc(cx, cy, 4 + beat / 100, th->pad);
    for (k = 0; k < 3; k++) grect(100 + k * 14, 232, 12, 3, nb.breath >= (k + 1) * 1000 ? th->pad : th->mute);   /* breaths in hand */
    if (nb.ring_t > 0) {                         /* a new ring: which knob turns it, in the ring's own colour */
        galpha(imin(256, nb.ring_t));
        gfit(120, 30 + 2 * ts(2, 4), 232, NB_KNOB[nb.rings - 1], NB_COL[nb.rings - 1], ts(2, 4));
        galpha(256);
    }
    if (mc.flash > 0 && mc.msg == T_MISS && mc.msg_t > 0) { galpha(mc.flash * 110 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
}

/* ---------------------------------------------------------------- all three */
static void mach_start(void)
{
    uint32_t i, now = tr_now_q8() >> 8;
    mc.state = MS_PLAY; mc.score = 0; mc.combo = 0; mc.t = 0; mc.msg_t = 0; mc.flash = 0;
    if (mc.which == 0) {
        mc.lives = 3; mr.t0 = (now / 96u + 1u) * 96u - 96u; mr.done_h = (now - mr.t0) / 48u; mr.hop_h = 0; mr.seed = now * 31u;
        /* nothing for the first beats: mr_obs keeps the first eight half-beats clear from where the clock stands */
        mr.t0 = (now / 96u) * 96u; mr.done_h = 0; mr.blk_h = 0; mr.blk_done = 0;
    } else if (mc.which == 1) {
        for (i = 0; i < sizeof jw; i++) ((uint8_t *)&jw)[i] = 0;
        mc.lives = 5; jw.seed = now * 31u + 7u; jw.spawn_t = 700; jw.prev_keys = in.keys;
#ifdef AC_TEST_CATCH
        jw.seed = 1;
#endif
    } else if (mc.which == 2) {
        for (i = 0; i < sizeof nb; i++) ((uint8_t *)&nb)[i] = 0;
        mc.lives = 5; nb.t0 = (now / 96u) * 96u; nb.breath = 3000; nb.rings = 1; nb.ring_t = 2200;
        for (i = 0; i < 4u; i++) nb.ang[i] = (int32_t)i * 16384 - 16384;
    }
#ifdef AC_TEST_CATCH                             /* (tests) lives enough to see a long way in */
    mc.lives = 99;
#endif
    snd_chime(0);
}
static void mach_enter(int which)
{
    uint32_t i;
    mc.which = (uint8_t)which; mc.state = MS_TITLE; mc.t = 0; mc.msg_t = 0; mc.flash = 0; mc.score = 0; mc.lives = 0;
    (void)i;
}
/* -1: REC at its start or end, back to the Arcave; 1: HOME, how it is played (arcave.c opens the sheet) */
static int mach_tick(int32_t dt)
{
    mc.t += dt;
    if (mc.msg_t > 0) mc.msg_t -= dt;
    if (mc.flash > 0) mc.flash -= dt;
    if (BDOWN(BTN_HOME)) return 1;
    if (mc.state != MS_PLAY) {
        if (BDOWN(BTN_REC)) return -1;
        if (BDOWN(BTN_PLAY)) mach_start();
        else { led_play_breathe = 1; led_breathe_want |= 1u << BTN_HOME | 1u << BTN_REC; }
        return 0;
    }
    if (mc.which == 0) run_tick(); else if (mc.which == 1) juice_tick(dt); else bloom_tick(dt);
    return 0;
}
static void mach_draw(void)
{
    if (mc.which == 0) run_draw(); else if (mc.which == 1) juice_draw(); else bloom_draw();
    if (mc.state == MS_PLAY) mach_hud();
    else mach_card(MACH_NAME[mc.which], th->caps[mc.which * 2 % 5]);
}
