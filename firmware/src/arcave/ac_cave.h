/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's cave: the hub, a platforming one. Rock rolls in on the ground floor, past the dark jukebox, to the
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
    if (dlg.shown >= dlg.total) {                /* the page is all there: the PLAY button breathes, as the one on the panel does */
        galpha(56 + ipulse((uint32_t)dlg.t, 2400) * 200 / 256);
        gtextr(228, y + 5 + 2 * sn, dlg.rest ? PB " \xE2\x86\x93" : PB " \xE2\x86\x92", th->pad, sn);
        galpha(256);
    }
}

/* ---------------------------------------------------------------- the cave */
#define CV_W 880
#define CV_GROUND 400
#define CV_XL (-240)                             /* the left wall of the room up on the left */
#define CV_DOOR_X (CV_XL + 22)                   /* the door outside, in that wall: the way out is through the gift shop */
#define CV_SHOP_X (-104)                         /* the gift shop, a stall in that room: the upgrades (ac_shop.h) */
typedef struct { int16_t x0, x1, y; uint8_t kind; } cv_plat_t;   /* kind 0: a ledge; 1: a springy cap */
static const cv_plat_t CV_PLAT[] = {
    {0, CV_W, CV_GROUND, 0},
    {640, 830, 344, 0},                          /* machine 1 */
    {372, 560, 288, 0},                          /* machine 2 */
    {612, 800, 232, 0},                          /* machine 3 */
    {578, 606, 384, 1},                          /* a cap by the first ledge */
    {586, 612, 312, 1},                          /* one between the first and second, for the third */
    {CV_XL, 132, 190, 0},                        /* high on the left wall: only the booster reaches it; further in is
                                                    a room with the gift shop and, past it, the door back outside */
    {788, 866, 176, 0},                          /* high on the right wall */
    {98, 126, 178, 1},                           /* a cap up there, toward the chimes */
    {866, 1112, 176, 2},                         /* (mended) the tunnel out of the right wall, off the end of the high
                                                    right ledge: level at its mouth, then down to the music room's door */
};
#define CV_TUN 9                                 /* (its place in the list) */
#define CV_TUN_X1 1112
#define CV_TUN_H 46                              /* the tunnel's height */
#define CV_TDOOR_X 1098                          /* the music room's door, in the tunnel's end */
static int32_t cave_tun_q8(int32_t x) { return Q8(176) + iclamp(x - Q8(884), 0, Q8(186)) * 60 / 186; }   /* the tunnel's floor under x (Q8) */
#define CV_JUKE_X 522                            /* the jukebox, on the ground past the Hat */
#define CV_DIP_W 26                              /* the dip it stands in: half its width, and its depth */
#define CV_DIP_D 4                               /* (the same slight dip as each machine's on its ledge, and the well's) */
#define CV_MDIP_D CV_DIP_D                       /* the dip each machine stands in, on its ledge */
#define CV_WELL_X 250                            /* the well down to UNDERSONG, the game for two (ac_duo.h), in the floor before the Hat */
#define CV_WELL_Y (CV_GROUND + CV_DIP_D)         /* (the ground at its middle, down in its dip) */
static uint8_t story;                            /* (below: the well and its dip are there only once the jukebox is mended, 4) */
static int32_t cave_dip_q8(int32_t x, int32_t mid, int32_t dep)   /* how far a dip of depth dep at mid lowers x (Q8) */
{
    int32_t j = iabs(x - Q8(mid)), u;
    if (j >= Q8(CV_DIP_W)) return 0;
    u = 256 - ((j / CV_DIP_W) * (j / CV_DIP_W) >> 8);
    return (Q8(dep) * u >> 8) * u >> 8;
}
/* both ends of the floor curve up into ramps, with a booster on each: roll out over it and it throws you up the wall */
static int32_t cave_floor_q8(int32_t x)          /* the ground's height under x (Q8) */
{
    int32_t d = x < Q8(110) ? Q8(110) - x : x > Q8(CV_W - 110) ? x - Q8(CV_W - 110) : 0;
    /* a shallow dip in front of the jukebox, and (mended) the well: Rock settles in them */
    return Q8(CV_GROUND) - ((d * d) >> 8) / 80 + cave_dip_q8(x, CV_JUKE_X, CV_DIP_D) + (story >= 4 ? cave_dip_q8(x, CV_WELL_X, CV_DIP_D) : 0);
}
#define CV_NCHIME 5
#define CHIME_X(i) (176 + (i) * 34)
#define CHIME_Y(i) (116 + (i) * (i) * 2)          /* an arch: the far ones hang lower */
#ifndef CV_RING_X                               /* the ring: out in the air beyond the high right ledge, too high for any hop
                                                   from below; only a big hop at full roll off the ledge's end reaches it */
#define CV_RING_X 670
#define CV_RING_Y 60
#endif
#define CV_NPLAT (sizeof CV_PLAT / sizeof CV_PLAT[0])
typedef struct { int16_t x, y; uint8_t s, c; } cv_shroom_t;
static const cv_shroom_t CV_SHROOM[] = {
    {100, 400, 90, 3}, {118, 400, 50, 0}, {214, 400, 120, 1}, {300, 400, 60, 4}, {340, 400, 80, 2}, {430, 400, 150, 0},
    {470, 400, 70, 3}, {558, 400, 90, 2}, {700, 400, 100, 4}, {776, 400, 140, 1}, {662, 344, 70, 2}, {812, 344, 90, 0}, {392, 288, 80, 4},
    {536, 288, 60, 1}, {634, 232, 70, 3}, {782, 232, 100, 2}, {122, 190, 50, 1}, {854, 176, 50, 4},
    {-30, 190, 70, 2}, {-56, 190, 40, 0},
};
#define CV_NSHROOM (sizeof CV_SHROOM / sizeof CV_SHROOM[0])
#define CV_HAT_X 392
static const int16_t CV_MACH[3][2] = {{740, 344}, {466, 288}, {706, 232}};
static const char *const MACH_NAME[3] = {"CRYSTAL ROAD", "JUICE WORKS", "NIGHT BLOOM"};
static int32_t cave_top_q8(uint32_t k, int32_t x)   /* the top of floor k under x (Q8): ledges 1..3 hold the machines
                                                       0..2, each in a slight dip as the jukebox is, to stop in front of */
{
    if (k == 0u) return cave_floor_q8(x);
    if (k <= 3u) return Q8(CV_PLAT[k].y) + cave_dip_q8(x, CV_MACH[k - 1u][0], CV_MDIP_D);
    if (k == 6u) return Q8(CV_PLAT[k].y) + cave_dip_q8(x, CV_SHOP_X, CV_MDIP_D);   /* (the gift shop stands in one too) */
    if (k == CV_TUN) return cave_tun_q8(x);
    return Q8(CV_PLAT[k].y);
}

enum { CVA_NONE, CVA_LAMP, CVA_M1, CVA_M2, CVA_M3, CVA_SCORES, CVA_EXIT, CVA_DISH, CVA_DUO, CVA_ROOM, CVA_SHOP };
#define CV_ENTER_MS 1100                         /* into the music room: Rock into the door, the camera after him */
#define CV_DIVE_HOP 520                          /* down the well: Rock hops over its rim, */
#define CV_DIVE_IN 1000                          /* drops into it, */
#define CV_DIVE_MS 1500                          /* and the camera goes in after it */
/* The well is an old one: a ring of stones, two posts and a little roof, a windlass with a crank, and a bucket on its
 * rope. PLAY by it: Rock hops into the bucket (CVW_HOP); a box says to lower him, with a knob turning in it (CVW_LOWER);
 * any knob winds the rope out, the bucket going down with only the top of Rock showing over its rim, and the camera
 * coming in on the well as it goes; once the bucket is under the rim the rope snaps (CVW_SNAP), and the camera dives
 * after him as it always has (cv.dive, from CV_DIVE_IN). REC while lowering: Rock hops back out. */
enum { CVW_NONE, CVW_HOP, CVW_LOWER, CVW_SNAP };
#define CVW_HOP_MS 560
#define CVW_SNAP_MS 900
#define CVW_STEP 12                              /* how far down a knob's click takes the bucket (of 256) */
#define CVW_ZMAX 836                             /* how close the camera is with the bucket all the way down */
#define CV_BOARD_X 824                           /* the scores board, on the small ledge high on the right wall */
#define CV_BOARD_Y 176
static struct {
    int32_t x, y, vx, vy, th, coy, drag, acc, camx, camy, t, fade, roll_px, sq[5], psst, power_t, boost_t, chime_t[CV_NCHIME], ring_t;
    int32_t dive, dive_x, dive_y;                /* dive: ms into the last of the way down (0: not diving); from where */
    int32_t wl, wt, low, zw;                     /* the well: phase (CVW_...), ms in it, how far the bucket is down (0..256), the zoom */
    uint8_t wturned;                             /* a knob has been turned at the bucket: the helper goes */
    int32_t door, enter;                         /* the music room's door: how far open (0..256); ms into going in (0: not) */
    uint8_t secrets, chimes, friend_awake;       /* secrets: bit 1 the chimes, 2 the ring, 3 the hollow behind the course's start
                                                    (bit 0 was the sleeper on the high ledge: no secret any more, he keeps the shop) */
    uint8_t g, inited, met, near;                /* near: 0 nothing, 1 the Hat, 2 the jukebox, 3..5 a machine */
    uint8_t hat_away;                            /* Rock has been away from the Hat since it last spoke */
    uint8_t at_juke, juke_ready;                 /* in front of the jukebox; it offers a new song (not until Rock has
                                                    left it once after the last one was kept) */
} cv;
static int32_t cvw_by(void) { return CV_WELL_Y - 58 + cv.low * 50 / 256; }   /* the bucket's rim (world y) */
static uint8_t story;                            /* 0 the intro, 1 in the cave, 2 asked to mend the jukebox, 3 the light is back, 4 power */

static void cave_enter(int fresh)
{
    if (!cv.inited || fresh) { cv.x = Q8(134); cv.y = cave_floor_q8(Q8(134)) - Q8(9); cv.vx = 0; cv.vy = 0; cv.th = 0; cv.inited = 1; cv.camx = 0; cv.camy = Q8(CV_GROUND - 250); cv.psst = 0; }
    cv.fade = 256; cv.drag = 0; cv.t = 0; cv.dive = 0; cv.wl = 0; cv.low = 0; cv.zw = 256; cv.enter = 0;
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
#ifdef DINGHY_HOST
    printf("%6u ms  cave secret %d found\n", fm1_ms, bit);
#endif
    n[0] = (char)('0' + (cv.secrets >> 1 & 1u) + (cv.secrets >> 2 & 1u) + (cv.secrets >> 3 & 1u));
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
    if (in.kdown) snd_keys(in.kdown, 0);         /* the keyboard is an instrument here: every key its note, in the tune's key */
    if (in.hikey >= 0) deco_key_fx(KEY_ZONE[in.hikey], cv.x, cv.y);   /* (and its colour's spark, if it wears one) */
    pfx_tick(dt);
    cv.fade = imax(0, cv.fade - dt * 256 / 1200);
    if (cv.power_t > 0) cv.power_t = imax(0, cv.power_t - dt);
    if (dlg_tick(dt)) { sy_rumble_want = 0; return CVA_NONE; }
    if (cv.wl) {                                 /* the well: into the bucket, lowered by the knobs, the rope snaps */
        int32_t fy = cave_floor_q8(Q8(CV_WELL_X)) - rock_rad(cv.th), tx, ty, ox = cv.x, k;
        cv.wt += dt;
        tx = iclamp(Q8(CV_WELL_X - 120), 0, Q8(CV_W - 240)); ty = Q8(CV_GROUND - 132);   /* the camera: the well to the middle */
        cv.camx += (tx - cv.camx) * imin(160, dt) / 160;
        cv.camy += (ty - cv.camy) * imin(300, dt) / 300;
        sy_rumble_want = 0;
        if (cv.wl == CVW_HOP) {                  /* a hop up and into the bucket */
            int32_t e = ease8(imin(256, cv.wt * 256 / CVW_HOP_MS));
            cv.x = cv.dive_x + (Q8(CV_WELL_X) - cv.dive_x) * e / 256;
            cv.y = cv.dive_y + (Q8(cvw_by() - 3) - cv.dive_y) * e / 256 - ((isin(imin(256, cv.wt * 256 / CVW_HOP_MS) * 128) * 40) >> 6);
            cv.th += (cv.x - ox) * 1372 / 256;
            if (cv.wt >= CVW_HOP_MS) { cv.wl = CVW_LOWER; cv.wt = 0; cv.th = 0; sfx(76, 0, 90, W_TRI, 2200, 2, 80); sfx(60, 30, 140, W_NOISE, 400, 2, 60); }
            return CVA_NONE;
        }
        if (cv.wl == CVW_LOWER) {
            k = in.any_knob - in.enc[ENC_ALGORITHM];
            if (k) {                             /* the rope out (or back in): a creak a click */
                int32_t was = cv.low;
                cv.low = iclamp(cv.low + k * CVW_STEP, 0, 256);
                if (cv.low != was) { cv.wturned = 1; sfx(50 + (int32_t)(cv.low >> 5), 0, 70, W_SQUARE, 900, 1, 40); sfx(96, 0, 40, W_NOISE, 300, 1, 20); }
            }
            cv.x = Q8(CV_WELL_X); cv.y = Q8(cvw_by() - 3);
            cv.zw = 256 + (CVW_ZMAX - 256) * ease8(cv.low) / 256;
            led_breathe_want |= 1u << BTN_REC;
            if (BDOWN(BTN_REC)) {                /* out again, onto the floor by the well */
                cv.wl = 0; cv.low = 0; cv.zw = 256; cv.x = Q8(CV_WELL_X + 30); cv.y = cave_floor_q8(Q8(CV_WELL_X + 30)) - rock_rad(cv.th);
                cv.vx = cv.vy = 0; cv.drag = 0; cv.g = 1; snd_ui(67);
                return CVA_NONE;
            }
            if (cv.low >= 256) {                 /* under the rim, as close as it goes: the rope snaps */
                cv.wl = CVW_SNAP; cv.wt = 0;
                sfx(96, 0, 160, W_NOISE, 3000, 1, 120); sfx_ex(79, 0, 500, W_SAW, 2600, 1, 120, 0, 0, 65000);
            }
            return CVA_NONE;
        }
        if (cv.wt >= CVW_SNAP_MS / 3 && cv.wt - dt < CVW_SNAP_MS / 3)   /* and down it goes */
            { sfx_ex(84, 0, 1300, W_SINE, 2600, 30, 230, 0, 0, 65250); sfx_ex(72, 420, 1100, W_TRI, 2000, 60, 230, 0, 0, 65200); }
        if (cv.wt >= CVW_SNAP_MS) {              /* the rest is the dive, from the camera's last push on */
            cv.wl = 0; cv.dive = CV_DIVE_IN; cv.x = Q8(CV_WELL_X); cv.y = fy + Q8(30);
        }
        return CVA_NONE;
    }
    if (cv.enter) {                              /* into the music room: a hop into the open door, the camera after it */
        int32_t e = ease8(imin(256, cv.enter * 256 / 500)), ox = cv.x;
        cv.enter += dt; cv.door = 256;
        cv.x = cv.dive_x + (Q8(CV_TDOOR_X) - cv.dive_x) * e / 256;
        cv.y = cv.dive_y + (Q8(236 - 22) - cv.dive_y) * e / 256 - ((isin(imin(256, cv.enter * 256 / 500) * 128) * 14) >> 6);
        cv.th += (cv.x - ox) * 1372 / 256 + dt * 40;
        sy_rumble_want = 0;
        if (cv.enter >= CV_ENTER_MS) { cv.enter = 0; cv.x = Q8(CV_TDOOR_X - 34); cv.y = cave_tun_q8(cv.x) - rock_rad(cv.th); cv.vx = cv.vy = 0; cv.drag = 0; cv.g = 1; return CVA_ROOM; }
        return CVA_NONE;
    }
    if (cv.dive) {                               /* down the well: a hop over the rim, the drop, the camera after it */
        int32_t fy = cave_floor_q8(Q8(CV_WELL_X)) - rock_rad(cv.th), tx, ty, ox = cv.x;
        cv.dive += dt;
        if (cv.dive < CV_DIVE_HOP) {
            int32_t e = ease8(cv.dive * 256 / CV_DIVE_HOP);
            cv.x = cv.dive_x + (Q8(CV_WELL_X) - cv.dive_x) * e / 256;
            cv.y = cv.dive_y + (fy - cv.dive_y) * e / 256 - ((isin(cv.dive * 32768 / CV_DIVE_HOP) * 26) >> 6);
        } else {
            int32_t q = imin(256, (cv.dive - CV_DIVE_HOP) * 256 / (CV_DIVE_IN - CV_DIVE_HOP));
            cv.x = Q8(CV_WELL_X); cv.y = fy + q * q * 30 / 256;
        }
        cv.th += (cv.x - ox) * 1372 / 256 + dt * 90;
        tx = iclamp(Q8(CV_WELL_X - 120), 0, Q8(CV_W - 240)); ty = Q8(CV_GROUND - 132);   /* the camera: the well's mouth to the middle */
        cv.camx += (tx - cv.camx) * imin(160, dt) / 160;
        cv.camy += (ty - cv.camy) * imin(300, dt) / 300;
        sy_rumble_want = 0;
        if (cv.dive >= CV_DIVE_MS) {             /* (Rock stands by the well again when it comes back up) */
            cv.dive = 0; cv.x = Q8(CV_WELL_X); cv.y = fy; cv.vx = cv.vy = 0; cv.drag = 0; cv.g = 1;
            return CVA_DUO;
        }
        return CVA_NONE;
    }
    /* every knob but ALGORITHM rolls: that one is the jukebox's */
    i = in.any_knob - in.enc[ENC_ALGORITHM];
    if (i) cv.drag = iclamp(cv.drag + i * 115, -384, 384);
    knob = iclamp(cv.drag, -256, 256);
    cv.drag -= cv.drag * imin(dt, 100) / 250;
    jump = in.hop;
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
        {   /* the dips in front of the jukebox, the machines and (mended) the well, all alike: they slow Rock and draw
               it to the middle, so only a firm roll goes by */
            int32_t dx, m, mx, my;
            for (m = 0; m < 6; m++) {            /* the jukebox, the three machines, the well, the gift shop */
                if (m == 4 && story < 4) continue;   /* (no well before the jukebox is mended) */
                mx = m == 0 ? CV_JUKE_X : m == 4 ? CV_WELL_X : m == 5 ? CV_SHOP_X : CV_MACH[m - 1][0];
                my = m == 0 || m == 4 ? CV_GROUND : m == 5 ? 190 : CV_MACH[m - 1][1];
                dx = (cv.x >> 8) - mx;
                if (cv.g && iabs(dx) < CV_DIP_W && iabs((cv.y >> 8) - my) < 30)
                    cv.vx -= ((GR * dx / 30 * (CV_DIP_W - iabs(dx)) / CV_DIP_W) >> 7) + cv.vx / 60;
            }
        }
        if (cv.g) cv.coy = 13; else if (cv.coy > 0) cv.coy--;
        if (first && jump >= 0 && cv.coy > 0) {
            cv.vy = -Q8(JMIN) - (345 - JMIN) * jump; cv.g = 0; cv.coy = 0;   /* (its sound: the key's own note, above) */
            deco_jump(KEY_ZONE[imax(in.hikey, 0)], cv.x, cv.y);
        }
        first = 0;
        cv.x += cv.vx >> 7; cv.y += cv.vy >> 7;
        {   /* the left wall: below the high ledge it stands at the ramp's end, above it the room goes on to its own */
            int32_t wl = cv.y + rad > Q8(190 + CV_MDIP_D) + 256 ? Q8(14) : Q8(CV_XL + 12);   /* (the ledge, down to the foot of the gift shop's dip) */
            if (cv.x < wl) { if (wl - cv.x > Q8(6)) cv.y = imin(cv.y, Q8(190) - rad); else cv.x = wl; cv.vx = imax(0, cv.vx); if (cv.x < Q8(CV_XL + 12)) cv.x = Q8(CV_XL + 12); }
        }
        if (cv.x > Q8(CV_W - 14)) {              /* the right wall; mended, the tunnel's mouth is open in it */
            int32_t tf = cave_tun_q8(cv.x);
            if (story >= 4 && cv.y + rad <= tf + Q8(3) && cv.y - rad >= tf - Q8(CV_TUN_H + 4)) {
                if (cv.x > Q8(CV_TDOOR_X - 12)) { cv.x = Q8(CV_TDOOR_X - 12); cv.vx = imin(0, cv.vx); }
                if (cv.x > Q8(CV_W - 4) && cv.y - rad < tf - Q8(CV_TUN_H)) { cv.y = tf - Q8(CV_TUN_H) + rad; cv.vy = imax(0, cv.vy); }
            } else { cv.x = Q8(CV_W - 14); cv.vx = imin(0, cv.vx); }
        }
        bot = cv.y + rad;
        cv.g = 0;
        for (k = 0; k < CV_NPLAT; k++) {
            const cv_plat_t *p = &CV_PLAT[k];
            int32_t top = cave_top_q8(k, cv.x);
            if (cv.vy < 0 || cv.x < Q8(p->x0 - 5) || cv.x > Q8(p->x1 + 5)) continue;
            if (p->kind == 2 && (story < 4 || cv.x <= Q8(CV_W - 14))) continue;
            if (k == 0u ? bot >= top : (py + rad <= top + 256 && bot >= top - (p->kind == 2 ? Q8(2) : 0))) {
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
    deco_jump_tick(dt, cv.g, cv.x, cv.y);
    if (cv.g) deco_trail(cv.x, cv.y + rock_rad(cv.th), cv.vx, dt);
    for (i = 0; i < 5; i++) if (cv.sq[i] > 0) cv.sq[i] = imax(0, cv.sq[i] - dt);
    if (cv.boost_t > 0) cv.boost_t -= dt;
    hx = cv.x >> 8; hy = cv.y >> 8;
    /* high up, where only boosting and bouncing reach: the gift shop, a row of chimes, a ring */
    if (iabs(hx - CV_SHOP_X) < 44 && iabs(hy - 182) < 24 && !cv.friend_awake) {   /* the shop's keeper wakes as Rock comes */
        cv.friend_awake = 1; sfx(67, 0, 300, W_TRI, 2800, 4, 200); sfx(72, 140, 300, W_TRI, 2800, 4, 200); sfx(76, 280, 600, W_TRI, 2800, 4, 200);
    }
    for (i = 0; i < CV_NCHIME; i++) {
        if (cv.chime_t[i] > 0) cv.chime_t[i] -= dt;
        else if (iabs(hx - CHIME_X(i)) < 13 && iabs(hy - CHIME_Y(i)) < 18) {
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
        int32_t tx = iclamp(cv.x - Q8(120) + cv.vx / 3, cv.y < Q8(200) ? Q8(CV_XL - 8) : 0, cv.x > Q8(CV_W - 14) ? Q8(CV_TUN_X1 + 30 - 240) : Q8(CV_W - 240)), ty = iclamp(cv.y - Q8(176), Q8(CV_GROUND - 372), Q8(CV_GROUND - 214));
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
    /* later the Hat has more to say (the tickets, once a machine has been played; a name for Rock, once a second has):
     * it calls as it did the first time, from whichever side it is on, and says it when Rock comes to it */
    if (iabs(hx - CV_HAT_X) > 80) cv.hat_away = 1;
    if (cv.met && story >= 4 && pf_hat_due()) {
        cv.psst += dt;
        if (cv.psst > 1500 && (cv.psst % 2600) < dt && iabs(hx - CV_HAT_X) > 130) { sfx(69, 0, 90, W_SQUARE, 1500, 2, 220); sfx(74, 110, 160, W_SQUARE, 1500, 2, 220); }
        if (cv.hat_away && iabs(hx - CV_HAT_X) < 46 && cv.g && hy > CV_GROUND - 40) {
            int due = pf_hat_due();
            cv.vx = 0; cv.psst = 0; cv.hat_away = 0;
            dlg_open(T_HAT, due == 1 ? T_HAT_TICKETS : T_HAT_NAME); pf_hat_told(due);
        }
    }
    /* what is in reach */
    cv.near = 0;
    if (iabs(hx - CV_HAT_X) < 40 && hy > CV_GROUND - 40) cv.near = 1;
    cv.at_juke = iabs(hx - CV_JUKE_X) < 30 && hy > CV_GROUND - 40;
    if (!cv.at_juke) cv.juke_ready = 1;
    if (cv.at_juke && (story < 4 || cv.juke_ready)) cv.near = 2;
    if (iabs(hx - CV_BOARD_X) < 22 && iabs(hy - (CV_BOARD_Y - 8)) < 22) cv.near = 6;
    if (hx < CV_DOOR_X + 26 && hy > 190 - 40 && hy < 190) cv.near = 7;
    if (iabs(hx - CV_SHOP_X) < 30 && hy > 190 - 40 && hy < 190) cv.near = 10;   /* the gift shop */
    if (story >= 4 && iabs(hx - CV_WELL_X) < 20 && hy > CV_GROUND - 40) cv.near = 8;   /* (no well before the jukebox is mended) */
    for (i = 0; i < 3; i++) if (iabs(hx - CV_MACH[i][0]) < 26 && iabs(hy - (CV_MACH[i][1] - 8)) < 22) cv.near = (uint8_t)(3 + i);
    /* the music room's door opens as Rock comes down the tunnel to it, and shuts behind him as he leaves */
    if (story >= 4 && hx > CV_TDOOR_X - 96 && cv.x > Q8(CV_W - 14)) {
        if (cv.door == 0) { sfx(60, 0, 500, W_SINE, 2600, 30, 200); sfx(67, 150, 500, W_SINE, 2200, 30, 200); sfx(76, 300, 900, W_SINE, 2200, 30, 220); sfx(96, 0, 300, W_NOISE, 500, 60, 100); }
        cv.door = imin(256, cv.door + dt * 256 / 900);
        if (hx > CV_TDOOR_X - 44) cv.near = 9;
    } else cv.door = imax(0, cv.door - dt * 256 / 600);
    if (cv.near) led_play_breathe = 1;
    if (BDOWN(BTN_PLAY) && cv.near) {
        if (cv.near == 1) dlg_open(T_HAT, story < 4 ? T_HAT_REMIND : T_HAT_FREE);
        else if (cv.near == 2) act = story < 4 ? CVA_LAMP : CVA_DISH;   /* mended: the dish again, for a new song */
        else if (cv.near == 6) act = CVA_SCORES;
        else if (cv.near == 7) act = CVA_EXIT;
        else if (cv.near == 10 && story >= 4) act = CVA_SHOP;
        else if (cv.near == 9) {                 /* into the music room */
            cv.enter = 1; cv.dive_x = cv.x; cv.dive_y = cv.y; cv.near = 0; snd_whoosh();
            sfx(72, 0, 700, W_SINE, 2400, 10, 220); sfx(79, 110, 700, W_SINE, 2400, 10, 220); sfx(84, 220, 700, W_SINE, 2400, 10, 220); sfx(91, 330, 1200, W_SINE, 2400, 10, 230);
        }
        else if (cv.near == 8 && story >= 4) {   /* the well: Rock dives in, and the game begins when the dive is over */
            cv.wl = CVW_HOP; cv.wt = 0; cv.low = 0; cv.zw = 256; cv.wturned = 0; cv.dive_x = cv.x; cv.dive_y = cv.y; cv.near = 0;
            sfx(72, 0, 120, W_TRI, 2200, 2, 120); sfx(96, 0, 70, W_NOISE, 500, 1, 160);
        }
        else if (story >= 4 && cv.near <= 5) act = CVA_M1 + cv.near - 3;
        else { banner_show("", TR(T_NOPOWER), 0); sfx_ex(43, 0, 260, W_SAW, 2400, 2, 120, 0, 0, 65300); }
    }
    led_key_want |= in.keys;
    return act;
}

/* the three machines, each built like its game. Dark (before the power) they are only their outlines; lit, they move:
 *   CRYSTAL ROAD  a crystal standing on end, shards at its foot; on its screen crystals come down a road to a tiny Rock
 *   JUICE WORKS   a juice press: a glass tank of juice on top, bubbling; four taps dripping into a glass that fills
 *   NIGHT BLOOM   a flower in a pot: petals turning round a round screen, leaves on the stem, spores drifting up */
static void cave_cabinet(int32_t x, int32_t y, int which, int32_t lit, uint32_t tm)
{
    int on = story >= 4;
    col_t c = on ? th->caps[which * 2 % 5] : th->rim, line = on ? c : th->rim;
    int32_t k, xy[12];
    galpha(lit);
    if (which == 0) {                            /* CRYSTAL ROAD */
        static const int8_t BODY[5][2] = {{-13, -5}, {-15, -40}, {0, -64}, {15, -40}, {13, -5}};
        static const int8_t SH[2][4][2] = {{{-14, -1}, {-22, -14}, {-19, -22}, {-12, -10}}, {{13, -1}, {20, -10}, {18, -16}, {12, -8}}};
        if (on) { galpha(50 + (tsin(tm, 40, 0) >> 9)); gdisc(x, y - 58, 16, c); galpha(lit); }
        for (k = 0; k < 2; k++) {                /* two shards at its foot */
            int32_t j;
            for (j = 0; j < 4; j++) { xy[2 * j] = x + SH[k][j][0]; xy[2 * j + 1] = y + SH[k][j][1]; }
            gpoly(xy, 4, th->panel); gpolyline(xy, 4, 1, on ? th->caps[(k * 3 + 1) % 5] : th->rim);
        }
        for (k = 0; k < 5; k++) { xy[2 * k] = x + BODY[k][0]; xy[2 * k + 1] = y + BODY[k][1]; }
        gpoly(xy, 5, th->panel); gpolyline(xy, 5, 1, line);
        gline(x, y - 64, x - 5, y - 44, line); gline(x, y - 64, x + 5, y - 44, line);   /* the facets of its point */
        grect(x - 16, y - 5, 33, 5, th->panel); gbox(x - 16, y - 5, 33, 5, line);       /* the plinth */
        gbox(x - 10, y - 38, 21, 16, th->rim);   /* the screen */
        for (k = 0; k < 3; k++) grect(x - 7 + k * 6, y - 15, 2, 2, th->rim);
        if (on) {                                /* the game on it: a road, crystals coming down it, Rock leaping to meet them */
            int32_t q;
            galpha(256);
            gline(x - 1, y - 35, x - 9, y - 24, th->rim); gline(x + 1, y - 35, x + 9, y - 24, th->rim);
            ghl(x - 9, x + 10, y - 24, th->rim);
            for (k = 0; k < 2; k++) {
                int32_t ph = (int32_t)((tm / 30u + (uint32_t)k * 13u) % 26u), side = (int32_t)((tm / 780u + (uint32_t)k) % 3u) - 1;
                int32_t cy = y - 35 + ph * 10 / 26, cxp = x + side * ph * 7 / 26, h = 1 + ph / 9;
                gline(cxp - 1, cy, cxp, cy - h, th->caps[(k * 2 + (int)(tm / 780u)) % 5]); gline(cxp, cy - h, cxp + 1, cy, th->caps[(k * 2 + (int)(tm / 780u)) % 5]);
            }
            q = (int32_t)((tm / 780u) % 3u) - 1;
            gdisc(x + q * 6, y - 26, 2, th->rock_rim);
            gp(x - 4, y - 58, th->text); gp(x - 3, y - 52, th->text);   /* glints on the point */
            if ((tm / 600u) & 1u) gp(x + 4, y - 48, th->text);
        }
    } else if (which == 1) {                     /* JUICE WORKS */
        int32_t tx = x, ty = y - 47, lvl;
        grect(x - 14, y - 34, 28, 34, th->panel); gbox(x - 14, y - 34, 28, 34, line);   /* the press */
        grect(x - 2, y - 37, 4, 4, th->panel); gbox(x - 2, y - 37, 4, 4, line);         /* its neck */
        gdisc(tx, ty, 12, th->panel);
        if (on) {                                /* the tank: juice to a wobbling level, bubbles in it */
            lvl = ty - 2 + (tsin(tm, 50, 0) >> 13);
            galpha(200);
            gclip_y0 = imax(0, lvl); gdisc(tx, ty, 11, th->caps[(tm / 1500u) % 5u]); gclip_y0 = 0;
            galpha(256);
            for (k = 0; k < 4; k++) {
                int32_t b = (int32_t)((tm / 30u + (uint32_t)k * 37u) % 18u);
                if (ty + 9 - b > lvl + 1) gp(tx - 6 + k * 4 + ((b / 4) & 1), ty + 9 - b, th->text);
            }
            galpha(lit);
        }
        gcirc(tx, ty, 12, line);
        garc(tx, ty, 9, 9, 34000, 42000, th->text, 0);                                     /* a shine on the glass */
        gbox(x - 10, y - 31, 21, 10, th->rim);   /* the ticket screen */
        for (k = 0; k < 4; k++) {                /* four taps, a glass under them */
            col_t tc = on ? th->caps[k + 1] : th->rim;
            grect(x - 9 + k * 6, y - 19, 3, 3, tc);
            if (on) {
                int32_t d = (int32_t)((tm / 25u + (uint32_t)k * 11u) % 16u);
                if (((tm / 900u) & 3u) == (uint32_t)k && d < 9) gp(x - 8 + k * 6, y - 15 + d, tc);
            }
        }
        gline(x - 5, y - 11, x - 4, y - 2, th->text); gline(x + 5, y - 11, x + 4, y - 2, th->text); ghl(x - 4, x + 5, y - 2, th->text);
        if (on) { int32_t f = (int32_t)((tm / 180u) % 9u); grect(x - 3, y - 2 - f, 7, f, th->caps[(tm / 900u) % 4u + 1u]); }
        {   /* the crank on its side */
            int32_t a = on ? (int32_t)(tm * 12u) : 0;
            gcirc(x + 17, y - 20, 3, line);
            gline(x + 17, y - 20, x + 17 + ((5 * icos(a)) >> 14), y - 20 + ((5 * isin(a)) >> 14), line);
        }
    } else {                                     /* NIGHT BLOOM */
        int32_t fx = x, fy = y - 46;
        static const int8_t POT[4][2] = {{-9, 0}, {9, 0}, {12, -12}, {-12, -12}};
        for (k = 0; k < 4; k++) { xy[2 * k] = x + POT[k][0]; xy[2 * k + 1] = y + POT[k][1]; }
        grect(x - 1, fy, 3, y - 12 - fy, on ? th->caps[2] : th->rim);                   /* the stem */
        gellf(x - 7, y - 24, 6, 3, on ? th->caps[2] : th->rim, 0);                       /* its leaves */
        gellf(x + 7, y - 30, 6, 3, on ? th->caps[2] : th->rim, 0);
        gpoly(xy, 4, th->panel); gpolyline(xy, 4, 1, line);                               /* the pot */
        ghl(x - 12, x + 13, y - 9, line);
        if (on) { galpha(50 + (tsin(tm, 30, 9000) >> 9)); gdisc(fx, fy, 24, c); galpha(lit); }
        for (k = 0; k < 8; k++) {                /* the petals, turning (four of them in the game's own colours) */
            int32_t a = (on ? (int32_t)(tm * 6u) : 0) + k * 8192, px = fx + ((15 * icos(a)) >> 14), py = fy + ((15 * isin(a)) >> 14);
            col_t pc = on ? th->caps[(k & 1) ? 4 : k / 2 % 4] : th->rim;
            gdisc(px, py, 5, th->panel); gcirc(px, py, 5, pc);
        }
        gdisc(fx, fy, 9, th->panel); gcirc(fx, fy, 9, line);                              /* the round screen */
        if (on) {                                /* on it, a ring of four petals of its own, the other way round */
            for (k = 0; k < 4; k++) {
                int32_t a = -(int32_t)(tm * 10u) + k * 16384;
                gdisc(fx + ((5 * icos(a)) >> 14), fy + ((5 * isin(a)) >> 14), 1, th->caps[k]);
            }
            gp(fx, fy, th->pad);
            for (k = 0; k < 5; k++) {            /* spores drifting up off it */
                int32_t q = (int32_t)((tm / 12u + (uint32_t)k * 197u) % 1000u);
                galpha(lit * (1000 - q) / 1000);
                gp(fx - 14 + k * 7 + (tsin(tm, 15, k * 13000) >> 12), fy - 12 - q * 26 / 1000, th->caps[(k + 1) % 5]);
            }
        }
    }
    galpha(256);
}

/* the well at (x, y), y its foot. front 0: the well as a layer behind Rock (all of it, the bucket hanging; once Rock is
 * in the bucket, only what is behind it: the roof, the posts, the windlass, the back of the ring and its dark mouth).
 * front 1, with Rock in the bucket: the front of the ring and its stones, then the rope, Rock and the bucket, cut off at
 * the ring's near lip, so they go down between its back and its front */
static void cave_well_stones(int32_t x, int32_t y)
{
    int32_t k;
    grect(x - 24, y - 24, 49, 24, th->panel);
    gellf(x, y - 24, 24, 6, th->panel, 1);       /* (the near half of the ring's top) */
    gellf(x, y - 24, 20, 4, th->bg, 1);          /* (the mouth's near half, dark) */
    for (k = 0; k < 3; k++) ghl(x - 24, x + 25, y - 12 + k * 6, th->mute);
    for (k = 0; k < 3; k++) { int32_t r = k & 1 ? 6 : 0, j; for (j = -21 + r; j < 24; j += 12) gline(x + j, y - 18 + k * 6, x + j, y - 12 + k * 6, th->mute); }
    garc(x, y - 24, 24, 6, 0, 32768, th->rim, 0); garc(x, y - 24, 20, 4, 0, 32768, th->mute, 0);
    gline(x - 24, y - 24, x - 24, y, th->rim); gline(x + 24, y - 24, x + 24, y, th->rim);
}
#define CVW_WOOD AC_RGB(92, 58, 34)              /* the bucket's boards, */
#define CVW_SEAM AC_RGB(44, 27, 15)              /* the dark joins between them, */
#define CVW_EDGE AC_RGB(140, 96, 58)             /* their worn top ends, */
#define CVW_HOOP AC_RGB(58, 58, 66)              /* its iron hoops */
#define CVW_IRON AC_RGB(150, 150, 160)           /* and its handle */
static void cave_well_load(int32_t x, int32_t y, int32_t by, int32_t snap, int inb, uint32_t tm, int32_t lit)
{
    int32_t k, cut = y - 34, xy[8];
    if (snap < 0) { gline(x, y - 64, x, by - 14, th->text); gline(x + 1, y - 64, x + 1, by - 14, th->stem); }
    else {                                       /* snapped: the top end springs up and swings, the rest goes down */
        int32_t up = imin(22, snap / 12), sw = snap < 600 ? (tsin(tm, 70, 0) * (600 - snap) / 600) >> 12 : 0;
        gline(x, y - 64, x + sw, cut - up, th->text); gline(x + 1, y - 64, x + 1 + sw, cut - up, th->stem);
        if (snap < 350) for (k = 0; k < 8; k++) {   /* the snap: a burst at the cut, frayed bits flying */
            int32_t a = k * 8192 + 2000, r0 = 2 + snap / 30, r1 = 5 + snap / 12;
            galpha(lit * (350 - snap) / 350);
            gline(x + ((r0 * icos(a)) >> 14), cut + ((r0 * isin(a)) >> 14), x + ((r1 * icos(a)) >> 14), cut + ((r1 * isin(a)) >> 14), k & 1 ? th->text : th->pad);
            galpha(lit);
        }
    }
    if (inb && snap < CVW_SNAP_MS / 2)           /* Rock in it, his eyes just over its rim */
        prock_draw(x, by - 3, 256, 0, (tm % 3100u) < 150u ? 30 : 256);
    xy[0] = x - 10; xy[1] = by; xy[2] = x + 10; xy[3] = by; xy[4] = x + 8; xy[5] = by + 10; xy[6] = x - 8; xy[7] = by + 10;
    gpoly(xy, 4, CVW_WOOD);                      /* the bucket: dark boards, */
    for (k = -1; k <= 1; k += 2) gline(x + k * 5, by + 1, x + k * 4, by + 9, CVW_SEAM);   /* the joins between them, */
    gline(x, by + 1, x, by + 9, CVW_SEAM);
    ghl(x - 9, x + 10, by, CVW_EDGE);            /* the boards' top ends, worn lighter, */
    ghl(x - 10, x + 11, by + 2, CVW_HOOP); ghl(x - 8, x + 9, by + 8, CVW_HOOP);   /* two iron hoops round them */
    gpolyline(xy, 4, 1, CVW_SEAM);
    if (snap < 0) garc(x, by, 10, 14, 32768, 65536, CVW_IRON, 0);   /* the handle, arched over Rock's head */
}
static void cave_well(int32_t x, int32_t y, int on, uint32_t tm, int front)
{
    int32_t by = y - (CV_WELL_Y - cvw_by()), k, lit = imax(110, cave_light(CV_WELL_X, CV_GROUND - 30)), ang = cv.low * 900;
    int32_t snap = cv.wl == CVW_SNAP ? cv.wt : cv.dive ? CVW_SNAP_MS : -1, xy[8];
    int inb = cv.wl >= CVW_LOWER || cv.dive;
    if (snap >= 0) by += imin(60, snap * snap / 2000);   /* (the bucket falls once the rope is gone) */
    galpha(lit);
    if (front) {
        cave_well_stones(x, y);
        gclip_y1 = iclamp(y - 20, 0, AC_H);      /* (below the near lip: inside the well, out of sight) */
        cave_well_load(x, y, by, snap, inb, tm, lit);
        gclip_y1 = AC_H;
        galpha(256);
        return;
    }
    /* the roof, the posts, the windlass and its crank */
    xy[0] = x - 34; xy[1] = y - 78; xy[2] = x + 34; xy[3] = y - 78; xy[4] = x; xy[5] = y - 100; gpoly(xy, 3, th->panel);
    gline(x - 34, y - 78, x, y - 100, th->rim); gline(x, y - 100, x + 34, y - 78, th->rim); ghl(x - 34, x + 35, y - 78, th->rim);
    for (k = 1; k < 4; k++) ghl(x - 34 + k * 8, x + 35 - k * 8, y - 78 - k * 5, th->mute);   /* (its shingles) */
    grect(x - 25, y - 78, 4, 58, th->stem); grect(x + 22, y - 78, 4, 58, th->stem);
    grect(x - 21, y - 69, 43, 3, th->stem);                                            /* the axle */
    grect(x - 9, y - 72, 19, 9, th->stem); gbox(x - 9, y - 72, 19, 9, th->rim);        /* the drum, the rope wound on it */
    for (k = 0; k < 4; k++) { int32_t o = ((k * 5 + (ang >> 11)) % 20 + 20) % 20 - 9; if (o > -9 && o < 9) gline(x + o, y - 71, x + o + 2, y - 64, th->text); }
    {   int32_t cx = x + 28, cy = y - 68, hx = cx + ((7 * icos(ang)) >> 14), hy = cy + ((7 * isin(ang)) >> 14);
        gline2(cx, cy, hx, hy, th->rim); grect(hx - 1, hy - 1, 3, 3, th->text); gdisc(cx, cy, 2, th->rim); }
    /* the back of the ring of stones, its dark mouth */
    gellf(x, y - 24, 24, 6, th->panel, 0); gellf(x, y - 24, 20, 4, th->bg, 0);
    garc(x, y - 24, 24, 6, 32768, 65536, th->rim, 0); garc(x, y - 24, 20, 4, 32768, 65536, th->mute, 0);
    if (on) {                                    /* its two lights, notes rising out of it */
        static const col_t WC[2] = {AC_RGB(240, 96, 200), AC_RGB(110, 232, 210)};
        for (k = 0; k < 2; k++) {
            int32_t q = (int32_t)((tm / 3u + (uint32_t)k * 500u) % 1000u);
            gdisc(x + (k ? 23 : -23), y - 82, 2, WC[k]);
            galpha(lit * (256 - q * 256 / 1000) / 256);
            if (!inb) gtext(x + (k ? 4 : -9) + (tsin(tm, 20, k * 20000) >> 12), y - 30 - q * 26 / 1000, "\xE2\x99\xAA", WC[k], 1);
            galpha(lit);
        }
    }
    if (!inb) { cave_well_stones(x, y); cave_well_load(x, y, by, snap, 0, tm, lit); }   /* (a layer: all of it, before Rock) */
    galpha(256);
}
/* over the zoomed well: what to do, and a knob turning; gone once a knob has been turned */
static void cave_well_help(uint32_t tm)
{
    static int32_t fade = 256;
    int32_t a, sc = ts(1, 3), h, ang = (int32_t)(tm % 1400u) * 46811 / 1000, k, cx = 120, cy = 186;   /* (the size every prompt in the cave has) */
    fade = cv.wturned ? imax(0, fade - 24) : imin(256, cv.wt * 256 / 300);
    if (fade <= 0) return;
    a = fade;
    while (sc > 1 && gblock_h(212, TR(T_WELL_LOWER), sc) > 3 * TLH(sc)) sc--;
    h = gblock_h(212, TR(T_WELL_LOWER), sc);
    galpha(a);
    grect(8, 8, 224, h + 14, th->panel); gbox(8, 8, 224, h + 14, th->pad);
    gblock(14, 15, 212, TR(T_WELL_LOWER), th->text, sc, 1);
    galpha(a * 220 / 256); gdisc(cx, cy, 30, th->bg); galpha(a);   /* a knob, turning, an arrow round it */
    gcirc(cx, cy, 16, th->text); gcirc(cx, cy, 15, th->text);
    for (k = 0; k < 12; k++) { int32_t q = k * 5461 + ang; gp(cx + ((18 * icos(q)) >> 14), cy + ((18 * isin(q)) >> 14), th->mute); }
    gline2(cx, cy, cx + ((12 * icos(ang - 16384)) >> 14), cy + ((12 * isin(ang - 16384)) >> 14), th->pad);
    garc(cx, cy, 25, 25, 40000, 62000, th->pad, 1);
    {   int32_t ex = cx + ((25 * icos(62000)) >> 14), ey = cy + ((25 * isin(62000)) >> 14);
        gline2(ex, ey, ex - 6, ey - 1, th->pad); gline2(ex, ey, ex - 1, ey + 6, th->pad); }
    galpha(256);
}

/* the gift shop: a market stall in the room up on the left. A counter of boards with a ticket on its front, the keeper
 * behind it (the rock who slept on this ledge: asleep until Rock comes), a shelf of hats, an awning striped in the
 * five colours. (x, y): the middle of its foot on the screen */
static void cave_shop(int32_t x, int32_t y, int on, uint32_t tm)
{
    static const uint8_t HATS[3] = {2, 10, 4};   /* (a party hat, a crown, a mushroom cap: ac_deco.h) */
    int32_t i;
    rk_t k;
    if (x < -60 || x > 300) return;
    galpha(imax(on ? 256 : 70, cave_light(CV_SHOP_X, 170)));
    grect(x - 38, y - 64, 2, 46, th->stem); grect(x + 36, y - 64, 2, 46, th->stem);   /* its posts */
    grect(x - 30, y - 41, 60, 2, th->stem);                                           /* the shelf, and what stands on it */
    for (i = 0; i < 3; i++) { k.x = x - 20 + i * 20; k.y = y - 36; k.z = 200; k.c = 16384; k.s = 0; deco_hat(&k, HATS[i], th->dark ? th->rock_rim : 0, tm); }
    rock_draw(x, y - 22, 210, 0, cv.friend_awake ? ((tm % 2900u) < 150u ? 30 : 256) : 0, th->rock, th->rock_rim, th->dark, th->caps[2], th->eye, th->pupil);
    if (!cv.friend_awake) gtext(x + 12 + (int32_t)((tm / 500u) % 3u) * 3, y - 40 - (int32_t)((tm / 500u) % 3u) * 4, "Z", th->mute, 1);
    grect(x - 38, y - 18, 76, 18, th->panel); gbox(x - 38, y - 18, 76, 18, th->rim);    /* the counter */
    for (i = 1; i < 4; i++) grect(x - 38 + i * 19, y - 17, 1, 16, th->mute);
    grect(x - 41, y - 20, 82, 3, th->rim);
    tix_one(x - 9, y - 13, 18, 9, th->caps[0]);
    for (i = 0; i < 8; i++) {                    /* the awning */
        col_t c = th->caps[i % 5];
        gdisc(x - 44 + i * 11 + 5, y - 66, 5, c); grect(x - 44 + i * 11, y - 78, 11, 12, c);
    }
    galpha(256);
}

/* the music room's door, in the end of the tunnel: the face of a geode, shut by an iris of crystal shards in the five
 * colours. open 0..256: the shards turn and draw back into the rim, and the light of the room comes out */
static void cave_tdoor(int32_t x, int32_t y, int32_t open, uint32_t tm)
{
    int32_t k, R = 21, e = ease8(open), spin = (int32_t)(tm * 5u) + e * 90, xy[8];
    galpha(24 + e * 40 / 256 + (tsin(tm, 30, 0) >> 11)); gdisc(x, y, R + 9, th->caps[(tm / 500u) % 5u]); galpha(256);
    gdisc(x, y, R, th->bg);
    if (e > 0) {                                 /* inside: rings of the five colours going away */
        for (k = 0; k < 5; k++) {
            int32_t rr = (int32_t)((tm / 14u + (uint32_t)k * 20u) % 100u) * (R - 2) / 100;
            galpha(e * (100 - rr * 100 / R) / 100); gcirc(x, y, rr, th->caps[k]);
        }
        galpha(e * 70 / 256); gdisc(x, y, (R - 4) * e / 256, th->caps[(tm / 300u) % 5u]); galpha(256);
    }
    for (k = 0; k < 8; k++) {                    /* the shards */
        int32_t a = spin + k * 8192, tip = (R - 1) * (256 - e) / 256 + 0, in_ = R - 1 - tip, ux = icos(a), uy = isin(a), a1 = a + 5200, a0 = a - 2000;
        if (tip < 3) continue;
        xy[0] = x + ((R * icos(a0)) >> 14); xy[1] = y + ((R * isin(a0)) >> 14);
        xy[2] = x + ((R * icos(a1)) >> 14); xy[3] = y + ((R * isin(a1)) >> 14);
        xy[4] = x + ((in_ * ux) >> 14) + ((3 * uy) >> 14); xy[5] = y + ((in_ * uy) >> 14) - ((3 * ux) >> 14);
        galpha(200); gpoly(xy, 3, th->panel); galpha(256);
        gpolyline(xy, 3, 1, th->caps[k % 5]);
        gline(xy[4], xy[5], (xy[0] + xy[2]) / 2, (xy[1] + xy[3]) / 2, th->caps[(k + 2) % 5]);
    }
    garc(x, y, R, R, 0, 65536, th->rim, 1);
    for (k = 0; k < 7; k++) {                    /* crystals round its rim */
        int32_t a = 30000 + k * 6200, l = 5 + (k * 5) % 6, bx = x + (((R + 1) * icos(a)) >> 14), by = y + (((R + 1) * isin(a)) >> 14);
        gline(bx - 2, by, bx + ((l * icos(a)) >> 14), by + ((l * isin(a)) >> 14), th->caps[k % 5]); gline(bx + 2, by, bx + ((l * icos(a)) >> 14), by + ((l * isin(a)) >> 14), th->caps[k % 5]);
    }
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
        if (p->kind == 2) {                      /* the tunnel (mended): its floor and roof, its end, the door, notes drifting up it */
            if (!on || SX(p->x1) < -40) continue;
            for (x = imax(CV_W - 4, cx - 4) & ~3; x < imin(p->x1, cx + 244); x += 4) {
                int32_t y0 = cave_tun_q8(x << 8) >> 8, y1 = cave_tun_q8((x + 4) << 8) >> 8;
                galpha(256); gline(SX(x), SY(y0), SX(x + 4), SY(y1), th->rim);
                galpha(150); gline(SX(x), SY(y0 - CV_TUN_H - 4 + (int32_t)(ihash((uint32_t)(x / 4)) * 4u >> 16)), SX(x + 4), SY(y1 - CV_TUN_H - 4 + (int32_t)(ihash((uint32_t)(x / 4 + 1)) * 4u >> 16)), th->rim);
                for (k = 0; k < 3; k++)
                    if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(150 - k * 40); gline(SX(x), SY(y0 + 3 + k * 4), SX(x + 4), SY(y1 + 3 + k * 4), th->bands[(k + 2) % 5]); }
            }
            galpha(256);
            gline(SX(p->x1), SY(236 - CV_TUN_H - 4), SX(p->x1), SY(236), th->rim);
            for (k = 0; k < 4; k++) {            /* notes drift up the tunnel and out of its mouth: something is down there */
                int32_t q = (int32_t)((tm / 5u + (uint32_t)k * 250u) % 1000u), nx = CV_TDOOR_X - 20 - q * 230 / 1000;
                galpha(q < 800 ? 200 : (1000 - q) * 200 / 200);
                gtext(SX(nx), SY((cave_tun_q8(nx << 8) >> 8) - 24 + (tsin(tm, 22, k * 16000) >> 12)), "\xE2\x99\xAA", th->caps[(k * 2 + 1) % 5], 1);
            }
            galpha(256);
            cave_tdoor(SX(CV_TDOOR_X), SY(236 - 22), cv.door, tm);
            if (SX(CV_TDOOR_X) < 300) {          /* its name over it */
                int32_t sc = ts(1, 2);
                galpha(150 + ipulse(tm, 1500) * 106 / 256);
                gfit(iclamp(SX(CV_TDOOR_X - 16), 60, 180), SY(236 - 56) - 5 * sc, 232, TR(T_MR_NAME), th->caps[(tm / 500u) % 5u], sc);
                galpha(256);
            }
            continue;
        }
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
            for (x = imax(4, cx - 4) & ~3; x < imin(CV_W - 4, cx + 244); x += 4) {
                int32_t y0 = cave_floor_q8(x << 8) >> 8, y1 = cave_floor_q8((x + 4) << 8) >> 8;
                lit = cave_light(x, y0);
                galpha(lit);
                gline(SX(x), SY(y0), SX(x + 4), SY(y1), th->rim);
                for (k = 0; k < 3; k++)
                    if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(lit * (150 - k * 40) / 256); gline(SX(x), SY(y0 + 3 + k * 4), SX(x + 4), SY(y1 + 3 + k * 4), th->bands[k % 5]); }
            }
            for (k = 1; k < 2; k++) {            /* the right booster shows itself: three chevrons running up the ramp
                                                    (the left one does not: rolling onto it is a surprise) */
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
            int32_t y0 = cave_top_q8((uint32_t)i, x << 8) >> 8, y1 = cave_top_q8((uint32_t)i, imin(x + 4, p->x1) << 8) >> 8;
            lit = cave_light(x, p->y);
            galpha(lit);
            gline(SX(x), SY(y0), SX(x + 4), SY(y1), th->rim);
            for (k = 0; k < 3; k++)
                if (((x / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(lit * (150 - k * 40) / 256); gline(SX(x), SY(y0 + 3 + k * 4), SX(x + 4), SY(y1 + 3 + k * 4), th->bands[(k + i) % 5]); }
        }
        if (i) {                                 /* a ledge's ends hang as rock */
            if (p->x0 != CV_XL) { galpha(cave_light(p->x0, p->y)); gline(SX(p->x0), SY(p->y), SX(p->x0 + 10), SY(p->y + 16), th->rim); }
            galpha(cave_light(p->x1, p->y)); gline(SX(p->x1), SY(p->y), SX(p->x1 - 10), SY(p->y + 16), th->rim);
        }
    }
    {   /* the walls, up to the roof: ragged, lit like the floors. Left: from the ramp's end to the high ledge, then the
           room's own wall; right: from the ramp's end all the way */
        static const int16_t WL[3][3] = {{4, 190, 0}, {CV_XL, CV_GROUND - 360, 190}, {CV_W - 4, CV_GROUND - 360, 0}};
        for (i = 0; i < 3; i++) {
            int32_t wx = WL[i][0], ya = WL[i][1], yb = WL[i][2] ? WL[i][2] : cave_floor_q8(wx << 8) >> 8, y, px = wx;
            if (SX(wx) < -8 || SX(wx) > 248) continue;
            for (y = ya; y < yb; y += 8) {
                int32_t nx = wx + (int32_t)(ihash((uint32_t)(y / 8 + i * 77 + 31)) * 5u >> 16) - 2, y1 = imin(yb, y + 8);
                if (y1 == yb) nx = wx;
                galpha(imax(on ? 140 : 40, cave_light(wx, y)));
                if (i == 2 && on && y >= 176 - CV_TUN_H - 6 && y < 176) { px = nx; continue; }   /* (the tunnel's mouth) */
                gline(SX(px), SY(y), SX(nx), SY(y1), th->rim);
                px = nx;
            }
        }
    }
    {   /* the door outside: an arch of the five colours with the evening in it, seen in any light */
        int32_t x = SX(CV_DOOR_X), y = SY(190);
        if (x > -40 && x < 280) {
            galpha(256);
            gellf(x, y, 15, 30, th->far1, 1); grect(x - 15, y, 31, 1, th->rim);
            for (k = 0; k < 4; k++) { galpha(230 - k * 45); garc(x, y, 18 + k * 4, 33 + k * 5, 32768, 65536, th->caps[(k + (int32_t)(tm / 800u)) % 5], 1); }
            galpha(40 + (tsin(tm, 20, 0) >> 10)); gdisc(x, y - 14, 34, th->pad);
            galpha(256);
            for (k = 0; k < 3; k++) gp(x - 8 + k * 7, y - 20 + ((k * 5) % 7) - 3, th->pad);   /* stars */
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
    /* high up: the gift shop, the chimes hanging from the roof, the ring */
    cave_shop(SX(CV_SHOP_X), SY(190 + CV_MDIP_D), on, tm);
    for (i = 0; i < CV_NCHIME; i++) {
        int32_t x = SX(CHIME_X(i)), y = SY(CHIME_Y(i)), on_ = cv.chime_t[i] > 0, xy[8];
        col_t c = th->caps[i % 5];
        int32_t got = cv.chimes >> i & 1;
        galpha(got ? 200 : 60);
        gline(x, SY(CV_GROUND - 372), x + (on_ ? (tsin(tm, 300, 0) >> 13) : 0), y - 8, th->stem);
        xy[0] = x; xy[1] = y - 8; xy[2] = x + 5; xy[3] = y; xy[4] = x; xy[5] = y + 9; xy[6] = x - 5; xy[7] = y;
        if (got) {                               /* rung: it stays lit, full of its colour, in a glow */
            galpha(70 + (tsin(tm, 25, i * 13000) >> 9)); gdisc(x, y, 13, c);
            galpha(256); gpoly(xy, 4, c); gpolyline(xy, 4, 1, th->text);
        } else { galpha(256); gpoly(xy, 4, th->bg); galpha(80); gpolyline(xy, 4, 1, th->mute); }
        if (on_) { galpha(cv.chime_t[i] * 200 / 450); gdisc(x, y, 16, c); }
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
        int32_t x = SX(CV_JUKE_X), y = SY(CV_GROUND + CV_DIP_D), sc = ts(1, 2);
        lit = cave_light(CV_JUKE_X, CV_GROUND - 20);
        galpha(lit);
        grect(x - 17, y - 34, 34, 34, th->panel); gellf(x, y - 34, 17, 16, th->panel, 1);
        gline(x - 17, y, x - 17, y - 34, th->rim); gline(x + 17, y, x + 17, y - 34, th->rim); ghl(x - 17, x + 18, y - 1, th->rim);
        garc(x, y - 34, 17, 16, 32768, 65536, th->rim, 0);
        gbox(x - 11, y - 30, 22, 12, th->rim);                       /* the window where the records are */
        galpha(imax(150, lit));                                      /* its name over it and an arrow down, in any light */
        if (on) {                                                    /* (playing: its name only in front of it, as a machine's) */
            if (cv.at_juke) gfit(x, y - 60 - 5 * ts(1, 3), 232, "JUKEBOX", th->caps[(tm / 500u) % 5u], ts(1, 3));
        } else if (x > -20 && x < 260) {
            gfit(x, y - 66 - 5 * sc, 232, "JUKEBOX", on ? th->caps[(tm / 500u) % 5u] : th->pad, sc);
            if (cv.near != 2) gtext(x - 3 * sc + sc / 2, y - 62 + (iabs(tsin(tm, 50, 0)) * 3 >> 14), "\xE2\x86\x93", th->pad, sc);
        } else if (cv.met && y > 150 && y < 300) {                    /* not mended yet and off the screen: which way it is */
            int32_t w = gtextw("JUKEBOX", sc) + 8 * sc, bob = tsin(tm, 60, 0) >> 13;
            galpha(150 + ipulse(tm, 1300) * 106 / 256);
            if (x < 0) { gtext(4 + bob, y - 66 - 5 * sc, "\xE2\x86\x90", th->pad, sc); gtext(4 + 8 * sc, y - 66 - 5 * sc, "JUKEBOX", th->pad, sc); }
            else { gtext(236 - w, y - 66 - 5 * sc, "JUKEBOX", th->pad, sc); gtext(236 - 6 * sc + bob, y - 66 - 5 * sc, "\xE2\x86\x92", th->pad, sc); }
        }
        galpha(lit);
        for (k = 0; k < 4; k++) ghl(x - 10, x + 11, y - 13 + k * 3, th->rim);   /* the grille */
        galpha(256);
        if (on) {
            for (k = 0; k < 4; k++) garc(x, y - 34, 14 - k * 3, 13 - k * 3, 32768, 65536, th->caps[(k + (int32_t)(tm / 250u)) % 5], 1);
            grect(x - 10, y - 29, 20, 10, th->caps[(tm / 500u) % 5u]);
            galpha(50 + (tsin(tm, 40, 0) >> 9)); gdisc(x, y - 24, 40, th->caps[(tm / 500u) % 5u]); galpha(256);
            for (k = 0; k < 3; k++) {            /* notes float off it */
                int32_t q = (int32_t)((tm / 3u + (uint32_t)k * 333u) % 1000u);
                galpha(256 - q * 256 / 1000);
                gtext(x + (k - 1) * 26 - 3 + (tsin(tm, 20, k * 20000) >> 12), y - 30 - q * 30 / 1000, "\xE2\x99\xAA", th->caps[(k * 2) % 5], 1);
            }
            galpha(256);
        } else if (cv.power_t == 0 && (tm % 3100u) < 90u) { galpha(200); gline(x - 3, y - 26, x + 2, y - 22, th->pad); gline(x + 2, y - 22, x - 1, y - 20, th->pad); galpha(256); }   /* a spark */
    }
    if (story >= 4) cave_well(SX(CV_WELL_X), SY(CV_WELL_Y), on, tm, 0);   /* the well, once the jukebox is mended (a surprise): behind Rock (its front again over the bucket) */
    {   /* the scores board: a little sign with a cup on it, readable in any light */
        int32_t x = SX(CV_BOARD_X), y = SY(CV_BOARD_Y);
        galpha(imax(110, cave_light(CV_BOARD_X, CV_BOARD_Y - 20)));
        gline(x, y, x, y - 14, th->stem);
        grect(x - 13, y - 34, 26, 20, th->panel); gbox(x - 13, y - 34, 26, 20, th->pad);
        grect(x - 4, y - 30, 8, 6, th->caps[3]); grect(x - 1, y - 24, 2, 3, th->caps[3]); grect(x - 4, y - 21, 8, 2, th->caps[3]);   /* the cup */
        gp(x - 6, y - 29, th->caps[3]); gp(x + 5, y - 29, th->caps[3]);
        galpha(256);
    }
    for (i = 0; i < 3; i++) cave_cabinet(SX(CV_MACH[i][0]), SY(CV_MACH[i][1] + CV_MDIP_D), i, cave_light(CV_MACH[i][0], CV_MACH[i][1] - 20), tm);
    /* the Hat, a little off the ground, watching */
    {
        int32_t x = SX(CV_HAT_X), y = SY(CV_GROUND - 22) + (tsin(tm, 16, 0) >> 12);
        int32_t bl = (tm % 4300u) < 180u ? iabs((int32_t)(tm % 4300u) - 90) * 256 / 90 : 256;
        galpha(34); gdisc(x, y - 6, 30, th->pad); galpha(256);
        hat_draw(x, y, 120, th->dark ? HEXC(0x0c1216) : th->hat, th->hat_eye, th->hat_pupil, bl, iclamp(((cv.x >> 8) - CV_HAT_X) * 256 / 70, -256, 256), th->dark, th->hat_rim, 256);
    }
    if (cv.enter) {                              /* into the door: smaller as he goes */
        int32_t q = imin(256, cv.enter * 256 / 600);
        if (cv.enter < 640) prock_draw(SX(cv.x >> 8), SY(cv.y >> 8), 256 - q * 150 / 256, cv.th, 256);
    }
    else if (cv.wl >= CVW_LOWER || cv.dive) cave_well(SX(CV_WELL_X), SY(CV_WELL_Y), on, tm, 1);   /* (Rock in the bucket: drawn with it) */
    else if (cv.dive >= CV_DIVE_HOP) {           /* into the well: smaller as it drops, and gone behind the near rim */
        int32_t q = imin(256, (cv.dive - CV_DIVE_HOP) * 256 / (CV_DIVE_IN - CV_DIVE_HOP));
        gclip_y1 = iclamp(SY(CV_WELL_Y) - 1, 0, AC_H);
        prock_draw(SX(cv.x >> 8), SY(cv.y >> 8), 256 - q * 120 / 256, cv.th, 256);
        gclip_y1 = AC_H;
    } else
    {   /* (what his keys threw off and what he leaves behind, then him, in the jump he wears) */
        pfx_draw(cv.camx & ~255, cv.camy & ~255, 256);
        prock_live(SX(cv.x >> 8), SY(cv.y >> 8), 256, cv.th, (tm % 3700u) < 160u ? iabs((int32_t)(tm % 3700u) - 80) * 256 / 80 : 256);
    }
    /* words */
    if (!dlg.on && cv.psst > 1200 && ((!cv.met && (cv.x >> 8) < CV_HAT_X - 130) || (cv.met && on && pf_hat_due() && iabs((cv.x >> 8) - CV_HAT_X) > 130))) {
        int32_t a = 120 + ipulse(tm, 1300) / 2, sc = ts(1, 3), w, h, left = (cv.x >> 8) > CV_HAT_X, x0;   /* (the Hat is to the left: the box and its arrow there) */
        while (sc > 1 && gtextw(TR(T_PSST), sc) + 12 + 8 * sc > 236) sc--;
        w = gtextw(TR(T_PSST), sc) + 12 + 8 * sc; h = 5 * sc + 10; x0 = left ? 2 : 238 - w;
        galpha(a);
        grect(x0, 150, w, h, th->panel); gbox(x0, 150, w, h, th->pad);
        gtext(x0 + 6 + (left ? 8 * sc : 0), 155, TR(T_PSST), th->text, sc);
        if (left) gtext(x0 + 4 - (tsin(tm, 60, 0) >> 13), 155, "\xE2\x86\x90", th->pad, sc);
        else gtext(238 - 7 * sc - 2 + (tsin(tm, 60, 0) >> 13), 155, "\xE2\x86\x92", th->pad, sc);
        galpha(256);
    }
    if (cv.near && !dlg.on) {
        const char *s = cv.near == 1 ? TR(T_TALK) : cv.near == 2 ? TR(on ? T_NEWSONG : T_FIX) : cv.near == 6 ? TR(T_SCORES_PROMPT) : cv.near == 7 ? TR(T_EXIT) : cv.near == 9 || cv.near == 10 ? TR(T_MR_ENTER) : TR(T_START);
        int32_t px = SX(cv.x >> 8), py = SY(cv.y >> 8) - 30;
        if (cv.near >= 3 && cv.near <= 5) {      /* a machine: its name over it, and what PLAY does over that (clear of the machine) */
            int32_t ty = SY(CV_MACH[cv.near - 3][1]) - 76 - 5 * ts(1, 3);
            gfit(SX(CV_MACH[cv.near - 3][0]), ty, 232, MACH_NAME[cv.near - 3], on ? th->caps[(cv.near - 3) * 2 % 5] : th->mute, ts(1, 3));
            if (!on) s = TR(T_NOPOWER);
            px = SX(CV_MACH[cv.near - 3][0]); py = ty - TLH(ts(1, 3)) - 4 - 6 + 6 * ts(1, 3);
        }
        if (cv.near == 7) { px = iclamp(SX(CV_DOOR_X), 60, 180); py = SY(190) - 102 + ts(1, 3); }   /* the door outside: over the arch, clear of the gift shop beside it */
        if (cv.near == 10) {                     /* the gift shop: its name over its awning, and what PLAY does over that */
            int32_t ty = SY(190) - 86 - 5 * ts(1, 3);
            gfit(iclamp(SX(CV_SHOP_X), 60, 180), ty, 232, TR(T_GIFT), on ? th->caps[(tm / 500u) % 5u] : th->mute, ts(1, 3));
            if (!on) s = TR(T_NOPOWER);
            px = iclamp(SX(CV_SHOP_X), 60, 180); py = ty - TLH(ts(1, 3)) - 4 - 6 + 6 * ts(1, 3);
        }
        if (cv.near == 8) {                      /* the well: its name, MULTIPLAYER under it, and what PLAY does over that */
            /* the name a size up, MULTIPLAYER (in every language) never wider than it: the name stands out */
            int32_t a = ts(1, 3), b = a + 1, m = a, my, ty;
            while (b > 1 && gtextw("UNDERSONG", b) > 232) b--;
            while (m > 1 && gtextw(TR(T_WELL_MULTI), m) > gtextw("UNDERSONG", b)) m--;
            my = SY(CV_WELL_Y) - 106 - 5 * m; ty = my - 5 * b - 3 * m;
            gfit(SX(CV_WELL_X), ty, 232, "UNDERSONG", on ? th->caps[(tm / 500u) % 5u] : th->mute, b);
            gfit(SX(CV_WELL_X), my, 232, TR(T_WELL_MULTI), on ? th->text : th->mute, m);
            if (!on) s = TR(T_NOPOWER);
            px = SX(CV_WELL_X); py = ty - TLH(ts(1, 3)) - 4 - 6 + 6 * ts(1, 3);
        }
        if (cv.near == 9) { px = iclamp(SX(CV_TDOOR_X - 16), 60, 180); py = SY(236 - 56) - 5 * ts(1, 2) - TLH(ts(1, 3)) - 2 + 6 * ts(1, 3) - 6; }   /* the door: over its name */
        if (cv.near == 2 && on) {                /* the mended jukebox: the same, over its name */
            px = SX(CV_JUKE_X); py = SY(CV_GROUND + CV_DIP_D) - 60 - 5 * ts(1, 3) - TLH(ts(1, 3)) - 4 - 6 + 6 * ts(1, 3);
        }
        galpha(160 + ipulse(tm, 1400) * 96 / 256);
        gfit(px, py + 6 - 6 * ts(1, 3), 232, s, th->text, ts(1, 3));
        galpha(256);
    }
    if (cv.power_t > 0) { galpha(cv.power_t * 256 / 1600); grect(0, 0, 240, 240, th->pad); galpha(256); }
    dlg_draw();
    if (cv.wl >= CVW_LOWER) {                    /* the camera comes in on the well as the bucket goes down, a shake as the rope snaps */
        int32_t sh = cv.wl == CVW_SNAP && cv.wt < 300 ? (tsin(tm, 40, 0) * (300 - cv.wt) / 300) >> 13 : 0;
        gzoom(SX(CV_WELL_X) + sh, SY(CV_WELL_Y) - 50 + (36 * ease8(cv.low) >> 8), cv.zw);
        if (cv.wl == CVW_LOWER) cave_well_help(tm);
    }
    if (cv.enter) {                              /* the camera goes in through the door after him, into its light */
        int32_t d = cv.enter, q = imin(256, d * 256 / CV_ENTER_MS), z = 256 + (q * q >> 8) * q / 256 * 2600 / 256;
        gzoom(SX(CV_TDOOR_X), SY(236 - 22), z);
        if (d > 600) gveil(th->pad, (uint32_t)imin(32, (d - 600) * 32 / (CV_ENTER_MS - 660)));
    }
    if (cv.dive) {                               /* the camera goes on in, down the well into the dark */
        int32_t d = cv.dive, z;
        if (d < CV_DIVE_HOP) z = 256 + ease8(d * 256 / CV_DIVE_HOP) * 160 / 256;
        else if (d < CV_DIVE_IN) z = 416 + ease8((d - CV_DIVE_HOP) * 256 / (CV_DIVE_IN - CV_DIVE_HOP)) * 420 / 256;
        else { int32_t q = imin(256, (d - CV_DIVE_IN) * 256 / (CV_DIVE_MS - CV_DIVE_IN)); z = CVW_ZMAX + q * q / 256 * q / 256 * 7000 / 256; }
        gzoom(SX(CV_WELL_X), SY(CV_WELL_Y) - 14, z);
        if (d > CV_DIVE_IN) gveil(th->bg, (uint32_t)imin(32, (d - CV_DIVE_IN) * 32 / (CV_DIVE_MS - CV_DIVE_IN - 60)));
    }
    gfade(cv.fade);
#undef SX
#undef SY
}
