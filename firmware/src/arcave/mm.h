/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's mystic machines: what a machine that is LOADED (not built into the firmware) is, and what it may use.
 *
 * The Arcave has three ledges. On each stands one of the three machines the firmware carries, or a machine loaded
 * from the site's MYSTIC MACHINES page into that ledge's slot of the machine bank: 40 KiB of the FM-1's data flash,
 * where it runs in place. A loaded machine is one C file compiled on its own (tools/mm_build.py): it calls nothing
 * of the firmware's by name and keeps no variables of its own. Everything comes through the three things each of its
 * functions is handed:
 *     st   its own RAM, as many bytes as its header asks for, all zero when a game starts
 *     c    this frame: the clocks, the keys, knobs and buttons, the game's score and lives, the lights it wants
 *     a    the firmware's functions (drawing, sound, numbers), by pointer
 * The firmware shows the cards before and after a game, the score and the hearts, the tickets, HOME (the machine's
 * own words on how it is played) and REC's way out, as for its own machines; the machine draws its picture and plays
 * its game. In the browser and in the tests the same file is compiled into the game (firmware/machines/, MM_TWINS in
 * ac_slots.h), so a machine is played and tested before it is ever loaded.
 *
 * The API only grows: a machine built for MM_API n runs on every firmware whose MM_API is n or more; new functions
 * go at the END of mm_api_t, new facts at the end of mm_ctx_t, and nothing is taken out or moved. docs/MACHINES.md
 * tells how to write one. */
#ifndef MM_H
#define MM_H
#include <stdint.h>

#define MM_API 1u
#define MM_MAGIC 0x48434D4Du                     /* "MMCH", the first word of a machine */
#define MM_NSLOT 3u                              /* a slot a ledge */
#define MM_SLOT 0xA000u                          /* 40 KiB: ten sectors of the FM-1's data flash */
#define MM_BASE 0x02093000u                      /* where slot 0 is run from on the FM-1 (flash 0x97000); slot i: + i * MM_SLOT */
#define MM_STATE_MAX 4096u                       /* the most RAM a machine may ask for */
#define MM_HEAD 20u                              /* a machine's first bytes are the same on every build: magic, api, state,
                                                    id, ver, size, crc (the CRC-32 of all of it but these last four bytes) */

typedef uint16_t mm_col_t;                       /* a colour of the screen (5-6-5) */
#define MM_RGB(r, g, b) ((mm_col_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* c->col[]: the colours of the look the player chose. The five caps are the game's five colours; the four zones are
 * the keyboard's (yellow, white, green, blue: c->zone[key] says which a key wears) */
enum { MMC_BG, MMC_TEXT, MMC_MUTE, MMC_PAD, MMC_PANEL, MMC_RIM, MMC_CAP0, MMC_CAP1, MMC_CAP2, MMC_CAP3, MMC_CAP4,
       MMC_ZONE0, MMC_ZONE1, MMC_ZONE2, MMC_ZONE3, MMC_N };
enum { MM_TITLE, MM_PLAY, MM_OVER };             /* c->state: its start card is up, a game, the card after one */
enum { MM_SINE, MM_TRI, MM_SQUARE, MM_SAW };     /* a->sfx's waves */
enum { MMB_OCT_DN, MMB_OCT_UP, MMB_FX, MMB_SCL, MMB_ENV, MMB_LFO, MMB_EDIT, MMB_GLO, MMB_HOME, MMB_SAVE, MMB_ARP,
       MMB_SEQ, MMB_PLAY, MMB_REC };             /* the buttons' bits (HOME, PLAY, REC and GLO are the firmware's) */
enum { MME_SELECT, MME_ALGORITHM, MME_KNOB1, MME_KNOB2, MME_KNOB3, MME_KNOB4, MME_PRESETS };   /* c->enc[] */
#define MM_NKEY 27                               /* the keys: bit 0 the lowest */

typedef struct mm_ctx {
    uint32_t ms;                                 /* the FM-1's clock, milliseconds */
    int32_t dt;                                  /* milliseconds since the last frame (draw: 0) */
    uint32_t beat_q8;                            /* the jukebox's clock, 256 a beat. It stands still while the game is held
                                                    (HOME's words, REC's question) and goes on from there, still on the beat */
    uint32_t bpm_q8;                             /* its tempo */
    uint32_t keys, kdown, kup;                   /* the keys held, pressed this frame, let go */
    uint32_t btn, bdown;                         /* the buttons */
    int32_t enc[8];                              /* each knob's clicks this frame */
    uint8_t lang, state, tsize, rsv;             /* the language (0 English, 1 Deutsch, 2 Dansk, 3 Français, 4 Español,
                                                    5 Türkçe, 6 Nederlands); MM_TITLE..; the text size 0..2 */
    int32_t score, lives;                        /* the game's: the machine changes them, the firmware shows them */
    uint32_t led_keys, led_glow;                 /* the keys it wants lit and glowing this frame (set in tick) */
    const mm_col_t *col;                         /* MMC_... */
    const uint8_t *zone;                         /* [MM_NKEY] the colour group 0..3 of each key */
    uint8_t *keep;                               /* eight bytes kept with the player's save for this machine */
} mm_ctx_t;

typedef struct mm_api {
    uint32_t api;                                /* the firmware's MM_API */
    /* the picture: 240 x 240, x to the right, y down. alpha: 256 solid, less lets what is under show, for all that follows */
    void (*alpha)(int32_t a256);
    void (*rect)(int32_t x, int32_t y, int32_t w, int32_t h, mm_col_t c);
    void (*box)(int32_t x, int32_t y, int32_t w, int32_t h, mm_col_t c);
    void (*line)(int32_t x0, int32_t y0, int32_t x1, int32_t y1, mm_col_t c);
    void (*disc)(int32_t cx, int32_t cy, int32_t r, mm_col_t c);
    void (*arc)(int32_t cx, int32_t cy, int32_t rx, int32_t ry, int32_t a0, int32_t a1, mm_col_t c, int thick);   /* 65536 a turn */
    void (*poly)(const int32_t *xy, int32_t n, mm_col_t c);
    void (*veil)(mm_col_t c, uint32_t a32);      /* the whole screen toward c, 0..32 */
    int32_t (*text)(int32_t x, int32_t y, const char *s, mm_col_t c, int32_t scale);
    int32_t (*textw)(const char *s, int32_t scale);
    int32_t (*fit)(int32_t cx, int32_t y, int32_t w, const char *s, mm_col_t c, int32_t scale);   /* centred, made smaller to fit w */
    int32_t (*block)(int32_t x, int32_t y, int32_t w, const char *s, mm_col_t c, int32_t scale, int centred);   /* wrapped; its height */
    int32_t (*ts)(int32_t kind, int32_t cap);    /* a scale for the player's text size: kind 1 small print, 2 text, 3 titles */
    void (*rock)(int32_t x, int32_t y, int32_t size, int32_t turn, int32_t eyes);   /* Rock as the player dressed him; size 600 as in the cave, eyes 0..256 */
    /* sound */
    void (*sfx)(int32_t note, uint32_t delay_ms, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t attack_ms, uint32_t echo);
    int32_t (*key_note)(int32_t key);            /* the note key 0..26 plays in the tune that is on: no key is wrong */
    void (*key_voice)(int32_t key, int32_t note);   /* that note in the voice the key's colour wears */
    void (*ui)(int32_t note);                    /* a small click of a note */
    /* numbers */
    int32_t (*sin)(int32_t a);                   /* 65536 a turn -> -16384..16384 */
    uint32_t (*rnd)(void);
    uint32_t (*hash)(uint32_t x);                /* 0..65535, the same every time for the same x */
    /* the game */
    void (*over)(void);                          /* the game is over: the card, the best score, the tickets */
    void (*say)(const char *s);                  /* a word over the game for a moment */
    void (*keep_changed)(void);                  /* c->keep was written: it goes into the save */
} mm_api_t;

typedef struct mm_head {
    uint32_t magic;                              /* MM_MAGIC */
    uint16_t api, state;                         /* the MM_API it was built for; the bytes of RAM it wants */
    uint16_t id, ver;                            /* its own number (docs/MACHINES.md keeps the list), its version */
    uint32_t size, crc;                          /* (tools/mm_build.py and the page fill these in) */
    uint8_t cap, lives, per_ticket, rsv;         /* its colour, 0..4 of the five; hearts at the start of a game (0: none
                                                    shown); a ticket for every so many points (and one for playing) */
    const char *name;                            /* in capitals, as over its cabinet (twenty letters at most) */
    const char *const *help;                     /* seven texts, a language each (0: the English one): how it is played */
    const uint8_t *sign;                         /* sixteen rows of sixteen dots, two bytes a row, the left dot the high
                                                    bit: its sign on the cabinet in the cave */
    void (*start)(void *st, mm_ctx_t *c, const mm_api_t *a);   /* a game begins */
    void (*tick)(void *st, mm_ctx_t *c, const mm_api_t *a);    /* a frame of the game */
    void (*draw)(void *st, mm_ctx_t *c, const mm_api_t *a);    /* its picture, under the cards too (c->state) */
} mm_head_t;

/* the machine's one object: MM_MACHINE(glowcaps) = { MM_MAGIC, MM_API, sizeof(my_state_t), ... }; */
#ifdef MM_MODULE
#define MM_MACHINE(name) __attribute__((section(".mm_head"), used)) const mm_head_t mm_machine_##name
#else
#define MM_MACHINE(name) static const mm_head_t mm_machine_##name
#endif
#endif
