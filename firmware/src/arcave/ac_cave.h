/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's cave: the hub, a platforming one. Rock rolls in on the ground floor, past the dark jukebox, to the
 * Hat; the three arcade machines stand on ledges above. Until the jukebox is mended the only light is the
 * mushrooms' and the only sound the hum and Rock's own. Also here: the banner that slides in from the top edge
 * and the dialogue box, which every scene may use. */

/* ---------------------------------------------------------------- the banner from the top edge */
static struct { const char *a, *b; int32_t t; uint8_t on, note; } banner;
static void banner_show(const char *small, const char *big, int note)
{
    banner.a = small; banner.b = big; banner.t = 0; banner.on = 1; banner.note = (uint8_t)note;
}
static void banner_draw(int32_t dt)
{
    int32_t y, w, t, k, sa = ts(1, 2), sb = ts(2, 4), tw = banner.note ? 196 : 220, ha, hb, h;
    if (!banner.on) return;
    banner.t += dt;
    t = banner.t;
    if (t > 2900) { banner.on = 0; return; }
    while (sb > 2 && gtextw(banner.b, sb) > tw) sb--;            /* a size down before it takes a second line */
    ha = banner.a[0] ? TLH(sa) : 0; hb = gblock_h(tw, banner.b, sb); h = ha + hb + 8;
    y = t < 260 ? -h + h * ease8(t * 256 / 260) / 256 : t > 2640 ? -h * ease8((t - 2640) * 256 / 260) / 256 : 0;
    w = hb > TLH(sb) ? 236 : imin(236, imax(gtextw(banner.b, sb), gtextw(banner.a, sa)) + (banner.note ? 48 : 24));
    grect(120 - w / 2, y, w, h, th->panel);
    for (k = 0; k < 5; k++)                      /* a seam of the five colours along its lower edge, running */
        grect(120 - w / 2 + ((k * w / 5 + t / 12) % w), y + h - 2, w / 5 - 2, 2, th->caps[k]);
    gbox(120 - w / 2, y - 1, w, h + 2, th->rim);
    if (banner.note) gtext(120 - w / 2 + 8, y + h / 2 - 6 + (tsin((uint32_t)t, 80, 0) >> 13), "\xE2\x99\xAA", th->caps[(t / 300) % 5], 2);
    if (ha) gfit(120 + (banner.note ? 10 : 0), y + 2 + 2 * sa, tw, banner.a, th->mute, sa);
    gblock(120 - tw / 2 + (banner.note ? 10 : 0), y + 3 + ha, tw, banner.b, th->text, sb, 1);
}

/* ---------------------------------------------------------------- the dialogue box
 * It keeps which line it is saying, not the words: when the language changes mid-sentence the box says the same
 * page in the new one. A page too long for the box (larger text) is shown a boxful at a time. */
static struct { int32_t shown, total, t, rest; uint16_t name_id, text_id; uint8_t on, top, page, lang_seen; uint16_t win; } dlg;
static char dlg_buf[220];
static void dlg_layout(int draw, int32_t y0)     /* the box's words from where it stands; fills total and rest */
{
    const char *p = TX[dlg.text_id][lang], *rest = 0;
    int32_t n = 0, k = dlg.page, sc = ts(2, 4), lines;
    while (k > 0 && *p) { if (*p == '|') k--; p++; }
    for (k = 0; k < dlg.win && *p && *p != '|'; k++) p++;
    while (*p && *p != '|' && n < 218) dlg_buf[n++] = *p++;
    dlg_buf[n] = 0;
    lines = gtext_wrap(12, y0, 216, dlg_buf, th->text, sc, TLH(sc), draw ? dlg.shown : -1, 0, &dlg.total, draw, sc >= 3 ? 4 : 6, &rest);
    dlg.rest = *rest ? (int32_t)(rest - dlg_buf) : 0;
    (void)lines;
}
static void dlg_open(int name_id, int text_id)
{
    dlg.name_id = (uint16_t)name_id; dlg.text_id = (uint16_t)text_id; dlg.page = 0; dlg.win = 0;
    dlg.shown = 0; dlg.t = 0; dlg.on = 1; dlg.lang_seen = lang;
    dlg_layout(0, 0);
}
/* returns 1 while the box is open (the scene under it should hold still) */
static int dlg_tick(int32_t dt)
{
    int32_t was;
    if (!dlg.on) return 0;
    if (dlg.lang_seen != lang) { dlg.lang_seen = lang; dlg.win = 0; dlg.t = 100000; }   /* the same page, in the new language, whole */
    dlg_layout(0, 0);
    dlg.t += dt;
    was = dlg.shown;
    dlg.shown = imin(dlg.total, dlg.t * 34 / 1000);
    if (dlg.shown > was && !(dlg.shown & 1) && dlg.t < 90000) sfx(57 + (int32_t)(ihash((uint32_t)(dlg.shown + dlg.total * 31)) * 5u >> 16) * 2, 0, 70, W_SQUARE, 1300, 2, 30);
    if (BDOWN(BTN_PLAY) || (in.kdown && dlg.shown >= dlg.total)) {
        if (dlg.shown < dlg.total) dlg.t = 100000;
        else if (dlg.rest) { dlg.win = (uint16_t)(dlg.win + dlg.rest); dlg.t = 0; dlg.shown = 0; snd_ui(72); }
        else {
            const char *p = TX[dlg.text_id][lang];
            int32_t k = dlg.page + 1;
            while (k > 0 && *p) { if (*p == '|') k--; p++; }
            if (k == 0) { dlg.page++; dlg.win = 0; dlg.t = 0; dlg.shown = 0; snd_ui(72); }
            else { dlg.on = 0; snd_ui(67); }
        }
    }
    if (dlg.shown >= dlg.total) led_play_breathe = 1;
    return 1;
}
static void dlg_draw(void)
{
    int32_t sc = ts(2, 4), sn = ts(1, 2), lines, h, y;
    if (!dlg.on) return;
    dlg_layout(0, 0);
    lines = gtext_wrap(0, 0, 216, dlg_buf, 0, sc, TLH(sc), -1, 0, 0, 0, sc >= 3 ? 4 : 6, 0);
    h = 6 + TLH(sn) + 2 + lines * TLH(sc) + 6;
    y = dlg.top ? 4 : 236 - h;
    grect(4, y, 232, h, th->panel);
    gbox(4, y, 232, h, th->rim);
    gtext(12, y + 5 + 2 * sn, TR(dlg.name_id), th->caps[3], sn);
    dlg_layout(1, y + 6 + TLH(sn) + 2 + 2 * sc);
    if (dlg.shown >= dlg.total && ipulse((uint32_t)dlg.t, 900) > 128) gtextr(228, y + 5 + 2 * sn, dlg.rest ? "PLAY \xE2\x86\x93" : "PLAY \xE2\x86\x92", th->pad, sn);
}

/* ---------------------------------------------------------------- the cave */
#define CV_W 880
#define CV_GROUND 400
typedef struct { int16_t x0, x1, y; uint8_t kind; } cv_plat_t;   /* kind 0: a ledge; 1: a springy cap */
static const cv_plat_t CV_PLAT[] = {
    {0, CV_W, CV_GROUND, 0},
    {640, 830, 344, 0},                          /* machine 1 */
    {372, 560, 288, 0},                          /* machine 2 */
    {612, 800, 232, 0},                          /* machine 3 */
    {578, 606, 384, 1},                          /* a cap by the first ledge */
    {586, 612, 312, 1},                          /* one between the first and second, for the third */
    {14, 132, 190, 0},                           /* high on the left wall: only the booster reaches it */
    {788, 866, 176, 0},                          /* high on the right wall */
    {98, 126, 178, 1},                           /* a cap up there, toward the chimes */
};
/* both ends of the floor curve up into ramps, with a booster on each: roll out over it and it throws you up the wall */
static int32_t cave_floor_q8(int32_t x)          /* the ground's height under x (Q8) */
{
    int32_t d = x < Q8(110) ? Q8(110) - x : x > Q8(CV_W - 110) ? x - Q8(CV_W - 110) : 0;
    return Q8(CV_GROUND) - ((d * d) >> 8) / 80;
}
#define CV_NCHIME 5
#define CHIME_X(i) (176 + (i) * 34)
#define CHIME_Y(i) (118 + ((i) & 1) * 16)
#define CV_FRIEND_X 44
#define CV_RING_X 746
#define CV_RING_Y 116
#define CV_NPLAT (sizeof CV_PLAT / sizeof CV_PLAT[0])
typedef struct { int16_t x, y; uint8_t s, c; } cv_shroom_t;
static const cv_shroom_t CV_SHROOM[] = {
    {100, 400, 90, 3}, {118, 400, 50, 0}, {214, 400, 120, 1}, {262, 400, 60, 4}, {340, 400, 80, 2}, {430, 400, 150, 0},
    {470, 400, 70, 3}, {700, 400, 100, 4}, {776, 400, 140, 1}, {662, 344, 70, 2}, {812, 344, 90, 0}, {392, 288, 80, 4},
    {536, 288, 60, 1}, {634, 232, 70, 3}, {782, 232, 100, 2}, {122, 190, 50, 1}, {854, 176, 50, 4},
};
#define CV_NSHROOM (sizeof CV_SHROOM / sizeof CV_SHROOM[0])
#define CV_JUKE_X 300
#define CV_HAT_X 392
static const int16_t CV_MACH[3][2] = {{740, 344}, {466, 288}, {706, 232}};
static const char *const MACH_NAME[3] = {"MYSTIC RUN", "THE LIGHT TABLE", "NIGHT BLOOM"};

enum { CVA_NONE, CVA_LAMP, CVA_M1, CVA_M2, CVA_M3 };
static struct {
    int32_t x, y, vx, vy, th, coy, drag, acc, camx, camy, t, fade, roll_px, sq[5], psst, power_t, boost_t, chime_t[CV_NCHIME], ring_t;
    uint8_t secrets, chimes, friend_awake;       /* secrets: bit 0 the sleeper, 1 the chimes, 2 the ring */
    uint8_t g, inited, met, near;                /* near: 0 nothing, 1 the Hat, 2 the jukebox, 3..5 a machine */
} cv;
static uint8_t story;                            /* 0 the intro, 1 in the cave, 2 asked to mend the jukebox, 3 the light is back, 4 power */

static void cave_enter(int fresh)
{
    if (!cv.inited || fresh) { cv.x = Q8(26); cv.y = cave_floor_q8(Q8(26)) - Q8(9); cv.vx = 0; cv.vy = 0; cv.th = 0; cv.inited = 1; cv.camx = 0; cv.camy = Q8(CV_GROUND - 250); cv.psst = 0; }
    cv.fade = 256; cv.drag = 0; cv.t = 0;
    if (story >= 2) cv.met = 1;                  /* the Hat has said its piece: no calling from off the screen */
    sy_drone_want = story >= 4 ? 0 : 150;
    sy_music_want = 256;
    dlg.top = 1;
}

static void cave_secret(int bit)
{
    static char n[4] = "0/3";
    if (cv.secrets >> bit & 1u) return;
    cv.secrets |= (uint8_t)(1u << bit);
    n[0] = (char)('0' + (cv.secrets & 1u) + (cv.secrets >> 1 & 1u) + (cv.secrets >> 2 & 1u));
    banner_show(TR(T_SECRET), n, 0);
    snd_chime(1);
}

static int32_t cave_light(int32_t x, int32_t y)  /* 0..256: how much mushroom light falls here */
{
    uint32_t i;
    int32_t l = 0;
    if (story >= 4) return 256;
    for (i = 0; i < CV_NSHROOM; i++) {
        int32_t dx = iabs(x - CV_SHROOM[i].x), dy = iabs(y - (CV_SHROOM[i].y - 10)), d = imax(dx, dy) + imin(dx, dy) / 2, r = 26 + CV_SHROOM[i].s / 2;
        if (d < r) l += (r - d) * 300 / r;
    }
    return iclamp(l, 22, 256);
}

/* returns what PLAY asked for: CVA_... */
static int cave_tick(int32_t dt)
{
    int32_t knob, jump = -1, first = 1, hx = cv.x >> 8, hy = cv.y >> 8, i;
    int act = CVA_NONE;
    cv.t += dt;
    cv.fade = imax(0, cv.fade - dt * 256 / 1200);
    if (cv.power_t > 0) cv.power_t = imax(0, cv.power_t - dt);
    if (dlg_tick(dt)) { sy_rumble_want = 0; return CVA_NONE; }
    /* every knob but ALGORITHM rolls: that one is the jukebox's */
    i = in.any_knob - in.enc[ENC_ALGORITHM];
    if (i) cv.drag = iclamp(cv.drag + i * 115, -384, 384);
    knob = iclamp(cv.drag, -256, 256);
    cv.drag -= cv.drag * imin(dt, 100) / 250;
    if (in.hikey >= 0) jump = in.hikey * 256 / 26;
    cv.acc += dt * 128;
    while (cv.acc >= 1000) {
        int32_t py = cv.y, rad = rock_rad(cv.th), bot, a;
        uint32_t k;
        cv.acc -= 1000;
        if (cv.g) {
            a = knob * TQ;
            if ((knob > 0 && cv.vx > 0) || (knob < 0 && cv.vx < 0)) a = (a * imax(0, 256 - iabs(cv.vx) * 256 / Q8(170))) >> 8;
            a += -cv.vx * 2 / 5 - (iabs(cv.vx) > 512 ? (cv.vx > 0 ? Q8(8) : -Q8(8)) : 0);
            if (cv.y > Q8(CV_GROUND - 130)) {    /* on a ramp the slope pulls you back in */
                int32_t px = cv.x >> 8;
                if (px < 110) a += GR * (110 - px) / 56; else if (px > CV_W - 110) a -= GR * (px - (CV_W - 110)) / 56;
                /* the booster: over it on the way out and it throws you up the wall */
                if ((px > 58 && px < 84 && cv.vx < -Q8(50)) || (px > CV_W - 84 && px < CV_W - 58 && cv.vx > Q8(50))) {
                    cv.vy = px < 200 ? -Q8(474) : -Q8(492); cv.vx = px < 200 ? -Q8(30) : Q8(30); cv.g = 0; cv.coy = 0; cv.boost_t = 500; a = 0;
                    snd_whoosh(); sfx_ex(50, 0, 500, W_SAW, 2600, 3, 200, 0, 0, 65750);
                }
            }
            cv.vx += a >> 7;
        } else cv.vx += (knob * 160) >> 7;       /* a little steering in the air */
        cv.vx = iclamp(cv.vx, -Q8(190), Q8(190));
        cv.vy += GR >> 7;
        if (cv.g) cv.coy = 13; else if (cv.coy > 0) cv.coy--;
        if (first && jump >= 0 && cv.coy > 0) {
            cv.vy = -Q8(JMIN) - (345 - JMIN) * jump; cv.g = 0; cv.coy = 0;
            sfx(key_note(in.hikey), 0, 700, W_TRI, 3600, 6, 200);
        }
        first = 0;
        cv.x += cv.vx >> 7; cv.y += cv.vy >> 7;
        if (cv.x < Q8(14)) { cv.x = Q8(14); cv.vx = imax(0, cv.vx); }
        if (cv.x > Q8(CV_W - 14)) { cv.x = Q8(CV_W - 14); cv.vx = imin(0, cv.vx); }
        bot = cv.y + rad;
        cv.g = 0;
        for (k = 0; k < CV_NPLAT; k++) {
            const cv_plat_t *p = &CV_PLAT[k];
            int32_t top = k == 0u ? cave_floor_q8(cv.x) : Q8(p->y);
            if (cv.vy < 0 || cv.x < Q8(p->x0 - 5) || cv.x > Q8(p->x1 + 5)) continue;
            if (k == 0u ? bot >= top : (py + rad <= top + 256 && bot >= top)) {
                cv.y = top - rad;
                if (p->kind) {                   /* a cap throws you up, and sings */
                    cv.vy = -Q8(368); cv.sq[k - 4u] = 250;
                    sfx(60 + (k == 4 ? 7 : 12), 0, 800, W_SINE, 3800, 4, 220); sfx(72 + (k == 4 ? 7 : 12), 60, 600, W_SINE, 2000, 4, 220);
                } else {
                    if (cv.vy > Q8(150)) { sfx_ex(38, 0, 200, W_SINE, 3600, 2, 200, 0, 0, 64600); sfx(96, 0, 60, W_NOISE, 500, 1, 160); }
                    cv.vy = 0; cv.g = 1;
                }
                break;
            }
        }
        cv.th += ((cv.vx * 1372) >> 8) >> 7;
        if (cv.g) {
            cv.roll_px += iabs(cv.vx) >> 7;
            if (cv.roll_px > Q8(44)) {           /* pebbles under the rock, off the walls */
                cv.roll_px = 0;
                sfx(45 + (int32_t)(ac_rnd() % 5u) * 2, 0, 90, W_TRI, 1500, 1, 230);
            }
        }
    }
    sy_rumble_want = cv.g ? imin(60, iabs(cv.vx) >> 9) : 0;
    sy_rumble_pitch = 70 + (iabs(cv.vx) >> 9);
    for (i = 0; i < 5; i++) if (cv.sq[i] > 0) cv.sq[i] = imax(0, cv.sq[i] - dt);
    if (cv.boost_t > 0) cv.boost_t -= dt;
    hx = cv.x >> 8; hy = cv.y >> 8;
    /* high up, where only boosting and bouncing reach: a sleeper, a row of chimes, a ring */
    if (iabs(hx - CV_FRIEND_X) < 34 && iabs(hy - 182) < 24) {
        if (!cv.friend_awake) { cv.friend_awake = 1; sfx(67, 0, 300, W_TRI, 2800, 4, 200); sfx(72, 140, 300, W_TRI, 2800, 4, 200); sfx(76, 280, 600, W_TRI, 2800, 4, 200); }
        cave_secret(0);
    }
    for (i = 0; i < CV_NCHIME; i++) {
        if (cv.chime_t[i] > 0) cv.chime_t[i] -= dt;
        else if (iabs(hx - CHIME_X(i)) < 10 && iabs(hy - CHIME_Y(i)) < 15) {
            cv.chime_t[i] = 450; cv.chimes |= (uint8_t)(1u << i);
            sfx(84 + SC_PEN[i], 0, 1600, W_SINE, 3400, 2, 230); sfx(96 + SC_PEN[i], 0, 700, W_SINE, 1200, 2, 230);
            if (cv.chimes == (1u << CV_NCHIME) - 1u) cave_secret(1);
        }
    }
    if (cv.ring_t > 0) cv.ring_t -= dt;
    else if ((hx - CV_RING_X) * (hx - CV_RING_X) + (hy - CV_RING_Y) * (hy - CV_RING_Y) < 300) {
        cv.ring_t = 3000; cv.vy = imin(cv.vy, -Q8(230)); snd_whoosh();
        cave_secret(2);
    }
    /* the camera keeps Rock a third of the way up, and never shows past the walls */
    {
        int32_t tx = iclamp(cv.x - Q8(120) + cv.vx / 3, 0, Q8(CV_W - 240)), ty = iclamp(cv.y - Q8(176), Q8(CV_GROUND - 372), Q8(CV_GROUND - 214));
        cv.camx += (tx - cv.camx) * imin(160, dt) / 160;
        cv.camy += (ty - cv.camy) * imin(240, dt) / 240;
    }
    /* someone calls from off the screen until you come */
    if (!cv.met) {
        cv.psst += dt;
        if (cv.psst > 1500 && (cv.psst % 2600) < dt && hx < CV_HAT_X - 130) { sfx(69, 0, 90, W_SQUARE, 1500, 2, 220); sfx(74, 110, 160, W_SQUARE, 1500, 2, 220); }
        if (hx > CV_HAT_X - 46 && cv.g) {
            cv.met = 1; cv.vx = 0;
            if (story < 2) { story = 2; dlg_open(T_HAT, T_HAT_HELLO); }
        }
    }
    /* what is in reach */
    cv.near = 0;
    if (iabs(hx - CV_HAT_X) < 40 && hy > CV_GROUND - 40) cv.near = 1;
    if (iabs(hx - CV_JUKE_X) < 30 && hy > CV_GROUND - 40) cv.near = 2;
    for (i = 0; i < 3; i++) if (iabs(hx - CV_MACH[i][0]) < 26 && iabs(hy - (CV_MACH[i][1] - 8)) < 22) cv.near = (uint8_t)(3 + i);
    if (cv.near) led_play_breathe = 1;
    if (BDOWN(BTN_PLAY) && cv.near) {
        if (cv.near == 1) dlg_open(T_HAT, story < 4 ? T_HAT_REMIND : T_HAT_FREE);
        else if (cv.near == 2) { if (story < 4) act = CVA_LAMP; else snd_ui(79); }
        else if (story >= 4) act = CVA_M1 + cv.near - 3;
        else { banner_show("", TR(T_NOPOWER), 0); sfx_ex(43, 0, 260, W_SAW, 2400, 2, 120, 0, 0, 65300); }
    }
    led_key_want |= in.keys;
    return act;
}

static void cave_cabinet(int32_t x, int32_t y, int which, int32_t lit, uint32_t tm)
{
    col_t c = th->caps[which * 2 % 5];
    int32_t k;
    galpha(lit);
    grect(x - 14, y - 44, 28, 44, th->panel);
    gbox(x - 14, y - 44, 28, 44, story >= 4 ? c : th->rim);
    gbox(x - 16, y - 50, 32, 8, story >= 4 ? c : th->rim);          /* the marquee */
    gbox(x - 10, y - 38, 20, 16, th->rim);                          /* the screen */
    grect(x - 9, y - 17, 18, 2, th->rim);                           /* the panel's edge */
    for (k = 0; k < 3; k++) grect(x - 7 + k * 6, y - 13, 2, 2, th->rim);
    if (story >= 4) {
        galpha(256);
        grect(x - 15, y - 49, 30, 6, c);
        if (which == 0) for (k = 0; k < 4; k++) grect(x - 8 + ((k * 5 + (int32_t)(tm / 90u)) % 17), y - 26 - (k & 1) * 4, 2, 3, th->caps[k]);
        else if (which == 1) for (k = 0; k < 3; k++) garc(x, y - 30, 2 + k * 2, 2 + k * 2, (int32_t)(tm * (8u + (uint32_t)k * 5u)), (int32_t)(tm * (8u + (uint32_t)k * 5u)) + 40000, th->caps[k + 1], 0);
        else for (k = 0; k < 4; k++) { int32_t a = (int32_t)(tm * 9u) + k * 16384; gp(x + ((6 * icos(a)) >> 14), y - 30 + ((6 * isin(a)) >> 14), th->caps[k]); gp(x, y - 30, th->pad); }
        galpha(60 + (tsin(tm, 30, which * 20000) >> 9)); gdisc(x, y - 30, 22, c);
    }
    galpha(256);
}

static void cave_draw(void)
{
    int32_t cx = cv.camx >> 8, cy = cv.camy >> 8, i, k, lit;
    uint32_t tm = (uint32_t)cv.t;
    int on = story >= 4;
#define SX(x) ((x) - cx)
#define SY(y) ((y) - cy)
    gclear(th->bg);
    /* the roof: a ragged line with teeth, lit only where a mushroom is near (or by the jukebox, once it plays) */
    for (i = -1; i < 14; i++) {
        int32_t wx = ((cx / 20) + i) * 20, h0 = 20 + (int32_t)(ihash((uint32_t)(wx / 20 + 900)) * 16u >> 16), h1 = 20 + (int32_t)(ihash((uint32_t)(wx / 20 + 901)) * 16u >> 16);
        int32_t ry = CV_GROUND - 392;
        galpha(on ? 140 : 40);
        gline(SX(wx), SY(ry + h0), SX(wx + 20), SY(ry + h1), th->rim);
        if (ihash((uint32_t)(wx / 20 + 77)) > 40000u) {
            int32_t l = 8 + (int32_t)(ihash((uint32_t)(wx / 20 + 5)) * 22u >> 16);
            gline(SX(wx + 4), SY(ry + h0), SX(wx + 9), SY(ry + h0 + l), th->rim); gline(SX(wx + 9), SY(ry + h0 + l), SX(wx + 14), SY(ry + h1), th->rim);
        }
    }
    galpha(256);
    /* the mushrooms and their light */
    for (i = 0; i < (int32_t)CV_NSHROOM; i++) {
        const cv_shroom_t *m = &CV_SHROOM[i];
        int32_t glow = 128 + (tsin(tm, 12, i * 9000) >> 7);
        if (SX(m->x) < -60 || SX(m->x) > 300) continue;
        galpha(14 + glow / 16); gdisc(SX(m->x), SY(m->y - 10), 18 + m->s / 4, th->caps[m->c]);
        galpha(256);
        mush_draw(SX(m->x), SY(m->y == CV_GROUND ? cave_floor_q8(m->x << 8) >> 8 : m->y), m->s, th->caps[m->c], th->stem, glow);
    }
    /* the floors */
    for (i = 0; i < (int32_t)CV_NPLAT; i++) {
        const cv_plat_t *p = &CV_PLAT[i];
        int32_t x;
        if (p->kind) {                           /* a springy cap on a thin stalk */
            int32_t q = cv.sq[i - 4] * 256 / 250, mx = (p->x0 + p->x1) / 2, ry = 9 * (256 - q * 4 / 10) / 256, base = i == 4 ? CV_GROUND : i == 5 ? 344 : 190;
            col_t c = th->caps[i == 4 ? 2 : i == 5 ? 0 : 3];
            if (i == 5) { grect(SX(mx - 1), SY(p->y), 2, 14, th->stem); gline(SX(mx), SY(p->y + 14), SX(640), SY(base), th->stem); }
            else grect(SX(mx - 1), SY(p->y), 3, base - p->y, th->stem);
            galpha(60); gdisc(SX(mx), SY(p->y), 20, c); galpha(256);
            gellf(SX(mx), SY(p->y + 9 - ry + q * 3 / 256), (p->x1 - p->x0) / 2 * (256 + q * 3 / 10) / 256, ry, c, 1);
            continue;
        }
        if (i == 0) {                            /* the ground: flat, and curving up at both ends */
            for (x = imax(p->x0, cx - 4) & ~3; x < imin(p->x1, cx + 244); x += 4) {
                int32_t y0 = cave_floor_q8(x << 8) >> 8, y1 = cave_floor_q8((x + 4) << 8) >> 8;
                lit = cave_light(x, y0);
                galpha(lit);
                gline(SX(x), SY(y0), SX(x + 4), SY(y1), th->rim);
                for (k = 0; k < 3; k++)
                    if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(lit * (150 - k * 40) / 256); gline(SX(x), SY(y0 + 3 + k * 4), SX(x + 4), SY(y1 + 3 + k * 4), th->bands[k % 5]); }
            }
            for (k = 0; k < 2; k++) {            /* the boosters: three chevrons running up each ramp */
                int32_t j;
                for (j = 0; j < 3; j++) {
                    int32_t bx = k ? CV_W - 82 + j * 9 : 82 - j * 9, by = (cave_floor_q8(bx << 8) >> 8) - 4, d = k ? 3 : -3;
                    galpha(cv.boost_t > 0 ? 256 : 90 + ((((int32_t)(tm / 120u) + 3 - j) % 3) == 0 ? 166 : 0));
                    gline2(SX(bx - d), SY(by - 4), SX(bx), SY(by), th->pad); gline2(SX(bx), SY(by), SX(bx - d), SY(by + 3), th->pad);
                }
            }
            galpha(256);
            continue;
        }
        for (x = imax(p->x0, cx - 4); x < imin(p->x1, cx + 244); x += 4) {
            lit = cave_light(x, p->y);
            galpha(lit);
            ghl(SX(x), SX(x + 4), SY(p->y), th->rim);
            for (k = 0; k < 3; k++)
                if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(lit * (150 - k * 40) / 256); ghl(SX(x), SX(x + 4), SY(p->y + 3 + k * 4), th->bands[(k + i) % 5]); }
        }
        if (i) {                                 /* a ledge's ends hang as rock */
            galpha(cave_light(p->x0, p->y)); gline(SX(p->x0), SY(p->y), SX(p->x0 + 10), SY(p->y + 16), th->rim);
            galpha(cave_light(p->x1, p->y)); gline(SX(p->x1), SY(p->y), SX(p->x1 - 10), SY(p->y + 16), th->rim);
        }
    }
    galpha(256);
    if (on) {                                    /* strings of lights from floor to floor */
        static const int16_t ST[3][4] = {{330, 300, 640, 320}, {560, 262, 640, 300}, {560, 250, 612, 208}};
        for (i = 0; i < 3; i++)
            for (k = 0; k <= 8; k++) {
                int32_t x = ST[i][0] + (ST[i][2] - ST[i][0]) * k / 8, y = ST[i][1] + (ST[i][3] - ST[i][1]) * k / 8 + (k * (8 - k)) * 3 / 4;
                col_t c = th->caps[(k + i + (int32_t)(tm / 400u)) % 5];
                galpha(70); gdisc(SX(x), SY(y), 4, c); galpha(256); grect(SX(x) - 1, SY(y) - 1, 2, 2, c);
            }
    }
    /* high up: the sleeper, the chimes hanging from the roof, the ring */
    rock_draw(SX(CV_FRIEND_X), SY(190 - 6), 210, 0, cv.friend_awake ? ((tm % 2900u) < 150u ? 30 : 256) : 0, th->rock, th->rock_rim, th->dark, th->caps[2], th->eye, th->pupil);
    if (!cv.friend_awake) gtext(SX(CV_FRIEND_X + 8) + (int32_t)((tm / 500u) % 3u) * 3, SY(168) - (int32_t)((tm / 500u) % 3u) * 4, "Z", th->mute, 1);
    for (i = 0; i < CV_NCHIME; i++) {
        int32_t x = SX(CHIME_X(i)), y = SY(CHIME_Y(i)), on_ = cv.chime_t[i] > 0, xy[8];
        col_t c = th->caps[i % 5];
        galpha(on_ ? 256 : (cv.chimes >> i & 1) ? 150 : 70);
        gline(x, SY(CV_GROUND - 372), x + (on_ ? (tsin(tm, 300, 0) >> 13) : 0), y - 7, th->stem);
        xy[0] = x; xy[1] = y - 7; xy[2] = x + 4; xy[3] = y; xy[4] = x; xy[5] = y + 8; xy[6] = x - 4; xy[7] = y;
        gpoly(xy, 4, th->dark ? 0 : th->panel); gpolyline(xy, 4, 1, c);
        if (on_) { galpha(cv.chime_t[i] * 200 / 450); gdisc(x, y, 12, c); }
    }
    galpha(256);
    if (cv.ring_t <= 0) {
        garc(SX(CV_RING_X), SY(CV_RING_Y), 11, 11, 0, 65536, th->pad, 1);
        for (k = 0; k < 6; k++) {
            int32_t a = (int32_t)(tm * 16u) + k * 10923;
            grect(SX(CV_RING_X) + ((16 * icos(a)) >> 14) - 1, SY(CV_RING_Y) + ((16 * isin(a)) >> 14) - 1, 2, 2, th->caps[k % 5]);
        }
    }
    /* the jukebox */
    {
        int32_t x = SX(CV_JUKE_X), y = SY(CV_GROUND);
        lit = cave_light(CV_JUKE_X, CV_GROUND - 20);
        galpha(lit);
        grect(x - 17, y - 34, 34, 34, th->panel); gellf(x, y - 34, 17, 16, th->panel, 1);
        gline(x - 17, y, x - 17, y - 34, th->rim); gline(x + 17, y, x + 17, y - 34, th->rim); ghl(x - 17, x + 18, y - 1, th->rim);
        garc(x, y - 34, 17, 16, 32768, 65536, th->rim, 0);
        gbox(x - 11, y - 30, 22, 12, th->rim);                       /* the window where the records are */
        for (k = 0; k < 4; k++) ghl(x - 10, x + 11, y - 13 + k * 3, th->rim);   /* the grille */
        galpha(256);
        if (on) {
            for (k = 0; k < 4; k++) garc(x, y - 34, 14 - k * 3, 13 - k * 3, 32768, 65536, th->caps[(k + (int32_t)(tm / 250u)) % 5], 1);
            grect(x - 10, y - 29, 20, 10, th->caps[(tm / 500u) % 5u]);
            galpha(50 + (tsin(tm, 40, 0) >> 9)); gdisc(x, y - 24, 40, th->caps[(tm / 500u) % 5u]); galpha(256);
            for (k = 0; k < 3; k++) {            /* notes float off it */
                int32_t q = (int32_t)((tm / 3u + (uint32_t)k * 333u) % 1000u);
                galpha(256 - q * 256 / 1000);
                gtext(x - 12 + k * 10 + (tsin(tm, 20, k * 20000) >> 12), y - 50 - q * 30 / 1000, "\xE2\x99\xAA", th->caps[(k * 2) % 5], 1);
            }
            galpha(256);
        } else if (cv.power_t == 0 && (tm % 3100u) < 90u) { galpha(200); gline(x - 3, y - 26, x + 2, y - 22, th->pad); gline(x + 2, y - 22, x - 1, y - 20, th->pad); galpha(256); }   /* a spark */
    }
    for (i = 0; i < 3; i++) cave_cabinet(SX(CV_MACH[i][0]), SY(CV_MACH[i][1]), i, cave_light(CV_MACH[i][0], CV_MACH[i][1] - 20), tm);
    /* the Hat, a little off the ground, watching */
    {
        int32_t x = SX(CV_HAT_X), y = SY(CV_GROUND - 22) + (tsin(tm, 16, 0) >> 12);
        int32_t bl = (tm % 4300u) < 180u ? iabs((int32_t)(tm % 4300u) - 90) * 256 / 90 : 256;
        galpha(34); gdisc(x, y - 6, 30, th->pad); galpha(256);
        hat_draw(x, y, 120, th->dark ? HEXC(0x0c1216) : th->hat, th->hat_eye, th->hat_pupil, bl, iclamp(((cv.x >> 8) - CV_HAT_X) * 256 / 70, -256, 256), th->dark, th->hat_rim, 256);
    }
    rock_draw(SX(cv.x >> 8), SY(cv.y >> 8), 256, cv.th, (tm % 3700u) < 160u ? iabs((int32_t)(tm % 3700u) - 80) * 256 / 80 : 256,
              th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    /* words */
    if (!cv.met && cv.psst > 1200 && (cv.x >> 8) < CV_HAT_X - 130) {
        int32_t a = 120 + ipulse(tm, 1300) / 2, sc = ts(1, 3), w, h;
        while (sc > 1 && gtextw(TR(T_PSST), sc) + 12 + 8 * sc > 236) sc--;
        w = gtextw(TR(T_PSST), sc) + 12 + 8 * sc; h = 5 * sc + 10;
        galpha(a);
        grect(238 - w, 150, w, h, th->panel); gbox(238 - w, 150, w, h, th->pad);
        gtext(244 - w, 155, TR(T_PSST), th->text, sc);
        gtext(238 - 7 * sc - 2 + (tsin(tm, 60, 0) >> 13), 155, "\xE2\x86\x92", th->pad, sc);
        galpha(256);
    }
    if (cv.near && !dlg.on) {
        const char *s = cv.near == 1 ? TR(T_TALK) : cv.near == 2 ? (on ? "" : TR(T_FIX)) : TR(T_START);
        int32_t px = SX(cv.x >> 8), py = SY(cv.y >> 8) - 30;
        if (cv.near >= 3) gfit(SX(CV_MACH[cv.near - 3][0]), SY(CV_MACH[cv.near - 3][1]) - 58 - 5 * ts(1, 3), 232, MACH_NAME[cv.near - 3], on ? th->caps[(cv.near - 3) * 2 % 5] : th->mute, ts(1, 3));
        if (cv.near >= 3 && !on) s = TR(T_NOPOWER);
        galpha(160 + ipulse(tm, 1400) * 96 / 256);
        gfit(px, py + 6 - 6 * ts(1, 3), 232, s, th->text, ts(1, 3));
        galpha(256);
    }
    if (cv.power_t > 0) { galpha(cv.power_t * 256 / 1600); grect(0, 0, 240, 240, th->pad); galpha(256); }
    dlg_draw();
    gfade(cv.fade);
#undef SX
#undef SY
}
