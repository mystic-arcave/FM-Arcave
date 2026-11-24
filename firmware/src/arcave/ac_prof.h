/* SPDX-License-Identifier: GPL-3.0-only */
/* The player's profile: the rock's name, the tickets the machines pay out, what has been bought with them and what is
 * worn. This file is the part without a picture (the save, the catalogue, the keys' voices); the pictures are in
 * ac_deco.h and the screens in ac_shop.h.
 *
 * THE CATALOGUE, AND HOW TO ADD TO IT
 * Everything that can be unlocked is one line of PF_ITEMS: its shelf (PC_...), its kind (its number on that shelf, from
 * 0 up, never reused), its price in tickets (0: everyone has it) and its name (names are names: not translated).
 * What is owned is kept as one bit an item, the bit's number being shelf * 16 + kind, so a save stays good when lines
 * are added: put a new line at the end of its shelf with the next kind (up to 16 a shelf), and give it a picture or a
 * sound where its shelf's are made:
 *   PC_BODY   Rock's stone            PF_BODY's colours, deco_body() for its pattern      ac_deco.h
 *   PC_HAT    on top of him           deco_hat()                                          ac_deco.h
 *   PC_FACE   round his eyes          deco_face()                                         ac_deco.h
 *   PC_TRAIL  what he leaves rolling  deco_trail()                                        ac_deco.h
 *   PC_KCOL   a key colour's light    PF_KCOL below
 *   PC_KFX    what a key throws off   deco_key_fx()                                       ac_deco.h
 *   PC_KSND   a key's voice           pf_key_voice() below
 *   PC_JUMP   how he jumps            deco_jump_tick()                                    ac_deco.h
 * The keyboard's four colours (yellow, white, green, blue: KEY_ZONE) each wear one of each of the last four. */

enum { PC_BODY, PC_HAT, PC_FACE, PC_TRAIL, PC_KCOL, PC_KFX, PC_KSND, PC_JUMP, PC_N };
#define PC_ROCK_N 4                              /* the first four are Rock's own; the other four are worn by a key colour */
typedef struct { uint8_t cat, kind; uint16_t price; const char *name; } pf_item_t;
static const pf_item_t PF_ITEMS[] = {
    {PC_BODY, 0, 0, "PLAIN ROCK"}, {PC_BODY, 1, 15, "MOSSY"}, {PC_BODY, 2, 40, "TOADSTOOL"}, {PC_BODY, 3, 60, "MARBLE"},
    {PC_BODY, 4, 80, "ZEBRA"}, {PC_BODY, 5, 120, "LAVA"}, {PC_BODY, 6, 120, "ICE"}, {PC_BODY, 7, 200, "GOLD"},
    {PC_BODY, 8, 300, "GALAXY"}, {PC_BODY, 9, 400, "DISCO"},
    {PC_HAT, 0, 0, "NO HAT"}, {PC_HAT, 1, 10, "SPROUT"}, {PC_HAT, 2, 25, "PARTY HAT"}, {PC_HAT, 3, 30, "FLOWER"},
    {PC_HAT, 4, 50, "MUSHROOM CAP"}, {PC_HAT, 5, 75, "HEADPHONES"}, {PC_HAT, 6, 100, "TOP HAT"}, {PC_HAT, 7, 150, "PROPELLER"},
    {PC_HAT, 8, 180, "HALO"}, {PC_HAT, 9, 250, "WIZARD HAT"}, {PC_HAT, 10, 500, "CROWN"},
    {PC_FACE, 0, 0, "JUST EYES"}, {PC_FACE, 1, 10, "BLUSH"}, {PC_FACE, 2, 20, "BROWS"}, {PC_FACE, 3, 40, "GLASSES"},
    {PC_FACE, 4, 60, "MOUSTACHE"}, {PC_FACE, 5, 90, "MONOCLE"}, {PC_FACE, 6, 110, "EYEPATCH"}, {PC_FACE, 7, 150, "SHADES"},
    {PC_TRAIL, 0, 0, "NO TRAIL"}, {PC_TRAIL, 1, 15, "DUST"}, {PC_TRAIL, 2, 40, "BUBBLES"}, {PC_TRAIL, 3, 70, "NOTES"},
    {PC_TRAIL, 4, 100, "SPARKLES"}, {PC_TRAIL, 5, 160, "FLAMES"}, {PC_TRAIL, 6, 250, "RAINBOW"},
    {PC_KCOL, 0, 0, "ITS OWN"}, {PC_KCOL, 1, 20, "PINK"}, {PC_KCOL, 2, 20, "ORANGE"}, {PC_KCOL, 3, 20, "PURPLE"},
    {PC_KCOL, 4, 20, "TEAL"}, {PC_KCOL, 5, 20, "RED"}, {PC_KCOL, 6, 60, "GOLD"}, {PC_KCOL, 7, 150, "RAINBOW"},
    {PC_KFX, 0, 0, "NOTHING"}, {PC_KFX, 1, 15, "SPARK"}, {PC_KFX, 2, 30, "RING"}, {PC_KFX, 3, 45, "NOTES"},
    {PC_KFX, 4, 60, "STARS"}, {PC_KFX, 5, 60, "BUBBLES"}, {PC_KFX, 6, 80, "HEARTS"}, {PC_KFX, 7, 120, "ZAP"},
    {PC_KSND, 0, 0, "ROCK TONE"}, {PC_KSND, 1, 20, "BELL"}, {PC_KSND, 2, 20, "CHIP"}, {PC_KSND, 3, 30, "BUZZ"},
    {PC_KSND, 4, 40, "PLUCK"}, {PC_KSND, 5, 60, "BUBBLE"}, {PC_KSND, 6, 80, "LASER"}, {PC_KSND, 7, 100, "WOBBLE"},
    {PC_KSND, 8, 120, "CHOIR"},
    {PC_JUMP, 0, 0, "HOP"}, {PC_JUMP, 1, 30, "FLIP"}, {PC_JUMP, 2, 30, "BACKFLIP"}, {PC_JUMP, 3, 60, "TWIRL"},
    {PC_JUMP, 4, 90, "BIG AIR"}, {PC_JUMP, 5, 140, "GHOST"}, {PC_JUMP, 6, 200, "STAR JUMP"},
};
#define PF_NITEM ((int32_t)(sizeof PF_ITEMS / sizeof PF_ITEMS[0]))

/* a key colour's light: the keys' own four first (as the keycaps are), then what PC_KCOL's kinds 1.. give */
static const col_t PF_KEYCOL[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
static const col_t PF_KCOL[8] = {0, AC_RGB(255, 96, 190), AC_RGB(255, 140, 40), AC_RGB(170, 96, 255), AC_RGB(40, 216, 200),
                                 AC_RGB(255, 60, 60), AC_RGB(255, 200, 60), 0};

/* ---------------------------------------------------------------- what is kept */
#define PF_NAME_MAX 12
enum { PF_NAMED = 1, PF_TOLD_TIX = 2, PF_TOLD_NAME = 4, PF_BT = 8 };
enum { PF_ONCE_BT = 1 };
typedef struct {
    uint8_t ver, flags, mach_done, once;         /* mach_done: a bit for each machine a round has been finished on; once:
                                                    PF_ONCE_BT, Bluetooth turned on the first time this firmware ran */
    char name[PF_NAME_MAX + 4];
    uint32_t tickets, won;                       /* in the pocket; won since the start */
    uint8_t own[16];                             /* a bit an item: shelf * 16 + kind */
    uint8_t eq[PC_ROCK_N];                       /* what Rock wears: a kind for each of his four shelves */
    uint8_t zone[4][4];                          /* what each key colour wears: light, effect, voice, jump */
    uint8_t rsv[20];                             /* (room for more without another size of save) */
} prof_save_t;
static prof_save_t pf;
static uint8_t pf_dirty;                         /* something here changed: arcave.c saves */

static int pf_cat_n(int cat) { int32_t i, n = 0; for (i = 0; i < PF_NITEM; i++) if (PF_ITEMS[i].cat == cat) n++; return n; }
static const pf_item_t *pf_item(int cat, int kind)
{
    int32_t i;
    for (i = 0; i < PF_NITEM; i++) if (PF_ITEMS[i].cat == cat && PF_ITEMS[i].kind == kind) return &PF_ITEMS[i];
    return &PF_ITEMS[0];
}
static int pf_owned(int cat, int kind)
{
    uint32_t b = (uint32_t)cat * 16u + (uint32_t)kind;
    if (kind < 0 || kind >= 16 || !pf_item(cat, kind)->price) return kind >= 0 && kind < pf_cat_n(cat);
    return pf.own[b >> 3] >> (b & 7u) & 1u;
}
static void pf_own(int cat, int kind) { uint32_t b = (uint32_t)cat * 16u + (uint32_t)kind; pf.own[b >> 3] |= (uint8_t)(1u << (b & 7u)); }
static int32_t pf_owned_n(void)                  /* how many of the ones that cost something are owned */
{
    int32_t i, n = 0;
    for (i = 0; i < PF_NITEM; i++) if (PF_ITEMS[i].price && pf_owned(PF_ITEMS[i].cat, PF_ITEMS[i].kind)) n++;
    return n;
}
static int32_t pf_priced_n(void) { int32_t i, n = 0; for (i = 0; i < PF_NITEM; i++) if (PF_ITEMS[i].price) n++; return n; }
static int pf_named(void) { return (pf.flags & PF_NAMED) && pf.name[0]; }
static void pf_reset(void)
{
    uint32_t i;
    for (i = 0; i < sizeof pf; i++) ((uint8_t *)&pf)[i] = 0;
    pf.ver = 1;
    pf.flags = PF_BT; pf.once = PF_ONCE_BT;      /* Bluetooth on from the start (the settings turn it off) */
}
/* after a load: nothing worn that is not owned or not in the catalogue, a name of letters the font has */
static void pf_check(void)
{
    int32_t i, z;
    if (pf.ver != 1) { pf_reset(); return; }
    if (!(pf.once & PF_ONCE_BT)) { pf.flags |= PF_BT; pf.once |= PF_ONCE_BT; pf_dirty = 1; }   /* a save from before Bluetooth was
                                                    on by default: on, once (turned off after this, it stays off) */
    pf.name[PF_NAME_MAX] = 0;
    for (i = 0; i < PF_NAME_MAX && pf.name[i]; i++) if (pf.name[i] < ' ' || pf.name[i] > 'Z') pf.name[i] = '?';
    for (i = 0; i < PC_ROCK_N; i++) if (!pf_owned(i, pf.eq[i])) pf.eq[i] = 0;
    for (z = 0; z < 4; z++) for (i = 0; i < 4; i++) if (!pf_owned(PC_ROCK_N + i, pf.zone[z][i])) pf.zone[z][i] = 0;
}
static col_t pf_kcol(int k, int z, uint32_t tm)  /* light number k on key colour z */
{
    static const col_t RB[6] = {AC_RGB(255, 60, 60), AC_RGB(255, 170, 40), AC_RGB(255, 235, 60), AC_RGB(60, 220, 100), AC_RGB(60, 140, 255), AC_RGB(190, 90, 255)};
    k &= 7;
    return k == 0 ? PF_KEYCOL[z & 3] : k == 7 ? RB[(tm / 110u + (uint32_t)z) % 6u] : PF_KCOL[k];
}
static col_t pf_zone_col(int z, uint32_t tm) { return pf_kcol(pf.zone[z & 3][0], z, tm); }   /* as it is worn now */

/* ---------------------------------------------------------------- the keys' voices
 * A key in the cave, on the course and in the music room's hall plays its note in the voice its colour wears. */
static void pf_voice(int kind, int32_t n)
{
    switch (kind) {
    default: sfx(n, 0, 600, W_TRI, 3600, 6, 160); break;                                                   /* ROCK TONE: as it always was */
    case 1: sfx(n + 12, 0, 1100, W_SINE, 3000, 2, 220); sfx(n + 31, 0, 380, W_SINE, 800, 2, 220); break;    /* BELL */
    case 2: sfx(n, 0, 200, W_SQUARE, 1500, 1, 60); break;                                                  /* CHIP */
    case 3: sfx(n - 12, 0, 420, W_SAW, 1900, 3, 90); break;                                                /* BUZZ */
    case 4: sfx(n, 0, 240, W_TRI, 4200, 1, 120); sfx(n + 12, 0, 70, W_SQUARE, 800, 1, 40); break;          /* PLUCK */
    case 5: sfx_ex(n + 12, 0, 150, W_SINE, 3400, 2, 120, 0, 0, 66300); break;                              /* BUBBLE: up and gone */
    case 6: sfx_ex(n + 24, 0, 300, W_SQUARE, 1300, 1, 140, 0, 0, 64700); break;                            /* LASER: down */
    case 7: sfx_ex(n - 12, 0, 520, W_SAW, 1500, 8, 140, 20, -30, 0); sfx_ex(n - 12, 0, 520, W_SAW, 1500, 8, 140, -20, 30, 0); break;   /* WOBBLE */
    case 8: sfx(n, 0, 1400, W_SINE, 2000, 160, 230); sfx(n + 7, 0, 1400, W_SINE, 1300, 200, 230); sfx(n + 12, 0, 1400, W_SINE, 1000, 240, 230); break;   /* CHOIR */
    }
}
static void pf_key_voice(int key, int32_t n) { pf_voice(pf.zone[KEY_ZONE[key]][2], n); }

/* ---------------------------------------------------------------- the tickets
 * A machine pays out when a round is over: the better it went, the more. The count is in the pocket at once; the strip
 * coming out of the machine (ac_deco.h) is only the showing of it. */
static struct { int32_t n, t; uint8_t on; col_t col; uint32_t before; } tix;
#define TIX_MAX 99
static int32_t tix_step_ms(void) { return iclamp(1500 / imax(1, tix.n), 26, 200); }   /* a ticket's time out of the slot */
static int32_t tix_feed_ms(void) { return tix.n * tix_step_ms(); }
#define TIX_TAIL 1500                            /* after the last one: the strip torn off and flown to the pocket, the sum */
static void tix_pay(int32_t n, col_t col)
{
    n = iclamp(n, 1, TIX_MAX);
    tix.before = pf.tickets;
    pf.tickets += (uint32_t)n; pf.won += (uint32_t)n; pf_dirty = 1;
    tix.n = n; tix.t = -350; tix.on = 1; tix.col = col;
#ifdef DINGHY_HOST
    printf("%6u ms  tickets: %d paid out, %u in the pocket\n", fm1_ms, n, pf.tickets);
#endif
}
/* a round on machine `which` is over: the Hat has something to say after the first, and after the second machine */
static void pf_round_done(int which) { if (!(pf.mach_done >> which & 1u)) { pf.mach_done |= (uint8_t)(1u << which); pf_dirty = 1; } }
/* what the Hat is waiting to say: 1 the tickets and EDIT, 2 the settings and a name for Rock; 0 nothing */
static int pf_hat_due(void)
{
    int n = (pf.mach_done & 1) + (pf.mach_done >> 1 & 1) + (pf.mach_done >> 2 & 1);
    if (n >= 1 && !(pf.flags & PF_TOLD_TIX)) return 1;
    if (n >= 2 && !pf_named() && !(pf.flags & PF_TOLD_NAME)) return 2;
    return 0;
}
static void pf_hat_told(int which)
{
    pf.flags |= which == 1 ? PF_TOLD_TIX : PF_TOLD_NAME; pf_dirty = 1;
#ifdef DINGHY_HOST
    printf("%6u ms  the Hat tells: %d\n", fm1_ms, which);
#endif
}

/* Bluetooth: bt_set, bt_state and bt_name are the platform's (firmware/src/bt.c on the FM-1; stand-ins in
 * tests/host.c and web/emu/web_host.c, where there is no radio and the settings do not show the line). ON or OFF is
 * kept here, PF_BT in pf.flags: on from the start (and once, on the first start after an update from a firmware where it
 * was off by default), until the player turns it off. */
