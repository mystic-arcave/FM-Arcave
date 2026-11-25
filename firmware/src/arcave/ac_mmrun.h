/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE: a loaded machine (mm.h) being played. ac_mach.h's frame for all machines (the cards, the score and the
 * hearts, the tickets, REC's question) calls these where it calls CRYSTAL ROAD's, JUICE WORKS' and NIGHT BLOOM's own;
 * here are the firmware's functions as the machine is handed them (MM_HOST), the frame's facts for it (mm_fill), and
 * HOME, which for a loaded machine is its own words on a page. The machine's RAM is mm_state, zero when its ledge is
 * walked up to and again when a game starts. */
_Static_assert(MS_TITLE == MM_TITLE && MS_PLAY == MM_PLAY && MS_OVER == MM_OVER, "mm.h: the states");
/* (the buttons and knobs in mm.h's order, whatever the board's own: platform.c) */
static const uint8_t MM_BTN[14] = {BTN_OCT_DOWN, BTN_OCT_UP, BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_HOME, BTN_SAVE,
                                   BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC};
static const uint8_t MM_ENC[7] = {ENC_SELECT, ENC_ALGORITHM, ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4, ENC_PRESETS};
_Static_assert(W_SINE == MM_SINE && W_TRI == MM_TRI && W_SQUARE == MM_SQUARE && W_SAW == MM_SAW, "mm.h: the waves");
_Static_assert(AC_NKEY == MM_NKEY && sizeof(col_t) == sizeof(mm_col_t), "mm.h: the keys, a colour");

static uint8_t mm_state[MM_STATE_MAX] AC_POOL;
static const mm_head_t *mm_h(void) { return mb.h[mb.cur]; }

static void mmh_sfx(int32_t note, uint32_t delay_ms, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms, uint32_t send)
{
    sfx(note, delay_ms, dur_ms, wave & 3u, vol > 6000u ? 6000u : vol, att_ms, send);
}
static void mmh_key_voice(int32_t key, int32_t note) { if (key >= 0 && key < AC_NKEY) pf_key_voice((int)key, note); }
static int32_t mmh_key_note(int32_t key) { return key_play(iclamp(key, 0, AC_NKEY - 1)); }
static void mmh_over(void) { mb.over = 1; }
static void mmh_say(const char *s) { mb.say = s; mc.msg = 0; mc.msg_t = 700; }
static void mmh_keep(void) { save_dirty = 1; }
static const mm_api_t MM_HOST = {
    MM_API,
    galpha, grect, gbox, gline, gdisc, garc, gpoly, gveil, gtext, gtextw, gfit, gblock, ts, prock_draw,
    mmh_sfx, mmh_key_note, mmh_key_voice, snd_ui,
    isin, ac_rnd, ihash,
    mmh_over, mmh_say, mmh_keep,
};

static void mm_fill(int32_t dt)
{
    mm_ctx_t *c = &mb.c;
    uint32_t i;
    c->ms = fm1_ms; c->dt = dt; c->beat_q8 = tr_now_q8() / 96u - mb.off; c->bpm_q8 = tr_bpm_q8;
    c->keys = in.keys; c->kdown = in.kdown; c->kup = in.kup; c->btn = c->bdown = 0;
    for (i = 0; i < 14u; i++) { c->btn |= (in.btn >> MM_BTN[i] & 1u) << i; c->bdown |= (in.bdown >> MM_BTN[i] & 1u) << i; }
    for (i = 0; i < 7u; i++) c->enc[i] = in.enc[MM_ENC[i]];
    c->enc[7] = 0;
    c->lang = lang; c->state = mc.state; c->tsize = (uint8_t)tsz(); c->score = mc.score; c->lives = mc.lives;
    c->led_keys = c->led_glow = 0;
    mb.col[MMC_BG] = th->bg; mb.col[MMC_TEXT] = th->text; mb.col[MMC_MUTE] = th->mute; mb.col[MMC_PAD] = th->pad;
    mb.col[MMC_PANEL] = th->panel; mb.col[MMC_RIM] = th->rim;
    for (i = 0; i < 5u; i++) mb.col[MMC_CAP0 + i] = th->caps[i];
    for (i = 0; i < 4u; i++) mb.col[MMC_ZONE0 + i] = PF_KEYCOL[i];
    c->col = mb.col; c->zone = KEY_ZONE; c->keep = ms.rec[mb.cur].keep;
}
static void mm_wipe(void) { uint32_t i, n = mm_h()->state; for (i = 0; i < n && i < MM_STATE_MAX; i++) mm_state[i] = 0; }
/* walked up to: its RAM zero (its picture is drawn under the start card), its best score from what is kept for it */
static void mm_enter(int slot)
{
    mb.cur = (uint8_t)slot; mb.help = 0; mb.over = 0; mb.say = 0; mb.off = 0;
    mm_wipe();
    ms_rec_for((uint32_t)slot);
    best[MACH_MM] = ms.rec[slot].best;
    bank_mark(1);
}
static void mm_leave(void) { bank_mark(0); mb.help = 0; }
static void mm_start(void)
{
    mm_wipe();
    mc.lives = mm_h()->lives; mb.over = 0; mb.say = 0; mb.off = 0;
    mm_fill(0); mb.c.state = MM_PLAY;
    mm_h()->start(mm_state, &mb.c, &MM_HOST);
    mc.score = mb.c.score; mc.lives = mb.c.lives;
}
static void mach_over(void);
static void mm_tick(int32_t dt)
{
    mm_fill(dt);
    mm_h()->tick(mm_state, &mb.c, &MM_HOST);
    mc.score = mb.c.score; mc.lives = mb.c.lives;
    led_key_want |= mb.c.led_keys & ((1u << AC_NKEY) - 1u); led_key_dim_want |= mb.c.led_glow & ((1u << AC_NKEY) - 1u);
    if (mb.over) { mb.over = 0; mach_over(); }
}
static void mm_draw(void)
{
    mm_fill(0);
    mm_h()->draw(mm_state, &mb.c, &MM_HOST);
    galpha(256);
}
/* the game was held for d ticks of the jukebox's clock (HOME's words, REC's question): the machine's clock stays
 * that far behind, made up to whole beats, so what it does still lands on the beat */
static void mm_held(uint32_t d) { mb.off += (d * 256u / 96u + 255u) & ~255u; }
/* HOME: the machine's own words on how it is played, over its picture; HOME again (or PLAY) puts them away, REC
 * leaves for the Arcave, as from the other machines' pages */
static void mm_help_draw(void)
{
    const mm_head_t *h = mm_h();
    const char *s = h->help[lang < 7u ? lang : 0u];
    int32_t a = ts(2, 3), y;
    if (!s) s = h->help[0];
    galpha(256); grect(0, 0, AC_W, AC_H, th->bg);
    y = 12 + 2 * ts(2, 3);
    y += 5 * gfit(120, y, 228, h->name, th->caps[h->cap % 5u], ts(2, 3)) + 10;
    while (a > 1 && gblock_h(216, s, a) > 226 - y - 2 * TLH(ts(1, 2))) a--;
    gblock(12, y, 216, s, th->text, a, 0);
    gfoot(238, TR(T_REC_BACK), th->mute, th->bg, ts(1, 2));
}
