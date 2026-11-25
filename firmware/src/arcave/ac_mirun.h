/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's music room: a loaded instrument (mi.h) in one of its four places. The room knows an instrument by its
 * rdesc_t (ac_room.h); for a loaded one that is made here out of the instrument's header, with functions of the
 * firmware's (one set a place) that hand its own functions their three things: its RAM (mi_state), this moment
 * (mi_c) and the firmware's functions (MI_HOST). ac_hall.h's room_kit says which places hold one; ac_slots.h has the
 * slots and the checks.
 *
 * Every call into a loaded instrument is marked for the platform (bank_mark): an FM-1 that restarts inside one does
 * not run anything loaded again until it has been switched off and on. */
_Static_assert(SK_N == MIS_N && SK_NONE == MI_SILENT && W_HAT == MI_HAT && W_NOISE == MI_NOISE, "mi.h: the sounds");
_Static_assert(RTI_TOP == MII_TOP && RTI_BTN == MII_BTN && TICKS_STEP == MI_TICKS_STEP && L_N == 7, "mi.h: the lesson's hand, the clock, the languages");
_Static_assert(sizeof(rt_step_t) == sizeof(mi_step_t) && __builtin_offsetof(rt_step_t, tx) == __builtin_offsetof(mi_step_t, tx), "mi.h: a step of a lesson");

static uint8_t mi_state[MI_NSLOT][MI_STATE_MAX];
static rdesc_t mi_desc[MI_NSLOT];
static mi_ctx_t mi_c;
static mm_col_t mi_colv[MIC_N];
static uint8_t mi_up;                            /* bit j: place j holds its loaded instrument */
static uint16_t mi_was[MI_NSLOT];                /* whose world mi_state[j] is (its number + 1; 0: nobody's) */
static uint8_t mi_cur, mi_depth, mi_voicing;     /* the place being run; calls into loaded code, nested; in its voice */
static int mi_on(int inst) { return inst >= 0 && inst < (int)MI_NSLOT && (mi_up >> inst & 1u); }

/* ---- the firmware's functions, as a loaded instrument is handed them */
static void mih_strike(int32_t part, int32_t vel) { if (part >= 0 && part < RP_N) ri_strike(mi_cur, (int)part, iclamp(vel, 1, 255)); }
static void mih_hit(int32_t voice, int32_t deg, int32_t vel, int32_t size, int32_t qual) { rm_hit(mi_cur, (int)(voice & 15), deg, vel, size, qual); }
static int mih_cross(uint32_t per, uint32_t off, uint32_t *idx) { return per ? rm_cross(per, off % per, idx) : 0; }
static int32_t mih_note(int32_t deg) { return rm_note(mi_cur, deg); }
static void mih_vo(int32_t note, uint32_t delay, int32_t dur, int wave, int32_t vol, int32_t att, int32_t det, int32_t pan)
{
    if (mi_voicing) rvo(iclamp(note, 0, 127), delay > 2000u ? 2000u : delay, iclamp(dur, 1, 8000), wave >= 0 && wave <= W_HAT ? wave : W_SINE, iclamp(vol, 0, 8000), iclamp(att, 0, 2000), det, pan);
}
static void mih_flt(int mode, int32_t cut, int32_t res, int32_t fenv, int32_t ms) { if (mi_voicing) rflt(mode == 2 ? 2 : 1, cut, res, fenv, ms); }
static void mih_glide(int32_t semis, int32_t ms) { if (mi_voicing) sfx_glide(semis, ms); }
static void mih_std(int32_t kind, int32_t deg, int32_t vel, int32_t size, int32_t qual) { if (mi_voicing && kind >= 0 && kind < SK_N) rm_std(mi_cur, (int)kind, deg, vel, iclamp(size, 0, 7), iclamp(qual, 0, 7)); }
static void mih_icon(int32_t i) { rt.icon = (uint8_t)(i >= MII_BTN ? RTI_BTN + MM_BTN[(i - MII_BTN) % 14] : i < 0 ? 0 : i); }
static void mih_label(const char *s) { rt.label = s; rt.label_t = 1000; }
static void mih_open(int edit, int menu, uint32_t kinds)
{
    rm.edit = edit || menu; rm.menu = menu != 0;
    if (rm.edit) ri[mi_cur].run = 0;
    if (menu) { rm.mrow = 0; rm.sc_mask = (uint8_t)(kinds & 0xFEu); }
}
static void mih_scatter(void) { ri_scatter(mi_cur); }
static void mih_layout(int n) { ri_load_algo(mi_cur, n < 0 ? 0 : n % RD[mi_cur]->nalgo); }
static void mih_changed(void) { RD[mi_cur]->init(); }
static const mi_api_t MI_HOST = {
    MI_API,
    galpha, grect, gbox, gline, gdisc, garc, gpoly, gtext, gtextw, gfit,
    isin, ac_rnd, ihash, isqrt,
    mih_strike, mih_hit, mih_cross, mih_note, rm_note_name, snd_ui,
    mih_vo, mih_flt, mih_glide, mih_std,
    mih_icon, mih_label, mih_open, mih_scatter, mih_layout, mih_changed,
};

/* this moment, for place j's instrument */
static mi_ctx_t *mi_fill(int j)
{
    mi_ctx_t *c = &mi_c;
    int i;
    mi_cur = (uint8_t)j;
    c->ms = rm.t; c->tq8 = rm.tq8; c->ptq8 = rm.ptq8; c->r = &ri[j];
    for (i = 0; i < 4; i++) c->k[i] = ri_k(j, i);
    c->edit = (uint8_t)(rm.edit && rm.focus == j); c->place = (uint8_t)j; c->lang = lang; c->tsize = (uint8_t)tsz();
    c->col = mi_colv;
    return c;
}
static void mi_cols(int j)
{
    uint32_t i;
    mi_colv[MMC_BG] = th->bg; mi_colv[MMC_TEXT] = th->text; mi_colv[MMC_MUTE] = th->mute; mi_colv[MMC_PAD] = th->pad;
    mi_colv[MMC_PANEL] = th->panel; mi_colv[MMC_RIM] = th->rim;
    for (i = 0; i < 5u; i++) mi_colv[MMC_CAP0 + i] = th->caps[i];
    for (i = 0; i < 4u; i++) mi_colv[MMC_ZONE0 + i] = PF_KEYCOL[i];
    mi_colv[MIC_OWN] = ri_col(j); mi_colv[MIC_STEM] = th->stem; mi_colv[MIC_ROCK] = th->rock;
}
/* into the instrument and out again: h its header, w its world, c this moment */
#define MI_IN(j) const mi_head_t *h = mb.ih[j]; void *w = mi_state[j]; uint8_t was_ = mi_cur; mi_ctx_t *c = mi_fill(j); \
                 if (mi_depth++ == 0) bank_mark(1)
#define MI_OUT() do { if (--mi_depth == 0) bank_mark(0); mi_cur = was_; } while (0)
static void mi_init_(int j) { MI_IN(j); h->init(w, c, &MI_HOST); MI_OUT(); }
static void mi_step_(int j) { MI_IN(j); h->step(w, c, &MI_HOST); MI_OUT(); }
static void mi_draw_(int j) { MI_IN(j); mi_cols(j); h->draw(w, c, &MI_HOST); galpha(256); MI_OUT(); }
static void mi_key_(int j, int i, int vel) { MI_IN(j); if (i >= 0 && i < RP_N) h->key(w, c, &MI_HOST, i, vel); MI_OUT(); }
static void mi_top_(int j, int i) { MI_IN(j); if (i >= 0 && i < 11) h->top(w, c, &MI_HOST, i); MI_OUT(); }
static void mi_layout_(int j, int n)
{
    int s2;
    MI_IN(j);
    h->layout(w, c, &MI_HOST, n);
    for (s2 = 0; s2 < RP_N; s2++) { rpart_t *q = &ri[j].p[s2]; q->what %= h->nkind; q->size &= 7u; q->qual &= 7u; q->note = (uint8_t)imin(q->note, 20); }
    MI_OUT();
}
static void mi_play_(int j, int on) { MI_IN(j); h->play(w, c, &MI_HOST, on); MI_OUT(); }
static void mi_voice(int inst, int voice, int32_t deg, int32_t vel, int32_t size, int32_t qual)
{
    MI_IN(inst);
    mi_voicing++;
    h->voice(w, c, &MI_HOST, voice, deg, vel, size, qual);
    mi_voicing--;
    MI_OUT();
}
/* as it stands in the room, at (x, fy) on the floor (ac_hall.h) */
static void mi_small(int j, int32_t x, int32_t fy)
{
    MI_IN(j);
    mi_cols(j); c->x = x; c->y = fy;
    h->small(w, c, &MI_HOST);
    galpha(256);
    MI_OUT();
}
/* a frame of step i of its lesson (ac_rtutor.h) */
static void mi_lesson(int j, int i, int32_t t, int32_t dt)
{
    MI_IN(j);
    c->step = i; c->t = t; c->dt = dt;
    if (h->nstep && i >= 0 && i < h->nstep) h->lesson(w, c, &MI_HOST);
    MI_OUT();
}
#define MI_PLACE(j) \
    static void mi_init##j(void) { mi_init_(j); } static void mi_step##j(void) { mi_step_(j); } \
    static void mi_draw##j(int e) { (void)e; mi_draw_(j); } static void mi_key##j(int i, int v) { mi_key_(j, i, v); } \
    static void mi_top##j(int i) { mi_top_(j, i); } static void mi_layout##j(int n) { mi_layout_(j, n); } \
    static void mi_play##j(int on) { mi_play_(j, on); }
MI_PLACE(0) MI_PLACE(1) MI_PLACE(2) MI_PLACE(3)
#undef MI_PLACE

/* place j holds its loaded instrument from now (0: the firmware's own again): its rdesc_t out of its header; a world
 * that was another instrument's is wiped (init is the room's to call: room_unpack, ri_defaults) */
static const rdesc_t *mi_set(int j, int on)
{
    static void (*const F_INIT[4])(void) = {mi_init0, mi_init1, mi_init2, mi_init3}, (*const F_STEP[4])(void) = {mi_step0, mi_step1, mi_step2, mi_step3};
    static void (*const F_DRAW[4])(int) = {mi_draw0, mi_draw1, mi_draw2, mi_draw3}, (*const F_TOP[4])(int) = {mi_top0, mi_top1, mi_top2, mi_top3};
    static void (*const F_LAY[4])(int) = {mi_layout0, mi_layout1, mi_layout2, mi_layout3}, (*const F_PLAY[4])(int) = {mi_play0, mi_play1, mi_play2, mi_play3};
    static void (*const F_KEY[4])(int, int) = {mi_key0, mi_key1, mi_key2, mi_key3};
    const mi_head_t *h = mb.ih[j];
    rdesc_t *d = &mi_desc[j];
    uint32_t i;
    if (!on || !h) { mi_up &= (uint8_t)~(1u << j); return 0; }
    if (mi_was[j] != (uint16_t)(h->id + 1u)) { for (i = 0; i < MI_STATE_MAX; i++) mi_state[j][i] = 0; mi_was[j] = (uint16_t)(h->id + 1u); }
    d->name = h->name;
    for (i = 0; i < 4u; i++) {
        d->kn[i] = h->kn[i]; d->kmax[i] = h->kmax[i] ? h->kmax[i] : 1u; d->kdef[i] = (uint8_t)imin(h->kdef[i], d->kmax[i]); d->kmid[i] = h->kmid[i];
        d->sqn[i] = h->sqn[i]; d->sqmax[i] = h->sqmax[i]; d->sqdef[i] = (uint8_t)imin(h->sqdef[i], h->sqmax[i]);
    }
    d->seln = h->seln; d->selmax = h->selmax; d->seldef = (uint8_t)imin(h->seldef, h->selmax); d->seqn = h->seqn;
    d->kinds = h->kinds; d->nkind = h->nkind; d->ksnd = h->ksnd; d->presets = h->presets; d->npreset = h->npreset; d->algos = h->algos; d->nalgo = h->nalgo;
    d->init = F_INIT[j]; d->step = F_STEP[j]; d->draw = F_DRAW[j]; d->key = F_KEY[j]; d->top = F_TOP[j]; d->layout = F_LAY[j]; d->play = F_PLAY[j];
    d->lesson = (const rt_step_t *)(const void *)h->steps; d->nlesson = h->nstep;
    mi_up |= (uint8_t)(1u << j);
    return d;
}
