/* SPDX-License-Identifier: GPL-3.0-only */
/* The roll's music. The course is a piece of music laid out along the land: twenty-eight places from the start to the
 * cave, each a bar of it (a chord, a bar of the melody). Where Rock is, is where the piece is: rolling on moves it on,
 * standing still holds the bar he stands in, rolling back plays it back. The harmony only ever moves on the beat (a new
 * place is taken up at the next half bar), every part reads its notes from the place's chord, and all of it is on the
 * white notes: whatever Rock does, it stays in tune and in time.
 *
 * How well he rolls is how much of it plays (the "flow": his speed, and what he earns by clean pad jumps, spores, caps
 * and rings, which wears off again):
 *   standing     the chord, breathing, and a slow music-box note every beat
 *   rolling      the arpeggio in eighths, the bass stepping along, the melody's outline (its notes on the beats)
 *   rolling well a soft kick, a tick between; with what he earns, the whole melody
 *   flying along the kick on every beat, sixteenths glittering an octave up, the melody doubled
 * Speed alone brings the outline and the kick; the whole melody and what is over it have to be earned.
 * In the air the kick and the bass's steps fall away and the arpeggio lifts an octave into the echo; he lands on the
 * chord's own low note and the beat comes back.
 *
 * The keys play the chord's five notes (the same key a step higher or lower as the harmony moves, never a wrong one),
 * with a bell an octave over them. A key that jumps throws a run up the scale after it, on the beat: three notes for a
 * hop, five for a big one. A launch pad's jump strums the chord; so do a ring and a landing the pad set up exactly.
 *
 * The tunes run in the audio interrupt (mus_roll, from mus_tick); the game sets what they read (rm_want, rm_flow, rm_air,
 * rm_lead: a byte each) and reads back what was played through a small queue (rm_evq), which is what the pictures
 * move to: the land's seams and rim on the bass and the kick, a mushroom for every arpeggio note, the melody as stars.
 * How strongly they move (rv.vis) fades in as Rock rolls off the platform he woke on, and out again back on it. */

enum { RC_C9, RC_EMB, RC_AM7, RC_F7, RC_GS, RC_G6, RC_CE, RC_AM9, RC_DM9, RC_EM7, RC_FLY };
typedef struct { uint8_t bass, pad[3], arp[4], pen, col; } rm_chord_t;   /* pen: which five notes the keys play; col: its colour (of th->caps) */
static const rm_chord_t RM_CH[] = {
    {36, {52, 55, 62}, {60, 64, 67, 74}, 0, 0},  /* C add9 */
    {35, {52, 55, 62}, {59, 64, 67, 71}, 2, 3},  /* E minor 7 over B */
    {33, {52, 57, 60}, {60, 64, 67, 69}, 0, 1},  /* A minor 7 */
    {29, {53, 57, 64}, {57, 60, 64, 69}, 1, 2},  /* F major 7 */
    {31, {50, 55, 60}, {60, 62, 67, 74}, 0, 4},  /* G sus4 */
    {31, {50, 55, 59}, {62, 64, 67, 71}, 2, 4},  /* G 6 */
    {40, {55, 60, 64}, {60, 64, 67, 72}, 0, 0},  /* C over E */
    {33, {52, 55, 59}, {60, 64, 67, 71}, 0, 1},  /* A minor 9 */
    {38, {53, 57, 64}, {62, 65, 69, 72}, 1, 2},  /* D minor 9 */
    {40, {50, 55, 59}, {59, 62, 64, 67}, 2, 3},  /* E minor 7 */
    {29, {57, 60, 64}, {59, 64, 69, 72}, 2, 2},  /* F major 7 with a sharp eleventh: the hollow behind the start */
};
/* the keys' five notes over each kind of chord, counted from C3: C's (A minor's), F's (D minor's), G's (E minor's) */
static const uint8_t RM_PEN[3][5] = {{0, 2, 4, 7, 9}, {0, 2, 5, 7, 9}, {2, 4, 7, 9, 11}};
/* the arpeggio: which of the chord's four notes on each eighth. One for each third of the course */
static const uint8_t RM_PAT[3][8] = {{0, 1, 2, 1, 3, 1, 2, 1}, {0, 2, 1, 2, 3, 2, 1, 2}, {0, 1, 2, 3, 1, 2, 3, 2}};

/* the piece. A place's melody is a bar of eighths: g a b (below C5), C D E F G A B (from C5), c d e (from C6), '.' a
 * rest. Four parts: FIRST LIGHT (eight bars: a rising figure and its answer), THE VALLEY (eight: minor, moving), THE
 * POOLS (four: a breath, two long notes a bar, where the pools of light begin), THE CLIMB (eight: a figure stepping up
 * towards the cave, left hanging on G for the cave's own C to answer) */
typedef struct { uint8_t ch, pat; const char *mel; } rm_place_t;
#define RM_NPLACE 28
enum { RM_START = RM_NPLACE, RM_END, RM_HOLLOW, RM_NP };
static const rm_place_t RM_PLACE[RM_NP] = {
    {RC_C9, 0, "gCDE..D."}, {RC_EMB, 0, "G.E.D.b."}, {RC_AM7, 0, "CDE...G."}, {RC_F7, 0, "E...C.a."},
    {RC_C9, 0, "gCDE..G."}, {RC_EM7, 0, "B.G.E.D."}, {RC_F7, 0, "A.G.E.C."}, {RC_GS, 0, "D...G..."},
    {RC_AM9, 1, "EA.Bc.BA"}, {RC_F7, 1, "A.E.F.EC"}, {RC_CE, 1, "G.EG.c.G"}, {RC_G6, 1, "B.A.G.D."},
    {RC_DM9, 1, "FA.EF.Ac"}, {RC_AM7, 1, "e.c.A.G."}, {RC_F7, 1, "Ac.AG.E."}, {RC_GS, 1, "D.E.G.A."},
    {RC_C9, 0, "G...E..."}, {RC_AM9, 0, "B...A..."}, {RC_F7, 0, "A...E..."}, {RC_GS, 0, "D...G.A."},
    {RC_F7, 2, "c.A.c..."}, {RC_G6, 2, "d.B.d..."}, {RC_AM7, 2, "e.c.e.dc"}, {RC_EM7, 2, "B.G...E."},
    {RC_F7, 2, "A.c.A.G."}, {RC_G6, 2, "B.d.B.A."}, {RC_AM7, 2, "c.e.d.c."}, {RC_G6, 2, "d...d.B."},
    {RC_C9, 0, 0},                               /* before the start: the first chord, waiting */
    {RC_C9, 2, "c...G.E."},                      /* the cave's mouth: home */
    {RC_FLY, 0, "B...E..."},                     /* the hollow */
};
static int32_t rm_mel(char c)
{
    static const char L[] = "gabCDEFGABcde";
    static const uint8_t N[] = {67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84, 86, 88};
    int32_t i;
    for (i = 0; L[i]; i++) if (L[i] == c) return N[i];
    return 0;
}

/* what the game sets and the tune reads */
static volatile uint8_t rm_want = RM_START, rm_flow, rm_air, rm_lead, rm_reset;
/* the tune's own: the place it is playing (0xFF: none yet), the step it took it up on, how many bars it has stayed */
static uint8_t rm_cur = 0xFF, rm_rep;
static uint32_t rm_at;
/* what was played, for the pictures: kind << 8 | note */
enum { RE_ARP = 1, RE_LEAD, RE_BASS, RE_KICK, RE_CHORD };
#define RM_EVN 32u
static volatile uint16_t rm_evq[RM_EVN];
static volatile uint32_t rm_evw;
static void rm_push(uint32_t kind, uint32_t note) { rm_evq[rm_evw % RM_EVN] = (uint16_t)(kind << 8 | (note & 255u)); rm_evw = rm_evw + 1u; }

static void mus_roll(uint32_t step)              /* in the interrupt: a sixteenth */
{
    uint32_t s = step & 15u, ls, k;
    int32_t fl = rm_flow, air = rm_air, lead = rm_lead, n, vol;
    const rm_place_t *p;
    const rm_chord_t *c;
    if (rm_reset) { rm_reset = 0; rm_cur = 0xFF; }
    if ((rm_cur == 0xFF && !(s & 3u)) || (rm_cur != 0xFF && !(s & 7u) && rm_want != rm_cur)) {   /* a new place: on the half bar */
        rm_cur = rm_want < RM_NP ? rm_want : RM_START; rm_at = step; rm_rep = 0;
        rm_push(RE_CHORD, rm_cur);
    }
    if (rm_cur == 0xFF) return;
    ls = step - rm_at;
    if (ls && !(ls & 15u) && rm_rep < 200u) rm_rep++;
    p = &RM_PLACE[rm_cur]; c = &RM_CH[p->ch];
    if (!(ls & 15u)) {                           /* the chord swells, the bass under it (and an octave over itself: a small speaker has no F1) */
        for (k = 0; k < 3u; k++) sy_now_fx(c->pad[k], fl < 36 ? 3600u : 2700u, W_SINE, 1600, 450, 30, (int32_t)k * 4 - 4, (int32_t)k * 34 - 34, 0, 70, 0);
        if (fl >= 60 && fl < 185) sy_now_fx(c->pad[1] + 12, 2400, W_TRI, 480, 700, 60, 5, 20, 0, 70, 0);
        sy_now(c->bass, 1400, W_SINE, 4000, 8, 0, 0, 0, 0);
        sy_now(c->bass + 12, 700, W_TRI, 850, 8, 0, 0, 0, 0);
        rm_push(RE_BASS, c->bass);
    } else if (!air && fl >= 60) {               /* rolling: the bass steps along, three and three and two */
        if (s == 6u) sy_now(c->bass + 12, 560, W_SINE, 2900, 6, 0, 0, 0, 0);
        else if (s == 10u) { sy_now(c->bass, 640, W_SINE, 3200, 6, 0, 0, 0, 0); sy_now(c->bass + 12, 360, W_TRI, 600, 6, 0, 0, 0, 0); }
        else if (s == 14u && fl >= 175) sy_now(c->pad[0] - 12, 400, W_SINE, 2800, 6, 0, 0, 0, 0);
    }
    if (!air) {                                  /* the kick: only on the ground, only when it rolls well */
        if (fl >= 105 && !(s & 7u)) { sy_now(47, 230, W_SINE, 4300, 1, 0, 0, 0, 65010); rm_push(RE_KICK, 0); }
        else if (fl >= 185 && !(s & 3u)) { sy_now(47, 200, W_SINE, 3200, 1, 0, 0, 0, 65010); rm_push(RE_KICK, 0); }
    }
    if (fl >= 135 && (s & 3u) == 2u) sy_now(0, 45, W_HAT, 600, 1, 0, 0, 20, 0);   /* the tick between the beats (it keeps time through a jump) */
    else if (fl >= 220 && (s & 1u)) sy_now(0, 30, W_HAT, 300, 1, 0, 0, -20, 0);
    {                                            /* the arpeggio: a note a beat standing, eighths rolling, sixteenths over them flying */
        const uint8_t *pat = RM_PAT[p->pat % 3u];
        uint32_t slow = fl < 36, i;
        if (!(ls % (slow ? 4u : 2u))) {
            i = ((slow ? ls / 4u : ls / 2u) + (uint32_t)rm_rep * 3u) & 7u;
            n = c->arp[pat[i]] + (air ? 12 : 0);
            vol = (fl < 60 ? 1900 : 1500) * (160 + imin(fl, 192) / 2) / 256;
            if (!(ls & 3u)) vol = vol * 5 / 4;
            sy_now_fx(n, slow ? 900u : 480u, fl < 60 ? W_SINE : W_TRI, (uint32_t)vol, 3, air ? 210u : 140u, 0, (i & 1u) ? 30 : -30, 0, air ? 80u : 0u, 0);
            rm_push(RE_ARP, (uint32_t)n);
        } else if (fl >= 175 && (ls & 1u)) {
            i = (ls / 2u + 2u) & 7u;
            sy_now_fx(c->arp[pat[i]] + 12, 260, W_SINE, 640, 2, 120, 0, (i & 1u) ? -40 : 40, 0, 0, 0);
        }
    }
    if (lead && p->mel && rm_rep < 2u && !(ls & 1u)) {   /* the melody: the place's bar, once and (softer, half of it) once
                                                    more; after that the place is left to its chord */
        uint32_t i = (ls / 2u) & 7u;
        if ((lead >= 2 || !(i & 3u)) && !(rm_rep && i >= 4u) && (n = rm_mel(p->mel[i])) != 0) {
            vol = rm_rep ? 1300 : lead >= 2 ? 2300 : 1500;
            sy_now_fx(n, 600, W_SINE, (uint32_t)vol, 4, 120, 0, 0, 0, 90, 0);
            sy_now_fx(n, 300, W_TRI, (uint32_t)vol * 2u / 5u, 2, 60, 5, 0, 0, 0, 0);
            if (fl >= 220) sy_now_fx(n + 12, 420, W_SINE, (uint32_t)vol / 3u, 4, 160, 0, 30, 0, 110, 0);
            rm_push(RE_LEAD, (uint32_t)n);
        }
    }
    if (rm_cur == RM_END && !rm_rep && ls < 8u) {  /* home: the chord opens upwards, a note a sixteenth */
        static const uint8_t BL[8] = {48, 55, 60, 64, 67, 72, 74, 79};
        sy_now_fx(BL[ls], 1500, W_TRI, 1800, 4, 160, 0, (ls & 1u) ? 40 : -40, 0, 120, 0);
        rm_push(RE_LEAD, BL[ls]);
    }
}

/* ---------------------------------------------------------------- the game's side */
#define RM_NMOTE 24
static struct {
    int32_t flow, bonus;                         /* Q8, 0 .. 255 * 256 */
    int32_t air_ms, beat, wash, run_t, rx, ry, camy, dir;
    int32_t vis, vis_q;                          /* how much the pictures move to the beat, 0..256 (and in Q8, to move it slowly) */
    uint32_t evr, star_at;
    uint8_t on, ticked, lead, col, run, arp_note, bus, mnext, shown, head;
    int8_t star_prev;
    struct { int32_t x, y; int16_t life, max; uint8_t c, kind; int8_t link; } mote[RM_NMOTE];   /* kind 0: a star of the melody (the
                                                    screen's own place, Q8); 1: a spark off Rock (the world's) */
    struct { int16_t ms; uint8_t note, on; } pend[8];   /* a run's notes still to come: a spark as each sounds */
    struct { int16_t idx, t; } sing[4];          /* the mushrooms the arpeggio's last notes popped (ac_intro.h picks them) */
} rv;
#define RM_SING_MS 320

static int rm_place_now(void) { int p = rm_cur; return p < RM_NP ? p : (rm_want < RM_NP ? rm_want : RM_START); }
static const rm_chord_t *rm_chord_now(void) { return &RM_CH[RM_PLACE[rm_place_now()].ch]; }
/* the keys' ladder over the chord that is sounding: step d (0: around C3) -> its note. With LINK on it is the one
 * shared key's, C's five notes, whatever the chord */
static int32_t rm_ladder(int32_t d)
{
    const uint8_t *pn = RM_PEN[mus_shared ? 0 : rm_chord_now()->pen % 3u];
    d = iclamp(d, 0, 22);
    return 48 + pn[d % 5] + 12 * (d / 5);
}
/* ms from now to the next line of the clock's grid (in ticks), and a sixteenth's length */
static uint32_t rm_ms_to(uint32_t grid)
{
    uint32_t g = grid << 8, left = g - tr_now_q8() % g;
    return (uint32_t)((uint64_t)left * 625u / tr_bpm_q8);
}
static uint32_t rm_step_ms(void) { return TICKS_STEP * 256u * 625u / tr_bpm_q8; }
static void roll_bonus(int32_t b) { rv.bonus = imin(Q8(110), rv.bonus + Q8(b)); }

static void rm_mote(int kind, int32_t x, int32_t y, int32_t life, int c)
{
    uint32_t i = rv.mnext % RM_NMOTE;
    rv.mnext = (uint8_t)(i + 1u);
    rv.mote[i].kind = (uint8_t)kind; rv.mote[i].x = x; rv.mote[i].y = y; rv.mote[i].life = rv.mote[i].max = (int16_t)life;
    rv.mote[i].c = (uint8_t)c; rv.mote[i].link = -1;
    if (kind == 0) {                             /* a star: joined to the one before it, if that was the note before */
        if (rv.star_prev >= 0 && fm1_ms - rv.star_at < 700u && rv.mote[rv.star_prev].life > 0 && rv.mote[rv.star_prev].kind == 0 && rv.mote[rv.star_prev].x < x)
            rv.mote[i].link = rv.star_prev;
        rv.star_prev = (int8_t)i; rv.star_at = fm1_ms;
    }
}
static void rm_spark(int32_t note) { rm_mote(1, rv.rx + Q8((note % 7) * 3 - 9), rv.ry - Q8(10), 520, rm_chord_now()->col); }

/* a key on the course: its note of the chord in the voice its colour wears, a bell an octave over it into the echo */
static void roll_keys(uint32_t down)
{
    int32_t k;
    for (k = 0; k < AC_NKEY; k++)
        if (down >> k & 1u) {
            int32_t n = rm_ladder(k * 15 / 26);
            pf_key_voice(k, n);
            sfx_fx(n + 12, 0, 650, W_SINE, 1400, 3, 190, 0, 0, 0, 100, 0);
            rm_spark(n);
#ifdef DINGHY_HOST
            printf("%6u ms  roll key %d plays %d (place %d)\n", fm1_ms, (int)k, (int)n, rm_place_now());   /* (tests) */
#endif
        }
}
/* the chord, strummed upwards as a harp's: a launch pad's jump (big: two octaves and a high note left ringing) */
static void roll_strum(int big)
{
    const rm_chord_t *c = rm_chord_now();
    uint32_t i, cnt = big ? 7u : 5u;
    for (i = 0; i < cnt; i++)
        sfx_fx(c->arp[i & 3u] + 12 * (int32_t)(i / 4u), i * 42u, 620, W_TRI, 1900, 4, 150, 0, (i & 1u) ? 36 : -36, 0, 90, 0);
    if (big) sfx_fx(c->arp[0] + 24, cnt * 42u, 1300, W_SINE, 2300, 6, 200, 0, 0, 0, 140, 0);
    roll_bonus(big ? 64 : 40);
}
/* a key that jumped: a run up the ladder from the key's own note, a note a sixteenth, on the grid */
static void roll_jump(int32_t key, int big)
{
    uint32_t t0 = rm_ms_to(TICKS_STEP), st = rm_step_ms(), i, cnt = big ? 5u : 3u, j;
    int32_t d0 = imax(key, 0) * 15 / 26;
    if (t0 < 45u) t0 += st;                      /* (not on top of the key's own note) */
    for (i = 0; i < cnt; i++) {
        int32_t n = rm_ladder(d0 + 1 + (int32_t)i) + 12;
        sfx_fx(n, t0 + i * st, 520, i + 1u == cnt ? W_TRI : W_SINE, 2100, 2, 180, 0, (i & 1u) ? 36 : -36, 0, 110, 0);   /* (the top one a little brighter) */
        for (j = 0; j < 8u; j++)
            if (!rv.pend[j].on) { rv.pend[j].on = 1; rv.pend[j].ms = (int16_t)(t0 + i * st); rv.pend[j].note = (uint8_t)n; break; }
    }
}
/* down again after a real jump: the chord's own low note, falling onto it (vn: how hard, Q8 px/s) */
static void roll_land(int32_t vn)
{
    const rm_chord_t *c = rm_chord_now();
    if (rv.air_ms < 220) return;
    sfx_ex(c->bass + 24, 0, 170, W_SINE, (uint32_t)iclamp(1500 + vn / 30, 1500, 3000), 2, 20, 0, 0, 65150);
    sfx(c->bass + 12, 0, 650, W_SINE, 2900, 4, 40);
}
static void roll_thud(void) { sfx_ex(rm_chord_now()->bass + 12, 0, 160, W_SINE, 2200, 2, 30, 0, 0, 64700); }   /* a hard bump on the ground */
/* a spore: the next note up the ladder while they keep coming (a line of them is a scale), on the nearest thirty-second */
static void roll_spore(void)
{
    sfx_fx(rm_ladder(10 + rv.run % 8u) + 12, rm_ms_to(12), 520, W_SINE, 2500, 6, 150, 0, 0, 0, 60, 0);
    rv.run++; rv.run_t = 1300;
    roll_bonus(10);
}
static void roll_jelly(int32_t c0)               /* a jelly cap: one of the chord's notes, sliding up into place */
{
    const rm_chord_t *c = rm_chord_now();
    sfx_glide(-7, 110);
    sfx_fx(c->arp[c0 & 3] + 12, 0, 650, W_SINE, 2600, 4, 160, 0, 0, 0, 80, 0);
    sfx(c->bass + 24, 0, 280, W_TRI, 1300, 2, 40);
    roll_bonus(30);
}

/* from the roll, every frame it runs: where Rock is (the world's, Q8), how fast, whether he is on the ground, which
 * place of the piece that is, the camera's top, and how far off the platform he is (0 on it .. 256 well away): the
 * pictures' beat follows that, never faster than over a second and a half, so it comes in and never jumps in */
static void roll_tick(int32_t dt, int32_t x, int32_t y, int32_t vx, int g, int place, int32_t camy, int32_t away)
{
    int32_t sp = imin(256, iabs(vx) / 280), tgt, f;   /* 280 px/s and over: as fast as counts */
    rv.ticked = 1; rv.rx = x; rv.ry = y; rv.camy = camy;
    rv.vis_q += iclamp(Q8(away) - rv.vis_q, -dt * 256 * 256 / 1500, dt * 256 * 256 / 1500);
    rv.vis = rv.vis_q >> 8;
    if (iabs(vx) > Q8(20)) rv.dir = vx > 0 ? 1 : -1;
    rm_want = (uint8_t)place;
    rv.bonus = imax(0, rv.bonus - dt * 256 * 14 / 1000);
    tgt = imin(Q8(255), sp * 150 + rv.bonus);    /* speed alone: as far as the melody's outline and the kick; the rest is earned */
    if (tgt > rv.flow) rv.flow = imin(tgt, rv.flow + imax(1, (tgt - rv.flow) * imin(dt, 200) / 2000));   /* (it builds up over a
                                                    couple of seconds: a burst of speed brings the parts in one by one) */
    else rv.flow -= (rv.flow - tgt) * imin(dt, 200) / 2600;
    f = rv.flow >> 8;
    rm_flow = (uint8_t)f;
    if (f >= 140) rv.lead = 2;                   /* the melody: all of it, its outline, none (each let go a little lower than it came) */
    else if (rv.lead == 2 && f < 105) rv.lead = 1;
    if (rv.lead == 1 && f < 36) rv.lead = 0;
    if (rv.lead == 0 && f >= 55) rv.lead = 1;
    rm_lead = rv.lead;
    if (g) rv.air_ms = 0; else rv.air_ms += dt;
    rm_air = rv.air_ms >= 160;
    if (rv.run_t > 0) { rv.run_t -= dt; if (rv.run_t <= 0) rv.run = 0; }
}

/* every frame, whatever the scene (arcave.c): on = the roll is what is on the screen. The roll's own tune takes over
 * from the title's as Rock wakes; its echo is three sixteenths long (the eighths answer themselves between the beats)
 * and goes back as it was when the roll is left; a roll held still (the settings, a sheet over it) calms down */
static void rollmus_frame(int32_t dt, int on)
{
    uint32_t i;
    if (on && mus_mode == MUS_INTRO) {
        for (i = 0; i < sizeof rv; i++) ((uint8_t *)&rv)[i] = 0;
        rv.star_prev = -1; rv.dir = 1; rv.shown = 0xFF; rv.evr = rm_evw;
        rm_want = RM_START; rm_flow = 0; rm_air = 0; rm_lead = 0; rm_reset = 1;
        mus_mode = MUS_ROLL;
    }
    on = on && mus_mode == MUS_ROLL;
    if (on) { sy_echo_len = imin((int32_t)SY_DLN, (int32_t)(22050u * 45u / imax((int32_t)(tr_bpm_q8 >> 8), 40))); sy_echo_fb = 140; rv.bus = 1; }
    else if (rv.bus) { sy_echo_len = SY_DLN; sy_echo_fb = 115; rv.bus = 0; }
    rv.on = (uint8_t)on;
    rv.beat = imax(0, rv.beat - dt * 256 / 340);
    rv.wash = imax(0, rv.wash - dt * 256 / 700);
    rv.arp_note = 0;
    for (i = 0; i < 4u; i++) if (rv.sing[i].t > 0) rv.sing[i].t = (int16_t)imax(0, rv.sing[i].t - dt);
    for (i = 0; i < RM_NMOTE; i++)
        if (rv.mote[i].life > 0) {               /* (the sparks rise; the stars drift back slowly, as the far sky does) */
            rv.mote[i].life = (int16_t)(rv.mote[i].life - dt);
            if (rv.mote[i].kind) rv.mote[i].y -= Q8(46) * dt / 1000; else rv.mote[i].x -= Q8(7) * dt / 1000;
        }
    if (!on) { rv.evr = rm_evw; return; }
    if (!rv.ticked) {                            /* held still under something: down to the chord and its slow notes */
        rv.flow -= rv.flow * imin(dt, 200) / 1200; rv.bonus = 0; rv.lead = 0; rv.air_ms = 0;
        rm_flow = (uint8_t)(rv.flow >> 8); rm_lead = 0; rm_air = 0;
    }
    rv.ticked = 0;
    for (i = 0; i < 8u; i++)
        if (rv.pend[i].on) { rv.pend[i].ms = (int16_t)(rv.pend[i].ms - dt); if (rv.pend[i].ms <= 0) { rv.pend[i].on = 0; rm_spark(rv.pend[i].note); } }
    if (rm_evw - rv.evr > RM_EVN) rv.evr = rm_evw - RM_EVN;
    while (rv.evr != rm_evw) {                   /* what the tune played since the last frame */
        uint32_t e = rm_evq[rv.evr % RM_EVN], kind = e >> 8, note = e & 255u;
        rv.evr++;
        if (kind == RE_BASS) rv.beat = 256;
        else if (kind == RE_KICK) rv.beat = imax(rv.beat, 200);
        else if (kind == RE_ARP) rv.arp_note = (uint8_t)note;
        else if (kind == RE_CHORD) { rv.col = RM_CH[RM_PLACE[note < RM_NP ? note : RM_START].ch].col; rv.wash = 256; }
        else if (kind == RE_LEAD) {              /* a star: the melody writes itself across the sky from left to right, the
                                                    higher the note the higher, and begins again at the left */
            rm_mote(0, Q8(34 + rv.head * 16), Q8(iclamp(92 - ((int32_t)note - 67) * 4, 8, 104)), 2600, rv.col);
            rv.head = (uint8_t)((rv.head + 1u) % 12u);
        }
    }
#ifdef DINGHY_HOST
    if (rm_cur != rv.shown) { rv.shown = rm_cur; printf("%6u ms  roll music: place %d, flow %d, melody %d\n", fm1_ms, (int)rm_cur, (int)(rv.flow >> 8), (int)rv.lead); }
#endif
}

/* out of the course (arcave.c's to_cave): the echo as it was, before whatever comes next sets its own */
static void rollmus_leave(void) { if (rv.bus) { sy_echo_len = SY_DLN; sy_echo_fb = 115; rv.bus = 0; } rv.on = 0; }

/* the melody's stars, joined in the order they were played: drawn before the land, which hides what is behind it */
static void roll_draw_sky(void)
{
    int32_t i;
    if (!rv.on) return;
    for (i = 0; i < RM_NMOTE; i++) {
        int32_t x, y, a, tw;
        col_t c;
        if (rv.mote[i].life <= 0 || rv.mote[i].kind) continue;
        x = rv.mote[i].x >> 8; y = rv.mote[i].y >> 8;
        a = imin(256, imin((rv.mote[i].max - rv.mote[i].life) * 256 / 90, rv.mote[i].life * 256 / 1100));
        c = th->caps[rv.mote[i].c % 5];
        if (rv.mote[i].link >= 0 && rv.mote[rv.mote[i].link].life > 0) {
            int32_t j = rv.mote[i].link, x2 = rv.mote[j].x >> 8, y2 = rv.mote[j].y >> 8;
            galpha(a * 90 / 256); gline(x, y, x2, y2, c);
        }
        tw = rv.mote[i].max - rv.mote[i].life < 260 ? 2 : ((rv.mote[i].life / 180 + i) & 1);   /* large as it sounds, then twinkling */
        galpha(a * 70 / 256); gdisc(x, y, 4 + tw, c);
        galpha(a); ghl(x - 2 - tw, x + 3 + tw, y, c); grect(x, y - 2 - tw, 1, 5 + 2 * tw, c);
        grect(x - 1, y - 1, 3, 3, th->text);
    }
    galpha(256);
}
/* the sparks off Rock: a key's note, each note of a jump's run */
static void roll_draw_fx(int32_t camx, int32_t camy, int32_t z)
{
    int32_t i;
    if (!rv.on) return;
    for (i = 0; i < RM_NMOTE; i++) {
        int32_t x, y, q;
        if (rv.mote[i].life <= 0 || !rv.mote[i].kind) continue;
        x = ((rv.mote[i].x - camx) * z) >> 16; y = ((rv.mote[i].y - camy) * z) >> 16;
        q = 256 - rv.mote[i].life * 256 / imax(1, rv.mote[i].max);
        galpha(256 - q); gcirc(x, y, 3 + q * 9 / 256, th->caps[rv.mote[i].c % 5]);
        galpha(imin(256, (256 - q) * 2)); grect(x - 1, y - 1, 3, 3, th->text);
    }
    galpha(256);
}
