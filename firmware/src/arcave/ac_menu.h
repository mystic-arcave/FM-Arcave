/* SPDX-License-Identifier: GPL-3.0-only */
/* The settings, on GLO: the game holds still under a sheet of dark glass. SELECT (or KNOB 1) moves, PRESETS (or
 * KNOB 2, or PLAY) changes; GLO closes. ABOUT tells what the Mystic Arcave is. */

enum { MI_LANG, MI_LOOK, MI_LINK, MI_ABOUT, MI_REPLAY, MI_RESET, MI_N };
enum { MA_NONE, MA_REPLAY, MA_RESET, MA_CHANGED };
static struct { uint8_t on, sel, about, sure; int32_t t; } mn;
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
    if (mn.about) {
        if (BDOWN(BTN_PLAY) || ch > 0) { mn.about++; snd_ui(72); if (mn.about > 4) mn.about = 0; }
        if (ch < 0 && mn.about > 1) mn.about--;
        if (BDOWN(BTN_GLO)) { mn.about = 0; snd_ui(67); }
        return MA_NONE;
    }
    if (BDOWN(BTN_GLO) || BDOWN(BTN_HOME)) { mn.on = 0; snd_ui(67); return MA_NONE; }   /* (arcave.c: this press does not reopen it) */
    if (mv) { mn.sel = (uint8_t)iclamp(mn.sel + (mv > 0 ? 1 : -1), 0, MI_N - 1); mn.sure = 0; snd_ui(64 + mn.sel); }
    if (BDOWN(BTN_PLAY) && !ch) ch = 1;
    if (ch) {
        int d = ch > 0 ? 1 : -1;
        switch (mn.sel) {
        case MI_LANG: set_lang(lang + d); act = MA_CHANGED; break;
        case MI_LOOK: set_look(look_i + 1); act = MA_CHANGED; break;
        case MI_LINK: link_id = (uint8_t)((link_id + 5 + d) % 5); act = MA_CHANGED; break;
        case MI_ABOUT: mn.about = 1; break;
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
    static const int LAB[MI_N] = {T_LANGUAGE, T_LOOK, T_LINK, T_ABOUT, T_REPLAY, T_RESET};
    static const int LKS[4] = {T_LINK_ALONE, T_LINK_FOLLOW, T_LINK_LEAD, T_LINK_CLOCK};
    int32_t i, y = 46;
    gveil(th->bg, 25);
    if (mn.about) {
        char buf[260]; const char *s = TR(T_ABOUT_1 + mn.about - 1); int32_t n = 0;
        while (*s && n < 240) buf[n++] = *s++;
        if (mn.about == 4) { s = APP_VER; while (*s && n < 256) buf[n++] = *s++; }
        buf[n] = 0;
        gtextc(120, 12, "FM ARCAVE", th->caps[3], 2);
        gtext_wrap(14, 42, 212, buf, th->text, 2, 15, -1, 0, 0, 1);
        for (i = 0; i < 4; i++) grect(102 + i * 10, 216, 6, 2, i == mn.about - 1 ? th->pad : th->mute);
        gtext(10, 226, TR(T_CLOSE), th->mute, 1); gtextr(230, 226, TR(T_PAGE), th->pad, 1);
        return;
    }
    gtextc(120, 12, TR(T_SETTINGS), th->text, 2);
    gtextc(120, 28, TR(T_PAUSED), th->mute, 1);
    for (i = 0; i < MI_N; i++) {
        const char *lab = TR(LAB[i]), *val = "";
        char idb[2] = {0, 0};
        int32_t sc, sel = i == mn.sel;
        if (i == MI_LANG) val = TR(T_LANG);
        else if (i == MI_LOOK) val = look_i ? "SLOW GLASS" : "BLACK OPAL";
        else if (i == MI_LINK) { if (link_id) { idb[0] = (char)('0' + link_id); val = idb; } else val = TR(T_OFF); }
        sc = gtextw(lab, 2) + gtextw(val, 2) + 16 <= 212 ? 2 : 1;
        if (sel) { grect(8, y - 5, 224, 21, th->panel); gbox(8, y - 5, 224, 21, th->pad); }
        gtext(14, y + (sc == 1 ? 3 : 0), lab, sel ? th->text : th->mute, sc);
        gtextr(226, y + (sc == 1 ? 3 : 0), val, sel ? th->pad : th->mute, sc);
        y += 24;
    }
    if (mn.sel == MI_LINK) gtextc(120, y + 2, link_id ? TR(LKS[lk.state]) : "", th->caps[3], 1);
    if (mn.sel == MI_RESET && mn.sure) gtextc(120, y + 2, TR(T_SURE), th->caps[0], 1);
    gtext_wrap(8, 206, 224, TR(T_MENU_HELP), th->mute, 1, 9, -1, 1, 0, 1);
    gtextc(120, 228, TR(T_CLOSE), th->pad, 1);
}
