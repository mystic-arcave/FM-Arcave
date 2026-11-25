/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's music room: what an instrument that is LOADED (not built into the firmware) is, and what it may use.
 *
 * The music room has four places. In each stands the instrument the firmware carries for it (ROOTS, THE GEODE,
 * MUSHROOM RACK, CRYSTAL SEAM), or an instrument loaded from the site's MACHINES page into that place's slot of the
 * bank: 40 KiB of the FM-1's data flash, where it runs in place, as a loaded machine does (mm.h; the same file
 * format, the same tool, the same link). A loaded instrument is one C file compiled on its own: it calls nothing of
 * the firmware's by name and keeps no variables of its own. Each of its functions is handed
 *     st   its own RAM (its world), as many bytes as its header asks for, all zero before init is first called
 *     c    this moment: the room's clock, the instrument as the room keeps it (its 16 parts, its laws), the colours
 *     a    the firmware's functions (drawing, the room's sounds, numbers), by pointer
 *
 * What an instrument of the room is (ac_room.h): a small physical world of 16 parts, one to each key of the bottom
 * row, stepped a hundred times a second in whole numbers. A sound comes only when something in that world meets
 * something else. The firmware gives every instrument the same controls (KNOB 1-4 its four laws, SELECT a fifth,
 * PRESETS, ALGORITHM's layouts, SEQ's steady device, ARP, LFO, ENV, FX, SCL, EDIT, REC, the loop, the mixer, playing
 * together), keeps and saves the layout and the laws, and shows the strip along the foot of the screen. The
 * instrument brings the names of those things, its world (init, step), its picture (draw, small), what its keys do,
 * its ready-made layouts, the sound of each of its voices, and its lesson.
 *
 * The API only grows: an instrument built for MI_API n runs on every firmware whose MI_API is n or more; new
 * functions go at the END of mi_api_t, new facts at the end of mi_ctx_t, nothing is taken out or moved.
 * docs/MACHINES.md tells how to write one; firmware/instruments/spring.c is the one to start from. */
#ifndef MI_H
#define MI_H
#include "mm.h"                                  /* (colours, waves, the file's first twenty bytes) */

#define MI_API 1u
#define MI_MAGIC 0x534E494Du                     /* "MINS", the first word of an instrument */
#define MI_NSLOT 4u                              /* a slot a place of the room */
#define MI_STATE_MAX 2048u                       /* the most RAM an instrument may ask for */
#define MI_NPART 16
#define MI_TICKS_STEP 24u                        /* the room's clock: ticks to a sixteenth; 96 a beat, 384 a bar */

/* a part: what stands there (0: nothing; a kind of the instrument's own, under 8), its size and quality 0..7, its
 * note as a step of the room's scale, 0..20 */
typedef struct { uint8_t what, size, note, qual; } mi_part_t;
/* the instrument as the room keeps it (and saves it, and tells the friends playing along) */
typedef struct {
    mi_part_t p[MI_NPART], mine[MI_NPART];       /* the layout, and the player's own (ALGORITHM 0) */
    uint8_t k[4], lfo[4], env[4], sq[4], ar[4];  /* the four laws as the knobs have them; their swings; the hit's shape; SEQ's and ARP's four */
    uint8_t lfo_rate, lfo_on, run, arp, seq, algo, preset, sel;   /* run: PLAY has it going; seq: its steady device is on; preset, sel: PRESETS, SELECT */
    int8_t oct;
    uint8_t fl[MI_NPART];                        /* each part's flash, 255 when struck, dying away (the firmware's) */
    uint16_t hand;                               /* the parts in hand (EDIT), a bit each */
    uint8_t arp_n;
} mi_inst_t;

enum { MIC_OWN = MMC_N, MIC_STEM, MIC_ROCK, MIC_N };   /* c->col[]: mm.h's, then its place's own colour, a stalk's, Rock's grey */
/* the firmware's own voices, for a->std (an instrument's own are numbered by itself, 0..15: h->voice) */
enum { MIS_CLICK, MIS_CRYSTAL, MIS_CRACK, MIS_ACID, MIS_SLIDE, MIS_MOSS, MIS_OOZE, MIS_POD, MIS_NUT, MIS_BELL, MIS_GOURD,
       MIS_HUSK, MIS_SALT, MIS_OPAL, MIS_SHARD, MIS_N };
enum { MI_HAT = 5, MI_NOISE = 4 };               /* waves besides mm.h's four: a hiss with a ring to it, plain noise */
#define MI_SILENT 255                            /* h->ksnd[kind]: a part of that kind makes no sound when struck */
/* a->icon: what the lesson's hand points at */
enum { MII_NONE, MII_K1, MII_K2, MII_K3, MII_K4, MII_SELECT, MII_PRESETS, MII_ALGO, MII_LOW, MII_TOP, MII_BTN = 16 };   /* MII_BTN + MMB_... */

typedef struct mi_ctx {
    uint32_t ms;                                 /* milliseconds in the room */
    uint32_t tq8, ptq8;                          /* the room's clock at this step and at the one before: ticks * 256 */
    mi_inst_t *r;                                /* the instrument as the room keeps it */
    int32_t k[4];                                /* the four laws as the world feels them now: the knob and its swing, sixteen to a click */
    uint8_t edit, place, lang, tsize;            /* EDIT has the world open (it stands still); its place 0..3; language; text size */
    int32_t x, y;                                /* small: the middle of its spot on the room's floor */
    int32_t t, dt;                               /* lesson: milliseconds into the step, and since the last frame */
    int32_t step;                                /* lesson: which step */
    const mm_col_t *col;                         /* MMC_... and MIC_... */
} mi_ctx_t;

typedef struct mi_api {
    uint32_t api;                                /* the firmware's MI_API */
    /* the picture: 240 x 240; an instrument has y 13..199 (the head above and the strip below are the firmware's) */
    void (*alpha)(int32_t a256);
    void (*rect)(int32_t x, int32_t y, int32_t w, int32_t h, mm_col_t c);
    void (*box)(int32_t x, int32_t y, int32_t w, int32_t h, mm_col_t c);
    void (*line)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, mm_col_t c);
    void (*disc)(int32_t cx, int32_t cy, int32_t r, mm_col_t c);
    void (*arc)(int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t a0, int32_t a1, mm_col_t c, int thick);   /* 65536 a turn */
    void (*poly)(const int32_t *xy, int32_t n, mm_col_t c);
    int32_t (*text)(int32_t x, int32_t y, const char *s, mm_col_t c, int32_t scale);
    int32_t (*textw)(const char *s, int32_t scale);
    int32_t (*fit)(int32_t cx, int32_t y, int32_t w, const char *s, mm_col_t c, int32_t scale);
    /* numbers */
    int32_t (*sin)(int32_t a);                   /* 65536 a turn -> -16384..16384 */
    uint32_t (*rnd)(void);
    uint32_t (*hash)(uint32_t x);
    uint32_t (*sqrt)(uint32_t v);
    /* the room */
    void (*strike)(int32_t part, int32_t vel);   /* a part is struck (vel 1..255): its flash, the voice of its kind, onto the loop if REC is on */
    void (*hit)(int32_t voice, int32_t deg, int32_t vel, int32_t size, int32_t qual);   /* one of its own voices, at a step of the scale */
    int (*cross)(uint32_t per, uint32_t off, uint32_t *idx);   /* in step: a line of the clock went by, every per ticks, off late */
    int32_t (*note)(int32_t deg);                /* a step of the room's scale as a note, in this instrument's octave */
    void (*note_name)(char *b6, int32_t note);
    void (*ui)(int32_t note);                    /* a small click (PLAY, a key that is not of the world) */
    /* in h->voice only: one oscillator of the sound, with the instrument's ENV and its strip of the mixer on it */
    void (*vo)(int32_t note, uint32_t delay_ms, int32_t dur_ms, int wave, int32_t vol, int32_t att_ms, int32_t detune, int32_t pan);
    void (*flt)(int mode, int32_t cut, int32_t res, int32_t fenv, int32_t ms);   /* a filter on the next vo: 1 low-pass, 2 band-pass; cut as a note */
    void (*glide)(int32_t semis, int32_t ms);    /* the next vo starts that far off its note and slides in */
    void (*std)(int32_t kind, int32_t deg, int32_t vel, int32_t size, int32_t qual);   /* one of the firmware's voices, MIS_... */
    /* in h->lesson only */
    void (*icon)(int32_t i);                     /* MII_... */
    void (*label)(const char *s);                /* a word over the picture for a second */
    void (*open)(int edit, int menu, uint32_t kinds);   /* EDIT open or shut; the scatter menu up, with these kinds ticked (a bit each) */
    void (*scatter)(void);
    void (*layout)(int n);                       /* a ready-made layout, as ALGORITHM picks it (0: the player's own) */
    void (*changed)(void);                       /* c->r->p was written by hand: the world is set up again (init) */
} mi_api_t;

typedef struct { uint16_t ms; const char *tx[7]; void *rsv; } mi_step_t;   /* a step of the lesson: how long, its caption in each language (0: the English) */
typedef struct { const char *name; uint8_t first, n; } mi_topic_t;         /* HOME held: steps first..first+n-1 under a name */

typedef struct mi_head {
    uint32_t magic;                              /* MI_MAGIC */
    uint16_t api, state;                         /* the MI_API it was built for; the bytes of RAM it wants */
    uint16_t id, ver;                            /* its own number (docs/MACHINES.md keeps the list), its version */
    uint32_t size, crc;                          /* (tools/mm_build.py and the page fill these in) */
    const char *name, *shortn, *tag;             /* "THE SPRING" (twenty letters at most), "SPRING" (six), "SP" (two) */
    const char *kn[4]; uint8_t kmax[4], kdef[4], kmid[4];   /* KNOB 1-4: names, the most clicks, where they start, 1: its bar grows from the middle */
    const char *seln; uint8_t selmax, seldef, nkind, npreset;   /* SELECT; how many kinds (kinds[0] is nothing there; 8 at most), PRESETS */
    const char *seqn, *sqn[4]; uint8_t sqmax[4], sqdef[4];      /* "SEQ: THE FROG", and SEQ held: KNOB 1-4 */
    const char *const *kinds;                    /* the kinds' names */
    const uint8_t *ksnd;                         /* the voice of each kind when a part of it is struck (MI_SILENT: none) */
    const char *const *presets;
    const char *const *algos; uint8_t nalgo, nstep, ntopic, rsv;   /* layouts (algos[0]: "YOUR OWN"); the lesson's steps and topics */
    const mi_step_t *steps;
    const mi_topic_t *topics;
    void (*init)(void *st, mi_ctx_t *c, const mi_api_t *a);    /* the world set up from the layout (also after every change of it) */
    void (*step)(void *st, mi_ctx_t *c, const mi_api_t *a);    /* 10 ms of it */
    void (*draw)(void *st, mi_ctx_t *c, const mi_api_t *a);    /* its picture, the whole screen's */
    void (*small)(void *st, mi_ctx_t *c, const mi_api_t *a);   /* as it stands in the room: about 60 wide, up from (c->x, c->y) */
    void (*key)(void *st, mi_ctx_t *c, const mi_api_t *a, int i, int vel);   /* a key of the bottom row, 0..15 */
    void (*top)(void *st, mi_ctx_t *c, const mi_api_t *a, int i);            /* a key of the top row, 0..10 */
    void (*layout)(void *st, mi_ctx_t *c, const mi_api_t *a, int n);         /* ready-made layout n >= 1 into c->r->p */
    void (*play)(void *st, mi_ctx_t *c, const mi_api_t *a, int on);          /* PLAY pressed */
    void (*voice)(void *st, mi_ctx_t *c, const mi_api_t *a, int voice, int32_t deg, int32_t vel, int32_t size, int32_t qual);
    void (*lesson)(void *st, mi_ctx_t *c, const mi_api_t *a);  /* a frame of step c->step, c->t ms into it (c->t - c->dt <= 0: its first) */
} mi_head_t;

/* the instrument's one object: MI_INSTRUMENT(spring) = { MI_MAGIC, MI_API, sizeof(my_world_t), ... }; */
#ifdef MM_MODULE
#define MI_INSTRUMENT(name) __attribute__((section(".mm_head"), used)) const mi_head_t mi_instrument_##name
#else
#define MI_INSTRUMENT(name) static const mi_head_t mi_instrument_##name
#endif
#endif
