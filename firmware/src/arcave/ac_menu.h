/* SPDX-License-Identifier: GPL-3.0-only */
/* The settings, on GLO: the game holds still under a sheet of dark glass. SELECT (or KNOB 1) moves, PRESETS (or
 * KNOB 2) changes; PLAY or OCT+ confirms or steps on (OCT- steps back: in here they do not change the language);
 * GLO closes. ABOUT tells what the Mystic Arcave is. The first line is the player's profile (ac_shop.h); the last
 * wipes the whole save, and only here: PLAY held on it for three seconds. BLUETOOTH is shown where there is a radio. */

enum { MI_PROFILE, MI_LANG, MI_TEXT, MI_EAR, MI_LOOK, MI_LINK, MI_BT, MI_ABOUT, MI_REPLAY, MI_SKIP, MI_RESET, MI_WIPE, MI_N };
enum { MA_NONE, MA_REPLAY, MA_RESET, MA_CHANGED, MA_SKIP, MA_PROFILE, MA_WIPE, MA_BT };
static struct { uint8_t on, sel, about, sure; int32_t t, win, rest, hold; } mn;
static int mi_shown(int i) { return i != MI_BT || bt_state() != BT_NONE; }
#ifndef APP_VER
#define APP_VER "dev"
#endif

static void menu_open(void) { mn.on = 1; mn.sel = 0; mn.about = 0; mn.sure = 0; mn.t = 0; mn.hold = 0; snd_ui(76); }
static void set_lang(int l) { lang = (uint8_t)(((l % L_N) + L_N) % L_N); }
static void set_look(int l) { look_i = (uint8_t)(l & 1); th = &LOOKS[look_i]; }

static int menu_tick(int32_t dt)
{
    int32_t mv = in.enc[ENC_SELECT] + in.enc[ENC_KNOB1], ch = in.enc[ENC_PRESETS] + in.enc[ENC_KNOB2];
    int act = MA_NONE, ok = BDOWN(BTN_PLAY) || BDOWN(BTN_OCT_UP);
    mn.t += dt;
    if (BDOWN(BTN_OCT_DOWN)) ch -= 1;
    if (mn.about) {                              /* PLAY: on, a screenful at a time, then the next part; GLO: back */
        if (ok || ch > 0) {
            snd_ui(72);
            if (mn.rest) mn.win += mn.rest;
            else { mn.about++; mn.win = 0; if (mn.about > 4) mn.about = 0; }
        }
        if (ch < 0) { if (mn.win) mn.win = 0; else if (mn.about > 1) mn.about--; }
        if (BDOWN(BTN_GLO)) { mn.about = 0; snd_ui(67); }
        led_play_breathe = 1;
        return MA_NONE;
    }
    if (BDOWN(BTN_GLO) || BDOWN(BTN_HOME)) { mn.on = 0; snd_ui(67); return MA_NONE; }   /* (arcave.c: this press does not reopen it) */
    if (mv) {                                    /* (past a line that is not shown) */
        int32_t k = mn.sel, d = mv > 0 ? 1 : -1;
        do k += d; while (k >= 0 && k < MI_N && !mi_shown(k));
        if (k >= 0 && k < MI_N) mn.sel = (uint8_t)k;
        mn.sure = 0; mn.hold = 0; snd_ui(64 + mn.sel);
    }
    if (mn.sel == MI_WIPE) {                     /* the whole save: PLAY held three seconds, a bar filling */
        led_play_breathe = 1; led_btn_want |= 1u << BTN_GLO;
        if (BHELD(BTN_PLAY)) {
            if (!mn.hold) snd_ui(60);
            mn.hold += dt;
            if (mn.hold >= 3000) { mn.hold = 0; mn.on = 0; return MA_WIPE; }
        } else mn.hold = 0;
        return MA_NONE;
    }
    if (ok && !ch) ch = 1;
    if (ch) {
        int d = ch > 0 ? 1 : -1;
        switch (mn.sel) {
        case MI_PROFILE: if (ok) { mn.on = 0; act = MA_PROFILE; } break;
        case MI_BT: act = MA_BT; break;          /* (arcave.c turns the radio and keeps the setting) */
        case MI_LANG: set_lang(lang + d); act = MA_CHANGED; break;
        case MI_TEXT: tsize = (uint8_t)((tsize + 3 + d) % 3); act = MA_CHANGED; break;
        case MI_EAR: sy_ear = !sy_ear; act = MA_CHANGED; break;
        case MI_LOOK: set_look(look_i + 1); act = MA_CHANGED; break;
        case MI_LINK: link_id = (uint8_t)((link_id + 5 + d) % 5); act = MA_CHANGED; break;
        case MI_ABOUT: mn.about = 1; mn.win = 0; break;
        case MI_REPLAY: if (ok) { mn.on = 0; act = MA_REPLAY; } break;
        case MI_SKIP: if (ok) { mn.on = 0; act = MA_SKIP; } break;
        case MI_RESET: if (ok) { if (mn.sure) { mn.on = 0; act = MA_RESET; } else mn.sure = 1; } break;
        }
        snd_ui(79);
    }
    led_btn_want |= 1u << BTN_GLO;
    return act;
}
static void menu_draw(void)
{
    static const int LAB[MI_N] = {T_PROFILE, T_LANGUAGE, T_TEXTSIZE, T_EAR, T_LOOK, T_LINK, T_BT, T_ABOUT, T_REPLAY, T_SKIP, T_RESET, T_WIPE_ALL};
    static const int BTS[5] = {T_OFF, T_BT_START, T_BT_ADV, T_BT_CONN, T_BT_FAIL};
    int bt_up = mn.sel == MI_BT && bt_state() != BT_OFF && bt_state() != BT_NONE;   /* the radio's line, and it is doing something */
    static const int LKS[4] = {T_LINK_ALONE, T_LINK_FOLLOW, T_LINK_LEAD, T_LINK_CLOCK};
    int32_t i, y, sh = ts(2, 4), sf = ts(1, 2), rh, vis, first, y0, foot;
    gveil(th->bg, 25);
    if (mn.about) {
        static char buf[280];
        const char *s = TR(T_ABOUT_1 + mn.about - 1), *rest = 0;
        int32_t n = 0, sc = ts(2, 4), top, room;
        while (*s && n < 240) buf[n++] = *s++;
        if (mn.about == 4) {                     /* the version; and the battery's raw reading (to set its scale by) */
            s = APP_VER; while (*s && n < 260) buf[n++] = *s++;
            if (batt_raw >= 0) { int32_t v = batt_raw; buf[n++] = ' '; buf[n++] = 'B'; buf[n++] = (char)('0' + v / 1000 % 10); buf[n++] = (char)('0' + v / 100 % 10); buf[n++] = (char)('0' + v / 10 % 10); buf[n++] = (char)('0' + v % 10); }
        }
        buf[n] = 0;
        gfit(120, 6 + 2 * sh, 232, "FM-ARCAVE", th->caps[3], sh);
        top = 6 + TLH(sh) + 4; room = 238 - TLH(sf) - 4 - top;
        gtext_wrap(12, top + 2 * sc, 216, buf + imin(mn.win, n), th->text, sc, TLH(sc), -1, 0, 0, 1, imax(1, room / TLH(sc)), &rest);
        mn.rest = *rest ? (int32_t)(rest - (buf + imin(mn.win, n))) : 0;
        for (i = 0; i < 4; i++) grect(10 + i * 10, 232 - 2 * sf, 6, 2 * sf, i == mn.about - 1 ? th->pad : th->mute);
        gtextr(230, 238 - TLH(sf) + 2 * sf, mn.rest ? PB " \xE2\x86\x93" : PB " \xE2\x86\x92", th->pad, sf);
        return;
    }
    {   /* the title, and the FM-1's battery in the top right corner (when there is one to read) */
        int32_t bp = batt_pct();
        gfit(120, 6 + 2 * sh, bp >= 0 ? 160 : 232, TR(T_SETTINGS), th->text, sh);
        if (bp >= 0 && !(batt_low && (fm1_ms / 500u) & 1u)) batt_draw(237, 3, bp, 1, th->mute, th->caps[0]);
    }
    y0 = 6 + TLH(sh) + 6;
    /* the foot: what the selected line has to say (or which knob does what), and how to close */
    foot = 238 - TLH(sf);
    /* (the radio: the name a computer lists it under, in the place of how to close) */
    gfit(120, foot + 2 * sf, 232, bt_up && bt_name()[0] ? bt_name() : TR(T_CLOSE), th->pad, sf);
    foot -= TLH(sf);
    if (bt_up) gfit(120, foot + 2 * sf, 232, TR(BTS[imin(bt_state(), 4)]), bt_state() == BT_FAILED ? th->caps[0] : th->caps[3], sf);   /* what it is doing */
    else
    gfit(120, foot + 2 * sf, 232, mn.sel == MI_LINK && link_id ? TR(LKS[lk.state]) : mn.sel == MI_RESET && mn.sure ? TR(T_SURE)
         : "SELECT \xE2\x86\x91\xE2\x86\x93   OCT+ OK", mn.sel == MI_RESET && mn.sure ? th->caps[0] : mn.sel == MI_LINK && link_id ? th->caps[3] : th->mute, sf);
    /* the lines: as many as fit, the list moving under the selected one. A line too long for the size wraps its
     * name onto a second row rather than shrink (small print is what the size setting is there to avoid) */
    {
        int32_t hs[MI_N], scs[MI_N], lw[MI_N], smin = imin(sh, ts(1, 2)), room = foot - 4 - y0, tot;
        const char *vals[MI_N];
        static char idb[2];
        rh = TLH(sh) + 6;
        for (i = 0; i < MI_N; i++) {
            const char *lab = TR(LAB[i]), *val = "";
            int32_t sc = sh;
            if (!mi_shown(i)) { vals[i] = val; scs[i] = sc; lw[i] = 216; hs[i] = 0; continue; }
            if (i == MI_PROFILE) val = pf_named() ? pf.name : "\xE2\x86\x92";
            else if (i == MI_BT) val = TR(pf.flags & PF_BT ? T_ON : T_OFF);
            else if (i == MI_LANG) val = TR(T_LANG);
            else if (i == MI_TEXT) val = TR(T_SMALL + tsize);
            else if (i == MI_EAR) val = TR(sy_ear ? T_ON : T_OFF);
            else if (i == MI_LOOK) val = look_i ? "SLOW GLASS" : "BLACK OPAL";
            else if (i == MI_LINK) { if (link_id) { idb[0] = (char)('0' + link_id); idb[1] = 0; val = idb; } else val = TR(T_OFF); }
            while (sc > smin && gtextw(lab, sc) + gtextw(val, sc) + 14 > 216) sc--;
            vals[i] = val; scs[i] = sc; lw[i] = 216 - (val[0] ? gtextw(val, sc) + 10 : 0);
            hs[i] = imax(rh, gblock_h(lw[i], lab, sc) + 8);
        }
        first = mn.sel;                          /* the selected line, then lines after it, then before it, while there is room */
        tot = hs[mn.sel]; vis = 1;
        while (first + vis < MI_N && tot + hs[first + vis] <= room && first + vis <= mn.sel + 1) { tot += hs[first + vis]; vis++; }
        while (first > 0 && tot + hs[first - 1] <= room) { first--; tot += hs[first]; vis++; }
        while (first + vis < MI_N && tot + hs[first + vis] <= room) { tot += hs[first + vis]; vis++; }
        for (i = first, y = y0; i < first + vis; y += hs[i], i++) {
            int32_t sel = i == mn.sel, bh = gblock_h(lw[i], TR(LAB[i]), scs[i]);
            if (!hs[i]) continue;
            if (sel) { grect(6, y, 228, hs[i] - 2, th->panel); gbox(6, y, 228, hs[i] - 2, i == MI_WIPE ? th->caps[0] : th->pad); }
            gblock(12, y + (hs[i] - 2 - bh) / 2, lw[i], TR(LAB[i]), sel ? th->text : th->mute, scs[i], 0);
            gtextr(228, y + (hs[i] - 2 - 5 * scs[i]) / 2 + 1, vals[i], sel ? th->pad : th->mute, scs[i]);
        }
    }
    if (first > 0) gtextc(120, y0 - 6, "\xE2\x86\x91", th->mute, 1);
    if (first + vis < MI_N) gtextc(120, foot - 5, "\xE2\x86\x93", th->mute, 1);
    if (mn.sel == MI_WIPE) {                     /* what holding PLAY here does, and a bar that fills while it is held */
        int32_t a = ts(1, 3), h = gblock_h(204, TR(T_WIPE), a), yb = y0 + 2;
        grect(8, yb, 224, h + 30, th->panel); gbox(8, yb, 224, h + 30, th->caps[0]);
        gblock(18, yb + 6, 204, TR(T_WIPE), th->text, a, 1);
        gbox(30, yb + h + 14, 180, 9, th->mute);
        grect(32, yb + h + 16, 176 * imin(mn.hold, 3000) / 3000, 5, th->caps[0]);
    }
}
