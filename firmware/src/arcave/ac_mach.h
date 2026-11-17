/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's three machines. All three run on the jukebox's clock, so what you do in them lands in the music.
 *   MYSTIC RUN        Rock runs on the beat; a key hops a crystal, a higher key hops higher. KNOB 1 is the tempo.
 *   THE LIGHT TABLE   three rings turn under a needle; keys drop light on them, and each drop sounds as it comes
 *                     round. KNOB 1-3 set how fast each ring turns, KNOB 4 the heat. REC wipes the table.
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
    for (i = 0; i < mc.lives; i++) gtext(8 + i * 9, 8, "\xE2\x99\xA5", th->caps[0], 1);
    if (mc.msg_t > 0) { galpha(imin(256, mc.msg_t * 256 / 300)); gtextc(120, 40 - (700 - mc.msg_t) / 60, TR(mc.msg), mc.msg == T_MISS ? th->caps[0] : th->pad, 2); galpha(256); }
}
static void mach_card(const char *name, int t1, col_t c)
{
    char b[16]; int32_t y, k;
    gveil(th->bg, 22);
    gtextc(120, 30, name, c, name[9] ? 2 : 3);
    y = 66;
    for (k = 0; k < 3; k++) y += gtext_wrap(12, y, 216, TR(t1 + k), th->text, 1, 10, -1, 1, 0, 1) * 10 + 6;
    if (mc.state == MS_OVER) {
        gtextc(120, y + 6, TR(T_GAMEOVER), th->caps[0], 2);
        num_str(b, mc.score); gtextc(120, y + 26, b, th->text, 3);
    }
    if (best[mc.which]) { num_str(b, (int32_t)best[mc.which]); gtextr(118, 186, TR(T_BEST), th->mute, 1); gtext(124, 186, b, th->pad, 1); }
    if (ipulse((uint32_t)mc.t, 1200) > 90) gtextc(120, 204, TR(mc.state == MS_OVER ? T_PLAY_AGAIN : T_PLAY_START), th->pad, 1);
    gtextc(120, 222, TR(T_HOME_BACK), th->mute, 1);
}
static void mach_over(void)
{
    mc.state = MS_OVER; mc.t = 0;
    if ((uint32_t)mc.score > best[mc.which]) { best[mc.which] = (uint32_t)mc.score; save_dirty = 1; }
    sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150);
}

/* ---------------------------------------------------------------- MYSTIC RUN */
static struct { uint32_t t0, done_h; int32_t hop_t, hop_h, hop_k; uint32_t seed; } mr;
#define MR_HOP 62                                /* ticks in the air */
static int mr_obs(uint32_t h)                    /* what stands at half-beat h: 0 nothing, 1 a crystal, 2 a tall one, 3 a spore */
{
    uint32_t r;
    if (h < 8u) return 0;
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
        mr.hop_t = (int32_t)now; mr.hop_k = in.hikey; mr.hop_h = 24 + in.hikey * 40 / 26;
        sfx(key_note(in.hikey) + 12, 0, 300, W_TRI, 3000, 3, 120);
    }
    for (h = mr.done_h + 1u; h * 48u <= now; h++) {
        int o = mr_obs(h), ht = mr_height(h * 48u);
        mr.done_h = h;
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
    {   /* low keys for a crystal, high ones for a tall one: the lights say which is next */
        uint32_t nh = (now / 96u + 1u) * 2u;
        int o = mr_obs(nh);
        if (o == 1) led_key_dim_want = (1u << AC_NKEY) - 1u;
        if (o == 2) led_key_want = ((1u << AC_NKEY) - 1u) & ~((1u << 14) - 1u);
    }
    if (now % 96u < 12u) led_btn_want |= 1u << BTN_PLAY;
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
        else {
            int32_t hh = o == 1 ? 13 : 30, xy[6], k;
            for (k = 0; k < (o == 1 ? 2 : 3); k++) {
                xy[0] = x - 5 + k * 5 - 4; xy[1] = gy; xy[2] = x - 5 + k * 5 + (k - 1); xy[3] = gy - hh + iabs(k - 1) * 5; xy[4] = x - 5 + k * 5 + 4; xy[5] = gy;
                gpoly(xy, 3, th->dark ? 0 : th->bands[k]); gpolyline(xy, 3, 1, th->caps[(h + (uint32_t)k) % 5u]);
            }
        }
    }
    rock_draw(rx, gy - 8 - mr_height(now), 256, (int32_t)(now * 400u), (tm % 3100u) < 150u ? 40 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    if (mc.flash > 0) { galpha(mc.flash * 120 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
    {
        char b[12]; num_str(b, (int32_t)(tr_bpm_q8 >> 8));
        gtext(8, 226, b, th->mute, 1); gtext(8 + gtextw(b, 1) + 4, 226, "BPM", th->mute, 1);
    }
#undef MRX
}

/* ---------------------------------------------------------------- THE LIGHT TABLE */
static const uint8_t LT_N[3] = {8, 12, 16}, LT_R[3] = {38, 64, 90}, LT_SPB[6] = {4, 6, 8, 12, 16, 24};
static struct { uint8_t note[3][16], rate[3], heat; int32_t glow[3][16]; int32_t last[3]; int32_t wipe; } lt;
static int32_t lt_slot(int r, uint32_t now_q8)   /* which slot of ring r is under the needle; also its turn so far */
{
    return (int32_t)(((now_q8 >> 8) * LT_SPB[lt.rate[r]] / TICKS_BAR) % LT_N[r]);
}
static void table_tick(int32_t dt)
{
    uint32_t now = tr_now_q8();
    int32_t r, k;
    for (r = 0; r < 3; r++) {
        int32_t d = in.enc[ENC_KNOB1 + r], s;
        if (d) { lt.rate[r] = (uint8_t)iclamp(lt.rate[r] + (d > 0 ? 1 : -1), 0, 5); snd_ui(60 + lt.rate[r] * 2 + r * 7); }
        s = lt_slot(r, now);
        if (s != lt.last[r]) {                   /* a slot comes under the needle: its drop sounds */
            lt.last[r] = s;
            if (lt.note[r][s]) {
                int32_t n = key_note(lt.note[r][s] - 1) + (r == 0 ? 0 : 12), dur = 250 + lt.heat * 160;
                sfx_ex(n, 0, (uint32_t)dur, r == 2 ? W_SINE : W_TRI, 3000, lt.heat > 4 ? 30 : 4, 120 + lt.heat * 12, 0, (r - 1) * 50, 0);
                if (lt.heat >= 3) sfx_ex(n + 7, 0, (uint32_t)dur, W_SINE, 1500, 20, 150, 0, (1 - r) * 50, 0);
                if (lt.heat >= 6) sfx_ex(n + 12, 30, (uint32_t)dur, W_SINE, 1200, 40, 180, 0, 0, 0);
                lt.glow[r][s] = 256;
            }
        }
        for (k = 0; k < LT_N[r]; k++) if (lt.glow[r][k] > 0) lt.glow[r][k] = imax(0, lt.glow[r][k] - dt);
    }
    if (in.enc[ENC_KNOB4]) lt.heat = (uint8_t)iclamp(lt.heat + in.enc[ENC_KNOB4], 0, 8);
    for (k = 0; k < AC_NKEY; k++)
        if (in.kdown >> k & 1u) {                /* low keys the inner ring, middle keys the middle, high keys the outer */
            r = k / 9;
            lt.note[r][lt_slot(r, now + (TICKS_STEP << 7))] = (uint8_t)(k + 1);
            lt.glow[r][lt_slot(r, now + (TICKS_STEP << 7))] = 256;
            sfx(key_note(k) + (r == 0 ? 0 : 12), 0, 500, W_TRI, 3200, 4, 140);
        }
    if (BDOWN(BTN_REC)) {
        for (r = 0; r < 3; r++) for (k = 0; k < 16; k++) lt.note[r][k] = 0;
        lt.wipe = 500; mach_msg(T_CLEARED); sfx(96, 0, 400, W_NOISE, 1600, 2, 200);
    }
    if (lt.wipe > 0) lt.wipe -= dt;
    led_key_want |= in.keys;
    led_btn_want |= 1u << BTN_REC;
}
static void table_draw(void)
{
    uint32_t now = tr_now_q8(), tm = (uint32_t)mc.t;
    int32_t cx = 120, cy = 124, r, k;
    gclear(th->bg);
    for (k = 5; k >= 0; k--) {                   /* the table: warm light that breathes with the heat */
        galpha(18 + lt.heat * 5 + k * 3);
        gdisc(cx + (tsin(tm, 7, k * 9000) >> 11), cy + (tsin(tm, 9, k * 7000) >> 11), 30 + k * 15, th->bands[(k + (int32_t)(tm / 900u)) % 5]);
    }
    galpha(256);
    for (r = 0; r < 3; r++) {
        uint32_t per = (TICKS_BAR / LT_SPB[lt.rate[r]]) * LT_N[r] * 16u;   /* ticks (Q4) in one turn of this ring */
        int32_t turn = (int32_t)(((now >> 4) % per) * 65536u / per);
        galpha(120); gcirc(cx, cy, LT_R[r], th->rim); galpha(256);
        for (k = 0; k < LT_N[r]; k++) {
            int32_t a = k * 65536 / LT_N[r] - turn - 16384, x = cx + ((LT_R[r] * icos(a)) >> 14), y = cy + ((LT_R[r] * isin(a)) >> 14);
            if (lt.note[r][k]) {
                col_t c = th->caps[(lt.note[r][k] - 1) % 5];
                int32_t g = lt.glow[r][k];
                galpha(60 + g / 3); gdisc(x, y, 5 + lt.heat / 2 + g / 40, c);
                galpha(256); gdisc(x, y, 2 + (g > 128), g > 128 ? th->text : c);
            } else gp(x, y, th->mute);
        }
    }
    gline(cx, cy - 16, cx, cy - 100, th->text); gdisc(cx, cy - 102, 2, th->pad);   /* the needle */
    gdisc(cx, cy, 6, th->panel); gcirc(cx, cy, 6, th->pad);
    for (r = 0; r < 3; r++) {                    /* how many steps a bar each ring takes */
        char b[4]; num_str(b, LT_SPB[lt.rate[r]]);
        gtextc(40 + r * 46, 226, b, th->caps[r + 1], 1);
    }
    for (k = 0; k < 8; k++) grect(170 + k * 7, 224 + (k < lt.heat ? 0 : 3), 5, k < lt.heat ? 7 : 1, k < lt.heat ? th->caps[0] : th->mute);
    if (lt.wipe > 0) { galpha(lt.wipe * 200 / 500); grect(0, 0, 240, 240, th->pad); galpha(256); }
    if (mc.msg_t > 0) gtextc(120, 8, TR(mc.msg), th->pad, 2);
}

/* ---------------------------------------------------------------- NIGHT BLOOM */
#define NB_MAX 14
static const uint8_t NB_R[4] = {26, 42, 58, 74};   /* drawn in from the frame: the spores have room to be seen coming */
#define NB_ARC 7000                              /* a petal's half width */
static struct {
    int32_t ang[4], hit[4];
    struct { uint32_t born; int16_t a, curl; uint8_t c, on, past; } sp[NB_MAX];
    struct { int16_t x, y; uint8_t c; } fl[48];
    uint32_t nfl, last_beat, t0, slow_until;
    int32_t breath, level, level_t;
} nb;
static void bloom_spawn(uint32_t now, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < NB_MAX; i++)
        if (!nb.sp[i].on) {
            nb.sp[i].on = 1; nb.sp[i].past = 0; nb.sp[i].born = now; nb.sp[i].c = (uint8_t)(ihash(seed) & 3u);
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
    int32_t k;
    for (k = 0; k < 4; k++) {
        if (in.enc[ENC_KNOB1 + k]) nb.ang[k] += in.enc[ENC_KNOB1 + k] * 2731;   /* a click of the knob: a twenty-fourth of a turn */
        if (nb.hit[k] > 0) nb.hit[k] -= dt;
    }
    if (in.kdown && nb.breath >= 1000) {         /* a breath: every spore is pushed back a beat */
        nb.breath -= 1000;
        for (i = 0; i < NB_MAX; i++) if (nb.sp[i].on && !nb.sp[i].past) nb.sp[i].born = imin((int32_t)nb.sp[i].born + 96, (int32_t)now);
        sfx(96, 0, 500, W_NOISE, 900, 60, 220); sfx(key_note(in.hikey), 0, 800, W_SINE, 2400, 40, 220);
        mc.flash = 200;
    }
    nb.breath = imin(3000, nb.breath + dt / 6);
    beat2 = now / (nb.level >= 3 ? 48u : 96u);
    if (beat2 != nb.last_beat) {
        nb.last_beat = beat2;
        if (nb.level >= 3 ? (!(beat2 & 1u) || ihash(beat2 * 5u + nb.t0) < 9000u + (uint32_t)nb.level * 3500u) : (nb.level >= 2 || !(beat2 & 1u) || beat2 > 16u))
            bloom_spawn(nb.level >= 3 ? beat2 * 48u : beat2 * 96u, beat2 * 11u + nb.t0);
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t age;
        if (!nb.sp[i].on) continue;
        age = (int32_t)(now - nb.sp[i].born);
        if (!nb.sp[i].past && age >= 192) {      /* it is at its ring: is the petal there? */
            int32_t rad, ang, d;
            bloom_pos(i, nb.sp[i].born + 192u, &rad, &ang);
            d = (int32_t)(int16_t)(uint16_t)(ang - nb.ang[nb.sp[i].c]);
            if (iabs(d) < NB_ARC) {
                int32_t c = nb.sp[i].c;
                nb.sp[i].on = 0; mc.combo++; mc.score += 1 + mc.combo / 5; nb.hit[c] = 220;
                sfx_ex(60 + 12 * (c / 2) + SC_PEN[(c * 2 + ((uint32_t)ang >> 13)) % 5], 0, 700, c & 1 ? W_TRI : W_SINE, 3200, 4, 170, 0, (icos(ang) * 60) >> 14, 0);
                if (nb.nfl < 48u) {              /* a flower opens where the garden has room */
                    uint32_t s = ihash(nb.nfl * 3u + nb.t0), e = nb.nfl % 4u;
                    nb.fl[nb.nfl].x = (int16_t)(e < 2u ? 6 + (int32_t)(s * 228u >> 16) : (e == 2u ? 6 + (int32_t)(s & 15u) : 218 + (int32_t)(s & 15u)));
                    nb.fl[nb.nfl].y = (int16_t)(e == 0u ? 28 + (int32_t)(s & 7u) : e == 1u ? 222 + (int32_t)(s & 7u) : 40 + (int32_t)(s * 170u >> 16));
                    nb.fl[nb.nfl].c = (uint8_t)c; nb.nfl++;
                }
                if (mc.score >= (nb.level + 1) * (nb.level + 1) * 12) { nb.level++; nb.level_t = 1200; snd_chime(1); }
            } else { nb.sp[i].past = 1; mc.combo = 0; }
        }
        if (nb.sp[i].past && age >= 336) {       /* it reached the flower: a petal falls */
            nb.sp[i].on = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
            sfx_ex(43, 0, 400, W_SAW, 3200, 2, 120, 0, 0, 65200);
            if (mc.lives <= 0) { mach_over(); return; }
        }
    }
    if (nb.level_t > 0) nb.level_t -= dt;
    led_btn_want |= (now % 96u < 14u) ? 1u << BTN_PLAY : 0u;
    led_key_dim_want = nb.breath >= 1000 ? (1u << AC_NKEY) - 1u : 0u;
    led_key_want |= in.keys;
}
static void bloom_draw(void)
{
    uint32_t now = (tr_now_q8() >> 8) - nb.t0, i, tm = (uint32_t)mc.t;
    int32_t cx = 120, cy = 126, k, beat = 256 - imin(256, (int32_t)(now % 96u) * 256 / 50);
    static const uint8_t CI[4] = {0, 2, 3, 4};
    sky_draw();
    for (i = 0; i < nb.nfl; i++) {               /* the garden so far */
        col_t c = th->caps[CI[nb.fl[i].c]];
        int32_t x = nb.fl[i].x, y = nb.fl[i].y, s = 2 + ((tsin(tm, 20, (int32_t)i * 7000) + 16384) >> 13);
        galpha(60); gdisc(x, y, s + 3, c); galpha(256);
        gp(x - s, y, c); gp(x + s, y, c); gp(x, y - s, c); gp(x, y + s, c); grect(x - 1, y - 1, 3, 3, c); gp(x, y, th->text);
    }
    for (k = 0; k < 4; k++) {
        col_t c = th->caps[CI[k]];
        galpha(96); garc(cx, cy, NB_R[k], NB_R[k], 0, 65536, c, 1);
        galpha(256);
        garc(cx, cy, NB_R[k] - 1, NB_R[k] - 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, nb.hit[k] > 0 ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 1, NB_R[k] + 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, nb.hit[k] > 0 ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 3, NB_R[k] + 3, nb.ang[k] - NB_ARC + 900, nb.ang[k] + NB_ARC - 900, c, 1);
        if (nb.hit[k] > 0) { galpha(nb.hit[k]); gdisc(cx + ((NB_R[k] * icos(nb.ang[k])) >> 14), cy + ((NB_R[k] * isin(nb.ang[k])) >> 14), 9, c); galpha(256); }
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t rad, ang, x, y;
        col_t c;
        if (!nb.sp[i].on) continue;
        bloom_pos(i, now, &rad, &ang);
        c = th->caps[CI[nb.sp[i].c]];
        x = cx + ((rad * icos(ang)) >> 14); y = cy + ((rad * isin(ang)) >> 14);
        galpha(nb.sp[i].past ? 50 : 100); gdisc(x, y, 7, c); galpha(256);
        gdisc(x, y, 3, nb.sp[i].past ? th->mute : c);
        if (!nb.sp[i].past) grect(x - 1, y - 1, 2, 2, th->text);
    }
    for (k = 0; k < 5; k++) {                    /* the flower: a petal for each life */
        int32_t a = k * 13107 + (int32_t)(tm * 2u), on = k < mc.lives;
        galpha(on ? 256 : 50);
        gdisc(cx + (((7 + beat / 64) * icos(a)) >> 14), cy + (((7 + beat / 64) * isin(a)) >> 14), 4, on ? th->caps[1] : th->mute);
    }
    galpha(256); gdisc(cx, cy, 4 + beat / 100, th->pad);
    for (k = 0; k < 3; k++) grect(100 + k * 14, 232, 12, 3, nb.breath >= (k + 1) * 1000 ? th->pad : th->mute);   /* breaths in hand */
    if (nb.level_t > 0) { char b[4]; num_str(b, nb.level + 1); galpha(imin(256, nb.level_t)); gtextc(120, 112, b, th->text, 5); galpha(256); }
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
        mr.t0 = (now / 96u) * 96u; mr.done_h = 0;
    } else if (mc.which == 2) {
        for (i = 0; i < sizeof nb; i++) ((uint8_t *)&nb)[i] = 0;
        mc.lives = 5; nb.t0 = (now / 96u) * 96u; nb.breath = 3000;
        for (i = 0; i < 4u; i++) nb.ang[i] = (int32_t)i * 16384 - 16384;
    }
    snd_chime(0);
}
static void mach_enter(int which)
{
    uint32_t i;
    mc.which = (uint8_t)which; mc.state = MS_TITLE; mc.t = 0; mc.msg_t = 0; mc.flash = 0; mc.score = 0; mc.lives = 0;
    if (which == 1) {
        for (i = 0; i < 3u; i++) lt.last[i] = -1;
        if (!lt.rate[0] && !lt.rate[1] && !lt.rate[2]) { lt.rate[0] = 2; lt.rate[1] = 3; lt.rate[2] = 4; lt.heat = 3; }
    }
}
/* -1: HOME, back to the cave */
static int mach_tick(int32_t dt)
{
    mc.t += dt;
    if (mc.msg_t > 0) mc.msg_t -= dt;
    if (mc.flash > 0) mc.flash -= dt;
    if (BDOWN(BTN_HOME)) return -1;
    if (mc.state != MS_PLAY) {
        if (BDOWN(BTN_PLAY)) mach_start();
        else if (ipulse((uint32_t)mc.t, 1200) > 128) led_btn_want |= 1u << BTN_PLAY;
        return 0;
    }
    if (mc.which == 0) run_tick(); else if (mc.which == 1) table_tick(dt); else bloom_tick(dt);
    return 0;
}
static void mach_draw(void)
{
    static const int T1[3] = {T_RUN_1, T_TABLE_1, T_BLOOM_1};
    if (mc.which == 0) run_draw(); else if (mc.which == 1) table_draw(); else bloom_draw();
    if (mc.state == MS_PLAY) { if (mc.which != 1) mach_hud(); }
    else mach_card(MACH_NAME[mc.which], T1[mc.which], th->caps[mc.which * 2 % 5]);
}
