/* SPDX-License-Identifier: GPL-3.0-only */
/* The profile's screens, laid over a paused game like the settings:
 *   NAME YOUR ROCK   the FM-1's keyboard drawn with a letter on every key (the top row A to K, the bottom row L to Z
 *                    and a space): a key types. SELECT or KNOB 1 goes along the places, PRESETS or KNOB 2 rolls the
 *                    letter in a place through the alphabet, the numbers and a few signs. FX throws the dice: a name
 *                    out of a small pool of rock names. OCT- rubs out, PLAY (or OCT+) keeps the name.
 *   the card         the settings' first line, once Rock has a name: him as he is worn, his tickets, and from there
 *                    the name again, the upgrades, the high scores.
 *   UPGRADES         EDIT, anywhere: the tickets spent. SELECT or KNOB 1 goes down a shelf, PRESETS or KNOB 2 (or
 *                    OCT- OCT+) to the next shelf; what is under the cursor is tried on at once. PLAY buys it (the Hat
 *                    takes the tickets) or puts it on; on the keys' shelves a key gives it to that key's colour. */

enum { PU_OFF, PU_NAME, PU_HELLO, PU_CARD, PU_SHOP };
enum { PA_NONE, PA_CLOSED, PA_MENU, PA_SCORES };
static struct {
    uint8_t on, from_menu, from_card;
    int32_t t;
    char buf[PF_NAME_MAX + 1];                   /* the name being written */
    uint8_t len, cur, dice_i, renaming;
    int32_t dice_t, hop_t;
    uint8_t sel;                                 /* the card's line */
    uint8_t cat, row[PC_N], cut_cat, cut_kind, flash_zone;   /* the shelf, the place on each shelf; what was just bought */
    int32_t cut, poor_t, open_t, flash_t, keys_t;
} pu;

/* ---------------------------------------------------------------- the dice: rock names */
static const char *const NM_POOL[] = {
    "ROCKY ROAD", "SIR ROCKALOT", "PEBBLES", "LIL PEBBLE", "BIG GRAVEL", "ROLLO", "ROCK N ROLL", "STONE COLD",
    "CLIFF HANGER", "GEODE DUDE", "MOSS BOSS", "NOT A POTATO", "JUST A ROCK", "ROCK BOTTOM", "ROCK SOLID", "HARD PLACE",
    "KINDA BOLDER", "IGGY NEOUS", "SEDI MENTAL", "META MORPH", "GNEISS GUY", "TUFF STUFF", "OF QUARTZ", "PUMICE PAL",
    "SLATE NIGHT", "MARBLE MOUTH", "GRANITE GARY", "MR BASALT", "STALAG MIKE", "STALAC TINA", "LAVA LAMP", "CRAGGLES",
    "RUBBLE", "DUSTY", "CHIP", "COBBLE", "PET ROCK", "ROCKTOPUS", "ROCKODILE", "ROCKAMOLE", "ROCKWEILER", "BOULDERELLA",
    "THE BOULDER", "ROLLING STAN", "SIR GRAVELOT", "MAGMA MIA", "ORE WHAT", "AGATE KEEPER", "TALC TO ME", "KARST AWAY",
    "LIMEY STONE", "KIDNEY STONE", "PAPER FEARS", "SMOOTH MOVE",
};
#define NM_POOL_N ((uint32_t)(sizeof NM_POOL / sizeof NM_POOL[0]))
static const char NM_ROLL[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-!?'.&";
#define NM_ROLL_N ((int32_t)sizeof NM_ROLL - 1)

static char nm_key_char(int k)                   /* key k's letter: the top row A to K, the bottom row L to Z and a space */
{
    int i, top = 0, bot = 0;
    for (i = 0; i < k; i++) { if (KEY_IS_SHARP[i]) top++; else bot++; }
    if (KEY_IS_SHARP[k]) return (char)('A' + top);
    return bot < 15 ? (char)('L' + bot) : ' ';
}
static void nm_put(char c)
{
    if (pu.cur >= PF_NAME_MAX) pu.cur = PF_NAME_MAX - 1;
    while (pu.len < pu.cur) pu.buf[pu.len++] = ' ';
    pu.buf[pu.cur] = c;
    if (pu.cur >= pu.len) pu.len = (uint8_t)(pu.cur + 1);
    pu.buf[pu.len] = 0;
    if (pu.cur < PF_NAME_MAX - 1) pu.cur++;
}
static void nm_trim(void)                        /* no spaces at either end */
{
    uint32_t i, a = 0;
    while (pu.len && pu.buf[pu.len - 1] == ' ') pu.len--;
    while (a < pu.len && pu.buf[a] == ' ') a++;
    for (i = a; i < pu.len; i++) pu.buf[i - a] = pu.buf[i];
    pu.len = (uint8_t)(pu.len - a); pu.buf[pu.len] = 0;
    if (pu.cur > pu.len) pu.cur = pu.len;
}
static void nm_dice(void)
{
    uint32_t k = ac_rnd() % NM_POOL_N;
    if (k == pu.dice_i) k = (k + 1u + ac_rnd() % (NM_POOL_N - 1u)) % NM_POOL_N;   /* never the same name twice running */
    pu.dice_i = (uint8_t)k; pu.dice_t = 560;
    for (k = 0; k < 6u; k++) sfx(72 + (int32_t)(ac_rnd() % 12u), k * 80u, 60, W_SQUARE, 1100, 1, 40);
    sfx(84, 520, 300, W_SINE, 2600, 2, 200);
}
static void pu_open(int what, int from_menu)
{
    uint32_t i;
    pfx_clear();
    pu.on = (uint8_t)what; pu.t = 0; pu.from_menu = (uint8_t)from_menu; pu.from_card = 0;
    pu.cut = 0; pu.poor_t = 0; pu.dice_t = 0; pu.hop_t = 0; pu.flash_t = 0; pu.keys_t = 0; pu.open_t = 0;
    if (what == PU_NAME) {
        pu.renaming = (uint8_t)pf_named();
        for (i = 0; i < PF_NAME_MAX && pf.name[i]; i++) pu.buf[i] = pf.name[i];
        pu.len = (uint8_t)i; pu.buf[i] = 0; pu.cur = (uint8_t)imin((int32_t)i, PF_NAME_MAX - 1); pu.dice_i = 255;
    }
    if (what == PU_SHOP) { pu.open_t = 1; sfx(60, 0, 300, W_TRI, 2400, 4, 160); sfx(67, 90, 300, W_TRI, 2400, 4, 160); sfx(72, 180, 500, W_TRI, 2400, 4, 160); }
    else snd_ui(76);
    sy_rumble_want = 0;
}
static void pu_close(void) { pu.on = PU_OFF; pfx_clear(); pj.on = 0; pj.spin = 0; pj.grow = 0; snd_ui(67); }

/* ---------------------------------------------------------------- NAME YOUR ROCK */
static int name_tick(int32_t dt)
{
    int32_t mv = in.enc[ENC_SELECT] + in.enc[ENC_KNOB1], ch = in.enc[ENC_PRESETS] + in.enc[ENC_KNOB2], k;
    if (pu.hop_t > 0) pu.hop_t -= dt;
    led_key_dim_want |= (1u << AC_NKEY) - 1u; led_key_want |= in.keys; led_key_dim_want &= ~led_key_want;
    led_breathe_want |= 1u << BTN_FX;
    if (pu.len) led_play_breathe = 1;
    if (pu.dice_t > 0) {                         /* the dice in the air: nothing else until it lands */
        pu.dice_t -= dt;
        if (pu.dice_t <= 0) {
            const char *s = NM_POOL[pu.dice_i % NM_POOL_N];
            for (k = 0; k < PF_NAME_MAX && s[k]; k++) pu.buf[k] = s[k];
            pu.len = (uint8_t)k; pu.buf[k] = 0; pu.cur = (uint8_t)imin(k, PF_NAME_MAX - 1); pu.hop_t = 420;
        }
        return PA_NONE;
    }
    if (BDOWN(BTN_GLO) || BDOWN(BTN_HOME) || BDOWN(BTN_REC)) {   /* no name, or the old one */
        snd_ui(67);
        if (pu.renaming) { pu.on = PU_CARD; pu.t = 0; return PA_NONE; }
        pu.on = PU_OFF; return pu.from_menu ? PA_MENU : PA_CLOSED;
    }
    if (BDOWN(BTN_FX)) { nm_dice(); return PA_NONE; }
    for (k = 0; k < AC_NKEY; k++)
        if (in.kdown >> k & 1u) { nm_put(nm_key_char(k)); pf_key_voice(k, key_play(k)); pu.hop_t = 300; }
    if (mv) { pu.cur = (uint8_t)iclamp(pu.cur + (mv > 0 ? 1 : -1), 0, imin(pu.len, PF_NAME_MAX - 1)); snd_ui(64 + pu.cur); }
    if (ch) {                                    /* the letter in this place, rolled */
        int32_t i = 0;
        while (pu.len <= pu.cur) pu.buf[pu.len++] = ' ';
        pu.buf[pu.len] = 0;
        while (i < NM_ROLL_N && NM_ROLL[i] != pu.buf[pu.cur]) i++;
        if (i >= NM_ROLL_N) i = 0;
        i = ((i + (ch > 0 ? 1 : -1)) % NM_ROLL_N + NM_ROLL_N) % NM_ROLL_N;
        pu.buf[pu.cur] = NM_ROLL[i]; snd_ui(60 + i % 24);
    }
    if (BDOWN(BTN_OCT_DOWN) && pu.len) {         /* rubbed out: the letter before the place (the first, at the start) */
        int32_t at = pu.cur > 0 ? pu.cur - 1 : 0;
        for (k = at; k < pu.len; k++) pu.buf[k] = pu.buf[k + 1];
        pu.len--; pu.buf[pu.len] = 0; if (pu.cur > 0) pu.cur--;
        snd_ui(55);
    }
    if (BDOWN(BTN_PLAY) || BDOWN(BTN_OCT_UP)) {
        nm_trim();
        if (!pu.len) { nm_dice(); return PA_NONE; }   /* nothing written: the dice decide */
        for (k = 0; k <= pu.len; k++) pf.name[k] = pu.buf[k];
        pf.flags |= PF_NAMED; pf_dirty = 1;
        pu.on = PU_HELLO; pu.t = 0;
        snd_chime(1);
        for (k = 0; k < 14; k++) pfx_add(k & 1 ? PX_STAR : PX_DOT, Q8(120), Q8(96), (icos(k * 4681) * 120) >> 14, ((isin(k * 4681) * 120) >> 14) - 120, 900, th->caps[k % 5]);
#ifdef DINGHY_HOST
        printf("%6u ms  Rock is named %s\n", fm1_ms, pf.name);
#endif
    }
    return PA_NONE;
}
/* a die, 21 px, its middle at (x, y), showing n */
static void dice_draw(int32_t x, int32_t y, int n, col_t c, col_t pip)
{
    static const uint16_t P[7] = {0, 16, 257, 273, 325, 341, 365};   /* a bit for each place of the 3 x 3 that has a pip */
    int32_t i;
    grect(x - 9, y - 10, 19, 21, c); grect(x - 10, y - 9, 21, 19, c);
    for (i = 0; i < 9; i++) if (P[iclamp(n, 1, 6)] >> i & 1u) grect(x - 7 + (i % 3) * 6, y - 7 + (i / 3) * 6, 3, 3, pip);
}
static void name_keyboard(int32_t y0, uint32_t down)
{
    int k;
    for (k = 0; k < AC_NKEY; k++) {
        int32_t x = gh_key_x(k) - 6, y = KEY_IS_SHARP[k] ? y0 : y0 + 19, dn = (int32_t)(down >> k & 1u);
        col_t zc = PF_KEYCOL[KEY_ZONE[k]], ink = dn ? 0 : th->text;
        char s[2];
        s[0] = nm_key_char(k); s[1] = 0;
        if (dn) grect(x, y + 1, 13, 17, zc);
        else { grect(x, y, 13, 17, th->panel); galpha(170); gbox(x, y, 13, 17, zc); galpha(256); }
        if (s[0] == ' ') { grect(x + 3, y + 11 + dn, 7, 2, ink); grect(x + 3, y + 8 + dn, 1, 3, ink); grect(x + 9, y + 8 + dn, 1, 3, ink); }
        else gtextc(x + 7, y + 4 + dn, s, ink, 2);
    }
}
static void name_slots(int32_t y, const char *s, int32_t len, int32_t cur, uint32_t tm, int scramble)
{
    int32_t i;
    for (i = 0; i < PF_NAME_MAX; i++) {
        int32_t x = 12 + i * 18;
        char c[2];
        c[0] = i < len ? s[i] : 0; c[1] = 0;
        if (scramble) c[0] = NM_ROLL[1 + ihash((uint32_t)i * 13u + tm / 50u) % 26u];
        if (i == cur && !scramble) {
            galpha(90 + ipulse(tm, 700) * 110 / 256); grect(x, y - 4, 17, 24, th->pad); galpha(256);
            grect(x, y + 20, 17, 3, th->pad);
        } else grect(x + 1, y + 21, 15, 1, i < len || scramble ? th->text : th->mute);
        if (c[0] && c[0] != ' ') gtextc(x + 9, y, c, i == cur && !scramble ? th->bg : th->text, 3);
    }
}
static void name_draw(void)
{
    uint32_t tm = (uint32_t)fm1_ms;
    int32_t st = ts(2, 3), fa = ts(1, 2), y, hop = pu.hop_t > 0 ? (isin(pu.hop_t * 32768 / 420) * 12) >> 14 : 0, rolling = pu.dice_t > 0;
    grect(0, 0, AC_W, AC_H, th->bg);
    gfit(120, 5 + 2 * st, 232, TR(T_NAME_TITLE), th->caps[3], st);
    deco_rock(100, 56 - hop, 480, tsin(tm, 20, 0) >> 6, (tm % 3100u) < 150u ? 30 : 256, pf.eq, tm);
    {   /* the dice, on FX */
        int32_t dx = 176 + (rolling ? (int32_t)(ihash(tm / 40u) % 9u) - 4 : 0), dy = 46 + (rolling ? -(isin(imin(560 - pu.dice_t, 560) * 32768 / 560) * 14 >> 14) : 0);
        dice_draw(dx, dy, rolling ? 1 + (int)(ihash(tm / 60u + 5u) % 6u) : 1 + pu.dice_i % 6, th->text, th->bg);
        gtextc(176, 62, "FX", th->pad, 2);
    }
    name_slots(84, pu.buf, pu.len, pu.cur, tm, rolling);
    gfit(120, 113, 232, TR(T_NAME_KNOBS), th->mute, 1);
    name_keyboard(124, in.keys);
    y = gfoot(238, TR(T_NAME_DONE), th->pad, th->bg, fa);
    gfoot(y - 1, TR(T_NAME_RUB), th->text, th->bg, fa);
}
/* the name kept: HELLO, and the name large, each letter its own colour, hopping */
static int hello_tick(int32_t dt)
{
    pfx_tick(dt);
    if (pu.t > 2400 || (pu.t > 400 && (BDOWN(BTN_PLAY) || BDOWN(BTN_GLO)))) { pu.on = PU_CARD; pu.t = 0; pu.sel = 1; pfx_clear(); }
    return PA_NONE;
}
static void hello_draw(void)
{
    uint32_t tm = (uint32_t)fm1_ms;
    int32_t sc = 4, w, x, i, hop = (iabs(isin((int32_t)(pu.t * 60))) * 18) >> 14;
    grect(0, 0, AC_W, AC_H, th->bg);
    gfit(120, 22, 232, TR(T_NAME_HELLO), th->mute, ts(2, 3));
    deco_rock(120, 104 - hop, 900, tsin(tm, 40, 0) >> 5, 256, pf.eq, tm);
    while (sc > 1 && gtextw(pf.name, sc) > 228) sc--;
    w = gtextw(pf.name, sc); x = 120 - w / 2;
    for (i = 0; pf.name[i]; i++) {
        char c[2];
        c[0] = pf.name[i]; c[1] = 0;
        gtext(x, 176 - ((iabs(isin((int32_t)(pu.t * 50 - i * 5000))) * 6) >> 14), c, th->caps[(i + pu.t / 200) % 5], sc);
        x += glyph_adv((uint32_t)(uint8_t)c[0], sc);
    }
    pfx_draw(0, 0, 256);
}

/* ---------------------------------------------------------------- the card */
static int card_tick(int32_t dt)
{
    int32_t mv = in.enc[ENC_SELECT] + in.enc[ENC_KNOB1];
    (void)dt;
    if (BDOWN(BTN_GLO) || BDOWN(BTN_HOME)) { pu.on = PU_OFF; snd_ui(67); return pu.from_menu ? PA_MENU : PA_CLOSED; }
    if (mv) { pu.sel = (uint8_t)iclamp(pu.sel + (mv > 0 ? 1 : -1), 0, 2); snd_ui(64 + pu.sel); }
    if (BDOWN(BTN_EDIT)) { pu_open(PU_SHOP, pu.from_menu); pu.from_card = 1; return PA_NONE; }
    if (BDOWN(BTN_PLAY) || BDOWN(BTN_OCT_UP)) {
        if (pu.sel == 0) pu_open(PU_NAME, pu.from_menu);
        else if (pu.sel == 1) { pu_open(PU_SHOP, pu.from_menu); pu.from_card = 1; }
        else { pu.on = PU_OFF; return PA_SCORES; }
    }
    led_play_breathe = 1; led_btn_want |= 1u << BTN_GLO; led_breathe_want |= 1u << BTN_EDIT;
    return PA_NONE;
}
static void card_draw(void)
{
    static const int LAB[3] = {T_PF_RENAME, T_SHOP, T_SCORES};
    uint32_t tm = (uint32_t)fm1_ms;
    char b[24];
    int32_t st = ts(3, 4), a = ts(1, 2), used, y, i, n, rh, ls;
    grect(0, 0, AC_W, AC_H, th->bg);
    used = gfit(120, 6 + 2 * st, 232, pf.name, th->caps[3], st);
    y = 6 + 2 * st + 5 * used + 6;
    grect(8, y, 224, 1, th->mute);
    deco_rock(56, y + 44, 700, tsin(tm, 20, 0) >> 6, (tm % 3100u) < 150u ? 30 : 256, pf.eq, tm);
    grect(26, y + 72, 60, 2, th->mute);
    /* beside him: the tickets in the pocket, large; won in all; how much of the catalogue is his */
    tix_pocket(230, y + 10, pf.tickets, 3, th->text);
    gtextr(230, y + 36, TR(T_PF_WON), th->mute, 1);
    dnum(b, (int32_t)pf.won); gtextr(230, y + 46, b, th->pad, 2);
    gtextr(230, y + 62, TR(T_PF_UNLOCKED), th->mute, 1);
    dnum(b, pf_owned_n()); for (n = 0; b[n]; n++) ;
    b[n++] = '/'; dnum(b + n, pf_priced_n());
    gtextr(230, y + 72, b, th->pad, 2);
    y += 92;
    rh = imin(TLH(ts(2, 3)) + 6, (238 - TLH(a) - 4 - y) / 3);
    ls = imin(ts(2, 3), imax(1, (rh - 6) / 5));
    for (i = 0; i < 3; i++, y += rh) {
        int32_t sel = i == pu.sel;
        if (sel) { grect(6, y, 228, rh - 2, th->panel); gbox(6, y, 228, rh - 2, th->pad); }
        gfit(120, y + (rh - 2 - 5 * ls) / 2, 220, TR(LAB[i]), sel ? th->text : th->mute, ls);
    }
    gfit(120, 238 - TLH(a) + 2 * a, 232, TR(T_CLOSE), th->pad, a);
}

/* ---------------------------------------------------------------- UPGRADES */
#define SP_RX 58                                 /* Rock on his stage */
#define SP_RY 72
static const int SP_CAT_T[PC_N] = {T_CAT_BODY, T_CAT_HAT, T_CAT_FACE, T_CAT_TRAIL, T_CAT_KCOL, T_CAT_KFX, T_CAT_KSND, T_CAT_JUMP};
static int32_t sp_hop_y(void)                    /* how far off his stage a hop has him (pixels) */
{
    int32_t t = 520 - pu.hop_t;
    return pu.hop_t > 0 ? 4 * 26 * t / 520 * (520 - t) / 520 : 0;
}
/* what is under the cursor shown off: he hops (in the jump on show), the keys' effect flies, the voice sounds */
static void sp_demo(int zone, int sound)
{
    int cat = pu.cat, row = pu.row[cat];
    int32_t x = Q8(SP_RX), y = Q8(SP_RY);
    if (pu.hop_t <= 0) {
        pu.hop_t = 520;
        deco_jump_style(cat == PC_JUMP ? row : pf.zone[zone][3], zone, x, y);
    }
    deco_fx_kind(cat == PC_KFX ? row : pf.zone[zone][1], pf_kcol(cat == PC_KCOL ? row : pf.zone[zone][0], zone, fm1_ms), x, y);
    if (sound) pf_voice(cat == PC_KSND ? row : pf.zone[zone][2], 60 + SC_PEN[zone] + 12);
    pu.flash_zone = (uint8_t)zone; pu.flash_t = 300;
}
static void sp_buy(const pf_item_t *it)
{
    uint32_t i;
    pf.tickets -= it->price; pf_own(it->cat, it->kind); pf_dirty = 1;
    if (it->cat < PC_ROCK_N) pf.eq[it->cat] = it->kind;       /* Rock's own: on at once */
    pu.cut = 1; pu.cut_cat = it->cat; pu.cut_kind = it->kind;
    for (i = 0; i < 8u; i++) sfx(76 + (int32_t)(i % 4u) * 3, i * 60u, 40, W_SQUARE, 800, 1, 30);
#ifdef DINGHY_HOST
    printf("%6u ms  bought %s for %u tickets, %u left\n", fm1_ms, it->name, it->price, pf.tickets);
#endif
}
static int shop_tick(int32_t dt)
{
    int32_t mv = in.enc[ENC_SELECT] + in.enc[ENC_KNOB1], ch = in.enc[ENC_PRESETS] + in.enc[ENC_KNOB2], k, n;
    const pf_item_t *it;
    pfx_tick(dt);
    if (pu.hop_t > 0) pu.hop_t -= dt;
    deco_jump_tick(dt, pu.hop_t <= 0, Q8(SP_RX), Q8(SP_RY) - Q8(sp_hop_y()));
    if (pu.poor_t > 0) pu.poor_t -= dt;
    if (pu.flash_t > 0) pu.flash_t -= dt;
    if (pu.keys_t > 0) pu.keys_t -= dt;
    if (pu.open_t > 0) { pu.open_t += dt; if (pu.open_t > 420) pu.open_t = 0; }
    if (pu.cut > 0) {                            /* the Hat is taking the tickets: PLAY goes straight to the unwrapping's end */
        int32_t was = pu.cut;
        pu.cut += dt;
        if (was < 700 && pu.cut >= 700) snd_whoosh();
        if (was < 1300 && pu.cut >= 1300) { snd_chime(1); sfx(48, 0, 900, W_SINE, 4200, 4, 120); if (pu.cut_cat >= PC_ROCK_N) sp_demo(0, 1); }
        if (pu.cut > 300 && pu.cut < 1900 && BDOWN(BTN_PLAY)) pu.cut = 1900;
        if (pu.cut >= 2700) { pu.cut = 0; if (pu.cut_cat >= PC_ROCK_N) pu.keys_t = 4000; }
        led_play_breathe = 1;
        return PA_NONE;
    }
    if (BDOWN(BTN_EDIT) || BDOWN(BTN_HOME) || BDOWN(BTN_GLO) || BDOWN(BTN_REC)) {
        if (pu.from_card) { pu.from_card = 0; pu.on = PU_CARD; pu.t = 0; pfx_clear(); pj.on = 0; pj.spin = 0; pj.grow = 0; snd_ui(67); return PA_NONE; }
        pu_close(); return PA_CLOSED;
    }
    if (BDOWN(BTN_OCT_UP)) ch += 1;
    if (BDOWN(BTN_OCT_DOWN)) ch -= 1;
    if (ch) { pu.cat = (uint8_t)((pu.cat + PC_N + (ch > 0 ? 1 : -1)) % PC_N); snd_ui(72 + pu.cat); pu.poor_t = 0; sp_demo(0, pu.cat == PC_KSND); }
    n = pf_cat_n(pu.cat);
    if (mv) {
        int32_t r = iclamp(pu.row[pu.cat] + (mv > 0 ? 1 : -1), 0, n - 1);
        if (r != pu.row[pu.cat]) { pu.row[pu.cat] = (uint8_t)r; snd_ui(64 + r); pu.poor_t = 0; sp_demo(0, pu.cat == PC_KSND); }
    }
    it = pf_item(pu.cat, pu.row[pu.cat]);
    if (BDOWN(BTN_PLAY)) {
        if (!pf_owned(it->cat, it->kind)) {
            if (pf.tickets >= it->price) sp_buy(it);
            else { pu.poor_t = 1300; sfx(43, 0, 220, W_SAW, 2200, 2, 60); sfx(42, 120, 300, W_SAW, 2200, 2, 60); }
        } else if (it->cat < PC_ROCK_N) { pf.eq[it->cat] = it->kind; pf_dirty = 1; snd_ui(84); sp_demo(0, 0); }
        else pu.keys_t = 4000;                   /* (the keys' shelves: a key gives it to its colour) */
    }
    for (k = 0; k < AC_NKEY; k++)
        if (in.kdown >> k & 1u) {
            int z = KEY_ZONE[k];
            if (pu.cat >= PC_ROCK_N && pf_owned(it->cat, it->kind) && pf.zone[z][pu.cat - PC_ROCK_N] != it->kind) {
                pf.zone[z][pu.cat - PC_ROCK_N] = it->kind; pf_dirty = 1; pu.keys_t = 0;
            }
            sp_demo(z, 0);
            pf_voice(pu.cat == PC_KSND ? it->kind : pf.zone[z][2], key_play(k));
        }
    led_key_want |= in.keys;
    if (pu.cat >= PC_ROCK_N) led_key_dim_want |= ((1u << AC_NKEY) - 1u) & ~in.keys;
    led_play_breathe = 1; led_btn_want |= 1u << BTN_EDIT;
    return PA_NONE;
}
/* a tick, 7 x 6, its top left at (x, y) */
static void sp_tick_mark(int32_t x, int32_t y, col_t c) { gline2(x, y + 3, x + 2, y + 5, c); gline2(x + 2, y + 5, x + 6, y, c); }
/* the Hat has taken the tickets and hands over what was bought */
static void shop_cut_draw(void)
{
    uint32_t tm = (uint32_t)fm1_ms;
    const pf_item_t *it = pf_item(pu.cut_cat, pu.cut_kind);
    int32_t t = pu.cut, i;
    uint8_t e[4];
    for (i = 0; i < 4; i++) e[i] = pf.eq[i];
    if (pu.cut_cat < PC_ROCK_N) e[pu.cut_cat] = pu.cut_kind;
    gveil(th->bg, 29);
    if (t >= 1300) { galpha(imin(256, (t - 1300) * 256 / 150)); grect(0, 0, AC_W, AC_H, th->bg); galpha(256); }
    if (t < 1500) {                              /* the Hat comes up, the tickets fly into him, he shakes */
        int32_t up = ease8(imin(t, 300) * 256 / 300), dn = t > 1300 ? ease8((t - 1300) * 256 / 200) : 0;
        int32_t hx = 120 + (t > 700 && t < 1300 ? (int32_t)(ihash(tm / 30u) % 7u) - 3 : 0), hy = 300 - up * 150 / 256 + dn * 170 / 256;
        if (t > 700 && t < 1300) { galpha((t - 700) * 120 / 600); gdisc(hx, hy - 30, 40 + (t - 700) / 12, th->pad); galpha(256); }
        hat_draw(hx, hy, 300, th->dark ? HEXC(0x0c1216) : th->hat, th->hat_eye, th->hat_pupil, t > 700 ? 256 : 200, 0, th->dark, th->hat_rim, 256);
        for (i = 0; i < 8; i++) {
            int32_t q = iclamp((t - 100 - i * 55) * 256 / 380, 0, 256);
            if (q <= 0 || q >= 256) continue;
            tix_one(206 + (hx - 206) * ease8(q) / 256 - 9, 14 + (hy - 60 - 14) * q / 256 - ((isin(q * 128) * 36) >> 14), 18, 9, th->caps[0]);
        }
    }
    if (t >= 1300) {                             /* a flash, and Rock in the middle in it, turning once; its name under him */
        int32_t q = imin(256, (t - 1300) * 256 / 600), z = 380 + ease8(q) * 560 / 256, sc = 4;
        int32_t hop = pu.cut_cat >= PC_ROCK_N ? sp_hop_y() : 0;
        if (t < 1500) { galpha((1500 - t) * 256 / 200); grect(0, 0, AC_W, AC_H, th->pad); galpha(256); }
        for (i = 0; i < 10; i++) {               /* a ring of sparks going round him */
            int32_t a = i * 6553 + (int32_t)(tm * 18u), r = 62 + (ipulse(tm, 900) >> 4);
            galpha(q); gdisc(120 + ((icos(a) * r) >> 14), 112 + ((isin(a) * r) >> 14), 1 + (i & 1), th->caps[i % 5]); galpha(256);
        }
        deco_rock_live(120, 116 - hop, z, (256 - ease8(q)) * 256, (tm % 3100u) < 150u ? 30 : 256, e);
        galpha(q);
        while (sc > 1 && gtextw(TR(T_SH_NEW), sc) > 220) sc--;
        gtextc(120, 22 - ((iabs(isin((int32_t)(tm * 50u))) * 4) >> 14), TR(T_SH_NEW), th->caps[(tm / 160u) % 5u], sc);
        gfit(120, 176, 228, TR(SP_CAT_T[pu.cut_cat]), th->mute, ts(1, 2));
        gfit(120, 176 + TLH(ts(1, 2)) + 4, 228, it->name, th->text, ts(3, 4));
        galpha(256);
        pfx_draw(Q8(SP_RX - 120), Q8(SP_RY - 116), 256);   /* (the keys' sparks, from where he stands now) */
    }
}
static void shop_draw(void)
{
    uint32_t tm = (uint32_t)fm1_ms;
    const pf_item_t *it = pf_item(pu.cat, pu.row[pu.cat]);
    int32_t n = pf_cat_n(pu.cat), a = ts(1, 2), i, y, yf, top, rh, vis, first, owned = pf_owned(it->cat, it->kind), z;
    uint8_t e[4];
    char b[16];
    col_t cc = th->caps[pu.cat % 5];
    for (i = 0; i < 4; i++) e[i] = pf.eq[i];
    if (pu.cat < PC_ROCK_N) e[pu.cat] = it->kind;             /* tried on */
    grect(0, 0, AC_W, AC_H, th->bg);
    /* the head: UPGRADES, and the pocket */
    gtext(6, 5, TR(T_SHOP), th->caps[3], gtextw(TR(T_SHOP), 2) > 120 ? 1 : 2);
    tix_pocket(234, 5, pf.tickets, 2, pu.poor_t > 0 && (pu.poor_t / 130) & 1 ? th->caps[0] : th->text);
    grect(0, 20, AC_W, 1, th->mute);
    /* the stage: Rock in what he wears with this tried on; under him the keyboard's four colours as they are lit */
    galpha(40); gdisc(SP_RX, SP_RY - 4, 40, cc); galpha(256);
    grect(SP_RX - 34, SP_RY + 22, 68, 2, th->mute);
    deco_rock_live(SP_RX, SP_RY - sp_hop_y(), 640, tsin(tm, 20, 0) >> 6, (tm % 3100u) < 150u ? 30 : 256, e);
    for (z = 0; z < 4; z++) {
        int32_t lit = pu.flash_t > 0 && pu.flash_zone == z;
        grect(SP_RX - 34 + z * 17, SP_RY + 27 - (lit ? 2 : 0), 15, lit ? 7 : 4, pf_kcol(pf.zone[z][0], z, tm));
    }
    pfx_draw(0, 0, 256);
    /* beside him: the shelf between its arrows, where it is among the shelves, the thing's name, its price or that it is his */
    gtext(116, 28, "\xE2\x86\x90", cc, 2); gtextr(236, 28, "\xE2\x86\x92", cc, 2);
    {
        int32_t sc = 2, w;
        while (sc > 1 && gtextw(TR(SP_CAT_T[pu.cat]), sc) > (pu.cat >= PC_ROCK_N ? 74 : 92)) sc--;
        w = gtextw(TR(SP_CAT_T[pu.cat]), sc) + (pu.cat >= PC_ROCK_N ? 14 : 0);
        if (pu.cat >= PC_ROCK_N) for (z = 0; z < 4; z++) grect(176 - w / 2 + (z & 1) * 5, 28 + (z >> 1) * 5, 4, 4, PF_KEYCOL[z]);   /* (the keyboard's) */
        gtext(176 - w / 2 + (pu.cat >= PC_ROCK_N ? 14 : 0), 28 + (10 - 5 * sc) / 2, TR(SP_CAT_T[pu.cat]), cc, sc);
    }
    for (i = 0; i < PC_N; i++) grect(176 - PC_N * 5 + i * 10 + 2, 43, 6, i == pu.cat ? 3 : 1, i == pu.cat ? cc : th->mute);
    gfit(176, 56, 120, it->name, th->text, ts(2, 3));
    y = 56 + 5 * ts(2, 3) + 8;
    if (owned) {
        int worn = it->cat < PC_ROCK_N ? pf.eq[it->cat] == it->kind : 0;
        const char *s = TR(worn ? T_ON : T_SH_OWNED);
        int32_t w = gtextw(s, 2);
        sp_tick_mark(176 - (w + 11) / 2, y + 2, th->caps[2]);
        gtext(176 - (w + 11) / 2 + 11, y, s, th->caps[2], 2);
    } else {
        int32_t w;
        dnum(b, it->price); w = gtextw(b, 3);
        tix_one(176 - (w + 26) / 2, y - 2, 20, 14, cc);
        gtext(176 - (w + 26) / 2 + 26, y - 1, b, pf.tickets >= it->price ? th->text : th->caps[0], 3);
    }
    /* the foot: what PLAY does here (or that the tickets are short), and how to leave */
    yf = gfoot(238, TR(T_SH_CLOSE), th->mute, th->bg, 1);
    if (pu.poor_t > 0) yf = gfoot(yf - 1, TR(T_SH_POOR), th->caps[0], th->bg, a);
    else if (!owned) yf = gfoot(yf - 1, TR(T_SH_BUY), th->pad, th->bg, a);
    else if (it->cat >= PC_ROCK_N) { galpha(pu.keys_t > 0 ? 150 + ipulse(tm, 500) * 106 / 256 : 256); yf = gfoot(yf - 1, TR(T_SH_KEYS), pu.keys_t > 0 ? th->caps[3] : th->pad, th->bg, a); galpha(256); }
    else if (pf.eq[it->cat] != it->kind) yf = gfoot(yf - 1, TR(T_SH_USE), th->pad, th->bg, a);
    /* the shelf: as many as fit, moving under the cursor */
    top = SP_RY + 38; rh = TLH(a) + 4; vis = imax(1, (yf - 4 - top) / rh);
    first = iclamp(pu.row[pu.cat] - vis / 2, 0, imax(0, n - vis));
    for (i = first, y = top; i < first + vis && i < n; i++, y += rh) {
        const pf_item_t *r = pf_item(pu.cat, i);
        int32_t sel = i == pu.row[pu.cat], own = pf_owned(r->cat, r->kind), sc = a, xr = 228;
        if (sel) { grect(4, y, 232, rh - 1, th->panel); gbox(4, y, 232, rh - 1, cc); if (pu.poor_t > 1000) gbox(5, y + 1, 230, rh - 3, th->caps[0]); }
        if (!own) {
            dnum(b, r->price);
            gtextr(xr, y + (rh - 1 - 5 * a) / 2 + 1, b, pf.tickets >= r->price ? (sel ? th->text : th->mute) : th->caps[0], a);
            xr -= gtextw(b, a) + 4;
            tix_one(xr - 10, y + (rh - 8) / 2, 10, 7, sel ? cc : th->mute); xr -= 14;
        } else if (r->cat < PC_ROCK_N) {
            if (pf.eq[r->cat] == r->kind) { gdisc(xr - 5, y + rh / 2 - 1, 3, th->caps[2]); xr -= 14; }
            else { sp_tick_mark(xr - 8, y + rh / 2 - 4, th->mute); xr -= 14; }
        } else {                                 /* which of the four colours wear it: a square each */
            for (z = 3; z >= 0; z--) {
                if (pf.zone[z][r->cat - PC_ROCK_N] == r->kind) grect(xr - 7, y + rh / 2 - 4, 7, 7, PF_KEYCOL[z]);
                else { galpha(90); gbox(xr - 7, y + rh / 2 - 4, 7, 7, th->mute); galpha(256); }
                xr -= 9;
            }
            xr -= 4;
        }
        while (sc > 1 && gtextw(r->name, sc) > xr - 22) sc--;
        gtext(10, y + (rh - 1 - 5 * sc) / 2 + 1, r->name, sel ? th->text : th->mute, sc);
        if (r->cat == PC_KCOL && r->kind) grect(10 + gtextw(r->name, sc) + 6, y + rh / 2 - 3, 6, 6, pf_kcol(r->kind, 0, tm));
    }
    if (first > 0) gtextc(120, top - 7, "\xE2\x86\x91", th->mute, 1);
    if (first + vis < n) gtextc(120, yf - 6, "\xE2\x86\x93", th->mute, 1);
    if (pu.cut > 0) shop_cut_draw();
    if (pu.open_t > 0) {                         /* the shutters, going apart as it opens */
        int32_t w = 120 - ease8(pu.open_t * 256 / 420) * 120 / 256;
        grect(0, 0, w, AC_H, th->panel); grect(AC_W - w, 0, w, AC_H, th->panel);
        grect(w - 2, 0, 2, AC_H, th->pad); grect(AC_W - w, 0, 2, AC_H, th->pad);
    }
}

/* ---------------------------------------------------------------- all of them
 * (noinline: the FM-1's compiler puts every screen that has one caller into arcave_frame, and with these in it too a
 * jump across that one function no longer reached: "fixup_extend_jump_imm17m2: imm out of range") */
static __attribute__((noinline)) int pu_tick(int32_t dt)
{
    pu.t += dt;
    switch (pu.on) {
    case PU_NAME: return name_tick(dt);
    case PU_HELLO: return hello_tick(dt);
    case PU_CARD: return card_tick(dt);
    case PU_SHOP: return shop_tick(dt);
    }
    return PA_NONE;
}
static __attribute__((noinline)) void pu_draw(void)
{
    switch (pu.on) {
    case PU_NAME: name_draw(); break;
    case PU_HELLO: hello_draw(); break;
    case PU_CARD: card_draw(); break;
    case PU_SHOP: shop_draw(); break;
    }
}
