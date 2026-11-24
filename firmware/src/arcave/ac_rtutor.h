/* SPDX-License-Identifier: GPL-3.0-only */
/* The music room's lessons. A lesson is the instrument itself, played by the lesson: each step holds a caption and an
 * act that works the instrument's own controls while the caption is up, so what is shown is what will happen. A
 * picture of the knob, key row or button in use stands beside the caption. PLAY: the next step. HOME or REC: enough.
 * The instrument as the player left it is put away first and brought back after; nothing else in the room is heard. */
static struct { rinst_t inst; uint8_t strip[8], edit, menu, sc_mask, scv[SCR_N]; } rt_keep;
static void rtutor_open(int who, int then)
{
    int i;
    if (!RD[who]->nlesson) return;
    rt_keep.inst = ri[who];
    for (i = 0; i < 8; i++) rt_keep.strip[i] = ((const uint8_t *)&rmx[who])[i];
    for (i = 0; i < SCR_N; i++) rt_keep.scv[i] = rm.scv[i];
    rt_keep.sc_mask = rm.sc_mask;
    rmx[who].mute = 0; rmx[who].lvl = 12;
    rt.on = 1; rt.who = (uint8_t)who; rt.then = (uint8_t)then; rt.step = 0; rt.t = 0; rt.icon = 0; rt.label_t = 0;
    rm.tut = (uint8_t)(who + 1); rm.focus = (uint8_t)who; rm.edit = 0; rm.menu = 0; rm.rec = 0;
    snd_ui(79);
}
static void rtutor_close(void)
{
    int i, who = rt.who;
    ri[who] = rt_keep.inst;
    for (i = 0; i < 8; i++) ((uint8_t *)&rmx[who])[i] = rt_keep.strip[i];
    for (i = 0; i < SCR_N; i++) rm.scv[i] = rt_keep.scv[i];
    rm.sc_mask = rt_keep.sc_mask;
    for (i = 0; i < RP_N; i++) ri[who].fl[i] = 0;
    RD[who]->init();
    rt.on = 0; rm.tut = 0; rm.edit = 0; rm.menu = 0; rm.focus = rt.then;
    rh.zoom = 0; rh.closing = 0;
    snd_ui(67);
}
static void rtutor_tick(int32_t dt)
{
    const rdesc_t *d = RD[rt.who];
    rt.t += dt;
    if (rt.label_t > 0) rt.label_t -= dt;
    d->lesson[rt.step].act(rt.t, dt);
    led_play_breathe = 1; led_breathe_want |= 1u << BTN_HOME;
    if (rt.icon >= RTI_BTN && (rt.t / 400) % 2 == 0) led_btn_want |= 1u << (rt.icon - RTI_BTN);
    ri_leds(rt.who);
    if (BDOWN(BTN_HOME) || BDOWN(BTN_REC)) { rtutor_close(); return; }
    if (BDOWN(BTN_PLAY) || rt.t >= d->lesson[rt.step].ms) {
        if (rt.step + 1 >= d->nlesson) { rtutor_close(); snd_chime(0); return; }
        rt.step++; rt.t = 0; snd_ui(72);
    }
}
static void rt_icon(int32_t x, int32_t y, int icon, col_t c)   /* the control in use, at (x, y), about 36 across */
{
    static const char *const KN[7] = {"KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4", "SELECT", "PRESETS", "ALGORITHM"};
    static const char *const BN[14] = {"OCT-", "OCT+", "FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"};
    int32_t k;
    if (icon >= RTI_K1 && icon <= RTI_ALGO) {    /* a knob, turning one way and back */
        int32_t a = (tsin(rm.t, 30, 0) * 12000) >> 14;
        gcirc(x, y - 4, 10, th->text); gcirc(x, y - 4, 9, th->text);
        gline2(x, y - 4, x + ((8 * icos(a - 16384)) >> 14), y - 4 + ((8 * isin(a - 16384)) >> 14), c);
        garc(x, y - 4, 14, 14, 36000, 62000, c, 0);
        gfit(x, y + 13, 58, KN[icon - RTI_K1], th->text, 1);
    } else if (icon == RTI_LOW || icon == RTI_TOP) {   /* the two rows of keys, the one in use lit */
        for (k = 0; k < 8; k++) { if (icon == RTI_LOW && ((rm.t / 260u) % 8u) == (uint32_t)k) grect(x - 20 + k * 5, y - 2, 4, 9, c); else gbox(x - 20 + k * 5, y - 2, 4, 9, icon == RTI_LOW ? th->text : th->mute); }
        for (k = 0; k < 6; k++) { int32_t kx = x - 18 + k * 5 + (k >= 2 ? 5 : 0) + (k >= 5 ? 5 : 0); if (icon == RTI_TOP && ((rm.t / 260u) % 6u) == (uint32_t)k) grect(kx, y - 12, 4, 8, c); else gbox(kx, y - 12, 4, 8, icon == RTI_TOP ? th->text : th->mute); }
        gfit(x, y + 13, 56, icon == RTI_LOW ? "LOW KEYS" : "TOP KEYS", th->text, 1);
    } else if (icon >= RTI_BTN && icon < RTI_BTN + 14) {   /* a button, pressed and let go */
        int down = (rm.t / 400u) % 2u == 0u;
        if (icon - RTI_BTN == BTN_PLAY) { play_big = 1; play_draw(x - play_w(1) / 2, y - 6, 1); play_big = 0; return; }
        if (down) grect(x - 16, y - 9, 32, 16, c); else gbox(x - 16, y - 9, 32, 16, c);
        gtextc(x, y - 3, BN[icon - RTI_BTN], down ? th->bg : th->text, 1);
    }
}
static void rtutor_draw(void)
{
    const rdesc_t *d = RD[rt.who];
    const char *s = d->lesson[rt.step].tx[lang] ? d->lesson[rt.step].tx[lang] : d->lesson[rt.step].tx[L_EN];
    int32_t sc = tsz() >= 2 ? 2 : 1, tw = rt.icon ? 172 : 228, h = imax(rt.icon ? 40 : 20, gblock_h(tw, s, sc)) + 14, y = AC_H - h, k;
    char tag[20] = "HOW TO PLAY  1/8";
    col_t c = ri_col(rt.who);
    gclear(th->bg);
    d->draw(rm.edit);
    if (rm.menu) ri_menu_draw(rt.who);
    if (rt.label_t > 0 && rt.label) { galpha(imin(256, rt.label_t)); gtextc(120, 100, rt.label, th->text, 2); galpha(256); }
    tag[13] = (char)('1' + rt.step); tag[15] = (char)('0' + d->nlesson);
    rm_head(d->name, tag, c);
    grect(0, y, AC_W, h, th->panel); ghl(0, AC_W, y, c);
    gblock(6, y + 5, tw, s, th->text, sc, 0);
    if (rt.icon) rt_icon(210, y + 20, rt.icon, c);
    for (k = 0; k < d->nlesson; k++) grect(6 + k * 8, AC_H - 6, 6, 2, k <= rt.step ? c : th->mute);   /* how far through */
    grect(6 + rt.step * 8, AC_H - 6, 6 * imin(rt.t, (int32_t)d->lesson[rt.step].ms) / (int32_t)d->lesson[rt.step].ms, 2, th->text);
    galpha(120 + ipulse(rm.t, 1400) * 136 / 256); gtextr(236, AC_H - 9, PB " \xE2\x86\x92", th->pad, 1); galpha(256);
}
