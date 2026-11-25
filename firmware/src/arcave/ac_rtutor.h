/* SPDX-License-Identifier: GPL-3.0-only */
/* The music room's lessons. A lesson is the instrument itself, played by the lesson: each step holds a caption and an
 * act that works the instrument's own controls while the caption is up, so what is shown is what will happen. A
 * picture of the knob, key row or button in use stands beside the caption. PLAY: the next step. HOME or REC: enough.
 * The instrument as the player left it is put away first and brought back after; nothing else in the room is heard.
 *
 * HOME tapped by an instrument (or the first time one is opened): its whole lesson, start to end.
 * HOME held, in an instrument, in the pool, or by either: HOW IT WORKS, the same lessons cut into topics, and more of
 * them (what LFO, ENV, FX, ARP, SCL, the presets, REC and the quick mix do; the pool's own). SELECT or any knob moves
 * down the list, PLAY plays the one chosen, and it comes back to the list; HOME: out. While the list is up the room
 * plays on as it was. The topics' own captions are in English only (the whole lessons are in every language). */
static struct {
    rinst_t inst; uint8_t strip[RI_N][8], mixer[12], run[RI_N], edit, menu, sc_mask, scv[SCR_N];
    uint8_t scale, root, bars, quant, etime, efb, loop_off, bpm;
} rt_keep;
static void rt_put_away(int who)
{
    int i, k;
    if (who < RI_N) rt_keep.inst = ri[who];
    for (k = 0; k < RI_N; k++) { for (i = 0; i < 8; i++) rt_keep.strip[k][i] = ((const uint8_t *)&rmx[k])[i]; rt_keep.run[k] = ri[k].run; }
    for (i = 0; i < 12; i++) rt_keep.mixer[i] = ((const uint8_t *)&mx)[i];
    for (i = 0; i < SCR_N; i++) rt_keep.scv[i] = rm.scv[i];
    rt_keep.sc_mask = rm.sc_mask; rt_keep.scale = rm.scale; rt_keep.root = rm.root; rt_keep.bars = rm.bars; rt_keep.quant = rm.quant;
    rt_keep.etime = rm.etime; rt_keep.efb = rm.efb; rt_keep.loop_off = rm.loop_off; rt_keep.bpm = rm.bpm;
}
static void rt_bring_back(void)
{
    int i, k, who = rt.who;
    if (who < RI_N) ri[who] = rt_keep.inst;
    for (k = 0; k < RI_N; k++) {
        for (i = 0; i < 8; i++) ((uint8_t *)&rmx[k])[i] = rt_keep.strip[k][i];
        if (ri[k].run != rt_keep.run[k]) { ri[k].run = rt_keep.run[k]; RD[k]->play(ri[k].run); }
    }
    for (i = 0; i < 12; i++) ((uint8_t *)&mx)[i] = rt_keep.mixer[i];
    for (i = 0; i < SCR_N; i++) rm.scv[i] = rt_keep.scv[i];
    rm.sc_mask = rt_keep.sc_mask; rm.scale = rt_keep.scale; rm.root = rt_keep.root; rm.bars = rt_keep.bars; rm.quant = rt_keep.quant;
    rm.etime = rt_keep.etime; rm.efb = rt_keep.efb; rm.loop_off = rt_keep.loop_off; rm.bpm = rt_keep.bpm;
    room_echo_set();
    if (who < RI_N) { for (i = 0; i < RP_N; i++) ri[who].fl[i] = 0; RD[who]->init(); }
    rm.tut = 0; rm.edit = 0; rm.menu = 0; rm.rec = 0; mx.menu = 0; rq.demo = 0; rt.vhold = 0;
}
/* steps lo..hi-1 of `steps` begin, on instrument who (RI_MIX: the pool) */
/* what a step does while it is up: its own act, or (a loaded instrument's lesson: mi.h) that instrument's one function */
static void rt_act(const rt_step_t *steps, int i, int32_t t, int32_t dt)
{
    if (rt.who < RI_N && mi_on(rt.who) && steps == RD[rt.who]->lesson) mi_lesson(rt.who, i, t, dt);
    else if (steps[i].act) steps[i].act(t, dt);
}
static void rt_begin(int who, const rt_step_t *steps, int lo, int hi)
{
    int i;
    rt_put_away(who);
    if (who < RI_N) { rmx[who].mute = 0; rmx[who].lvl = 12; }
    rt.on = 1; rt.who = (uint8_t)who; rt.steps = steps; rt.lo = (uint8_t)lo; rt.hi = (uint8_t)hi; rt.step = (uint8_t)lo;
    rt.t = 0; rt.icon = 0; rt.label_t = 0; rt.vhold = 0;
    rm.tut = (uint8_t)(who + 1); rm.focus = (uint8_t)who; rm.edit = 0; rm.menu = 0; rm.rec = 0; rm.arm = 0; mx.menu = 0;
    /* a topic out of the middle: what the steps before it set up, without their time (as PLAY pressed through them);
       a topic that is not of the instrument's own lesson starts from that lesson's first picture */
    if (who < RI_N && RD[who]->nlesson) {
        if (steps == RD[who]->lesson) for (i = 0; i < lo; i++) rt_act(steps, i, 0, 0);
        else rt_act(RD[who]->lesson, 0, 0, 0);
    }
    snd_ui(79);
}
static void rtutor_open(int who, int then)       /* the whole lesson */
{
    if (who >= RI_N || !RD[who]->nlesson) return;
    rt.then = (uint8_t)then; rt.list = 0;
    rt_begin(who, RD[who]->lesson, 0, RD[who]->nlesson);
}
static void rtutor_close(void)
{
    if (rt.on && !(rt.list == 1)) rt_bring_back();
    rt.on = 0; rt.list = 0; rm.tut = 0; rm.focus = rt.then;
    rh.zoom = 0; rh.closing = 0;
    snd_ui(67);
}

/* ---------------------------------------------------------------- the topics' own steps: every instrument's */
static void rtx_say(const char *s) { rt.label = s; rt.label_t = 1000; }
static void rtx_say_who(int who, const char *what)   /* the instrument that stands in a place, and what it does: "THE GEODE ALONE" */
{
    static char b[36];
    const char *n = RD[who]->name;
    int k = 0;
    while (*n && k < 22) b[k++] = *n++;
    b[k++] = ' ';
    while (*what && k < 35) b[k++] = *what++;
    b[k] = 0;
    rtx_say(b);
}
static int rtx_part(int who)                     /* a part of it with something to hear */
{
    int s;
    for (s = 0; s < RP_N; s++) if (ri[who].p[s].what && RD[who]->ksnd[ri[who].p[s].what] != SK_NONE) return s;
    return 0;
}
#define RT_EVERY(ms_) (t / (ms_) != (t - dt) / (ms_))
static void rtx_lfo(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[rt.who];
    if (RT_FIRST) { r->lfo_on = 1; r->lfo[0] = r->lfo[1] = r->lfo[2] = r->lfo[3] = 0; r->lfo_rate = 2; }
    rt.vhold = t > 600 && t < 6200 ? BTN_LFO + 1 : 0;
    rt.icon = t < 1400 ? RTI_BTN + BTN_LFO : t < 3600 ? RTI_K1 : t < 6200 ? RTI_SELECT : RTI_BTN + BTN_LFO;
    if (RT_AT(1500)) r->lfo[0] = 3;
    if (RT_AT(2100)) r->lfo[0] = 6;
    if (RT_AT(2700)) r->lfo[0] = 8;
    if (RT_AT(3800)) r->lfo_rate = 1;
    if (RT_AT(4600)) r->lfo_rate = 0;
    if (RT_AT(6400)) { r->lfo_on = 0; rtx_say("SWINGS OFF"); }
    if (RT_AT(7400)) { r->lfo_on = 1; rtx_say("SWINGS ON"); }
}
static void rtx_env(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[rt.who];
    int i;
    if (RT_FIRST) for (i = 0; i < 4; i++) r->env[i] = 4;
    rt.vhold = t > 500 ? BTN_ENV + 1 : 0;
    rt.icon = t < 1200 ? RTI_BTN + BTN_ENV : t < 2700 ? RTI_K1 : t < 4200 ? RTI_K2 : t < 5700 ? RTI_K3 : RTI_K4;
    if (RT_AT(1300)) r->env[0] = 7;
    if (RT_AT(2600)) r->env[0] = 4;
    if (RT_AT(2800)) r->env[1] = 8;
    if (RT_AT(3600)) r->env[1] = 1;
    if (RT_AT(4300)) { r->env[1] = 5; r->env[2] = 8; }
    if (RT_AT(5000)) r->env[2] = 1;
    if (RT_AT(5600)) r->env[2] = 4;
    if (RT_AT(5800)) r->env[3] = 8;
    if (RT_AT(6700)) r->env[3] = 0;
    if (t > 700 && RT_EVERY(430)) ri_strike(rt.who, rtx_part(rt.who), 170);
}
static void rtx_fx(int32_t t, int32_t dt)
{
    int w = rt.who;
    if (RT_FIRST) { rmx[w].echo = 0; rmx[w].rev = 0; rmx[w].drv = 0; rmx[w].lvl = 12; rmx[w].mute = 0; }
    rt.vhold = t > 500 && t < 6400 ? BTN_FX + 1 : 0;
    rt.icon = t < 1200 ? RTI_BTN + BTN_FX : t < 2600 ? RTI_K1 : t < 3900 ? RTI_K2 : t < 5100 ? RTI_K3 : t < 6400 ? RTI_K4 : RTI_BTN + BTN_FX;
    if (RT_AT(1300)) rmx[w].echo = 7;
    if (RT_AT(2700)) { rmx[w].echo = 2; rmx[w].rev = 8; }
    if (RT_AT(4000)) { rmx[w].rev = 3; rmx[w].drv = 7; }
    if (RT_AT(5200)) { rmx[w].drv = 1; rmx[w].lvl = 5; }
    if (RT_AT(5900)) rmx[w].lvl = 12;
    if (RT_AT(6600)) { rmx[w].mute = 1; rtx_say("MUTED"); }
    if (RT_AT(7700)) { rmx[w].mute = 0; rtx_say("HEARD"); }
    if (t > 700 && RT_EVERY(520)) ri_strike(w, rtx_part(w), 170);
}
static void rtx_arp(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[rt.who];
    int k;
    if (RT_FIRST) { r->arp = 0; r->ar[0] = 0; r->ar[1] = 0; r->ar[2] = 1; r->ar[3] = 1; r->arp_n = 0; }
    rt.vhold = t > 3000 ? BTN_ARP + 1 : 0;
    rt.icon = t < 3000 ? RTI_BTN + BTN_ARP : t < 6000 ? RTI_K1 : RTI_K3;
    if (RT_AT(1000)) { r->arp = 1; rtx_say(ARP_NAME[0]); ri_tend(rt.who); }
    if (RT_AT(2000)) ri_tend(rt.who);
    for (k = 1; k < 4; k++) if (RT_AT(2400 + k * 900)) { r->ar[0] = (uint8_t)k; rtx_say(ARP_NAME[k]); ri_tend(rt.who); }
    if (RT_AT(6200)) { r->ar[2] = 3; ri_tend(rt.who); }
    if (RT_AT(7100)) ri_tend(rt.who);
}
static void rtx_scale(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[rt.who];
    if (RT_FIRST) { rm.scale = 1; rm.root = 9; r->oct = 0; }
    rt.icon = t < 3600 ? RTI_BTN + BTN_SEL : t < 5600 ? RTI_K1 : t < 6700 ? RTI_BTN + BTN_OCT_UP : RTI_BTN + BTN_OCT_DOWN;
    if (RT_AT(1200)) { rm.scale = 2; rtx_say(RSC_NAME[2]); }
    if (RT_AT(2400)) { rm.scale = 5; rtx_say(RSC_NAME[5]); }
    if (RT_AT(3800)) { rm.root = 0; rtx_say("ROOT C"); }
    if (RT_AT(4700)) { rm.root = 5; rtx_say("ROOT F"); }
    if (RT_AT(5800)) { r->oct = 1; rtx_say("OCTAVE UP"); }
    if (RT_AT(6800)) { r->oct = -1; rtx_say("OCTAVE DOWN"); }
    if (RT_AT(7700)) r->oct = 0;
    if (t > 300 && RT_EVERY(300)) {              /* up the scale and round again, in the instrument's own voice */
        int s = rtx_part(rt.who);
        rm_voice(rt.who, RD[rt.who]->ksnd[r->p[s].what], (t / 300) % 8, 190, 3, 3, 0);
    }
}
static void rtx_layouts(int32_t t, int32_t dt)
{
    rinst_t *r = &ri[rt.who];
    const rdesc_t *d = RD[rt.who];
    int k;
    if (RT_FIRST) { r->preset = 0; r->run = 1; }
    rt.icon = t < 3600 ? RTI_PRESETS : RTI_ALGO;
    for (k = 1; k < 4; k++) if (RT_AT(k * 900)) { r->preset = (uint8_t)(k % d->npreset); rtx_say(d->presets[r->preset]); d->init(); }
    for (k = 0; k < 3; k++) if (RT_AT(3800 + k * 1000)) { ri_load_algo(rt.who, (2 + k) % d->nalgo); rtx_say(d->algos[r->algo]); }
    if (RT_AT(6900)) { ri_load_algo(rt.who, 0); rtx_say(d->algos[0]); }
}
static void rtx_rec(int32_t t, int32_t dt)
{
    static const uint8_t K[5] = {2, 6, 9, 6, 12};
    int k;
    rt.icon = t < 1400 ? RTI_BTN + BTN_REC : t < 4700 ? RTI_LOW : RTI_BTN + BTN_REC;
    rm.rec = t > 900 && t < 4900;                /* (only its light: in a lesson nothing is recorded) */
    if (RT_AT(900)) rtx_say("REC");
    for (k = 0; k < 5; k++) if (RT_AT(1500 + k * 600)) RD[rt.who]->key(K[k], 200);
    if (RT_AT(4900)) rtx_say("ON THE LOOP");
    if (RT_AT(6400)) rtx_say("HELD: WIPED");
}
static void rtx_quick(int32_t t, int32_t dt)     /* the quick mix itself is shown, worked by the lesson */
{
    int k;
    if (RT_FIRST) for (k = 0; k < RI_N; k++) { rmx[k].mute = rmx[k].solo = 0; if (!ri[k].run) { ri[k].run = 1; RD[k]->play(1); } }
    rq.demo = 1;
    rt.icon = t < 1200 ? RTI_BTN + BTN_GLO : t < 6000 ? RTI_LOW : RTI_K1;
    if (RT_AT(1400)) rmx[0].mute = 1;
    if (RT_AT(2500)) { rmx[0].mute = 0; rmx[1].solo = 1; }
    if (RT_AT(3700)) { rmx[1].solo = 0; ri[2].run = 0; RD[2]->play(0); }
    if (RT_AT(4900)) { ri[2].run = 1; RD[2]->play(1); }
    if (RT_AT(6200)) rmx[3].lvl = 16;
    if (RT_AT(6900)) rmx[3].lvl = 5;
}
/* [0..7]: every instrument's; [7] is the pool's last too */
static const rt_step_t RT_SHARED[8] = {
    {8600, {"HOLD LFO AND TURN A KNOB: THAT LAW NOW SWINGS BY ITSELF, IN TIME. STILL HOLDING, SELECT SETS HOW SLOWLY. A TAP ON LFO: ALL SWINGS OFF, OR ON AGAIN."}, rtx_lfo},
    {7600, {"HOLD ENV AND TURN KNOB 1 TO 4: THE ATTACK, LENGTH, BEND AND BRIGHTNESS OF EVERY HIT THIS INSTRUMENT MAKES."}, rtx_env},
    {8800, {"HOLD FX AND TURN KNOB 1 TO 4: ECHO, REVERB, DRIVE AND LEVEL. IT IS THIS INSTRUMENT'S STRIP OF THE MIXER. A TAP ON FX: MUTE."}, rtx_fx},
    {8200, {"TAP ARP: EVERY FEW BARS THE LAYOUT CHANGES A LITTLE BY ITSELF. HOLD ARP AND TURN KNOB 1 TO 4: HOW, HOW OFTEN, HOW MUCH, AND WHETHER IT COMES HOME AGAIN."}, rtx_arp},
    {8400, {"TAP SCL: THE NEXT SCALE, FOR THE WHOLE ROOM. HOLD SCL AND TURN KNOB 1: THE ROOT. OCT- AND OCT+: THIS INSTRUMENT'S OCTAVE."}, rtx_scale},
    {8000, {"PRESETS CHANGES WHAT IS LOOSE IN THIS WORLD. ALGORITHM STEPS THROUGH READY-MADE LAYOUTS. THE FIRST ONE IS YOUR OWN: WHATEVER YOU EDIT GOES THERE."}, rtx_layouts},
    {7600, {"REC RECORDS EVERYTHING THAT SOUNDS ONTO THE LOOP, WHICH GOES ROUND WITH THE ROOM. REC AGAIN: ENOUGH. HOLD REC A SECOND: THIS INSTRUMENT'S PART OF THE LOOP IS WIPED."}, rtx_rec},
    {8000, {"HOLD GLO ANYWHERE IN THE ROOM: THE QUICK MIX. FOUR BOTTOM KEYS FOR EACH INSTRUMENT: MUTE, SOLO, STOP, WIPE ITS LOOP. KNOB 1 TO 4: THE LEVELS. FX, SCL, ENV, LFO OR EDIT GOES STRAIGHT TO THAT ONE. LET GO AND IT IS GONE."}, rtx_quick},
};

/* ---------------------------------------------------------------- the pool's */
static void rtp_reset(void)
{
    static const uint8_t PAN[RI_N] = {3, 6, 9, 13};
    int k;
    for (k = 0; k < RI_N; k++) { rmx[k].lvl = 12; rmx[k].pan = PAN[k]; rmx[k].mute = rmx[k].solo = 0; if (!ri[k].run) { ri[k].run = 1; RD[k]->play(1); } }
    mx.stir_on = 0; mx.dive_on = 0; mx.dive_mask = 0; mx.menu = 0; mx.sel = 0; mx.all_stop = 0;
}
static void rtp_levels(int32_t t, int32_t dt)
{
    static const uint8_t WHO[5] = {0, 0, 1, 2, 3}, LV[5] = {16, 4, 16, 3, 15};
    int k;
    if (RT_FIRST) rtp_reset();
    rt.icon = t < 2600 ? RTI_K1 : t < 3600 ? RTI_K2 : t < 4600 ? RTI_K3 : RTI_K4;
    for (k = 0; k < 5; k++) if (RT_AT(1000 + k * 900)) { rmx[WHO[k]].lvl = LV[k]; mx.sel = WHO[k]; }
    (void)dt;
}
static void rtp_pan(int32_t t, int32_t dt)
{
    static const uint8_t PN[5] = {6, 3, 0, 10, 16};
    int k;
    if (RT_FIRST) rtp_reset();
    rt.icon = t < 2000 ? RTI_SELECT : RTI_PRESETS;
    if (RT_AT(800)) mx.sel = 1;
    if (RT_AT(1500)) mx.sel = 2;
    for (k = 0; k < 5; k++) if (RT_AT(2200 + k * 650)) rmx[2].pan = PN[k];
    (void)dt;
}
static void rtp_keys(int32_t t, int32_t dt)
{
    if (RT_FIRST) rtp_reset();
    rt.icon = RTI_LOW; rt.vhold = 255;           /* (no button: only so the caption stands at the top, clear of the strip of keys) */
    if (RT_AT(1200)) { rmx[0].mute = 1; mx.sel = 0; rtx_say_who(0, "MUTED"); }
    if (RT_AT(2400)) { rmx[0].mute = 0; rmx[1].solo = 1; mx.sel = 1; rtx_say_who(1, "ALONE"); }
    if (RT_AT(3800)) { rmx[1].solo = 0; ri[2].run = 0; RD[2]->play(0); mx.sel = 2; rtx_say_who(2, "STOPS"); }
    if (RT_AT(5200)) { ri[2].run = 1; RD[2]->play(1); rtx_say("AND PLAYS"); }
    (void)dt;
}
static void rtp_drift(int32_t t, int32_t dt)
{
    if (RT_FIRST) rtp_reset();
    rt.icon = t < 3500 ? RTI_BTN + BTN_LFO : RTI_BTN + BTN_ARP;
    if (RT_AT(900)) { mx.stir_on = 1; mx.stir = 8; mx.stir_rate = 0; rtx_say("THE CURRENT"); }
    if (RT_AT(3600)) { mx.stir_on = 0; mx.dive_on = 1; mx.dive_mask = 1u << 1; rtx_say("A DIVE"); }
    if (RT_AT(4800)) mx.dive_mask = 1u << 3;
    if (RT_AT(6000)) { mx.dive_mask = 0; mx.dive_on = 0; }
    (void)dt;
}
static void rtp_loop(int32_t t, int32_t dt)
{
    if (RT_FIRST) rtp_reset();
    rt.vhold = t > 2400 && t < 6000 ? BTN_SEQ + 1 : 0;
    rt.icon = t < 2400 ? RTI_BTN + BTN_SEQ : t < 4200 ? RTI_K1 : t < 6000 ? RTI_K2 : RTI_BTN + BTN_REC;
    if (RT_AT(900)) { rm.loop_off = 1; rtx_say("THE LOOP RESTS"); }
    if (RT_AT(1800)) { rm.loop_off = 0; rtx_say("AND PLAYS"); }
    if (RT_AT(2700)) rm.bars = 2;
    if (RT_AT(3400)) rm.bars = 8;
    if (RT_AT(4400)) rm.quant = 1;
    if (RT_AT(5100)) rm.quant = 2;
    (void)dt;
}
static void rtp_tone(int32_t t, int32_t dt)
{
    if (RT_FIRST) rtp_reset();
    rt.vhold = t > 2600 && t < 5200 ? BTN_FX + 1 : t >= 5200 ? BTN_ENV + 1 : 0;
    rt.icon = t < 2600 ? RTI_ALGO : t < 3600 ? RTI_K1 : t < 5200 ? RTI_K2 : RTI_BTN + BTN_ENV;
    if (RT_AT(900)) rm.bpm = (uint8_t)imin(200, rm.bpm + 24);
    if (RT_AT(1900)) rm.bpm = rt_keep.bpm;
    if (RT_AT(2900)) { rm.etime = 3; room_echo_set(); }
    if (RT_AT(3800)) { rm.efb = 8; room_echo_set(); }
    if (RT_AT(5500)) rmx[0].tone = 0;
    if (RT_AT(6300)) rmx[0].tone = 8;
    if (RT_AT(7000)) rmx[1].tone = 2;
    (void)dt;
}
static void rtp_menu(int32_t t, int32_t dt)
{
    int k;
    if (RT_FIRST) rtp_reset();
    rt.icon = t < 1000 ? RTI_BTN + BTN_EDIT : t < 4400 ? RTI_SELECT : RTI_K1;
    if (RT_AT(900)) { mx.menu = 1; mx.row = 0; }
    for (k = 1; k < 7; k++) if (RT_AT(1100 + k * 500)) mx.row = (uint8_t)k;
    if (RT_AT(4700)) rm.efb = 8;
    if (RT_AT(5300)) rm.efb = 2;
    (void)dt;
}
static const rt_step_t RT_POOL[7] = {
    {6200, {"EACH INSTRUMENT IS A STONE AFLOAT IN THE POOL. NEAR THE FRONT IT IS LOUD, FAR AWAY IT IS QUIET. KNOB 1 TO 4 MOVE THE FOUR STONES."}, rtp_levels},
    {6000, {"SELECT PUTS A STONE IN HAND. PRESETS CARRIES IT TO THE LEFT OR THE RIGHT, AND THAT IS WHERE YOU HEAR IT."}, rtp_pan},
    {7000, {"THE BOTTOM ROW: FOUR KEYS FOR EACH STONE. MUTE IT, SOLO IT, STOP ITS WORLD, WIPE ITS PART OF THE LOOP. THE STRIP OF KEYS UNDER THE POOL SHOWS WHAT IS ON."}, rtp_keys},
    {7000, {"TAP LFO: THE CURRENT CARRIES THE STONES ROUND, SO THE MIX MOVES. TAP ARP: DIVES, BY TURNS A STONE GOES UNDER FOR A WHILE. HOLD EITHER AND TURN KNOB 1 AND 2 TO SET IT."}, rtp_drift},
    {7200, {"TAP SEQ: THE LOOP RESTS, OR PLAYS AGAIN. HOLD SEQ: KNOB 1 ITS LENGTH IN BARS, KNOB 2 THE SIEVE THAT TIDIES WHAT IS RECORDED. HOLD REC A SECOND: ALL OF IT IS WIPED."}, rtp_loop},
    {8000, {"ALGORITHM: THE TEMPO. HOLD FX: KNOB 1 THE ECHO'S TIME, KNOB 2 HOW LONG IT GOES ON. HOLD ENV: KNOB 1 TO 4, THE TONE OF EACH INSTRUMENT."}, rtp_tone},
    {6200, {"EDIT OPENS THE MENU: EVERY SETTING OF THE ROOM IN ONE LIST. SELECT A ROW, TURN A KNOB TO SET IT. EDIT AGAIN CLOSES IT."}, rtp_menu},
};

/* ---------------------------------------------------------------- HOW IT WORKS: the list of topics */
typedef struct { const char *name; uint8_t first, n; } rtopic_t;   /* steps first..first+n-1 of the instrument's own lesson */
static const rtopic_t RTP_ROOTS[6] = {{"ROOTS AND PODS", 0, 2}, {"THE WIND", 2, 1}, {"LENGTH AND LEAN", 3, 1}, {"SEQ: THE BEETLE", 4, 1}, {"EDIT: YOUR OWN", 5, 1}, {"TOP ROW: PUFFS", 6, 1}};
static const rtopic_t RTP_GEODE[6] = {{"SHELL AND STONE", 0, 2}, {"WHAT GROWS INSIDE", 2, 1}, {"STONES AND GRIP", 6, 1}, {"SEQ: THE PLECTRUM", 5, 1}, {"EDIT: YOUR OWN", 3, 1}, {"SCATTER", 4, 1}};
static const rtopic_t RTP_RACK[6] = {{"POD AND CAPS", 0, 2}, {"SPRING AND TILT", 2, 1}, {"SIEVE, PODS, ENDS", 3, 1}, {"SEQ: THE RESET", 4, 1}, {"THE KEYS", 5, 1}, {"EDIT: YOUR OWN", 6, 1}};
static const rtopic_t RTP_SEAM[7] = {{"GROW AND BREAK", 0, 1}, {"STRESS AND A TAP", 1, 1}, {"SHARDS", 2, 1}, {"RING", 3, 1}, {"SEQ: THE DRIP", 4, 1}, {"EDIT: YOUR OWN", 5, 1}, {"TOP ROW: SHAKES", 6, 1}};
static const rtopic_t *const RTP_OWN_[RI_N] = {RTP_ROOTS, RTP_GEODE, RTP_RACK, RTP_SEAM};
static const uint8_t RTP_OWN_N_[RI_N] = {6, 6, 6, 7};
_Static_assert(sizeof(rtopic_t) == sizeof(mi_topic_t) && __builtin_offsetof(rtopic_t, first) == __builtin_offsetof(mi_topic_t, first), "mi.h: a topic");
/* the topics of the instrument that stands in a place: the firmware's own, or the loaded one's */
static const rtopic_t *rtp_own(int who) { return mi_on(who) ? (const rtopic_t *)(const void *)mb.ih[who]->topics : RTP_OWN_[who]; }
static int rtp_own_n(int who) { return mi_on(who) ? mb.ih[who]->ntopic : RTP_OWN_N_[who]; }
static const char *const RTP_SHARED[8] = {"LFO: SWINGS", "ENV: A HIT'S SHAPE", "FX AND MUTE", "ARP: TENDS ITSELF", "SCALE AND OCTAVE", "PRESETS, LAYOUTS", "REC: THE LOOP", "QUICK MIX: GLO"};
static const char *const RTP_POOL[7] = {"THE STONES", "LEFT AND RIGHT", "MUTE, SOLO, STOP", "CURRENT AND DIVES", "THE LOOP", "TEMPO, ECHO, TONE", "THE MENU"};
#define RTL_MAX 18
static struct { const char *name; const rt_step_t *steps; uint8_t lo, hi; } rtl[RTL_MAX];
static uint8_t rtl_n;
static void rtl_add(const char *name, const rt_step_t *steps, int lo, int hi)
{
    if (rtl_n >= RTL_MAX) return;
    rtl[rtl_n].name = name; rtl[rtl_n].steps = steps; rtl[rtl_n].lo = (uint8_t)lo; rtl[rtl_n].hi = (uint8_t)hi; rtl_n++;
}
static void rtopics_open(int who, int then)
{
    int k;
    rtl_n = 0;
    if (who < RI_N) {
        if (RD[who]->nlesson) rtl_add("THE WHOLE LESSON", RD[who]->lesson, 0, RD[who]->nlesson);
        for (k = 0; k < rtp_own_n(who); k++) rtl_add(rtp_own(who)[k].name, RD[who]->lesson, rtp_own(who)[k].first, rtp_own(who)[k].first + rtp_own(who)[k].n);
        for (k = 0; k < 8; k++) rtl_add(RTP_SHARED[k], RT_SHARED, k, k + 1);
    } else {
        rtl_add("THE WHOLE LESSON", RT_POOL, 0, 7);
        for (k = 0; k < 7; k++) rtl_add(RTP_POOL[k], RT_POOL, k, k + 1);
        rtl_add(RTP_SHARED[7], RT_SHARED, 7, 8);
    }
    rt.on = 1; rt.list = 1; rt.who = (uint8_t)who; rt.then = (uint8_t)then; rt.row = 0; rt.vhold = 0;
    rm.edit = 0; rm.menu = 0; mx.menu = 0; rhome.down = 0;
    snd_ui(79); sfx(84, 60, 160, W_TRI, 1800, 3, 60);
#ifdef DINGHY_HOST
    printf("%6u ms  HOW IT WORKS: %s, %d topics\n", fm1_ms, who < RI_N ? RD[who]->name : "THE POOL", rtl_n);
#endif
}

static void rtutor_tick(int32_t dt)
{
    rt.t += dt;
    if (rt.list == 1) {                          /* the list: the room plays on under it */
        int32_t mv = in.any_knob;
        led_play_breathe = 1; led_breathe_want |= 1u << BTN_HOME;
        if (mv) { rt.row = (uint8_t)iclamp(rt.row + (mv > 0 ? 1 : -1), 0, rtl_n - 1); snd_ui(64 + rt.row); }
        if (BDOWN(BTN_HOME) || BDOWN(BTN_REC)) { rtutor_close(); return; }
        if (BDOWN(BTN_PLAY)) {
#ifdef DINGHY_HOST
            printf("%6u ms  topic: %s\n", fm1_ms, rtl[rt.row].name);
#endif
            rt_begin(rt.who, rtl[rt.row].steps, rtl[rt.row].lo, rtl[rt.row].hi);
            rt.list = 2;
        }
        return;
    }
    if (rt.label_t > 0) rt.label_t -= dt;
    rq.demo = 0;
    rt_act(rt.steps, rt.step, rt.t, dt);
    led_play_breathe = 1; led_breathe_want |= 1u << BTN_HOME;
    if (rt.icon >= RTI_BTN && (rt.t / 400) % 2 == 0) led_btn_want |= 1u << (rt.icon - RTI_BTN);
    if (rt.who < RI_N) ri_leds(rt.who); else mix_leds();
    if (BDOWN(BTN_HOME) || BDOWN(BTN_REC) || BDOWN(BTN_PLAY) || rt.t >= rt.steps[rt.step].ms) {
        int over = BDOWN(BTN_HOME) || BDOWN(BTN_REC) || rt.step + 1 >= rt.hi;
        if (!over) { rt.step++; rt.t = 0; rt.vhold = 0; rq.demo = 0; rm.rec = 0; snd_ui(72); return; }
        if (rt.list) {                           /* a topic out of the list: the list again, the room as it was */
            rt_bring_back();
            rt.list = 1; rm.focus = rt.then;
            if (!(BDOWN(BTN_HOME) || BDOWN(BTN_REC))) { snd_chime(0); if (rt.row + 1 < rtl_n) rt.row++; }   /* (seen through: the next one is chosen) */
            else snd_ui(67);
            return;
        }
        rtutor_close();
        if (!(BDOWN(BTN_HOME) || BDOWN(BTN_REC))) snd_chime(0);
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
static void rtopics_draw(void)
{
    int32_t top = iclamp(rt.row - 4, 0, imax(0, rtl_n - 10)), k, own = rt.who < RI_N ? 1 + rtp_own_n(rt.who) : rtl_n - 1;
    col_t c = ri_col(rt.who);
    gclear(th->bg);
    rm_head(rt.who < RI_N ? RD[rt.who]->name : "THE POOL", "HOW IT WORKS", c);
    for (k = 0; k < 10 && top + k < rtl_n; k++) {
        int row = top + k, sel = row == rt.row;
        int32_t y = 17 + k * 18, sc = 2;
        char nb[3] = {(char)(row >= 10 ? '1' : ' '), (char)('0' + row % 10), 0};
        while (sc > 1 && gtextw(rtl[row].name, sc) > 200) sc--;
        if (sel) { grect(2, y, 236, 17, th->panel); gbox(2, y, 236, 17, c); }
        if (row == own) ghl(8, 232, y - 1, th->mute);   /* (under the line: what every instrument has) */
        if (row) gtextr(22, y + 6, nb, th->mute, 1);
        gtext(28, y + (sc == 2 ? 4 : 6), rtl[row].name, sel ? th->text : row == 0 ? c : th->mute, sc);
        if (sel) { galpha(140 + ipulse(rm.t, 1200) * 116 / 256); gtextr(232, y + 6, PB, th->pad, 1); galpha(256); }
    }
    if (top > 0) gtextc(120, 13, "\xE2\x86\x91", th->mute, 1);
    if (top + 10 < rtl_n) gtextc(120, 199, "\xE2\x86\x93", th->mute, 1);
    grect(0, 206, AC_W, 34, th->bg); ghl(0, AC_W, 206, th->mute);
    gfit(120, 213, 232, "SELECT: CHOOSE    " PB ": WATCH IT", th->text, ts(1, 2));
    gfit(120, 229, 232, "HOME: BACK", th->mute, 1);
}
static void rq_draw(int32_t y);
static void rtutor_draw(void)
{
    const rt_step_t *st;
    const char *s;
    int32_t sc = tsz() >= 2 ? 2 : 1, tw, h, y, k, n = rt.hi - rt.lo, up;
    char tag[20] = "HOW TO PLAY  1/8";
    col_t c = ri_col(rt.who);
    if (rt.list == 1) { rtopics_draw(); return; }
    st = &rt.steps[rt.step];
    s = st->tx[lang] ? st->tx[lang] : st->tx[L_EN];
    tw = rt.icon ? 172 : 228; h = imax(rt.icon ? 40 : 20, gblock_h(tw, s, sc)) + 14;
    up = rt.vhold && !rq.demo;                   /* a held button's page is on the strip: the caption goes to the top */
    y = up ? 13 : AC_H - h;
    if (rt.who < RI_N) {
        gclear(th->bg);
        RD[rt.who]->draw(rm.edit);
        if (rm.menu) ri_menu_draw(rt.who);
        if (rt.label_t > 0 && rt.label) { galpha(imin(256, rt.label_t)); gtextc(120, up ? 150 : 100, rt.label, th->text, 2); galpha(256); }
        tag[13] = (char)('1' + rt.step - rt.lo); tag[15] = (char)('0' + imin(n, 9));
        rm_head(RD[rt.who]->name, tag, c);
        if (up) ri_strip(rt.who);
    } else {
        mix_draw();
        if (rt.label_t > 0 && rt.label) { galpha(imin(256, rt.label_t)); gtextc(120, up ? 150 : 60, rt.label, th->text, 2); galpha(256); }
    }
    if (rq.demo) rq_draw(14);
    grect(0, y, AC_W, h, th->panel); ghl(0, AC_W, up ? y + h : y, c);
    gblock(6, y + 5, tw, s, th->text, sc, 0);
    if (rt.icon) rt_icon(210, y + 20, rt.icon, c);
    for (k = 0; k < n; k++) grect(6 + k * 8, y + h - 6, 6, 2, k <= rt.step - rt.lo ? c : th->mute);   /* how far through */
    grect(6 + (rt.step - rt.lo) * 8, y + h - 6, 6 * imin(rt.t, (int32_t)st->ms) / (int32_t)st->ms, 2, th->text);
    galpha(120 + ipulse(rm.t, 1400) * 136 / 256); gtextr(236, y + h - 9, PB " \xE2\x86\x92", th->pad, 1); galpha(256);
}
