/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's music room: what its four instruments and its mixer share. The room is down a tunnel from the high
 * right ledge of the cave, behind a door that is only there once the jukebox is mended (ac_hall.h).
 *
 *   ROOTS          pods hanging on roots: a drum kit that rattles                  ac_roots.h
 *   THE GEODE      a shell that turns, stones loose inside it                      ac_geode.h
 *   MUSHROOM RACK  pods bouncing along a row of caps: an acid bass                 ac_rack.h
 *   CRYSTAL SEAM   crystals that grow, break and rain shards: lead and glitter     ac_seam.h
 *   THE POOL       the mixer, in the middle of the room                            ac_hall.h
 *
 * Every instrument is a small physical world of 16 parts, one to each key of the bottom row, stepped a hundred times
 * a second in whole numbers. A sound comes only when something in that world meets something else. They all keep
 * going while Rock rolls about the room. One set of controls serves all four:
 *
 *   KNOB 1-4     the world's four laws (each instrument names its own)
 *   SELECT, PRESETS, ALGORITHM   a fifth law; what is loose in the world; ready-made layouts (0: your own)
 *   bottom row   strike a part by hand          top row   the instrument's own
 *   PLAY         start and stop it              REC       record what sounds onto the loop (held: wipe this one's)
 *   SEQ          its steady device (the plectrum, the beetle, the reset, the drip); held, KNOB 1-4 set it
 *   ARP          the layout tends itself; held, KNOB 1-4: how, how often, how much, whether it comes home
 *   LFO          held, KNOB 1-4: that law swings by itself in time; SELECT: how slowly. A tap: all swings on or off
 *   ENV          held, KNOB 1-4: ATTACK, LENGTH, BEND, BRIGHT of every hit
 *   FX           held, KNOB 1-4: ECHO, REVERB, DRIVE, LEVEL (its strip of the mixer). A tap: mute
 *   SCL          the scale; held, KNOB 1: the root         OCT- OCT+   the octave
 *   EDIT         stops the world and opens it: hold bottom keys (they stay in hand), KNOB 1-4: WHAT, SIZE, NOTE,
 *                QUALITY; top row: a kind at one press, then turn, turn, mirror, clear, scatter;
 *                ARP: the scatter menu (random layouts under conditions); PLAY: hear it; EDIT: close
 *   SAVE         keeps the whole room             HOME   back to the room */

static const uint8_t R_LOW[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t R_TOP[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static int32_t key_row_i(int32_t key)            /* a key's place along its own row */
{
    int32_t i, n = 0;
    for (i = 0; i < key; i++) if (KEY_IS_SHARP[i] == KEY_IS_SHARP[key]) n++;
    return n;
}

enum { RI_ROOTS, RI_GEODE, RI_RACK, RI_SEAM, RI_N, RI_MIX = RI_N, RI_WALK = 255 };
#define RP_N 16
/* a part (what: 0 nothing; size, qual 0..7; note: a step of the scale, 0..20) and an instrument as the room keeps it
 * (the layout and the player's own, the four laws, their swings 0..8, the hit's shape, SEQ's and ARP's four, each
 * part's flash, the parts in hand, changes since the layout last came home) are mi.h's: a loaded instrument (the
 * site's MACHINES page; ac_slots.h, ac_mirun.h) is handed the same thing */
typedef mi_part_t rpart_t;
typedef mi_inst_t rinst_t;
_Static_assert(RP_N == MI_NPART && RI_N == MI_NSLOT, "mi.h: the parts, the places");
static rinst_t ri[RI_N];
static struct { uint8_t lvl, pan, echo, rev, drv, tone, mute, solo; } rmx[RI_N];
/* the room's own settings (the settings' MUSIC ROOM line: ac_hall.h), kept with the room.
 *   rock      the sounds Rock makes rolling about the room: 0 on, 1 soft, 2 off
 *   metro     a click on every beat: 0 off, 1 on, 2 only while recording
 *   countin   REC waits for the next bar line, counted in by clicks
 *   silent    coming in, every instrument is stopped (the pool's PLAY starts them)
 *   nolesson  an instrument opened for the first time does not start its lesson
 *   vol       the room's own volume, 0..8
 *   alone     playing together is off: this FM-1 does not say it is here (ac_roomnet.h)
 *   mine      playing together, only the instrument this player is on is heard here (one FM-1, one instrument) */
static struct { uint8_t rock, metro, countin, silent, nolesson, vol, alone, mine; } rset;
static int32_t rkv(int32_t v) { return rset.rock == 1 ? v / 3 : v; }   /* one of Rock's sounds, as loud as the setting has it */
/* playing together (ac_roomnet.h) */
static int rn_auth(int inst);                    /* this FM-1 is the one that tends instrument inst, and records it (RI_MIX: the pool's dives) */
static int rn_deaf(int inst);
static int rn_loop_room(int inst);               /* playing together, each instrument has a quarter of the loop: there is room in its own */                    /* hearing only one's own instrument, and inst is not it */

/* a lesson (ac_rtutor.h): steps, each a caption in every language and what the demonstration does while it is up */
typedef struct { uint16_t ms; const char *tx[L_N]; void (*act)(int32_t t, int32_t dt); } rt_step_t;
enum { RTI_NONE, RTI_K1, RTI_K2, RTI_K3, RTI_K4, RTI_SELECT, RTI_PRESETS, RTI_ALGO, RTI_LOW, RTI_TOP, RTI_BTN = 16 };   /* (RTI_BTN + BTN_...) */
static struct {
    uint8_t on, who, step, icon, then;
    uint8_t vhold;                               /* a button the lesson holds down, + 1: the strip shows its page */
    uint8_t list, row, lo, hi;                   /* the list of topics is up (of who); its row; steps lo..hi-1 are what plays */
    const rt_step_t *steps;                      /* the steps being played */
    int32_t t, label_t; const char *label;
} rt;
#define RHELD(b) (BHELD(b) || (rt.on && rt.vhold == (b) + 1))
#define RT_AT(ms_) (t >= (ms_) && t - dt < (ms_))  /* in a lesson's act: this frame is the one where ms_ goes by */
#define RT_FIRST (t - dt <= 0)
typedef struct {
    const char *name;
    const char *kn[4]; uint8_t kmax[4], kdef[4], kmid[4];   /* (kmid: its bar grows from the middle) */
    const char *seln; uint8_t selmax, seldef;
    const char *seqn, *sqn[4]; uint8_t sqmax[4], sqdef[4];
    const char *const *kinds; uint8_t nkind;     /* kinds[0]: nothing there */
    const uint8_t *ksnd;                         /* the sound of each kind (SK_...) */
    const char *const *presets; uint8_t npreset;
    const char *const *algos; uint8_t nalgo;     /* algos[0]: the player's own layout */
    void (*init)(void);                          /* the world set up from the layout */
    void (*step)(void);                          /* 10 ms of it */
    void (*draw)(int edit);
    void (*key)(int i, int vel);                 /* a bottom key (playing) */
    void (*top)(int i);                          /* a top key (playing) */
    void (*layout)(int n);                       /* a ready-made layout into p[], n >= 1 */
    void (*play)(int on);                        /* PLAY pressed */
    const rt_step_t *lesson; uint8_t nlesson;
} rdesc_t;
static const rdesc_t *RD[RI_N];

#define RSC_N 8
static const char *const RSC_NAME[RSC_N] = {"MAJOR PENTA", "MINOR PENTA", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "HARMONIC", "WHOLE TONE"};
static const uint8_t RSC[RSC_N][8] = {{5, 0, 2, 4, 7, 9}, {5, 0, 3, 5, 7, 10}, {7, 0, 2, 4, 5, 7, 9, 11}, {7, 0, 2, 3, 5, 7, 8, 10},
                                      {7, 0, 2, 3, 5, 7, 9, 10}, {7, 0, 1, 3, 5, 7, 8, 10}, {7, 0, 2, 3, 5, 7, 8, 11}, {6, 0, 2, 4, 6, 8, 10}};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define RL_N 256
typedef struct { uint16_t t; uint8_t ik, deg, vel, sq; } rev_t;   /* a recorded hit: when in the loop; instrument << 4 | sound; ... */

/* the scatter menu's conditions */
enum { SCR_FILL, SCR_CLUMP, SCR_SIZES, SCR_NOTES, SCR_QUALS, SCR_REPEAT, SCR_WHERE, SCR_N };
static const char *const SCR_NAME[SCR_N] = {"FILL", "CLUMPS", "SIZES", "NOTES", "QUALITY", "REPEAT", "WHERE"};
static const uint8_t SCR_MAX[SCR_N] = {8, 4, 3, 4, 2, 3, 1};
static const char *const SCR_VAL[SCR_N][5] = {{0}, {0}, {"KEEP", "ALL ALIKE", "VARIED", "WILD"}, {"KEEP", "CLIMB", "ANY", "CHORD", "ONE"},
                                              {"KEEP", "VARIED", "WILD"}, {"NONE", "MIRROR", "HALVES", "QUARTERS"}, {"ALL PARTS", "IN HAND"}};

static struct {
    uint8_t on, focus, edit, menu, mrow, scale, root, bars, rec, quant, etime, efb, bpm, loop_off, tut, dirty;   /* tut: a lesson is on, of instrument tut - 1: only it is heard */
    uint8_t scv[SCR_N], sc_mask;                 /* the scatter's conditions; which kinds it may use (bit = kind) */
    uint8_t held_used;                           /* a knob was turned while a button was held: its tap does nothing */
    int32_t acc, sub;                            /* ms not yet stepped; ms into this frame's stepping (a sound's delay) */
    uint32_t tq8, ptq8;                          /* the clock at this step and the one before (ticks Q8) */
    uint32_t t;                                  /* ms in the room */
    uint32_t arm;                                /* REC counted in: the tick at which it starts (0: not waiting) */
    int32_t note_t; const char *note_a; char note_b[28];   /* a line over the strip: what was just changed */
    rev_t loop[RL_N]; uint16_t loop_n;
    uint8_t last_acid[RI_N];                     /* each instrument's last bass note (a slide starts from it) */
    int32_t pulse[RI_N];                         /* each one's level just now, for the lights and the pool */
} rm;

/* the sound kinds */
enum { SK_CLICK, SK_CRYSTAL, SK_CRACK, SK_ACID, SK_SLIDE, SK_MOSS, SK_OOZE, SK_POD, SK_NUT, SK_BELL, SK_GOURD, SK_HUSK, SK_SALT, SK_OPAL, SK_SHARD, SK_N };
#define SK_NONE 255                              /* a kind that makes no sound at all */
/* a place whose instrument is a loaded one (ac_mirun.h): its sounds are its own, numbered by itself */
static int mi_on(int inst);
static void mi_voice(int inst, int voice, int32_t deg, int32_t vel, int32_t size, int32_t qual);
static int ri_nsnd(int inst) { return mi_on(inst) ? 16 : SK_N; }   /* how many sounds the instrument in a place has */

static col_t ri_col(int i) { return i == RI_ROOTS ? th->caps[2] : i == RI_GEODE ? th->caps[3] : i == RI_RACK ? th->caps[0] : i == RI_SEAM ? th->caps[4] : th->pad; }
static int32_t rm_note(int inst, int32_t deg)    /* a step of the room's scale -> a note, in the instrument's octave */
{
    const uint8_t *s = RSC[rm.scale % RSC_N];
    int32_t n = s[0];
    deg = iclamp(deg, 0, 40);
    return 36 + rm.root + 12 * ri[inst].oct + 12 * (deg / n) + s[1 + deg % n];
}
static void rm_note_name(char *b, int32_t note)
{
    const char *s = NOTE_NAME[note % 12];
    int n = 0;
    while (*s) b[n++] = *s++;
    b[n++] = (char)('0' + iclamp(note / 12 - 1, 0, 9)); b[n] = 0;
}
static void rm_say(const char *a, const char *b)  /* what was just changed, over the strip for a moment */
{
    int n = 0;
    rm.note_a = a; rm.note_t = 1500;
    while (b && b[n] && n < 26) { rm.note_b[n] = b[n]; n++; }
    rm.note_b[n] = 0;
}
static void rm_say_n(const char *a, int32_t v)
{
    char b[8]; int n = 0;
    if (v < 0) { b[n++] = '-'; v = -v; }
    if (v >= 100) b[n++] = (char)('0' + v / 100 % 10);
    if (v >= 10) b[n++] = (char)('0' + v / 10 % 10);
    b[n++] = (char)('0' + v % 10); b[n] = 0;
    rm_say(a, b);
}

/* ---------------------------------------------------------------- the sounds
 * One voice of the room's: the instrument's strip of the mixer and its ENV go onto every note of it. */
static struct { int32_t vol, pan, send, rs, drv, att, len, bend, bright, flt; uint32_t delay; } rc;
static void rn_loop_tx(const rev_t *e);          /* (ac_roomnet.h: a hit recorded here, told to the others) */
static void rn_key_tx(int inst, int top, int i); /* (a key of an instrument, played by hand) */
static void rn_wipe_tx(int who);                 /* (a part of the loop wiped; RI_N: all of it) */
static int32_t rm_lvl_eff(int inst);             /* (ac_hall.h: the pool moves the levels and the pans) */
static int32_t rm_pan_eff(int inst);
static void rflt(int mode, int32_t cut, int32_t res, int32_t fenv, int32_t ms) { sfx_filter(mode, cut + rc.bright, res, fenv, ms); rc.flt = 1; }
static void rvo(int32_t note, uint32_t delay, int32_t dur, int wave, int32_t vol, int32_t att, int32_t det, int32_t pan)
{
    static const uint16_t BEND[9] = {64700, 65050, 65300, 65450, 0, 0, 0, 0, 0};   /* BEND under the middle: it falls; over: it rises into its note */
    if (rc.bend > 4 && !sy_x.gl) sfx_glide(-(rc.bend - 4) * 3, 50 + rc.bend * 10);
    if (!rc.flt && rc.bright < 0) sfx_filter(1, 112 + rc.bright * 2, 0, 0, 100);   /* darker than it is: a low-pass */
    rc.flt = 0;
    vol = (vol * rc.vol) >> 8;
    if (vol < 30) { sy_x.fmode = 0; sy_x.gl = 0; return; }
    sfx_fx(note, rc.delay + delay, (uint32_t)imax(20, dur * rc.len >> 8), (uint32_t)wave, (uint32_t)imin(vol, 30000), (uint32_t)(att + rc.att),
           (uint32_t)rc.send, det, iclamp(pan + rc.pan, -84, 84), BEND[rc.bend], (uint32_t)rc.rs, (uint32_t)rc.drv);
}
static void rm_std(int inst, int kind, int32_t deg, int32_t vel, int32_t size, int32_t qual);
static void rm_voice(int inst, int kind, int32_t deg, int32_t vel, int32_t size, int32_t qual, uint32_t delay)
{
    static const uint16_t ATT[9] = {0, 3, 8, 16, 30, 60, 110, 190, 320}, LEN[9] = {70, 110, 160, 210, 256, 360, 512, 760, 1100};
    const rinst_t *r = &ri[inst];
    int32_t k, any = 0;
    for (k = 0; k < RI_N; k++) any |= rmx[k].solo;
    if (rm.tut && rm.tut <= RI_N ? inst != rm.tut - 1 : (rmx[inst].mute || (any && !rmx[inst].solo))) return;   /* (the pool's lesson: all four) */
    if (!rm.tut && rn_deaf(inst)) return;
    vel = iclamp(vel, 1, 255); size = iclamp(size, 0, 7); qual = iclamp(qual, 0, 7);
    rc.vol = (96 + vel * 160 / 255) * rm_lvl_eff(inst) / 12 * rset.vol / 8;
    rc.pan = rm_pan_eff(inst); rc.send = imin(255, rmx[inst].echo * 30); rc.rs = imin(255, rmx[inst].rev * 30); rc.drv = rmx[inst].drv;
    rc.att = ATT[r->env[0]]; rc.len = LEN[r->env[1]]; rc.bend = r->env[2]; rc.bright = (r->env[3] - 4) * 6 + (rmx[inst].tone - 8) * 5;
    rc.delay = delay; rc.flt = 0;
    rm.pulse[inst] = imax(rm.pulse[inst], vel);
    if (mi_on(inst)) mi_voice(inst, kind, deg, vel, size, qual); else rm_std(inst, kind, deg, vel, size, qual);
    sy_x.fmode = 0; sy_x.gl = 0;
}
/* one of the firmware's own sounds (SK_...), on the voice rm_voice has set up */
static void rm_std(int inst, int kind, int32_t deg, int32_t vel, int32_t size, int32_t qual)
{
    static const int8_t CH[8][4] = {{0, 4, -1, -1}, {0, 2, 4, -1}, {0, 3, 4, -1}, {0, 2, 4, 6}, {0, 2, 4, 8}, {0, 4, 9, -1}, {0, 1, 4, -1}, {0, 3, 6, 9}};
    int32_t n = rm_note(inst, deg), k, d;
    switch (kind) {
    case SK_CLICK:
        rflt(2, 96 + qual * 3, 110, 0, 40); rvo(60, 0, 22 + size * 6, W_HAT, 1500, 0, 0, 0);
        rvo(n + 48, 0, 14, W_SQUARE, 500, 0, 0, 0);
        break;
    case SK_CRYSTAL:
        d = n + 36 - 12 * (size / 3);
        rvo(d, 0, 500 + size * 220, W_SINE, 3000, 1, 0, 0);
        rvo(d + 12, 0, 260 + size * 90, W_SINE, 900, 1, 0, 20);
        if (qual >= 3) rvo(d + 19, 0, 200 + size * 60, qual >= 6 ? W_TRI : W_SINE, 300 + qual * 110, 1, 7, -20);
        break;
    case SK_CRACK:
        d = n + 36 - 12 * (size / 3);
        rflt(2, 104, 60, 0, 60); rvo(60, 0, 90, W_HAT, 2600, 0, 0, 0);
        rvo(d + 12, 0, 300, W_SINE, 1800, 0, 0, -30); rvo(d + 19, 35, 300, W_SINE, 1600, 0, 0, 30); rvo(d + 24, 70, 420, W_SINE, 1500, 0, 0, 0);
        break;
    case SK_ACID: case SK_SLIDE:
        d = n - (size >= 5 ? 12 : 0);
        rflt(1, 46 + vel / 9, 110 + qual * 17, 12 + vel / 8 + qual * 3, kind == SK_SLIDE ? 300 : 120 + size * 28);
        if (kind == SK_SLIDE && rm.last_acid[inst]) sfx_glide(rm.last_acid[inst] - d, 70);
        rvo(d, 0, kind == SK_SLIDE ? 360 + size * 60 : 200 + size * 55, qual & 1 ? W_SQUARE : W_SAW, qual & 1 ? 5200 : 3800, 1, 0, 0);
        rm.last_acid[inst] = (uint8_t)d;
        break;
    case SK_MOSS:                                /* a noise engine: noise through a filter that the note tunes */
        rflt(qual < 3 ? 1 : 2, n + (qual < 3 ? 24 : 36), 30 + qual * 28, qual >= 5 ? 10 : 0, 80 + size * 30);
        rvo(60, 0, 40 + size * size * 14, W_HAT, qual < 3 ? 5200 : 3400 + qual * 500, size * 3, 0, 0);
        if (qual >= 6) { rflt(2, n + 48, 200, 0, 60); rvo(n + 60, 0, 30 + size * size * 8, W_NOISE, 1500, size * 2, 0, 30); }
        break;
    case SK_OOZE:                                /* a chord out of the scale, on the part's note */
        for (k = 0; k < 4; k++) {
            if (CH[qual][k] < 0) continue;
            d = rm_note(inst, deg + CH[qual][k]) + 12;
            rvo(d, (uint32_t)k * 9u, 900 + size * 250, qual & 1 ? W_TRI : W_SINE, 1700, 24 + size * 10, k * 3 - 4, (k - 1) * 36);
        }
        if (size >= 4) rvo(n, 0, 900 + size * 200, W_SINE, 1800, 30, 0, 0);
        break;
    case SK_POD:
        rflt(2, 98 + qual * 3, 90, 0, 40); rvo(60, 0, 30 + size * 14, W_HAT, 2000, 0, 0, 0);
        rflt(2, 101 + qual * 3, 90, 0, 40); rvo(60, 18, 24 + size * 10, W_HAT, 1100, 0, 0, 0);
        break;
    case SK_NUT:
        rvo(n + 24, 0, 60 + size * 22, qual >= 5 ? W_TRI : W_SINE, 3400, 0, 0, 0);
        rvo(n + 43, 0, 26, qual >= 3 ? W_SQUARE : W_TRI, 500 + qual * 120, 0, 0, 0);
        break;
    case SK_BELL: {
        static const uint8_t PT[8] = {12, 14, 16, 17, 19, 21, 23, 26};
        rvo(n + 24, 0, 800 + size * 220, W_SINE, 2200, 0, 0, 0);
        rvo(n + 24 + PT[qual], 0, 400 + size * 120, W_SINE, 1000, 0, 9, 25);
        rvo(n + 55, 0, 90, W_SINE, 600, 0, 0, -25);
        break; }
    case SK_GOURD:
        sfx_glide(12 + qual * 2, 36 + size * 8);
        rvo(n - 12, 0, 170 + size * 60, W_SINE, 6400, 0, 0, 0);
        rflt(1, 80, 0, 0, 30); rvo(60, 0, 16, W_HAT, 900, 0, 0, 0);
        break;
    case SK_HUSK:
        rflt(2, 84 + qual * 2, 30, 0, 60); rvo(60, 0, 80 + size * 26, W_HAT, 3000, 0, 0, 0);
        sfx_glide(5, 40); rvo(n + 12, 0, 80, W_TRI, 1900, 0, 0, 0);
        break;
    case SK_SALT:
        rvo(n + 36, 0, 40 + size * 12, W_SQUARE, 1300, 0, 0, 0);
        rflt(2, 108, 80, 0, 30); rvo(60, 0, 18, W_HAT, 1300, 0, 0, 0);
        break;
    case SK_OPAL:
        for (k = -1; k <= 1; k++) rvo(n + 24, 0, 700 + size * 200, W_SINE, 1500, 12, k * (6 + qual), k * 50);
        if (qual >= 4) rvo(n + 31, 20, 500 + size * 150, W_SINE, 800, 12, 0, 0);
        break;
    default:                                     /* SK_SHARD */
        rvo(n + 36 + (qual & 1 ? 12 : 0), 0, 150 + size * 30, W_SINE, 1100, 0, 0, (int32_t)(ihash((uint32_t)(deg * 7 + vel)) % 120u) - 60);
        break;
    }
}
/* a hit in an instrument's world: heard now (at this step of the frame), and recorded if REC is on */
static void rm_hit(int inst, int kind, int32_t deg, int32_t vel, int32_t size, int32_t qual)
{
    rm_voice(inst, kind, deg, vel, size, qual, (uint32_t)rm.sub);
    if (rm.rec && !rm.tut && rm.loop_n < RL_N && rn_auth(inst) && rn_loop_room(inst)) {   /* (together: whoever tends it records it, and tells the rest) */
        uint32_t len = (uint32_t)rm.bars * TICKS_BAR, t = (rm.tq8 >> 8) % len;
        rev_t *e = &rm.loop[rm.loop_n++];
        if (rm.quant) { uint32_t q = rm.quant == 1 ? TICKS_STEP : TICKS_STEP * 2u; t = ((t + q / 2u) / q * q) % len; }
        e->t = (uint16_t)t; e->ik = (uint8_t)(inst << 4 | kind); e->deg = (uint8_t)iclamp(deg, 0, 40); e->vel = (uint8_t)iclamp(vel, 1, 255);
        e->sq = (uint8_t)(iclamp(size, 0, 7) << 4 | iclamp(qual, 0, 7));
        {   /* the same sound at the same place twice is one hit of the loop (and so it is on every FM-1 playing together) */
            int k;
            for (k = 0; k + 1 < rm.loop_n; k++) if (rm.loop[k].t == e->t && rm.loop[k].ik == e->ik && rm.loop[k].deg == e->deg) { rm.loop_n--; return; }
        }
        rn_loop_tx(e);
    }
}
/* REC tapped: onto the loop, or off it. With COUNT-IN on it starts on a bar line, at least two beats on, clicked in */
static void rm_rec_tap(void)
{
    if (rm.arm) { rm.arm = 0; rm_say("REC", "OFF"); snd_ui(72); return; }
    if (!rm.rec && rset.countin) {
        uint32_t tk = rm.tq8 >> 8, bar = (tk / TICKS_BAR + 1u) * TICKS_BAR;
        if (bar - tk < TICKS_BEAT * 2u) bar += TICKS_BAR;
        rm.arm = bar; rm_say("REC", "AFTER THE COUNT"); snd_ui(84); return;
    }
    rm.rec = !rm.rec; rm_say("REC", rm.rec ? "ON THE LOOP" : "OFF"); snd_ui(rm.rec ? 84 : 72);
}
/* HOME in the room: a tap acts as it is let go; held, the lessons by topic come up (ac_rtutor.h) */
#define RHOME_MS 600
static struct { int32_t t; uint8_t down; } rhome;
static int rm_home(int32_t dt)                   /* 0: nothing; 1: a tap, just let go; 2: held long enough */
{
    if (BDOWN(BTN_HOME)) { rhome.down = 1; rhome.t = 0; }
    if (!rhome.down) return 0;
    if (BHELD(BTN_HOME)) { rhome.t += dt; if (rhome.t >= RHOME_MS) { rhome.down = 0; return 2; } return 0; }
    rhome.down = 0;
    return 1;
}
static void rtopics_open(int who, int then);
/* a part of an instrument struck: its kind's sound, its flash */
static void ri_strike(int inst, int s, int32_t vel)
{
    const rpart_t *p = &ri[inst].p[s];
    ri[inst].fl[s] = 255;
    if (RD[inst]->ksnd[p->what] == SK_NONE) return;   /* (nothing there, and nothing to hear) */
    rm_hit(inst, RD[inst]->ksnd[p->what], p->note, vel, p->size, p->qual);
}
/* a law as the world feels it: the knob plus its swing, in sixteenths of a click */
static int32_t ri_k(int inst, int i)
{
    static const uint16_t PER[6] = {96, 192, 384, 768, 1536, 3072};
    const rinst_t *r = &ri[inst];
    int32_t v = r->k[i] * 16, mx = RD[inst]->kmax[i] * 16;
    if (r->lfo_on && r->lfo[i]) {
        uint32_t per = PER[r->lfo_rate % 6u];
        v += (isin((int32_t)(((rm.tq8 >> 4) % (per * 16u)) * 4096u / per)) * (int32_t)r->lfo[i] * mx / 8) >> 15;
    }
    return iclamp(v, 0, mx);
}
/* a boundary of the clock went by in this step: every `per` ticks, `off` ticks late. *idx gets which one */
static int rm_cross(uint32_t per, uint32_t off, uint32_t *idx)
{
    uint32_t a = ((rm.ptq8 >> 8) + per * 64u - off) / per, b = ((rm.tq8 >> 8) + per * 64u - off) / per;
    if (idx) *idx = b;
    return a != b;
}

/* ---------------------------------------------------------------- layouts: scatter, and the layout tending itself */
static void ri_touch(int inst)                   /* the layout was changed by hand: it is the player's own now */
{
    rinst_t *r = &ri[inst];
    int s;
    for (s = 0; s < RP_N; s++) r->mine[s] = r->p[s];
    r->algo = 0; rm.dirty = 1;
    RD[inst]->init();
}
static void ri_scatter(int inst)
{
    rinst_t *r = &ri[inst];
    const rdesc_t *d = RD[inst];
    int s, n = 0, kinds[8], rep = rm.scv[SCR_REPEAT], span = rep == 2 ? 8 : rep == 3 ? 4 : 16, run_kind = 0, run_left = 0;
    for (s = 1; s < d->nkind; s++) if (rm.sc_mask >> s & 1u) kinds[n++] = s;
    for (s = 0; s < RP_N; s++) {
        rpart_t q = r->p[s];
        int src = rep == 1 && s >= 8 ? 15 - s : s % span;
        if (rm.scv[SCR_WHERE] && !(r->hand >> s & 1u)) continue;
        if (src != s) { r->p[s] = r->p[src]; continue; }
        if (run_left > 0) { q.what = (uint8_t)run_kind; run_left--; }
        else {
            q.what = n && (int32_t)(ac_rnd() % 8u) < rm.scv[SCR_FILL] ? (uint8_t)kinds[ac_rnd() % (uint32_t)n] : 0u;
            run_kind = q.what; run_left = rm.scv[SCR_CLUMP] ? (int)(ac_rnd() % (uint32_t)(rm.scv[SCR_CLUMP] + 1)) : 0;
        }
        if (rm.scv[SCR_SIZES] == 1) q.size = r->p[0].size;
        else if (rm.scv[SCR_SIZES] == 2) q.size = (uint8_t)(2u + ac_rnd() % 3u);
        else if (rm.scv[SCR_SIZES] == 3) q.size = (uint8_t)(ac_rnd() % 8u);
        if (rm.scv[SCR_NOTES] == 1) q.note = (uint8_t)(s * 14 / 15);
        else if (rm.scv[SCR_NOTES] == 2) q.note = (uint8_t)(ac_rnd() % 12u);
        else if (rm.scv[SCR_NOTES] == 3) q.note = (uint8_t)((ac_rnd() % 5u) * 2u);
        else if (rm.scv[SCR_NOTES] == 4) q.note = r->p[0].note;
        if (rm.scv[SCR_QUALS] == 1) q.qual = (uint8_t)(2u + ac_rnd() % 3u);
        else if (rm.scv[SCR_QUALS] == 2) q.qual = (uint8_t)(ac_rnd() % 8u);
        r->p[s] = q;
    }
    ri_touch(inst);
    sfx(84, 0, 60, W_SQUARE, 900, 0, 60); sfx(91, 50, 60, W_SQUARE, 900, 0, 60); sfx(96, 100, 90, W_SQUARE, 900, 0, 60);
}
static const char *const ARP_NAME[4] = {"TURN", "CREEP", "SWAP", "BLOOM"};
static void ri_tend(int inst)                    /* ARP: once every so many bars the layout changes a little by itself */
{
    rinst_t *r = &ri[inst];
    int s, k, amt = 1 + r->ar[2];
    if (r->ar[3] && r->arp_n >= 8) {             /* home again */
        for (s = 0; s < RP_N; s++) r->p[s] = r->mine[s];
        r->arp_n = 0; RD[inst]->init(); return;
    }
    r->arp_n++;
    for (k = 0; k < (r->ar[0] == 0 ? 1 : amt); k++) {
        int a = (int)(ac_rnd() % RP_N), b = (a + 1 + (int)(ac_rnd() % 3u)) % RP_N;
        rpart_t t;
        switch (r->ar[0]) {
        case 0: t = r->p[RP_N - 1]; for (s = RP_N - 1; s > 0; s--) r->p[s] = r->p[s - 1]; r->p[0] = t; break;   /* the whole pattern one part round */
        case 1:                                  /* something creeps onto the bare part beside it */
            for (s = 0; s < RP_N && !(r->p[(a + s) % RP_N].what && !r->p[(a + s + 1) % RP_N].what); s++) ;
            if (s < RP_N) { r->p[(a + s + 1) % RP_N] = r->p[(a + s) % RP_N]; if (ac_rnd() & 1u) r->p[(a + s) % RP_N].what = 0; }
            break;
        case 2: t = r->p[a]; r->p[a] = r->p[b]; r->p[b] = t; break;
        default: {                               /* one part comes up new, of a kind already here */
            uint8_t w = r->p[b].what ? r->p[b].what : (uint8_t)(1u + ac_rnd() % (uint32_t)(RD[inst]->nkind - 1));
            r->p[a].what = r->p[a].what ? 0u : w; r->p[a].note = (uint8_t)(ac_rnd() % 10u); r->p[a].size = (uint8_t)(1u + ac_rnd() % 5u);
            break; }
        }
    }
    RD[inst]->init();
}
static void ri_load_algo(int inst, int n)
{
    rinst_t *r = &ri[inst];
    int s;
    if (n == 0) for (s = 0; s < RP_N; s++) r->p[s] = r->mine[s];
    else RD[inst]->layout(n);
    r->algo = (uint8_t)n; r->arp_n = 0;
    RD[inst]->init();
}
static void ri_defaults(int inst)
{
    rinst_t *r = &ri[inst];
    const rdesc_t *d = RD[inst];
    int i;
    for (i = 0; i < (int)sizeof *r; i++) ((uint8_t *)r)[i] = 0;
    for (i = 0; i < 4; i++) { r->k[i] = d->kdef[i]; r->sq[i] = d->sqdef[i]; r->env[i] = 4; }
    r->ar[0] = 0; r->ar[1] = 1; r->ar[2] = 0; r->ar[3] = 1;
    r->sel = d->seldef; r->lfo_rate = 2; r->lfo_on = 1; r->run = 1; r->hand = 1;
    d->layout(1);
    for (i = 0; i < RP_N; i++) r->mine[i] = r->p[i];
    r->algo = 0;
    rmx[inst].lvl = 12; rmx[inst].pan = 8; rmx[inst].echo = inst == RI_RACK ? 1 : 3; rmx[inst].rev = inst == RI_SEAM ? 4 : 2; rmx[inst].drv = inst == RI_RACK ? 2 : 0;
    rmx[inst].tone = 8; rmx[inst].mute = rmx[inst].solo = 0;
    d->init();
}

/* ---------------------------------------------------------------- the screen round an instrument */
#define RSTRIP_Y 200
static void rm_bar(int32_t x, int32_t y, int32_t w, int32_t v, int32_t mx, int mid, col_t c)
{
    gbox(x, y, w, 5, th->mute);
    if (mx <= 0) return;
    if (mid) { int32_t h = (w - 2) / 2, d = (v - mx / 2) * h / (mx / 2); if (d >= 0) grect(x + 1 + h, y + 1, imax(1, d), 3, c); else grect(x + 1 + h + d, y + 1, -d, 3, c); }
    else grect(x + 1, y + 1, (w - 2) * v / mx, 3, c);
}
/* the four knobs along the foot of the screen: their names, a bar each, and a word for what the bar says */
static void rm_knobs(const char *const n[4], const int32_t v[4], const int32_t mx[4], const uint8_t mid[4], const char *const word[4], col_t c, int turned)
{
    int i;
    grect(0, RSTRIP_Y + 10, AC_W, AC_H - RSTRIP_Y - 10, th->bg);
    for (i = 0; i < 4; i++) {
        int32_t x = i * 60;
        if (!n[i]) continue;
        if (turned >> i & 1) grect(x + 1, RSTRIP_Y + 11, 58, 28, th->panel);
        gfit(x + 30, RSTRIP_Y + 14, 56, n[i], turned >> i & 1 ? th->text : c, 1);
        rm_bar(x + 6, RSTRIP_Y + 22, 48, v[i], mx[i], mid ? mid[i] : 0, turned >> i & 1 ? th->text : c);
        if (word && word[i]) gfit(x + 30, RSTRIP_Y + 31, 56, word[i], th->mute, 1);
    }
}
static void rm_head(const char *name, const char *tag, col_t c)
{
    uint32_t tick = rm.tq8 >> 8, beat = tick / TICKS_BEAT % 4u, k;
    grect(0, 0, AC_W, 12, th->bg);
    gtext(4, 3, name, c, 1);
    if (tag) gtext(8 + gtextw(name, 1), 3, tag, th->text, 1);
    for (k = 0; k < 4u; k++) { if (k == beat) grect(190 + (int32_t)k * 7, 3, 5, 5, k ? th->text : c); else gbox(190 + (int32_t)k * 7, 3, 5, 5, th->mute); }
    if (rm.arm) {                                /* counted in: the beats still to go */
        char nb[2] = {(char)('0' + iclamp((int32_t)((rm.arm - tick + TICKS_BEAT - 1u) / TICKS_BEAT), 1, 9)), 0};
        gcirc(228, 5, 4, AC_RGB(240, 60, 60)); gtextr(222, 3, nb, AC_RGB(240, 60, 60), 1);
    } else if (rm.rec) { if ((rm.t / 300u) & 1u) gdisc(228, 5, 3, AC_RGB(240, 60, 60)); gcirc(228, 5, 4, AC_RGB(240, 60, 60)); }
    else if (rm.loop_n) gcirc(228, 5, 3, th->mute);
    ghl(0, AC_W, 12, th->mute);
}
static uint8_t rm_turned;                        /* which of KNOB 1-4 moved lately (bits), for the strip */
static int32_t rm_turned_t;

/* an instrument's strip: what the knobs are just now depends on what is held */
static void ri_strip(int inst)
{
    static const char *const EN[4] = {"ATTACK", "LENGTH", "BEND", "BRIGHT"}, *const FN[4] = {"ECHO", "REVERB", "DRIVE", "LEVEL"},
                      *const AN[4] = {"HOW", "EVERY", "AMOUNT", "HOME"}, *const ED[4] = {"WHAT", "SIZE", "NOTE", "QUALITY"},
                      *const EV[4] = {"1 BAR", "2 BARS", "4 BARS", "8 BARS"}, *const LR[6] = {"1 BEAT", "2 BEATS", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
    static const uint8_t M8[4] = {8, 8, 8, 8};
    const rinst_t *r = &ri[inst];
    const rdesc_t *d = RD[inst];
    const char *n[4], *w[4] = {0, 0, 0, 0}, *hint = 0;
    int32_t v[4], mx[4], i;
    const uint8_t *mid = 0;
    static char nb[6];
    col_t c = ri_col(inst);
    if (rm.edit) {
        int s = 0;
        while (s < RP_N - 1 && !(r->hand >> s & 1u)) s++;
        for (i = 0; i < 4; i++) n[i] = ED[i];
        v[0] = r->p[s].what; mx[0] = d->nkind - 1; w[0] = d->kinds[r->p[s].what];
        v[1] = r->p[s].size + 1; mx[1] = 8; v[2] = r->p[s].note; mx[2] = 20; v[3] = r->p[s].qual + 1; mx[3] = 8;
        rm_note_name(nb, rm_note(inst, r->p[s].note)); w[2] = nb;
        hint = "HOLD KEYS  ARP: SCATTER  EDIT: CLOSE";
    } else if (RHELD(BTN_LFO)) {
        for (i = 0; i < 4; i++) { n[i] = d->kn[i]; v[i] = r->lfo[i]; mx[i] = 8; }
        hint = r->lfo_on ? "LFO: SWING   SELECT: HOW SLOW" : "LFO: SWING (OFF: TAP LFO)"; w[0] = LR[r->lfo_rate % 6u];
    } else if (RHELD(BTN_ENV)) { for (i = 0; i < 4; i++) { n[i] = EN[i]; v[i] = r->env[i]; mx[i] = 8; } mid = M8; hint = "ENV: THE SHAPE OF A HIT"; }
    else if (RHELD(BTN_FX)) {
        for (i = 0; i < 4; i++) n[i] = FN[i];
        v[0] = rmx[inst].echo; v[1] = rmx[inst].rev; v[2] = rmx[inst].drv; v[3] = rmx[inst].lvl; mx[0] = mx[1] = mx[2] = 8; mx[3] = 16;
        hint = rmx[inst].mute ? "FX: MUTED (TAP FX)" : "FX: ITS STRIP OF THE MIXER";
    } else if (RHELD(BTN_SEQ)) { for (i = 0; i < 4; i++) { n[i] = d->sqn[i]; v[i] = r->sq[i]; mx[i] = d->sqmax[i]; } hint = d->seqn; }
    else if (RHELD(BTN_ARP)) {
        for (i = 0; i < 4; i++) { n[i] = AN[i]; v[i] = r->ar[i]; }
        mx[0] = 3; mx[1] = 3; mx[2] = 3; mx[3] = 1; w[0] = ARP_NAME[r->ar[0] & 3u]; w[1] = EV[r->ar[1] & 3u]; w[3] = r->ar[3] ? "YES" : "NO";
        hint = "ARP: THE LAYOUT TENDS ITSELF";
    } else {
        for (i = 0; i < 4; i++) { n[i] = d->kn[i]; v[i] = ri_k(inst, i); mx[i] = d->kmax[i] * 16; }
        mid = d->kmid;
    }
    rm_knobs(n, v, mx, mid, w, c, rm_turned_t > 0 ? rm_turned : 0);
    grect(0, RSTRIP_Y, AC_W, 10, th->bg);
    if (rm.note_t > 0) { int32_t x = gtext(4, RSTRIP_Y + 2, rm.note_a, th->mute, 1); gtext(x + 5, RSTRIP_Y + 2, rm.note_b, th->text, 1); }
    else if (hint) gtext(4, RSTRIP_Y + 2, hint, th->mute, 1);
    else {                                       /* nothing held: what is switched on */
        int32_t x = 4;
        x = gtext(x, RSTRIP_Y + 2, "SEQ", r->seq ? c : th->mute, 1) + 6; x = gtext(x, RSTRIP_Y + 2, "ARP", r->arp ? c : th->mute, 1) + 6;
        x = gtext(x, RSTRIP_Y + 2, "LFO", r->lfo_on && (r->lfo[0] | r->lfo[1] | r->lfo[2] | r->lfo[3]) ? c : th->mute, 1) + 10;
        x = gtext(x, RSTRIP_Y + 2, d->presets[r->preset], th->text, 1) + 8;
        x = gtext(x, RSTRIP_Y + 2, d->algos[r->algo], th->mute, 1);
        if (x < 172) gtextr(236, RSTRIP_Y + 2, "HOME: ROOM", th->mute, 1);
    }
}
/* the scatter menu, over the stopped world */
static void ri_menu_draw(int inst)
{
    const rdesc_t *d = RD[inst];
    int32_t i, x = 26, y = 20, w = 188, h = 166;
    char nb[4];
    grect(x, y, w, h, th->panel); gbox(x, y, w, h, ri_col(inst));
    gtext(x + 6, y + 6, "SCATTER", th->text, 1); gtextr(x + w - 6, y + 6, "TOP KEYS: KINDS", th->mute, 1);
    for (i = 1; i < d->nkind; i++) {             /* the kinds it may use, one to each top key, three to a row */
        int on = rm.sc_mask >> i & 1;
        int32_t cx = x + 6 + ((i - 1) % 3) * 59, cy = y + 17 + ((i - 1) / 3) * 13;
        if (on) grect(cx, cy, 57, 11, th->bg);
        gbox(cx, cy, 57, 11, on ? ri_col(inst) : th->mute);
        gfit(cx + 28, cy + 3, 54, d->kinds[i], on ? th->text : th->mute, 1);
    }
    for (i = 0; i < SCR_N; i++) {
        int32_t ry = y + 48 + i * 14;
        if (i == rm.mrow) { grect(x + 3, ry - 2, w - 6, 12, th->bg); gtext(x + 6, ry + 1, "\xE2\x86\x92", th->pad, 1); }
        gtext(x + 16, ry + 1, SCR_NAME[i], i == rm.mrow ? th->text : th->mute, 1);
        if (SCR_VAL[i][0]) gtextr(x + w - 8, ry + 1, SCR_VAL[i][rm.scv[i]], th->text, 1);
        else { rm_bar(x + w - 60, ry + 1, 40, rm.scv[i], SCR_MAX[i], 0, ri_col(inst)); nb[0] = (char)('0' + rm.scv[i]); nb[1] = 0; gtextr(x + w - 8, ry + 1, nb, th->text, 1); }
    }
    gtext(x + 6, y + h - 12, "SELECT: ROW  KNOB: SET  " PB ": ROLL", th->mute, 1);
}

/* ---------------------------------------------------------------- the controls every instrument shares.
 * Returns 1 when HOME asks for the room again. */
static void room_save_now(void);
static void room_keep(void);                    /* (SAVE: ac_roomnet.h) */
static void mix_step(void);
static int ri_tick(int inst, int32_t dt)
{
    rinst_t *r = &ri[inst];
    const rdesc_t *d = RD[inst];
    int32_t kn[4], i, s, any = 0;
    uint32_t low = 0, top = 0, lowh = 0;
    for (i = 0; i < 4; i++) { kn[i] = in.enc[ENC_KNOB1 + i]; if (kn[i]) { any = 1; rm_turned = (uint8_t)(1u << i); rm_turned_t = 500; } }
    for (i = 0; i < 16; i++) { if (in.kdown >> R_LOW[i] & 1u) low |= 1u << i; if (in.keys >> R_LOW[i] & 1u) lowh |= 1u << i; }
    for (i = 0; i < 11; i++) if (in.kdown >> R_TOP[i] & 1u) top |= 1u << i;
    if (rm_turned_t > 0) rm_turned_t -= dt;
    if (rm.note_t > 0) rm.note_t -= dt;
    if (BDOWN(BTN_LFO) || BDOWN(BTN_ENV) || BDOWN(BTN_FX) || BDOWN(BTN_SEQ) || BDOWN(BTN_ARP) || BDOWN(BTN_SEL)) rm.held_used = 0;
    i = rm_home(dt);
    if (i == 2) { if (rm.edit) r->run = 1; rtopics_open(inst, inst); return 0; }   /* held: how it works, topic by topic (EDIT is closed, its world goes on) */
    if (i == 1) { if (rm.menu) rm.menu = 0; else if (rm.edit) { rm.edit = 0; r->run = 1; } else return 1; snd_ui(67); }
    if (BDOWN(BTN_SAVE)) { room_keep(); rm_say("SAVED", "THE WHOLE ROOM"); snd_chime(0); }
    if (BDOWN(BTN_OCT_DOWN) != BDOWN(BTN_OCT_UP)) { r->oct = (int8_t)iclamp(r->oct + (BDOWN(BTN_OCT_UP) ? 1 : -1), -2, 2); rm_say_n("OCTAVE", r->oct); ri_strike(inst, 0, 160); }
    /* SCL: the scale; held with KNOB 1, the root */
    if (BHELD(BTN_SEL) && kn[0]) { rm.root = (uint8_t)((rm.root + 12 + (kn[0] > 0 ? 1 : 11)) % 12); rm.held_used = 1; rm_say("ROOT", NOTE_NAME[rm.root]); kn[0] = 0; }
    if ((in.bup >> BTN_SEL & 1u) && !rm.held_used) { rm.scale = (uint8_t)((rm.scale + 1) % RSC_N); rm_say("SCALE", RSC_NAME[rm.scale]); snd_ui(79); }
    /* REC: onto the loop; held a second, this instrument's part of the loop is wiped */
    {
        static int32_t rec_t;
        if (BDOWN(BTN_REC)) rec_t = 0;
        if (BHELD(BTN_REC)) {
            rec_t += dt;
            if (rec_t >= 900 && rec_t - dt < 900) {
                uint16_t n = 0;
                for (i = 0; i < rm.loop_n; i++) if ((rm.loop[i].ik >> 4) != inst) rm.loop[n++] = rm.loop[i];
                rm.loop_n = n; rm.rec = 0; rm.arm = 0; rm_say("LOOP", "THIS ONE WIPED"); snd_ui(55); rn_wipe_tx(inst);
            }
        } else if ((in.bup >> BTN_REC & 1u) && rec_t < 900) rm_rec_tap();
    }
    if (rm.menu) {                               /* the scatter menu */
        if (in.enc[ENC_SELECT]) rm.mrow = (uint8_t)iclamp(rm.mrow + (in.enc[ENC_SELECT] > 0 ? 1 : -1), 0, SCR_N - 1);
        i = kn[0] + kn[1] + kn[2] + kn[3];
        if (i) rm.scv[rm.mrow] = (uint8_t)iclamp(rm.scv[rm.mrow] + (i > 0 ? 1 : -1), 0, SCR_MAX[rm.mrow]);
        for (i = 0; i + 1 < d->nkind && i < 11; i++) if (top >> i & 1u) { rm.sc_mask ^= (uint8_t)(1u << (i + 1)); snd_ui(76); }
        if (BDOWN(BTN_PLAY)) ri_scatter(inst);
        if (BDOWN(BTN_ARP) || BDOWN(BTN_EDIT)) { rm.menu = 0; snd_ui(67); in.bdown &= ~(1u << BTN_EDIT); }
        return 0;
    }
    if (BDOWN(BTN_EDIT)) { rm.edit = !rm.edit; rm.held_used = 1; r->run = !rm.edit; if (!r->hand) r->hand = 1; snd_ui(rm.edit ? 79 : 67); return 0; }
    if (rm.edit) {
        int ch = 0;
        if (BDOWN(BTN_ARP)) { rm.menu = 1; if (!rm.sc_mask) rm.sc_mask = 0xFE; snd_ui(79); return 0; }
        if (low) { r->hand = (uint16_t)lowh; for (s = 0; s < RP_N; s++) if (low >> s & 1u) { ri_strike(inst, s, 150); break; } }   /* keys held together: all in hand, and they stay */
        if (in.enc[ENC_SELECT]) {                /* one part in hand, walking round */
            for (s = 0; s < RP_N - 1 && !(r->hand >> s & 1u); s++) ;
            s = (s + RP_N + (in.enc[ENC_SELECT] > 0 ? 1 : -1)) % RP_N; r->hand = (uint16_t)(1u << s);
        }
        for (s = 0; s < RP_N; s++) {
            rpart_t *p = &r->p[s];
            if (!(r->hand >> s & 1u)) continue;
            if (kn[0]) { p->what = (uint8_t)iclamp(p->what + kn[0], 0, d->nkind - 1); ch = 1; }
            if (kn[1]) { p->size = (uint8_t)iclamp(p->size + kn[1], 0, 7); ch = 1; }
            if (kn[2]) { p->note = (uint8_t)iclamp(p->note + kn[2], 0, 20); ch = 1; }
            if (kn[3]) { p->qual = (uint8_t)iclamp(p->qual + kn[3], 0, 7); ch = 1; }
            for (i = 0; i < d->nkind && i < 6; i++) if (top >> i & 1u) { p->what = (uint8_t)i; ch = 1; }
        }
        if (top >> 6 & 1u) { rpart_t t = r->p[0]; for (s = 0; s < RP_N - 1; s++) r->p[s] = r->p[s + 1]; r->p[RP_N - 1] = t; ch = 1; }
        if (top >> 7 & 1u) { rpart_t t = r->p[RP_N - 1]; for (s = RP_N - 1; s > 0; s--) r->p[s] = r->p[s - 1]; r->p[0] = t; ch = 1; }
        if (top >> 8 & 1u) { for (s = 0; s < 8; s++) { rpart_t t = r->p[s]; r->p[s] = r->p[15 - s]; r->p[15 - s] = t; } ch = 1; }
        if (top >> 9 & 1u) { for (s = 0; s < RP_N; s++) r->p[s].what = 0; ch = 1; }
        if (top >> 10 & 1u) { if (!rm.sc_mask) rm.sc_mask = 0xFE; ri_scatter(inst); }
        if (ch) {
            ri_touch(inst);
            for (s = 0; s < RP_N; s++) if (r->hand >> s & 1u) { if (kn[0] || kn[1] || kn[2] || kn[3] || (top & 63u)) ri_strike(inst, s, 150); break; }
        }
        if (in.enc[ENC_ALGORITHM]) { ri_load_algo(inst, (r->algo + d->nalgo + (in.enc[ENC_ALGORITHM] > 0 ? 1 : -1)) % d->nalgo); rm_say("LAYOUT", d->algos[r->algo]); }
        if (BDOWN(BTN_PLAY)) { r->run = !r->run; d->play(r->run); }   /* hear it turn, still open */
        return 0;
    }
    /* playing */
    if (BHELD(BTN_LFO)) {
        for (i = 0; i < 4; i++) if (kn[i]) { r->lfo[i] = (uint8_t)iclamp(r->lfo[i] + kn[i], 0, 8); r->lfo_on = 1; }
        if (in.enc[ENC_SELECT]) { r->lfo_rate = (uint8_t)iclamp(r->lfo_rate + (in.enc[ENC_SELECT] > 0 ? 1 : -1), 0, 5); any = 1; }
        if (any) rm.held_used = 1;
    } else if (BHELD(BTN_ENV)) { for (i = 0; i < 4; i++) if (kn[i]) r->env[i] = (uint8_t)iclamp(r->env[i] + kn[i], 0, 8); if (any) { rm.held_used = 1; ri_strike(inst, r->hand ? 0 : 0, 150); } }
    else if (BHELD(BTN_FX)) {
        if (kn[0]) rmx[inst].echo = (uint8_t)iclamp(rmx[inst].echo + kn[0], 0, 8);
        if (kn[1]) rmx[inst].rev = (uint8_t)iclamp(rmx[inst].rev + kn[1], 0, 8);
        if (kn[2]) rmx[inst].drv = (uint8_t)iclamp(rmx[inst].drv + kn[2], 0, 8);
        if (kn[3]) rmx[inst].lvl = (uint8_t)iclamp(rmx[inst].lvl + kn[3], 0, 16);
        if (any) rm.held_used = 1;
    } else if (BHELD(BTN_SEQ)) { for (i = 0; i < 4; i++) if (kn[i]) { r->sq[i] = (uint8_t)iclamp(r->sq[i] + kn[i], 0, d->sqmax[i]); r->seq = 1; } if (any) rm.held_used = 1; }
    else if (BHELD(BTN_ARP)) {
        static const uint8_t AM[4] = {3, 3, 3, 1};
        for (i = 0; i < 4; i++) if (kn[i]) { r->ar[i] = (uint8_t)iclamp(r->ar[i] + kn[i], 0, AM[i]); r->arp = 1; }
        if (any) rm.held_used = 1;
    } else {
        for (i = 0; i < 4; i++) if (kn[i]) r->k[i] = (uint8_t)iclamp(r->k[i] + kn[i], 0, d->kmax[i]);
        if (in.enc[ENC_SELECT]) { r->sel = (uint8_t)iclamp(r->sel + (in.enc[ENC_SELECT] > 0 ? 1 : -1), 0, d->selmax); rm_say_n(d->seln, r->sel); }
        if (in.enc[ENC_PRESETS]) { r->preset = (uint8_t)((r->preset + d->npreset + (in.enc[ENC_PRESETS] > 0 ? 1 : -1)) % d->npreset); rm_say("PRESET", d->presets[r->preset]); d->init(); }
        if (in.enc[ENC_ALGORITHM]) { ri_load_algo(inst, (r->algo + d->nalgo + (in.enc[ENC_ALGORITHM] > 0 ? 1 : -1)) % d->nalgo); rm_say("LAYOUT", d->algos[r->algo]); }
    }
    if ((in.bup >> BTN_LFO & 1u) && !rm.held_used) { r->lfo_on = !r->lfo_on; rm_say("SWINGS", r->lfo_on ? "ON" : "OFF"); }
    if ((in.bup >> BTN_FX & 1u) && !rm.held_used) { rmx[inst].mute = !rmx[inst].mute; rm_say("MUTE", rmx[inst].mute ? "ON" : "OFF"); }
    if ((in.bup >> BTN_SEQ & 1u) && !rm.held_used) { r->seq = !r->seq; rm_say(d->seqn, r->seq ? "ON" : "OFF"); snd_ui(r->seq ? 84 : 72); }
    if ((in.bup >> BTN_ARP & 1u) && !rm.held_used) { r->arp = !r->arp; r->arp_n = 0; rm_say("TENDS ITSELF", r->arp ? ARP_NAME[r->ar[0] & 3u] : "OFF"); snd_ui(r->arp ? 84 : 72); }
    if (BDOWN(BTN_PLAY)) { r->run = !r->run; d->play(r->run); }
    for (s = 0; s < 16; s++) if (low >> s & 1u) { d->key(s, 200); rn_key_tx(inst, 0, s); }
    for (s = 0; s < 11; s++) if (top >> s & 1u) { d->top(s); rn_key_tx(inst, 1, s); }
    return 0;
}
/* the lights while an instrument is open */
static void ri_leds(int inst)
{
    const rinst_t *r = &ri[inst];
    int s;
    for (s = 0; s < RP_N; s++) {
        if (rm.edit) { if (r->hand >> s & 1u) led_key_want |= 1u << R_LOW[s]; else if (r->p[s].what) led_key_dim_want |= 1u << R_LOW[s]; }
        else if (r->fl[s] > 120) led_key_want |= 1u << R_LOW[s];
        else if (r->p[s].what) led_key_dim_want |= 1u << R_LOW[s];
    }
    if (rm.menu) { for (s = 1; s < RD[inst]->nkind; s++) if (rm.sc_mask >> s & 1u) led_key_want |= 1u << R_TOP[s - 1]; }
    else if (rm.edit) for (s = 0; s < 11; s++) led_key_dim_want |= 1u << R_TOP[s];
    if (rm.edit) led_btn_want |= 1u << BTN_EDIT;
    if (r->seq) led_btn_want |= 1u << BTN_SEQ;
    if (r->arp) led_btn_want |= 1u << BTN_ARP;
    if (r->lfo_on && (r->lfo[0] | r->lfo[1] | r->lfo[2] | r->lfo[3])) led_btn_want |= 1u << BTN_LFO;
    if (rm.rec || (rm.arm && (rm.t / 150u) & 1u)) led_btn_want |= 1u << BTN_REC;
    if (r->run && (rm.tq8 >> 8) % TICKS_BEAT < 20u) led_btn_want |= 1u << BTN_PLAY;
    if (rmx[inst].mute) led_breathe_want |= 1u << BTN_FX;
}

/* ---------------------------------------------------------------- the room's time: every instrument stepped, the loop played */
static void rm_frame(int32_t dt)
{
    static const uint16_t EVERY[4] = {384, 768, 1536, 3072};
    uint32_t t0 = tr_now_q8(), step = tr_bpm_q8 * 96u / 6000u, n = 0, len = (uint32_t)rm.bars * TICKS_BAR;
    int i, s;
    rm.t += (uint32_t)dt;
    rm.acc += dt;
    for (i = 0; i < RI_N; i++) rm.pulse[i] = rm.pulse[i] * 3 / 4;
    while (rm.acc >= 10 && n < 8u) {
        uint32_t a, b;
        rm.acc -= 10;
        rm.sub = (int32_t)n * 10;
        rm.tq8 = t0 + n * step;
        if (rm.ptq8 == 0 || rm.tq8 - rm.ptq8 > (TICKS_BAR << 8)) rm.ptq8 = rm.tq8 - step;
        if ((int32_t)(rm.tq8 - rm.ptq8) > 0) {
            a = (rm.ptq8 >> 8) % len; b = (rm.tq8 >> 8) % len;
            if (a != b && !rm.loop_off && !rm.tut) for (i = 0; i < rm.loop_n; i++) {   /* the loop: what was recorded where the clock now is */
                const rev_t *e = &rm.loop[i];
                if (a < b ? (e->t > a && e->t <= b) : (e->t > a || e->t <= b)) rm_voice(e->ik >> 4, e->ik & 15, e->deg, e->vel, e->sq >> 4, e->sq & 15, (uint32_t)rm.sub);
            }
            {   /* the click: the metronome, and REC's count; at the bar line counted to, REC is on */
                uint32_t bi;
                if (rm_cross(TICKS_BEAT, 0, &bi)) {
                    int click = rm.arm || rset.metro == 1 || (rset.metro == 2 && rm.rec);
                    if (rm.arm && (rm.tq8 >> 8) + 4u >= rm.arm) { rm.arm = 0; rm.rec = 1; rm_say("REC", "ON THE LOOP"); }
                    if (click && !rm.tut) sfx(bi % 4u ? 89 : 96, (uint32_t)rm.sub, 30, W_SQUARE, bi % 4u ? 1500 : 2600, 0, 0);
                }
            }
            for (i = 0; i < RI_N; i++) {
                rinst_t *r = &ri[i];
                if (r->arp && rn_auth(i) && !(rm.edit && rm.focus == i) && rm_cross(EVERY[r->ar[1] & 3u], 0, 0)) ri_tend(i);
                RD[i]->step();
                for (s = 0; s < RP_N; s++) if (r->fl[s]) r->fl[s] = (uint8_t)(r->fl[s] > 12 ? r->fl[s] - 12 : 0);
            }
            mix_step();
            rm.ptq8 = rm.tq8;
        }
        n++;
    }
    if (rm.acc > 30) rm.acc = 0;
    rm.sub = 0;
}

#include "ac_mirun.h"                         /* (a loaded instrument in a place of the room) */
