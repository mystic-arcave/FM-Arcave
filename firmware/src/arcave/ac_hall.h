/* SPDX-License-Identifier: GPL-3.0-only */
/* The music room itself: the inside of a great geode, down the tunnel from the cave's high right ledge. Rock rolls
 * about it; the four instruments stand along it and THE POOL, the mixer, lies in the middle. By one of them a box
 * says what PLAY and HOME do there. Each also has a button of its own, lit while Rock is loose in the room:
 *   FX ROOTS    SCL THE GEODE    ENV THE POOL    LFO MUSHROOM RACK    EDIT CRYSTAL SEAM
 * Once one is open those buttons are that instrument's own (ac_room.h). Everything keeps playing while Rock rolls.
 *
 * THE POOL: each instrument is a stone afloat in it. Near the front is loud, far is quiet; left and right are left
 * and right.
 *   KNOB 1-4    how near each stone is (its level)     SELECT   which stone is in hand    PRESETS   its place left to right
 *   ALGORITHM   the tempo                              PLAY     everything stops, or starts
 *   bottom row  four keys a stone: MUTE, SOLO, STOP IT, WIPE ITS LOOP
 *   LFO         the current: the stones drift round, so the mix moves. Held: KNOB 1 how far, KNOB 2 how slowly
 *   ARP         dives: by turns a stone goes under for a while. Held: KNOB 1 how often, KNOB 2 how many at once
 *   SEQ         the loop plays or rests. Held: KNOB 1 its length in bars, KNOB 2 how recording is tidied (the sieve)
 *   FX          held: KNOB 1 the echo's time, KNOB 2 how long it goes on
 *   ENV         held: KNOB 1-4 the tone of each       REC   record; held: wipe the whole loop
 *   EDIT        its menu: every setting of the room in one list (SELECT a row, KNOB 1-4 set it) */

#define RH_W 560
#define RH_FLOOR 186
#define RH_DOOR_X 34
static const int16_t RH_ST[5] = {112, 197, 280, 363, 448};   /* ROOTS, THE GEODE, THE POOL, MUSHROOM RACK, CRYSTAL SEAM */
static const uint8_t RH_WHO[5] = {RI_ROOTS, RI_GEODE, RI_MIX, RI_RACK, RI_SEAM};
static const uint8_t RH_BTN[5] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT};
static const char *const RH_BTN_NAME[5] = {"FX", "SCL", "ENV", "LFO", "EDIT"};
static struct {
    int32_t x, y, vx, vy, th, camx, drag, acc, intro, zoom, leave;
    uint8_t g, near, inited, want, closing;      /* near: 0 nothing, 1..5 a station, 6 the door; want: the station being opened */
    int32_t sq[2], boost_t, chime_t[7], fete;    /* the caps' squash; the booster's flash; each chime ringing; all seven rung: a shower of notes */
    struct { int32_t x, y, t; uint8_t on; } bub[3];   /* bubbles off the pool */
    int8_t ride, coy;                            /* the bubble Rock is in (-1: none) */
    uint8_t chimes, tile[36], last_tile;         /* the floor lights where Rock rolls, and plays */
} rh;
/* Ways about the room. Two crystal slides high on the walls, sloping down toward the middle; a springy cap under the
 * foot of each, which throws Rock up through the slide onto it (and sings); the right end of the floor curves up to a
 * booster that throws him onto the right slide; bubbles rise off the pool on the beat and carry him up to the seven
 * chimes under the roof (a key pops the bubble). The floor is a marimba: it lights and sounds softly where he rolls. */
typedef struct { int16_t x0, x1, y0, y1; uint8_t kind; } rh_plat_t;   /* kind 0: a ledge (sloping: a slide); 1: a springy cap */
static const rh_plat_t RH_PLAT[4] = {{62, 172, 100, 126, 0}, {388, 498, 126, 100, 0}, {142, 168, 170, 170, 1}, {392, 418, 170, 170, 1}};
#define RH_CHIME_X(i) (190 + (i) * 30)
#define RH_CHIME_Y(i) (50 + ((i) - 3) * ((i) - 3) * 2)
static int32_t rh_floor_q8(int32_t x)            /* the floor under x (Q8): level, curving up at the right end */
{
    int32_t d = x > Q8(RH_W - 110) ? x - Q8(RH_W - 110) : 0;
    return Q8(RH_FLOOR) - ((d * d) >> 8) / 80;
}
static int32_t rh_top_q8(int k, int32_t x)
{
    const rh_plat_t *p = &RH_PLAT[k];
    return Q8(p->y0) + (iclamp(x, Q8(p->x0), Q8(p->x1)) - Q8(p->x0)) / (p->x1 - p->x0) * (p->y1 - p->y0);
}
static int32_t rh_scale_note(int32_t deg) { const uint8_t *sc = RSC[rm.scale % RSC_N]; return 60 + rm.root + 12 * (deg / sc[0]) + sc[1 + deg % sc[0]]; }
static struct {
    uint8_t sel, stir_on, stir, stir_rate, dive_on, dive_every, dive_max, dive_mask, rsv, menu, row, all_stop;
    int32_t ripple[RI_N];
} mx;
static uint8_t room_seen;                        /* bit i: instrument i's lesson has been shown */

static int32_t rm_lvl_eff(int inst)
{
    static const uint16_t PER[4] = {384, 768, 1536, 3072};
    int32_t l = rmx[inst].lvl;
    if (mx.dive_on && (mx.dive_mask >> inst & 1u)) return 0;
    if (mx.stir_on && mx.stir) {
        uint32_t per = PER[mx.stir_rate & 3u];
        l += (isin((int32_t)(((rm.tq8 >> 4) % (per * 16u)) * 4096u / per) + inst * 16384) * mx.stir * l / 10) >> 14;
    }
    return iclamp(l, 0, 18);
}
static int32_t rm_pan_eff(int inst)
{
    static const uint16_t PER[4] = {384, 768, 1536, 3072};
    int32_t p = (rmx[inst].pan - 8) * 10;
    if (mx.stir_on && mx.stir) {
        uint32_t per = PER[mx.stir_rate & 3u];
        p += (icos((int32_t)(((rm.tq8 >> 4) % (per * 16u)) * 4096u / per) + inst * 16384) * mx.stir * 9) >> 14;
    }
    return iclamp(p, -80, 80);
}
static void room_echo_set(void)
{
    static const uint8_t ST[4] = {2, 3, 4, 6};   /* sixteenths */
    uint32_t n = 60000u / (uint32_t)imax(40, rm.bpm) * ST[rm.etime & 3u] / 4u * 2205u / 100u;
    sy_echo_len = n > SY_DLN ? SY_DLN : n < 400u ? 400u : n;
    sy_echo_fb = 40 + rm.efb * 24;
}
static void room_defaults(void)
{
    int i;
    for (i = 0; i < (int)sizeof rm; i++) ((uint8_t *)&rm)[i] = 0;
    for (i = 0; i < (int)sizeof mx; i++) ((uint8_t *)&mx)[i] = 0;
    rm.focus = RI_WALK; rm.bars = 4; rm.bpm = 110; rm.etime = 1; rm.efb = 4; rm.scale = 1; rm.root = 9; rm.quant = 0;
    rm.scv[SCR_FILL] = 4; rm.scv[SCR_CLUMP] = 1; rm.scv[SCR_SIZES] = 2; rm.scv[SCR_NOTES] = 2; rm.scv[SCR_QUALS] = 1; rm.sc_mask = 0xFE;
    mx.stir = 4; mx.stir_rate = 2; mx.dive_every = 1; mx.dive_max = 0;
    for (i = 0; i < RI_N; i++) ri_defaults(i);
    rmx[RI_ROOTS].pan = 3; rmx[RI_SEAM].pan = 13; rmx[RI_GEODE].pan = 6; rmx[RI_RACK].pan = 9;
    rmx[RI_ROOTS].lvl = 11; rmx[RI_GEODE].lvl = 13; rmx[RI_RACK].lvl = 12; rmx[RI_SEAM].lvl = 9;
    ri[RI_RACK].oct = 0; ri[RI_ROOTS].oct = 0;
    rm.on = 1;
}

/* ---- the room in the save (arcave.c keeps it after the game's own) */
typedef struct {
    uint8_t ver, scale, root, bars, quant, etime, efb, bpm, seen, sc_mask, scv[SCR_N], mixer[12];
    uint8_t strip[RI_N][8];
    rinst_t inst[RI_N];
    uint16_t loop_n;
    rev_t loop[RL_N];
} room_save_t;
#define ROOM_VER 1
static room_save_t *room_sv;                     /* (arcave.c points it at its place in the save) */
static void room_pack(void)
{
    room_save_t *s = room_sv;
    int i, k;
    if (!s || !rm.on) return;
    s->ver = ROOM_VER; s->scale = rm.scale; s->root = rm.root; s->bars = rm.bars; s->quant = rm.quant; s->etime = rm.etime; s->efb = rm.efb; s->bpm = rm.bpm;
    s->seen = room_seen; s->sc_mask = rm.sc_mask;
    for (i = 0; i < SCR_N; i++) s->scv[i] = rm.scv[i];
    for (i = 0; i < 12; i++) s->mixer[i] = ((const uint8_t *)&mx)[i];
    for (i = 0; i < RI_N; i++) { for (k = 0; k < 8; k++) s->strip[i][k] = ((const uint8_t *)&rmx[i])[k]; s->inst[i] = ri[i]; }
    s->loop_n = rm.loop_n;
    for (i = 0; i < RL_N; i++) s->loop[i] = rm.loop[i];
}
static void room_unpack(void)                    /* a saved room back into the room (anything out of range: its default) */
{
    const room_save_t *s = room_sv;
    int i, k;
    room_defaults();
    if (!s || s->ver != ROOM_VER) return;
    room_seen = s->seen;
    rm.scale = s->scale % RSC_N; rm.root = s->root % 12u; rm.bars = s->bars == 1 || s->bars == 2 || s->bars == 8 ? s->bars : 4; rm.quant = s->quant % 3u;
    rm.etime = s->etime & 3u; rm.efb = (uint8_t)imin(s->efb, 8); rm.bpm = (uint8_t)iclamp(s->bpm, 50, 200); rm.sc_mask = s->sc_mask;
    for (i = 0; i < SCR_N; i++) rm.scv[i] = (uint8_t)imin(s->scv[i], SCR_MAX[i]);
    for (i = 0; i < 12; i++) ((uint8_t *)&mx)[i] = s->mixer[i];
    mx.stir = (uint8_t)imin(mx.stir, 8); mx.stir_rate &= 3u; mx.dive_every &= 3u; mx.dive_max = (uint8_t)imin(mx.dive_max, 2); mx.sel &= 3u; mx.menu = 0; mx.row = 0; mx.dive_mask = 0; mx.all_stop = 0;
    for (i = 0; i < RI_N; i++) {
        const rdesc_t *d = RD[i];
        rinst_t *r = &ri[i];
        for (k = 0; k < 8; k++) ((uint8_t *)&rmx[i])[k] = s->strip[i][k];
        rmx[i].lvl = (uint8_t)imin(rmx[i].lvl, 16); rmx[i].pan = (uint8_t)imin(rmx[i].pan, 16); rmx[i].echo = (uint8_t)imin(rmx[i].echo, 8); rmx[i].rev = (uint8_t)imin(rmx[i].rev, 8);
        rmx[i].drv = (uint8_t)imin(rmx[i].drv, 8); rmx[i].tone = (uint8_t)imin(rmx[i].tone, 8); rmx[i].mute &= 1u; rmx[i].solo &= 1u;
        *r = s->inst[i];
        for (k = 0; k < RP_N; k++) {
            r->p[k].what %= d->nkind; r->p[k].size &= 7u; r->p[k].qual &= 7u; r->p[k].note = (uint8_t)imin(r->p[k].note, 20);
            r->mine[k].what %= d->nkind; r->mine[k].size &= 7u; r->mine[k].qual &= 7u; r->mine[k].note = (uint8_t)imin(r->mine[k].note, 20);
            r->fl[k] = 0;
        }
        for (k = 0; k < 4; k++) {
            static const uint8_t AM[4] = {3, 3, 3, 1};
            r->k[k] = (uint8_t)imin(r->k[k], d->kmax[k]); r->sq[k] = (uint8_t)imin(r->sq[k], d->sqmax[k]); r->lfo[k] = (uint8_t)imin(r->lfo[k], 8);
            r->env[k] = (uint8_t)imin(r->env[k], 8); r->ar[k] = (uint8_t)imin(r->ar[k], AM[k]);
        }
        r->lfo_rate %= 6u; r->lfo_on &= 1u; r->run = 1; r->arp &= 1u; r->seq &= 1u; r->algo %= d->nalgo; r->preset %= d->npreset; r->sel = (uint8_t)imin(r->sel, d->selmax);
        r->oct = (int8_t)iclamp(r->oct, -2, 2); if (!r->hand) r->hand = 1; r->arp_n = 0;
        d->init();
    }
    rm.loop_n = (uint16_t)imin(s->loop_n, RL_N);
    for (i = 0; i < RL_N; i++) { rm.loop[i] = s->loop[i]; if ((rm.loop[i].ik >> 4) >= RI_N || (rm.loop[i].ik & 15u) >= SK_N) rm.loop_n = (uint16_t)imin(rm.loop_n, i); }
}
static void room_save_now(void) { room_pack(); save_dirty = 1; }

/* ---------------------------------------------------------------- the lessons (HOME by an instrument; the first time it is opened) */
#include "ac_rtutor.h"

/* ---------------------------------------------------------------- THE POOL */
enum { MR_TEMPO, MR_SCALE, MR_ROOT, MR_BARS, MR_SIEVE, MR_ETIME, MR_EFB, MR_STIR, MR_STIRRATE, MR_DIVE, MR_DIVEMAX,
       MR_LEVEL, MR_PAN, MR_ECHO, MR_REVERB, MR_DRIVE, MR_TONE, MR_OCT, MR_WIPE, MR_RESET, MR_N };
static const char *const MR_NAME[MR_N] = {"TEMPO", "SCALE", "ROOT", "LOOP BARS", "SIEVE", "ECHO TIME", "ECHO LASTS", "CURRENT", "CURRENT TURNS IN", "DIVES EVERY", "DIVES AT ONCE",
                                          "LEVEL", "LEFT - RIGHT", "ECHO", "REVERB", "DRIVE", "TONE", "OCTAVE", "WIPE THE LOOP", "ROOM AS NEW"};
static const char *const MX_BARS[4] = {"1 BAR", "2 BARS", "4 BARS", "8 BARS"}, *const MX_SIEVE[3] = {"OFF", "SIXTEENTHS", "EIGHTHS"}, *const MX_ET[4] = {"AN EIGHTH", "3 SIXTEENTHS", "A QUARTER", "3 EIGHTHS"};
static uint8_t *mx_chan(int row, int i) { return row == MR_LEVEL ? &rmx[i].lvl : row == MR_PAN ? &rmx[i].pan : row == MR_ECHO ? &rmx[i].echo : row == MR_REVERB ? &rmx[i].rev : row == MR_DRIVE ? &rmx[i].drv : &rmx[i].tone; }
static void mx_bars_step(int d) { static const uint8_t B[4] = {1, 2, 4, 8}; int i = rm.bars == 1 ? 0 : rm.bars == 2 ? 1 : rm.bars == 8 ? 3 : 2; rm.bars = B[iclamp(i + d, 0, 3)]; }
static int mix_tick(int32_t dt)
{
    int32_t kn[4], i, any = 0, d;
    for (i = 0; i < 4; i++) { kn[i] = in.enc[ENC_KNOB1 + i]; if (kn[i]) { any = 1; rm_turned = (uint8_t)(1u << i); rm_turned_t = 500; } }
    if (rm_turned_t > 0) rm_turned_t -= dt;
    if (rm.note_t > 0) rm.note_t -= dt;
    for (i = 0; i < RI_N; i++) { mx.ripple[i] += dt; if (rm.pulse[i] > 120 && mx.ripple[i] > 220) mx.ripple[i] = 0; }
    if (BDOWN(BTN_LFO) || BDOWN(BTN_ENV) || BDOWN(BTN_FX) || BDOWN(BTN_SEQ) || BDOWN(BTN_ARP) || BDOWN(BTN_SEL)) rm.held_used = 0;
    if (BDOWN(BTN_HOME)) { snd_ui(67); if (mx.menu) mx.menu = 0; else return 1; }
    if (BDOWN(BTN_SAVE)) { room_save_now(); rm_say("SAVED", "THE WHOLE ROOM"); snd_chime(0); }
    if (BDOWN(BTN_EDIT)) { mx.menu = !mx.menu; snd_ui(mx.menu ? 79 : 67); }
    {
        static int32_t rec_t;
        if (BDOWN(BTN_REC)) rec_t = 0;
        if (BHELD(BTN_REC)) { rec_t += dt; if (rec_t >= 900 && rec_t - dt < 900) { rm.loop_n = 0; rm.rec = 0; rm_say("LOOP", "ALL OF IT WIPED"); snd_ui(55); } }
        else if ((in.bup >> BTN_REC & 1u) && rec_t < 900) { rm.rec = !rm.rec; rm_say("REC", rm.rec ? "ON THE LOOP" : "OFF"); snd_ui(rm.rec ? 84 : 72); }
    }
    if (in.enc[ENC_ALGORITHM] && !tr_follow) { rm.bpm = (uint8_t)iclamp(rm.bpm + in.enc[ENC_ALGORITHM] * 2, 50, 200); rm_say_n("TEMPO", rm.bpm); room_echo_set(); }
    if (mx.menu) {                               /* the menu: every setting, a row each */
        if (in.enc[ENC_SELECT]) mx.row = (uint8_t)iclamp(mx.row + (in.enc[ENC_SELECT] > 0 ? 1 : -1), 0, MR_N - 1);
        d = kn[0] + kn[1] + kn[2] + kn[3]; d = d > 0 ? 1 : d < 0 ? -1 : 0;
        if (mx.row >= MR_LEVEL && mx.row <= MR_TONE) { for (i = 0; i < 4; i++) if (kn[i]) { uint8_t *p = mx_chan(mx.row, i); *p = (uint8_t)iclamp(*p + kn[i], 0, mx.row <= MR_PAN ? 16 : 8); } }
        else if (mx.row == MR_OCT) { for (i = 0; i < 4; i++) if (kn[i]) ri[i].oct = (int8_t)iclamp(ri[i].oct + kn[i], -2, 2); }
        else if (d) switch (mx.row) {
        case MR_TEMPO: if (!tr_follow) rm.bpm = (uint8_t)iclamp(rm.bpm + d, 50, 200); break;
        case MR_SCALE: rm.scale = (uint8_t)((rm.scale + RSC_N + d) % RSC_N); break;
        case MR_ROOT: rm.root = (uint8_t)((rm.root + 12 + d) % 12); break;
        case MR_BARS: mx_bars_step(d); break;
        case MR_SIEVE: rm.quant = (uint8_t)iclamp(rm.quant + d, 0, 2); break;
        case MR_ETIME: rm.etime = (uint8_t)iclamp(rm.etime + d, 0, 3); break;
        case MR_EFB: rm.efb = (uint8_t)iclamp(rm.efb + d, 0, 8); break;
        case MR_STIR: mx.stir = (uint8_t)iclamp(mx.stir + d, 0, 8); mx.stir_on = mx.stir > 0; break;
        case MR_STIRRATE: mx.stir_rate = (uint8_t)iclamp(mx.stir_rate + d, 0, 3); break;
        case MR_DIVE: mx.dive_every = (uint8_t)iclamp(mx.dive_every + d, 0, 3); break;
        case MR_DIVEMAX: mx.dive_max = (uint8_t)iclamp(mx.dive_max + d, 0, 2); break;
        default: break;
        }
        if (any) room_echo_set();
        if (BDOWN(BTN_PLAY)) {
            if (mx.row == MR_WIPE) { rm.loop_n = 0; rm.rec = 0; rm_say("LOOP", "ALL OF IT WIPED"); snd_ui(55); }
            else if (mx.row == MR_RESET) { uint8_t seen = room_seen; room_defaults(); room_seen = seen; rm.focus = RI_MIX; mx.menu = 1; mx.row = MR_RESET; room_echo_set(); rm_say("THE ROOM", "AS NEW"); snd_chime(0); }
        }
        return 0;
    }
    if (BHELD(BTN_LFO)) { if (kn[0]) { mx.stir = (uint8_t)iclamp(mx.stir + kn[0], 0, 8); mx.stir_on = 1; } if (kn[1]) mx.stir_rate = (uint8_t)iclamp(mx.stir_rate + kn[1], 0, 3); if (any) rm.held_used = 1; }
    else if (BHELD(BTN_ARP)) { if (kn[0]) { mx.dive_every = (uint8_t)iclamp(mx.dive_every + kn[0], 0, 3); mx.dive_on = 1; } if (kn[1]) mx.dive_max = (uint8_t)iclamp(mx.dive_max + kn[1], 0, 2); if (any) rm.held_used = 1; }
    else if (BHELD(BTN_SEQ)) { if (kn[0]) mx_bars_step(kn[0] > 0 ? 1 : -1); if (kn[1]) rm.quant = (uint8_t)iclamp(rm.quant + kn[1], 0, 2); if (any) rm.held_used = 1; }
    else if (BHELD(BTN_FX)) { if (kn[0]) rm.etime = (uint8_t)iclamp(rm.etime + kn[0], 0, 3); if (kn[1]) rm.efb = (uint8_t)iclamp(rm.efb + kn[1], 0, 8); if (any) { rm.held_used = 1; room_echo_set(); } }
    else if (BHELD(BTN_ENV)) { for (i = 0; i < 4; i++) if (kn[i]) rmx[i].tone = (uint8_t)iclamp(rmx[i].tone + kn[i], 0, 8); if (any) rm.held_used = 1; }
    else if (BHELD(BTN_SEL)) { if (kn[0]) { rm.root = (uint8_t)((rm.root + 12 + (kn[0] > 0 ? 1 : 11)) % 12); rm.held_used = 1; rm_say("ROOT", NOTE_NAME[rm.root]); } }
    else {
        for (i = 0; i < 4; i++) if (kn[i]) { rmx[i].lvl = (uint8_t)iclamp(rmx[i].lvl + kn[i], 0, 16); mx.sel = (uint8_t)i; }
        if (in.enc[ENC_SELECT]) mx.sel = (uint8_t)((mx.sel + 4 + (in.enc[ENC_SELECT] > 0 ? 1 : -1)) % 4);
        if (in.enc[ENC_PRESETS]) rmx[mx.sel].pan = (uint8_t)iclamp(rmx[mx.sel].pan + (in.enc[ENC_PRESETS] > 0 ? 1 : -1), 0, 16);
    }
    if ((in.bup >> BTN_LFO & 1u) && !rm.held_used) { mx.stir_on = !mx.stir_on; rm_say("THE CURRENT", mx.stir_on ? "ON" : "OFF"); snd_ui(mx.stir_on ? 84 : 72); }
    if ((in.bup >> BTN_ARP & 1u) && !rm.held_used) { mx.dive_on = !mx.dive_on; mx.dive_mask = 0; rm_say("DIVES", mx.dive_on ? "ON" : "OFF"); snd_ui(mx.dive_on ? 84 : 72); }
    if ((in.bup >> BTN_SEQ & 1u) && !rm.held_used) { rm.loop_off = !rm.loop_off; rm_say("THE LOOP", rm.loop_off ? "RESTS" : "PLAYS"); }
    if ((in.bup >> BTN_SEL & 1u) && !rm.held_used) { rm.scale = (uint8_t)((rm.scale + 1) % RSC_N); rm_say("SCALE", RSC_NAME[rm.scale]); snd_ui(79); }
    if (BDOWN(BTN_OCT_DOWN) || BDOWN(BTN_OCT_UP)) { for (i = 0; i < RI_N; i++) ri[i].oct = (int8_t)iclamp(ri[i].oct + (BDOWN(BTN_OCT_UP) ? 1 : -1), -2, 2); rm_say("OCTAVE", BDOWN(BTN_OCT_UP) ? "ALL UP" : "ALL DOWN"); }
    if (BDOWN(BTN_PLAY)) { mx.all_stop = !mx.all_stop; for (i = 0; i < RI_N; i++) { ri[i].run = !mx.all_stop; RD[i]->play(ri[i].run); } rm_say("EVERYTHING", mx.all_stop ? "STOPS" : "PLAYS"); }
    for (i = 0; i < 16; i++) if (in.kdown >> R_LOW[i] & 1u) {
        int who = i / 4, k;
        mx.sel = (uint8_t)who;
        switch (i % 4) {
        case 0: rmx[who].mute = !rmx[who].mute; rm_say(RD[who]->name, rmx[who].mute ? "MUTED" : "HEARD"); break;
        case 1: rmx[who].solo = !rmx[who].solo; rm_say(RD[who]->name, rmx[who].solo ? "ALONE" : "WITH THE REST"); break;
        case 2: ri[who].run = !ri[who].run; RD[who]->play(ri[who].run); rm_say(RD[who]->name, ri[who].run ? "PLAYS" : "STOPS"); break;
        default: { uint16_t n = 0; for (k = 0; k < rm.loop_n; k++) if ((rm.loop[k].ik >> 4) != who) rm.loop[n++] = rm.loop[k]; rm.loop_n = n; rm_say(RD[who]->name, "ITS LOOP WIPED"); break; }
        }
        snd_ui(72 + who * 3);
    }
    return 0;
}
/* dives: once every so many bars one stone goes under, or one comes back up */
static void mix_step(void)
{
    static const uint16_t EV[4] = {384, 768, 1536, 3072};
    int i, n = 0;
    if (!mx.dive_on || !rm_cross(EV[mx.dive_every & 3u], 0, 0)) return;
    for (i = 0; i < RI_N; i++) n += mx.dive_mask >> i & 1;
    i = (int)(ac_rnd() % RI_N);
    if (n > mx.dive_max || ((mx.dive_mask >> i & 1u) && n)) { for (i = 0; i < RI_N; i++) if (mx.dive_mask >> i & 1u) { mx.dive_mask &= (uint8_t)~(1u << i); break; } }
    else mx.dive_mask |= (uint8_t)(1u << i);
}
static void mix_stone_xy(int i, int32_t *x, int32_t *y)
{
    *x = 120 + rm_pan_eff(i) * 68 / 80;
    *y = 44 + rm_lvl_eff(i) * 112 / 16;
}
static void mix_draw(void)
{
    static const char *const FXN[4] = {"ECHO TIME", "ECHO LASTS", 0, 0}, *const LFN[4] = {"HOW FAR", "HOW SLOW", 0, 0}, *const ARN[4] = {"HOW OFTEN", "AT ONCE", 0, 0}, *const SQN[4] = {"LOOP BARS", "SIEVE", 0, 0};
    const char *n[4], *w[4] = {0, 0, 0, 0}, *hint = 0;
    int32_t v[4], m[4], i, k;
    gclear(th->bg);
    /* the pool: seen from above its near edge, far at the top */
    galpha(30); gellf(120, 104, 112, 84, th->caps[4], 0); galpha(256);
    garc(120, 104, 112, 84, 0, 65536, th->rim, 1);
    for (k = 1; k < 4; k++) { galpha(40); garc(120, 104, 112 - k * 26, 84 - k * 20, 0, 65536, th->mute, 0); }
    galpha(256);
    gtextc(120, 22, "QUIET", th->mute, 1); gtextc(120, 182, "LOUD", th->mute, 1); gtext(12, 101, "L", th->mute, 1); gtextr(228, 101, "R", th->mute, 1);
    if (mx.stir_on && mx.stir) for (k = 0; k < 6; k++) {   /* the current: marks going round */
        static const uint16_t PER[4] = {384, 768, 1536, 3072};
        uint32_t per = PER[mx.stir_rate & 3u];
        int32_t a = (int32_t)(((rm.tq8 >> 4) % (per * 16u)) * 4096u / per) + k * 10923;
        galpha(120); gline(120 + ((96 * icos(a)) >> 14), 104 + ((72 * isin(a)) >> 14), 120 + ((96 * icos(a + 1500)) >> 14), 104 + ((72 * isin(a + 1500)) >> 14), th->pad); galpha(256);
    }
    for (i = 0; i < RI_N; i++) {
        int32_t x, y, r = 9, under = (mx.dive_on && (mx.dive_mask >> i & 1u)) || rmx[i].mute;
        col_t c = ri_col(i);
        mix_stone_xy(i, &x, &y);
        if (mx.ripple[i] < 900 && !under) { galpha(256 - mx.ripple[i] * 256 / 900); garc(x, y, 10 + mx.ripple[i] / 30, 7 + mx.ripple[i] / 44, 0, 65536, c, 0); galpha(256); }
        galpha(under ? 70 : 256);
        gellf(x, y + 3, r + 2, 4, th->bg, 0);
        gdisc(x, y, r, under ? th->bg : c); gcirc(x, y, r, c);
        if (i == mx.sel) gcirc(x, y, r + 3, th->text);
        if (rmx[i].solo) gtextc(x, y - 2, "S", th->bg, 1);
        else if (!ri[i].run) grect(x - 2, y - 2, 5, 5, th->bg);
        galpha(256);
        if (i == mx.sel) gfit(x, y + r + 6, 90, RD[i]->name, th->text, 1);
    }
    if (mx.menu) {
        int32_t top = iclamp(mx.row - 5, 0, MR_N - 11), x = 14, y = 18, wd = 212, h = 172;
        char nb[8];
        grect(x, y, wd, h, th->panel); gbox(x, y, wd, h, th->pad);
        gtext(x + 6, y + 5, "THE POOL", th->text, 1);
        for (i = 0; i < RI_N; i++) gfit(x + 104 + i * 27, y + 5, 26, i == 0 ? "RT" : i == 1 ? "GE" : i == 2 ? "MR" : "CS", ri_col(i), 1);
        for (k = 0; k < 11; k++) {
            int row = top + k;
            int32_t ry = y + 18 + k * 13;
            const char *val = 0;
            if (row == mx.row) { grect(x + 3, ry - 2, wd - 6, 12, th->bg); gtext(x + 5, ry + 1, "\xE2\x86\x92", th->pad, 1); }
            gtext(x + 14, ry + 1, MR_NAME[row], row == mx.row ? th->text : th->mute, 1);
            nb[0] = 0;
            if (row >= MR_LEVEL && row <= MR_TONE) { for (i = 0; i < 4; i++) rm_bar(x + 104 + i * 27, ry + 1, 24, *mx_chan(row, i), row <= MR_PAN ? 16 : 8, row == MR_PAN, ri_col(i)); continue; }
            if (row == MR_OCT) { for (i = 0; i < 4; i++) rm_bar(x + 104 + i * 27, ry + 1, 24, ri[i].oct + 2, 4, 1, ri_col(i)); continue; }
            switch (row) {
            case MR_TEMPO: nb[0] = (char)(rm.bpm >= 100 ? '0' + rm.bpm / 100 : ' '); nb[1] = (char)('0' + rm.bpm / 10 % 10); nb[2] = (char)('0' + rm.bpm % 10); nb[3] = 0; val = nb; break;
            case MR_SCALE: val = RSC_NAME[rm.scale]; break;
            case MR_ROOT: val = NOTE_NAME[rm.root]; break;
            case MR_BARS: val = MX_BARS[rm.bars == 1 ? 0 : rm.bars == 2 ? 1 : rm.bars == 8 ? 3 : 2]; break;
            case MR_SIEVE: val = MX_SIEVE[rm.quant % 3u]; break;
            case MR_ETIME: val = MX_ET[rm.etime & 3u]; break;
            case MR_STIRRATE: val = MX_BARS[mx.stir_rate & 3u]; break;
            case MR_DIVE: val = MX_BARS[mx.dive_every & 3u]; break;
            case MR_WIPE: case MR_RESET: val = PB; break;
            default: break;
            }
            if (val) gtextr(x + wd - 8, ry + 1, val, th->text, 1);
            else rm_bar(x + wd - 60, ry + 1, 52, row == MR_EFB ? rm.efb : row == MR_STIR ? mx.stir : mx.dive_max + 1, row == MR_DIVEMAX ? 3 : 8, 0, th->pad);
        }
        gtext(x + 6, y + h - 10, "SELECT: ROW   KNOBS: SET   EDIT: CLOSE", th->mute, 1);
    }
    for (i = 0; i < 4; i++) { n[i] = RD[i]->name; v[i] = rmx[i].lvl; m[i] = 16; }
    if (!mx.menu) {
        if (BHELD(BTN_LFO)) { for (i = 0; i < 4; i++) n[i] = LFN[i]; v[0] = mx.stir; m[0] = 8; v[1] = mx.stir_rate; m[1] = 3; w[1] = MX_BARS[mx.stir_rate & 3u]; hint = "LFO: THE CURRENT"; }
        else if (BHELD(BTN_ARP)) { for (i = 0; i < 4; i++) n[i] = ARN[i]; v[0] = mx.dive_every; m[0] = 3; w[0] = MX_BARS[mx.dive_every & 3u]; v[1] = mx.dive_max + 1; m[1] = 3; hint = "ARP: DIVES"; }
        else if (BHELD(BTN_SEQ)) { for (i = 0; i < 4; i++) n[i] = SQN[i]; v[0] = rm.bars; m[0] = 8; w[0] = MX_BARS[rm.bars == 1 ? 0 : rm.bars == 2 ? 1 : rm.bars == 8 ? 3 : 2]; v[1] = rm.quant; m[1] = 2; w[1] = MX_SIEVE[rm.quant % 3u]; hint = "SEQ: THE LOOP"; }
        else if (BHELD(BTN_FX)) { for (i = 0; i < 4; i++) n[i] = FXN[i]; v[0] = rm.etime; m[0] = 3; w[0] = MX_ET[rm.etime & 3u]; v[1] = rm.efb; m[1] = 8; hint = "FX: THE ECHO"; }
        else if (BHELD(BTN_ENV)) { for (i = 0; i < 4; i++) { v[i] = rmx[i].tone; m[i] = 8; } hint = "ENV: THE TONE OF EACH"; }
    }
    {
        static const char *const NM[4] = {"ROOTS", "GEODE", "RACK", "SEAM"};
        if (!hint || BHELD(BTN_ENV)) for (i = 0; i < 4; i++) n[i] = NM[i];
        rm_knobs(n, v, m, 0, w, th->pad, rm_turned_t > 0 ? rm_turned : 0);
    }
    grect(0, RSTRIP_Y, AC_W, 10, th->bg);
    if (rm.note_t > 0) { int32_t x = gtext(4, RSTRIP_Y + 2, rm.note_a, th->mute, 1); gtext(x + 5, RSTRIP_Y + 2, rm.note_b, th->text, 1); }
    else if (hint) gtext(4, RSTRIP_Y + 2, hint, th->mute, 1);
    else {
        int32_t x = 4;
        x = gtext(x, RSTRIP_Y + 2, "CURRENT", mx.stir_on ? th->pad : th->mute, 1) + 6; x = gtext(x, RSTRIP_Y + 2, "DIVES", mx.dive_on ? th->pad : th->mute, 1) + 6;
        x = gtext(x, RSTRIP_Y + 2, "LOOP", !rm.loop_off && rm.loop_n ? th->pad : th->mute, 1) + 6;
        gtextr(236, RSTRIP_Y + 2, "EDIT: MENU  HOME: ROOM", th->mute, 1);
    }
    {
        char tb[12]; int k2 = 0;
        if (rm.bpm >= 100) tb[k2++] = (char)('0' + rm.bpm / 100);
        tb[k2++] = (char)('0' + rm.bpm / 10 % 10); tb[k2++] = (char)('0' + rm.bpm % 10); tb[k2++] = ' '; tb[k2++] = 'B'; tb[k2++] = 'P'; tb[k2++] = 'M'; tb[k2] = 0;
        rm_head("THE POOL", tb, th->pad);
    }
}
static void mix_leds(void)
{
    int i;
    for (i = 0; i < RI_N; i++) {
        if (rmx[i].mute) led_key_dim_want |= 1u << R_LOW[i * 4]; else led_key_want |= 1u << R_LOW[i * 4];
        if (rmx[i].solo) led_key_want |= 1u << R_LOW[i * 4 + 1];
        if (ri[i].run) led_key_want |= 1u << R_LOW[i * 4 + 2];
        led_key_dim_want |= 1u << R_LOW[i * 4 + 3];
    }
    if (mx.stir_on) led_btn_want |= 1u << BTN_LFO;
    if (mx.dive_on) led_btn_want |= 1u << BTN_ARP;
    if (!rm.loop_off && rm.loop_n) led_btn_want |= 1u << BTN_SEQ;
    if (mx.menu) led_btn_want |= 1u << BTN_EDIT;
    if (rm.rec) led_btn_want |= 1u << BTN_REC;
    if (!mx.all_stop && (rm.tq8 >> 8) % TICKS_BEAT < 20u) led_btn_want |= 1u << BTN_PLAY;
}

/* ---------------------------------------------------------------- the room, Rock loose in it */
static void room_enter(void)
{
    if (!rm.on) room_unpack();
    rm.focus = RI_WALK; rm.edit = 0; rm.menu = 0; rm.ptq8 = 0; rm.acc = 0;
    rh.x = Q8(RH_DOOR_X + 26); rh.y = Q8(RH_FLOOR) - rock_rad(0); rh.vx = Q8(60); rh.vy = 0; rh.th = 0; rh.g = 1; rh.camx = 0; rh.drag = 160;
    rh.ride = -1; rh.intro = 1; rh.zoom = 0; rh.leave = 0; rh.want = 0; rh.closing = 0; rh.inited = 1;
    mus_set(MUS_OFF, 0); sy_drone_want = 0; sy_rumble_want = 0; sy_music_want = 256;
    if (!tr_follow) tr_bpm_q8 = (uint32_t)rm.bpm << 8;
    take_bus(0);                                 /* (the player's song's echo and reverb off: the room's own) */
    room_echo_set();
    sfx(57, 0, 900, W_SINE, 2400, 60, 200); sfx(64, 120, 900, W_SINE, 2000, 60, 200); sfx(72, 260, 1400, W_SINE, 2000, 60, 220);
}
static void room_leave(void) { sy_echo_len = SY_DLN; sy_echo_fb = 115; sy_x.fmode = 0; sy_x.gl = 0; }
static void room_open(int st)                    /* a station opens (1..5): the picture goes in on it first */
{
    rh.want = (uint8_t)st; rh.zoom = 1; rh.closing = 0;
    sfx(72, 0, 200, W_TRI, 2000, 4, 120); sfx(79, 70, 300, W_TRI, 2000, 4, 160);
}
/* returns -1 when Rock goes back out of the door */
static int room_tick(int32_t dt)
{
    int32_t i, knob, hx;
    if (!tr_follow) tr_bpm_q8 = (uint32_t)rm.bpm << 8;
    if (rt.on) { rtutor_tick(dt); rm_frame(dt); return 0; }
    rm_frame(dt);
    if (rm.focus != RI_WALK) {
        int back = rm.focus == RI_MIX ? mix_tick(dt) : ri_tick(rm.focus, dt);
        if (rm.focus == RI_MIX) mix_leds(); else ri_leds(rm.focus);
        if (back) { rm.focus = RI_WALK; rm.edit = 0; rm.menu = 0; mx.menu = 0; rh.zoom = 260; rh.closing = 1; }
        return 0;
    }
    if (rh.intro) { rh.intro += dt; if (rh.intro > 1100) rh.intro = 0; }
    if (rh.closing) { rh.zoom -= dt; if (rh.zoom <= 0) { rh.zoom = 0; rh.closing = 0; rh.want = 0; } }
    else if (rh.zoom) {
        rh.zoom += dt;
        if (rh.zoom >= 260) {
            int who = RH_WHO[rh.want - 1];
            rm.focus = (uint8_t)who; rm_turned_t = 0; rm.note_t = 0;
            if (who != RI_MIX && !(room_seen >> who & 1u)) { room_seen |= (uint8_t)(1u << who); rtutor_open(who, who); }
        }
        return 0;
    }
    if (rh.leave) { rh.leave += dt; rh.x -= Q8(1) * dt / 12; rh.th -= dt * 60; if (rh.leave > 700) { rh.leave = 0; return -1; } return 0; }
    /* rolling and hopping, as in the cave */
    i = in.any_knob;
    if (i) rh.drag = iclamp(rh.drag + i * 115, -384, 384);
    knob = iclamp(rh.drag, -256, 256);
    rh.drag -= rh.drag * imin(dt, 100) / 250;
    if (in.kdown) snd_keys(in.kdown, 0);
    if (in.hikey >= 0) deco_key_fx(KEY_ZONE[in.hikey], rh.x, rh.y);
    pfx_tick(dt); deco_jump_tick(dt, rh.g, rh.x, rh.y);
    if (rh.g) deco_trail(rh.x, rh.y + rock_rad(rh.th), rh.vx, dt);
    for (i = 0; i < 2; i++) if (rh.sq[i] > 0) rh.sq[i] = imax(0, rh.sq[i] - dt);
    for (i = 0; i < 7; i++) if (rh.chime_t[i] > 0) rh.chime_t[i] -= dt;
    for (i = 0; i < 36; i++) rh.tile[i] = (uint8_t)(rh.tile[i] > 10 ? rh.tile[i] - 10 : 0);
    if (rh.boost_t > 0) rh.boost_t -= dt;
    if (rh.fete > 0) rh.fete -= dt;
    {   /* bubbles: one off the pool every two beats; they rise, wobbling, and pop under the roof */
        static uint32_t last;
        uint32_t beat = (rm.tq8 >> 8) / (TICKS_BEAT * 2u);
        if (beat != last) {
            last = beat;
            for (i = 0; i < 3; i++) if (!rh.bub[i].on) { rh.bub[i].on = 1; rh.bub[i].t = 0; rh.bub[i].x = Q8(RH_ST[2] - 20 + (int32_t)(ac_rnd() % 40u)); rh.bub[i].y = Q8(RH_FLOOR - 6); break; }
        }
        for (i = 0; i < 3; i++) {
            if (!rh.bub[i].on) continue;
            rh.bub[i].t += dt; rh.bub[i].y -= dt * 256 * 36 / 1000;
            rh.bub[i].x += (tsin(rm.t, 20, i * 20000) * dt) >> 14;
            if (rh.ride == i) {
                rh.bub[i].x = iclamp(rh.bub[i].x + knob * dt * 60 / 1000, Q8(40), Q8(RH_W - 40));
                rh.x = rh.bub[i].x; rh.y = rh.bub[i].y; rh.vx = knob * 60; rh.vy = 0; rh.g = 0; rh.th += knob * dt / 30;
                if (in.hop >= 0) { rh.bub[i].y = 0; rh.vy = -Q8(170); in.hop = -1; }
            } else if (rh.ride < 0 && rh.bub[i].t > 250 && iabs(rh.x - rh.bub[i].x) < Q8(13) && iabs(rh.y - rh.bub[i].y) < Q8(14)) {
                rh.ride = (int8_t)i; sfx(72, 0, 200, W_SINE, 1800, 20, 200);
            }
            if (rh.bub[i].y < Q8(64)) {          /* pop */
                rh.bub[i].on = 0;
                if (rh.ride == i) { rh.ride = -1; rh.vy = imin(rh.vy, -Q8(60)); }
                if (iabs((rh.bub[i].x >> 8) - (rh.camx >> 8) - 120) < 140) { sfx(rh_scale_note(10 + i * 2), 0, 260, W_SINE, 1400, 0, 220); sfx(100, 0, 30, W_HAT, 500, 0, 100); }
            }
        }
    }
    rh.acc += dt * 128;
    while (rh.acc >= 1000) {
        int32_t rad = rock_rad(rh.th), a, py = rh.y, bot, k, px;
        rh.acc -= 1000;
        if (rh.ride >= 0) continue;
        px = rh.x >> 8;
        if (rh.g) {
            a = knob * TQ;
            if ((knob > 0 && rh.vx > 0) || (knob < 0 && rh.vx < 0)) a = (a * imax(0, 256 - iabs(rh.vx) * 256 / Q8(170))) >> 8;
            a += -rh.vx * 2 / 5 - (iabs(rh.vx) > 512 ? (rh.vx > 0 ? Q8(8) : -Q8(8)) : 0);
            if (rh.y > Q8(RH_FLOOR - 30)) {
                if (px > RH_W - 110) a -= GR * (px - (RH_W - 110)) / 56;   /* the ramp pulls back in */
                if (px > RH_W - 84 && px < RH_W - 58 && rh.vx > Q8(50)) {   /* the booster: up and back, onto the right slide */
                    rh.vy = -Q8(372); rh.vx = -Q8(84); rh.g = 0; rh.boost_t = 500; rh.drag = 0; knob = 0; a = 0;
                    snd_whoosh(); sfx_ex(50, 0, 500, W_SAW, 2400, 3, 200, 0, 0, 65530);
                }
                for (i = 0; i < 5; i++) {        /* a slight dip in front of each station: Rock settles there */
                    int32_t dx = px - RH_ST[i];
                    if (iabs(dx) < 22) rh.vx -= ((GR * dx / 34 * (22 - iabs(dx)) / 22) >> 7) + rh.vx / 70;
                }
            } else for (k = 0; k < 2; k++) if (px >= RH_PLAT[k].x0 && px <= RH_PLAT[k].x1) a += GR * (RH_PLAT[k].y1 - RH_PLAT[k].y0) * 3 / (RH_PLAT[k].x1 - RH_PLAT[k].x0);   /* a slide */
            rh.vx += a >> 7;
        } else rh.vx += (knob * 160) >> 7;
        rh.vx = iclamp(rh.vx, -Q8(210), Q8(210));
        rh.vy += GR >> 7;
        if (rh.g) rh.coy = 13; else if (rh.coy > 0) rh.coy--;
        if (in.hop >= 0 && rh.coy > 0) { rh.vy = -Q8(JMIN) - (345 - JMIN) * in.hop; rh.g = 0; rh.coy = 0; in.hop = -1; deco_jump(KEY_ZONE[imax(in.hikey, 0)], rh.x, rh.y); }
        rh.x += rh.vx >> 7; rh.y += rh.vy >> 7;
        if (rh.x < Q8(RH_DOOR_X - 6)) { rh.x = Q8(RH_DOOR_X - 6); rh.vx = imax(0, rh.vx); }
        if (rh.x > Q8(RH_W - 22)) { rh.x = Q8(RH_W - 22); rh.vx = imin(0, rh.vx); }
        if (rh.y - rad < Q8(34)) { rh.y = Q8(34) + rad; rh.vy = imax(0, rh.vy); }
        bot = rh.y + rad; rh.g = 0;
        for (k = -1; k < 4 && rh.vy >= 0; k++) {
            int32_t top = k < 0 ? rh_floor_q8(rh.x) : rh_top_q8(k, rh.x);
            if (k >= 0 && (rh.x < Q8(RH_PLAT[k].x0 - 5) || rh.x > Q8(RH_PLAT[k].x1 + 5))) continue;
            if (k < 0 ? bot >= top : (py + rad <= top + 256 && bot >= top - Q8(2))) {
                rh.y = top - rad;
                if (k >= 2) {                    /* a cap throws you up, and sings a chord of the room's scale */
                    rh.vy = -Q8(372); rh.sq[k - 2] = 250;
                    sfx(rh_scale_note(k == 2 ? 0 : 2), 0, 800, W_SINE, 3000, 4, 220); sfx(rh_scale_note(k == 2 ? 4 : 6), 60, 600, W_SINE, 1800, 4, 220);
                } else {
                    if (rh.vy > Q8(150)) { sfx_ex(38, 0, 200, W_SINE, 3000, 2, 200, 0, 0, 64600); sfx(96, 0, 60, W_NOISE, 400, 1, 160); }
                    rh.vy = 0; rh.g = 1;
                }
                break;
            }
        }
        rh.th += ((rh.vx * 1372) >> 8) >> 7;
    }
    hx = rh.x >> 8;
    if (rh.g && rh.y > Q8(RH_FLOOR - 30) && hx / 16 != rh.last_tile && hx / 16 < 36) {   /* the floor: a soft note where he rolls onto it */
        rh.last_tile = (uint8_t)(hx / 16); rh.tile[rh.last_tile] = 255;
        sfx(rh_scale_note(rh.last_tile % 10), 0, 220, W_SINE, 700, 2, 160);
    }
    for (i = 0; i < 7; i++)                      /* the chimes under the roof */
        if (rh.chime_t[i] <= 0 && iabs(hx - RH_CHIME_X(i)) < 12 && iabs((rh.y >> 8) - RH_CHIME_Y(i) - 6) < 16) {
            rh.chime_t[i] = 500; rh.chimes |= (uint8_t)(1u << i);
            sfx(rh_scale_note(7 + i), 0, 1500, W_SINE, 3000, 2, 230); sfx(rh_scale_note(7 + i) + 12, 0, 600, W_SINE, 1000, 2, 230);
            if (rh.chimes == 127u) { rh.chimes = 0; rh.fete = 2600; for (knob = 0; knob < 12; knob++) sfx(rh_scale_note(5 + knob), (uint32_t)knob * 90u, 500, W_TRI, 1500, 2, 220); }
        }
    {
        int32_t tx = iclamp(rh.x - Q8(120) + rh.vx / 3, 0, Q8(RH_W - 240));
        rh.camx += (tx - rh.camx) * imin(160, dt) / 160;
    }
    rh.near = 0;
    if (rh.y > Q8(RH_FLOOR - 34)) {              /* (down on the floor) */
        for (i = 0; i < 5; i++) if (iabs(hx - RH_ST[i]) < 24) rh.near = (uint8_t)(i + 1);
        if (hx < RH_DOOR_X + 22) rh.near = 6;
    }
    for (i = 0; i < 5; i++) {                    /* each has its button, wherever Rock is: he is taken there */
        led_btn_want |= 1u << RH_BTN[i];
        if (BDOWN(RH_BTN[i])) { rh.x = Q8(RH_ST[i]); rh.vx = 0; rh.camx = iclamp(rh.x - Q8(120), 0, Q8(RH_W - 240)); rh.near = (uint8_t)(i + 1); room_open(i + 1); return 0; }
    }
    if (rh.near) led_play_breathe = 1;
    if (rh.near && rh.near <= 5 && RH_WHO[rh.near - 1] != RI_MIX) led_breathe_want |= 1u << BTN_HOME;
    if (BDOWN(BTN_PLAY) && rh.near) {
        if (rh.near == 6) { rh.leave = 1; snd_ui(67); }
        else room_open(rh.near);
    }
    if (BDOWN(BTN_HOME) && rh.near && rh.near <= 5 && RH_WHO[rh.near - 1] != RI_MIX) { room_seen |= (uint8_t)(1u << RH_WHO[rh.near - 1]); rtutor_open(RH_WHO[rh.near - 1], RI_WALK); }
    if (BDOWN(BTN_SAVE)) { room_save_now(); banner_show("", "SAVED", 0); snd_chime(0); }
    led_key_want |= in.keys;
    return 0;
}

/* a station as it stands in the room, small, moving with what its instrument is doing */
static void room_station(int st, int32_t x, int32_t fy)
{
    int who = RH_WHO[st], s;
    col_t c = ri_col(who);
    int32_t pu = who < RI_N ? rm.pulse[who] : 0;
    if (pu > 20) { galpha(pu / 5); gdisc(x, fy - 40, 34, c); galpha(256); }
    if (who == RI_GEODE) {
        gline(x - 12, fy, x - 6, fy - 12, th->rim); gline(x + 12, fy, x + 6, fy - 12, th->rim);   /* its cradle */
        ge_draw_at(x, fy - 36, 26, 0);
    } else if (who == RI_ROOTS) {                /* roots down from the roof, a pod on each */
        for (s = 0; s < 8; s++) {
            const rinst_t *r = &ri[RI_ROOTS];
            int32_t hot = imax(r->fl[s * 2], r->fl[s * 2 + 1]), rx = x - 28 + s * 8, len = 70 + ((s * 37) % 40), sw = (tsin(rm.t, 14 + s, s * 9000) >> 12) + (hot ? (tsin(rm.t, 200, 0) * hot >> 20) : 0);
            gline(rx, fy - 170, rx + sw / 2, fy - 170 + len / 2, th->stem); gline(rx + sw / 2, fy - 170 + len / 2, rx + sw, fy - 170 + len, th->stem);
            if (r->p[s * 2].what || r->p[s * 2 + 1].what) gdisc(rx + sw, fy - 168 + len, 2 + (hot > 100), hot > 100 ? th->text : c);
        }
    } else if (who == RI_RACK) {                 /* a shelf of caps, a pod hopping along it */
        const rinst_t *r = &ri[RI_RACK];
        int32_t hop = (int32_t)((rm.tq8 >> 8) % 384u), hs = hop / 48, ph = hop % 48;
        grect(x - 30, fy - 16, 60, 2, th->stem); gline(x - 26, fy - 14, x - 26, fy, th->stem); gline(x + 26, fy - 14, x + 26, fy, th->stem);
        for (s = 0; s < 8; s++) {
            int32_t hot = imax(r->fl[s * 2], r->fl[s * 2 + 1]), cx = x - 25 + s * 7;
            if (!(r->p[s * 2].what | r->p[s * 2 + 1].what)) continue;
            grect(cx, fy - 22, 1, 6, th->stem); gellf(cx, fy - 22 + hot / 90, 4, 4 - hot / 100, hot > 100 ? th->text : c, 1);
        }
        if (r->run) gdisc(x - 25 + hs * 7 + ph * 7 / 48, fy - 27 - ((ph * (48 - ph)) >> 5), 2, th->text);
    } else if (who == RI_SEAM) {                 /* crystals up out of the floor */
        const rinst_t *r = &ri[RI_SEAM];
        for (s = 0; s < 8; s++) {
            int32_t hot = imax(r->fl[s * 2], r->fl[s * 2 + 1]), cx = x - 26 + s * 7 + (s & 1), h = 10 + r->p[s * 2].size * 4 + ((s * 11) % 9), xy[6];
            if (!(r->p[s * 2].what | r->p[s * 2 + 1].what)) continue;
            xy[0] = cx - 3; xy[1] = fy; xy[2] = cx + (s % 3) - 1; xy[3] = fy - h; xy[4] = cx + 3; xy[5] = fy;
            galpha(70 + hot * 180 / 256); gpoly(xy, 3, c); galpha(256); gpolyline(xy, 3, 0, hot > 100 ? th->text : c);
        }
    } else {                                     /* the pool, in the floor, the four stones afloat in it */
        gellf(x, fy + 1, 34, 7, th->bg, 0); garc(x, fy + 1, 34, 7, 0, 65536, th->pad, 0);
        galpha(50); gellf(x, fy + 1, 32, 6, th->caps[4], 0); galpha(256);
        for (s = 0; s < RI_N; s++) {
            int32_t sx = x + rm_pan_eff(s) * 26 / 80, sy = fy - 3 + rm_lvl_eff(s) * 7 / 16;
            if (rm.pulse[s] > 60) { galpha(rm.pulse[s]); garc(sx, sy, 5, 2, 0, 65536, ri_col(s), 0); galpha(256); }
            gdisc(sx, sy, 2, ri_col(s));
        }
    }
}
static void room_draw(void)
{
    int32_t cx = rh.camx >> 8, i, k, fy = RH_FLOOR;
    uint32_t tm = rm.t;
    if (rt.on) { rtutor_draw(); return; }
    if (rm.focus == RI_MIX) { mix_draw(); return; }
    if (rm.focus != RI_WALK) {
        const rinst_t *r = &ri[rm.focus];
        gclear(th->bg);
        RD[rm.focus]->draw(rm.edit);
        if (rm.menu) ri_menu_draw(rm.focus);
        rm_head(RD[rm.focus]->name, rm.edit ? "EDIT" : !r->run ? "STOPPED" : 0, ri_col(rm.focus));
        ri_strip(rm.focus);
        return;
    }
#define SX(x) ((x) - cx)
    gclear(th->bg);
    /* the roof: the inside of a geode, crystal teeth all along it that light with the music */
    for (i = -1; i < 14; i++) {
        int32_t wx = ((cx / 20) + i) * 20, n = wx / 20, d = wx - RH_W / 2, ry = 14 + d * d / 3400, h = 10 + (int32_t)(ihash((uint32_t)(n + 700)) * 26u >> 16), xy[6];
        int who = (int)(ihash((uint32_t)(n + 31)) % RI_N);
        xy[0] = SX(wx); xy[1] = ry; xy[2] = SX(wx + 9 + (n % 5)); xy[3] = ry + h; xy[4] = SX(wx + 20); xy[5] = 14 + (d + 20) * (d + 20) / 3400;
        galpha(26 + rm.pulse[who] * 150 / 256); gpoly(xy, 3, ri_col(who));
        galpha(150); gpolyline(xy, 3, 0, th->rim);
    }
    galpha(256);
    for (k = 0; k < 14; k++) {                   /* motes adrift */
        int32_t q = (int32_t)((tm / 16u + (uint32_t)k * 173u) % 1000u), mxp = (int32_t)(ihash((uint32_t)k + 9u) % RH_W);
        galpha(40 + (1000 - q) * 120 / 1000);
        gp(SX(mxp + (tsin(tm, 9, k * 7000) >> 10)), fy - 20 - q * 140 / 1000, th->caps[k % 5]);
    }
    galpha(256);
    /* the floor: it curves up at the right end; its tiles light where Rock has rolled */
    for (i = (cx - 4) & ~3; i < cx + 244; i += 4) {
        int32_t y0, y1, t;
        if (i < 6 || i > RH_W - 12) continue;
        y0 = rh_floor_q8(i << 8) >> 8; y1 = rh_floor_q8((i + 4) << 8) >> 8; t = i / 16 < 36 ? rh.tile[i / 16] : 0;
        gline(SX(i), y0, SX(i + 4), y1, th->rim);
        if (t) { galpha(t); grect(SX(i), y0 + 1, 4, 3, th->caps[(i / 16) % 5]); }
        for (k = 0; k < 3; k++) if (((i / 4 + k * 3) % (5 + k * 2)) < 3 + k) { galpha(150 - k * 40); gline(SX(i), y0 + 4 + k * 4, SX(i + 4), y1 + 4 + k * 4, th->bands[k % 5]); }
        galpha(256);
    }
    for (k = 0; k < 3; k++) {                    /* the booster's chevrons, running up the ramp */
        int32_t bx = RH_W - 82 + k * 9, by = (rh_floor_q8(bx << 8) >> 8) - 4;
        galpha(rh.boost_t > 0 ? 256 : 90 + ((((int32_t)(tm / 120u) + 3 - k) % 3) == 0 ? 166 : 0));
        gline2(SX(bx - 3), by - 4, SX(bx), by, th->pad); gline2(SX(bx), by, SX(bx - 3), by + 3, th->pad);
    }
    galpha(256);
    for (k = 0; k < 2; k++) {                    /* the two slides: long crystals laid against the walls, a little rock sat at the head of each, nodding in time */
        const rh_plat_t *p = &RH_PLAT[k];
        int32_t xy[8], hx0 = k ? p->x1 : p->x0, hy0 = k ? p->y1 : p->y0, beat = (int32_t)((rm.tq8 >> 8) % TICKS_BEAT);
        xy[0] = SX(p->x0); xy[1] = p->y0; xy[2] = SX(p->x1); xy[3] = p->y1; xy[4] = SX(p->x1 + (k ? 6 : -10)); xy[5] = p->y1 + 9; xy[6] = SX(p->x0 + (k ? 10 : -6)); xy[7] = p->y0 + 9;
        galpha(60); gpoly(xy, 4, th->caps[k ? 4 : 3]); galpha(256); gpolyline(xy, 4, 1, th->caps[k ? 4 : 3]);
        gline(SX(p->x0 + 20), p->y0 + 2 + (p->y1 - p->y0) * 20 / 110, SX(p->x1 - 20), p->y1 + 2 - (p->y1 - p->y0) * 20 / 110, th->text);
        rock_draw(SX(hx0 + (k ? -9 : 9)), hy0 - 7 - (beat < 14 ? 2 : 0), 170, 0, (tm % (2700u + (uint32_t)k * 900u)) < 150u ? 30 : 256, th->rock, th->rock_rim, th->dark, th->caps[2], th->eye, th->pupil);
    }
    for (k = 2; k < 4; k++) {                    /* the caps */
        const rh_plat_t *p = &RH_PLAT[k];
        int32_t q = rh.sq[k - 2] * 256 / 250, mxp = (p->x0 + p->x1) / 2, ry = 9 * (256 - q * 4 / 10) / 256;
        col_t c = th->caps[k == 2 ? 0 : 1];
        grect(SX(mxp - 1), p->y0, 3, RH_FLOOR - p->y0, th->stem);
        galpha(50 + q / 3); gdisc(SX(mxp), p->y0, 18, c); galpha(256);
        gellf(SX(mxp), p->y0 + 9 - ry + q * 3 / 256, 13 * (256 + q * 3 / 10) / 256, ry, c, 1);
    }
    for (k = 0; k < 7; k++) {                    /* the chimes under the roof */
        int32_t x = SX(RH_CHIME_X(k)), y = RH_CHIME_Y(k), on_ = rh.chime_t[k] > 0, xy[8];
        col_t c = th->caps[k % 5];
        gline(x, 16, x + (on_ ? (tsin(tm, 300, 0) >> 13) : 0), y - 8, th->stem);
        xy[0] = x; xy[1] = y - 8; xy[2] = x + 5; xy[3] = y; xy[4] = x; xy[5] = y + 9; xy[6] = x - 5; xy[7] = y;
        if (rh.chimes >> k & 1u) { galpha(70); gdisc(x, y, 12, c); galpha(256); gpoly(xy, 4, c); gpolyline(xy, 4, 1, th->text); }
        else { gpoly(xy, 4, th->bg); galpha(140); gpolyline(xy, 4, 1, c); galpha(256); }
        if (on_) { galpha(rh.chime_t[k] * 200 / 500); gdisc(x, y, 15, c); galpha(256); }
    }
    for (k = 0; k < 8; k++) {                    /* glow-worms: adrift, flashing on the beat, each a hair after the last */
        int32_t wx = (int32_t)(ihash((uint32_t)k + 40u) % (RH_W - 80)) + 40 + (tsin(tm, 5 + k, k * 9000) >> 9), wy = 70 + (int32_t)(ihash((uint32_t)k + 60u) % 80u) + (tsin(tm, 7, k * 5000) >> 10);
        int32_t ph = (int32_t)(((rm.tq8 >> 8) + (uint32_t)k * 12u) % TICKS_BEAT), b = ph < 30 ? 256 - ph * 8 : 16;
        galpha(b / 3); gdisc(SX(wx), wy, 5, th->pad); galpha(imax(60, b)); gdisc(SX(wx), wy, 1, th->pad);
    }
    galpha(256);
    for (k = 0; k < 3; k++) if (rh.bub[k].on) {  /* bubbles */
        int32_t x = SX(rh.bub[k].x >> 8), y = rh.bub[k].y >> 8, r = rh.ride == k ? 13 : imin(10, 3 + rh.bub[k].t / 60);
        galpha(40); gdisc(x, y, r, th->caps[4]); galpha(220); gcirc(x, y, r, th->caps[(k * 2 + 4) % 5]); galpha(256);
        garc(x, y, r - 3, r - 3, 36000, 44000, th->text, 0);
    }
    if (rh.fete > 0) for (k = 0; k < 16; k++) {  /* all seven chimes: notes come down like confetti */
        int32_t q = 2600 - rh.fete, y = 30 + (q + k * 37) % 400 * 150 / 400;
        galpha(imin(256, rh.fete / 4)); gtext((int32_t)(ihash((uint32_t)k + 5u) % 230u) + (tsin(tm, 30, k * 8000) >> 12), y, "\xE2\x99\xAA", th->caps[k % 5], 1 + (k & 1));
    }
    galpha(256);
    for (k = 0; k < 2; k++) {                    /* the walls */
        int32_t wx = k ? RH_W - 8 : 6, y, px = wx;
        for (y = 30; y < fy; y += 8) { int32_t nx = wx + (int32_t)(ihash((uint32_t)(y / 8 + k * 77)) * 5u >> 16) - 2; gline(SX(px), y, SX(nx), imin(fy, y + 8), th->rim); px = nx; }
    }
    {   /* the door back to the tunnel: an arch of the five colours */
        int32_t x = SX(RH_DOOR_X - 14), y = fy;
        gellf(x, y, 13, 30, th->far1, 1);
        for (k = 0; k < 3; k++) { galpha(220 - k * 60); garc(x, y, 15 + k * 4, 32 + k * 5, 32768, 65536, th->caps[(k + (int32_t)(tm / 700u)) % 5], 1); }
        galpha(256);
    }
    for (i = 0; i < 5; i++) {
        int32_t x = SX(RH_ST[i]);
        if (x < -60 || x > 300) continue;
        room_station(i, x, fy);
        if (rh.near != i + 1) {                  /* its button, on a little plate over it */
            int32_t w = gtextw(RH_BTN_NAME[i], 1) + 8;
            grect(x - w / 2, fy + 18, w, 11, th->panel); gbox(x - w / 2, fy + 18, w, 11, ri_col(RH_WHO[i]));
            gtextc(x, fy + 21, RH_BTN_NAME[i], th->text, 1);
        }
    }
    pfx_draw(cx * 256, 0, 256);
    prock_live(SX(rh.x >> 8), rh.y >> 8, 256, rh.th, (tm % 3700u) < 160u ? iabs((int32_t)(tm % 3700u) - 80) * 256 / 80 : 256);
    if (rh.near && !rh.zoom) {                   /* the box: what is here, and what PLAY and HOME do */
        int st = rh.near - 1;
        int32_t sc = ts(1, 3), s1 = ts(1, 2), x, y = 28;
        if (rh.near == 6) { galpha(160 + ipulse(tm, 1400) * 96 / 256); gfit(iclamp(SX(RH_DOOR_X), 60, 180), 92, 232, TR(T_MR_OUT), th->text, s1); galpha(256); }
        else {
            const char *nm = RH_WHO[st] == RI_MIX ? "THE POOL" : RD[RH_WHO[st]]->name;
            int32_t w = imax(gtextw(nm, sc), imax(gtextw(TR(T_MR_OPEN), s1), gtextw(TR(T_MR_HOW), s1))) + 16, h = TLH(sc) + TLH(s1) * (RH_WHO[st] == RI_MIX ? 1 : 2) + 8;
            w = imin(w, 236); x = iclamp(SX(RH_ST[st]) - w / 2, 2, 238 - w);
            grect(x, y, w, h, th->panel); gbox(x, y, w, h, ri_col(RH_WHO[st]));
            gfit(x + w / 2, y + 4 + 2 * sc, w - 8, nm, ri_col(RH_WHO[st]), sc);
            galpha(160 + ipulse(tm, 1400) * 96 / 256);
            gfit(x + w / 2, y + 4 + TLH(sc) + 2 * s1, w - 8, TR(T_MR_OPEN), th->text, s1);
            galpha(256);
            if (RH_WHO[st] != RI_MIX) gfit(x + w / 2, y + 4 + TLH(sc) + TLH(s1) + 2 * s1, w - 8, TR(T_MR_HOW), th->mute, s1);
        }
    }
    if (rh.zoom) gzoom(SX(RH_ST[rh.want - 1]), fy - 40, 256 + ease8(imin(256, rh.zoom)) * 900 / 256);
    if (rh.zoom > 130) gveil(th->bg, (uint32_t)imin(32, (rh.zoom - 130) * 32 / 120));
    if (rh.intro) {                              /* coming in: the room opens from the middle, ring after ring of the five colours */
        int32_t q = rh.intro, r0 = q * 190 / 900;
        for (k = 0; k < AC_H; k++) {
            int32_t dy = iabs(k - 120), w = r0 > dy ? (int32_t)isqrt((uint32_t)(r0 * r0 - dy * dy)) : 0;
            if (w < 120) { ghl(0, 120 - w, k, 0); ghl(120 + w, AC_W, k, 0); }
        }
        for (k = 0; k < 5; k++) { int32_t rr = r0 + 3 + k * 5; if (rr < 190) { galpha(256 - k * 44); garc(120, 120, rr, rr, 0, 65536, th->caps[k], 1); } }
        galpha(256);
        if (q < 1100) { galpha(q < 800 ? 256 : (1100 - q) * 256 / 300); gfit(120, 104, 232, TR(T_MR_NAME), th->text, ts(3, 4)); galpha(256); }
    }
    if (rh.leave) gfade(rh.leave * 256 / 700);
#undef SX
}
