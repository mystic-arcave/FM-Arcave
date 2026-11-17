/* SPDX-License-Identifier: GPL-3.0-only */
/* The settings, on GLO: the game holds still under a sheet of dark glass. SELECT (or KNOB 1) moves, PRESETS (or
 * KNOB 2, or PLAY) changes; GLO closes. ABOUT tells what the Mystic Arcave is. */

enum { MI_LANG, MI_TEXT, MI_LOOK, MI_LINK, MI_ABOUT, MI_REPLAY, MI_RESET, MI_N };
enum { MA_NONE, MA_REPLAY, MA_RESET, MA_CHANGED };
static struct { uint8_t on, sel, about, sure; int32_t t, win, rest; } mn;
#ifndef APP_VER
#define APP_VER "dev"
#endif

static void menu_open(void) { mn.on = 1; mn.sel = 0; mn.about = 0; mn.sure = 0; mn.t = 0; snd_ui(76); }
static void set_lang(int l) { lang = (uint8_t)(((l % L_N) + L_N) % L_N); }
static void set_look(int l) { look_i = (uint8_t)(l & 1); th = &LOOKS[look_i]; }

static int menu_tick(int32_t dt)
{
    int32_t mv = in.enc[ENC_SELECT] + in.enc[ENC_KNOB1], ch = in.enc[ENC_PRESETS] + in.enc[ENC_KNOB2];
    int act = MA_NONE;
    mn.t += dt;
    if (mn.about) {                              /* PLAY: on, a screenful at a time, then the next part; GLO: back */
        if (BDOWN(BTN_PLAY) || ch > 0) {
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
    if (mv) { mn.sel = (uint8_t)iclamp(mn.sel + (mv > 0 ? 1 : -1), 0, MI_N - 1); mn.sure = 0; snd_ui(64 + mn.sel); }
    if (BDOWN(BTN_PLAY) && !ch) ch = 1;
    if (ch) {
        int d = ch > 0 ? 1 : -1;
        switch (mn.sel) {
        case MI_LANG: set_lang(lang + d); act = MA_CHANGED; break;
        case MI_TEXT: tsize = (uint8_t)((tsize + 3 + d) % 3); act = MA_CHANGED; break;
        case MI_LOOK: set_look(look_i + 1); act = MA_CHANGED; break;
        case MI_LINK: link_id = (uint8_t)((link_id + 5 + d) % 5); act = MA_CHANGED; break;
        case MI_ABOUT: mn.about = 1; mn.win = 0; break;
        case MI_REPLAY: if (BDOWN(BTN_PLAY)) { mn.on = 0; act = MA_REPLAY; } break;
        case MI_RESET: if (BDOWN(BTN_PLAY)) { if (mn.sure) { mn.on = 0; act = MA_RESET; } else mn.sure = 1; } break;
        }
        snd_ui(79);
    }
    led_btn_want |= 1u << BTN_GLO;
    return act;
}
static void menu_draw(void)
{
    static const int LAB[MI_N] = {T_LANGUAGE, T_TEXTSIZE, T_LOOK, T_LINK, T_ABOUT, T_REPLAY, T_RESET};
    static const int LKS[4] = {T_LINK_ALONE, T_LINK_FOLLOW, T_LINK_LEAD, T_LINK_CLOCK};
    int32_t i, y, sh = ts(2, 4), sf = ts(1, 2), rh, vis, first, y0, foot;
    gveil(th->bg, 25);
    if (mn.about) {
        static char buf[280];
        const char *s = TR(T_ABOUT_1 + mn.about - 1), *rest = 0;
        int32_t n = 0, sc = ts(2, 4), top, room;
        while (*s && n < 240) buf[n++] = *s++;
        if (mn.about == 4) { s = APP_VER; while (*s && n < 270) buf[n++] = *s++; }
        buf[n] = 0;
        gfit(120, 6 + 2 * sh, 232, "FM ARCAVE", th->caps[3], sh);
        top = 6 + TLH(sh) + 4; room = 238 - TLH(sf) - 4 - top;
        gtext_wrap(12, top + 2 * sc, 216, buf + imin(mn.win, n), th->text, sc, TLH(sc), -1, 0, 0, 1, imax(1, room / TLH(sc)), &rest);
        mn.rest = *rest ? (int32_t)(rest - (buf + imin(mn.win, n))) : 0;
        for (i = 0; i < 4; i++) grect(10 + i * 10, 232 - 2 * sf, 6, 2 * sf, i == mn.about - 1 ? th->pad : th->mute);
        gtextr(230, 238 - TLH(sf) + 2 * sf, mn.rest ? "PLAY \xE2\x86\x93" : "PLAY \xE2\x86\x92", th->pad, sf);
        return;
    }
    gfit(120, 6 + 2 * sh, 232, TR(T_SETTINGS), th->text, sh);
    y0 = 6 + TLH(sh) + 6;
    /* the foot: what the selected line has to say (or which knob does what), and how to close */
    foot = 238 - TLH(sf);
    gfit(120, foot + 2 * sf, 232, TR(T_CLOSE), th->pad, sf);
    foot -= TLH(sf);
    gfit(120, foot + 2 * sf, 232, mn.sel == MI_LINK && link_id ? TR(LKS[lk.state]) : mn.sel == MI_RESET && mn.sure ? TR(T_SURE)
         : "SELECT \xE2\x86\x91\xE2\x86\x93  PRESETS \xE2\x86\x90\xE2\x86\x92", mn.sel == MI_RESET && mn.sure ? th->caps[0] : mn.sel == MI_LINK && link_id ? th->caps[3] : th->mute, sf);
    /* the lines: as many as fit, the list moving under the selected one */
    rh = TLH(sh) + 6; vis = imax(1, (foot - 4 - y0) / rh);
    first = iclamp(mn.sel - vis / 2, 0, imax(0, MI_N - vis));
    for (i = first, y = y0; i < MI_N && i < first + vis; i++, y += rh) {
        const char *lab = TR(LAB[i]), *val = "";
        char idb[2] = {0, 0};
        int32_t sc = sh, sel = i == mn.sel;
        if (i == MI_LANG) val = TR(T_LANG);
        else if (i == MI_TEXT) val = TR(T_SMALL + tsize);
        else if (i == MI_LOOK) val = look_i ? "SLOW GLASS" : "BLACK OPAL";
        else if (i == MI_LINK) { if (link_id) { idb[0] = (char)('0' + link_id); val = idb; } else val = TR(T_OFF); }
        while (sc > 1 && gtextw(lab, sc) + gtextw(val, sc) + 14 > 216) sc--;
        if (sel) { grect(6, y, 228, rh - 2, th->panel); gbox(6, y, 228, rh - 2, th->pad); }
        gtext(12, y + (rh - 2 - 5 * sc) / 2 + 1, lab, sel ? th->text : th->mute, sc);
        gtextr(228, y + (rh - 2 - 5 * sc) / 2 + 1, val, sel ? th->pad : th->mute, sc);
    }
    if (first > 0) gtextc(120, y0 - 6, "\xE2\x86\x91", th->mute, 1);
    if (first + vis < MI_N) gtextc(120, foot - 5, "\xE2\x86\x93", th->mute, 1);
}
