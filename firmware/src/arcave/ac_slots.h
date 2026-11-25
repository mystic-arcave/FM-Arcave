/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE: the three ledges' slots, the music room's four, and the bank (mm.h has what a loaded machine is, mi.h a
 * loaded instrument).
 *
 * Each ledge of the Arcave has a slot. It says which of the firmware's own three machines stands there (a new install:
 * CRYSTAL ROAD, JUICE WORKS, NIGHT BLOOM, as ever), and whether the machine LOADED into that ledge's slot of the bank
 * stands there instead. The bank is 3 x 40 KiB of the FM-1's data flash (the platform's bank_at / bank_erase /
 * bank_write); the site's MYSTIC MACHINES page fills it over PLAY WITH MY FM-1's link (ac_remote.h: mb_cmd, mb_chunk,
 * mb_info below), and nothing in the game itself chooses machines. A slot whose machine is gone, damaged, built for a
 * newer firmware, or not to be run this time (the FM-1 restarted inside a loaded machine: bank_runs) shows the
 * firmware's own machine again, so a ledge is never empty and nothing loaded can keep the FM-1 from starting.
 *
 * What is checked before a loaded machine is ever run (mb_check): its first word, its length, the CRC-32 of all of it,
 * the API it was built for, the RAM it asks for, and (FM-1) that every address in its header lies inside its own
 * slot, which is also what refuses a machine the page did not move to this slot. Off the FM-1 (the browser, the
 * tests) nothing runs from the bank: every loadable machine is built in as well, and a slot's image only says which.
 *
 * The music room's four places have a slot each in the same way, after the ledges' three in the bank (a bank slot b
 * is ledge b, or place b - 3): the firmware's own instrument of that place stands there (ROOTS, THE GEODE, MUSHROOM
 * RACK, CRYSTAL SEAM), or the instrument loaded into the place's slot. What the room keeps for an instrument (its
 * layout, its laws) is kept for both: the save holds, beside the four that stand in the room, the four that do not
 * just now (is_.other), so the firmware's instrument is as the player left it when it comes back. */
#include "mm.h"
#include "mi.h"
#define MB_NSLOT (MM_NSLOT + MI_NSLOT)           /* the bank's slots: the ledges', then the room's places' */

#ifndef MM_XIP
#include "../../machines/glowcaps.c"
static const mm_head_t *const MM_TWINS[] = {&mm_machine_glowcaps};
#include "../../instruments/spring.c"
static const mi_head_t *const MI_TWINS[] = {&mi_instrument_spring};
#endif

static const char *const MACH_NAME[3] = {"CRYSTAL ROAD", "JUICE WORKS", "NIGHT BLOOM"};
#define MACH_MM 3                                /* mc.which: the machine loaded into the ledge's slot */
#define MS_LOADED 0x80u                          /* ms.slot[i]: the loaded machine stands there (else, and under it: 0..2) */
typedef struct { uint16_t id; uint8_t flags, rsv; uint32_t best; uint8_t keep[8]; } mach_rec_t;   /* what is kept for a loaded machine */
typedef struct { uint8_t ver, slot[MM_NSLOT]; mach_rec_t rec[MM_NSLOT]; } mach_save_t;             /* (a part of the save: arcave.c) */
static mach_save_t ms;
/* the room's places. slot[j]: MS_LOADED when the loaded instrument stands there. act[j]: whose state the room's save
 * holds for place j (IK_OWN + j: the firmware's; IK_LOADED | its number: a loaded one's); oid[j], other[j]: the one put
 * by (IK_NONE: none). ac_hall.h's room_unpack sorts them out */
#define IK_LOADED 0x8000u
#define IK_NONE 0xFFFFu
typedef struct { uint8_t ver, slot[MI_NSLOT], rsv[3]; uint16_t act[MI_NSLOT], oid[MI_NSLOT]; mi_inst_t other[MI_NSLOT]; } inst_save_t;
static inst_save_t is_;
enum { MBS_EMPTY, MBS_OK, MBS_OTHER, MBS_BAD };  /* a slot of the bank: nothing; a machine that runs here; one that is whole
                                                    but not for this firmware; something damaged or cut short */
static struct {
    const mm_head_t *h[MM_NSLOT];                /* the machine a slot holds, when it runs here */
    const mi_head_t *ih[MI_NSLOT];               /* the instrument a place's slot holds, when it runs here */
    uint32_t len[MB_NSLOT];
    uint16_t id[MB_NSLOT], ver[MB_NSLOT];
    uint8_t st[MB_NSLOT];
    uint8_t cur, help, over;                     /* being played: its slot; HOME's words up; it said its game is over */
    uint32_t off;                                /* how far its clock is behind the jukebox's (the game was held) */
    const char *say;
    mm_ctx_t c;
    mm_col_t col[MMC_N];
} mb;
static struct { uint8_t open, only, slot, erase, ask; uint32_t off; } mbw;   /* a slot being written from the page */
static uint8_t mb_infob[16 + 32 * MB_NSLOT] AC_POOL;
static void save_now(void);

static uint32_t mm_rd(const uint8_t *p, uint32_t n) { uint32_t v = 0; while (n--) v = v << 8 | p[n]; return v; }
static uint32_t mm_crc_run(uint32_t c, const uint8_t *p, uint32_t n)
{
    uint32_t k;
    while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return c;
}
#ifdef MM_XIP
static int mb_in(const void *q, const uint8_t *p, uint32_t size, uint32_t need)
{
    uintptr_t a = (uintptr_t)q, lo = (uintptr_t)p;
    return a >= lo + sizeof(mm_head_t) && a <= lo + size - need;
}
#endif
/* bank slot b: its first word, its length, its CRC, the API and the RAM it asks for. Its bytes when all of that is
 * right for this firmware (mb.st[b] is MBS_OTHER until the caller has looked at the rest), else 0 */
static const uint8_t *mb_whole(uint32_t b, uint32_t magic, uint32_t api_max, uint32_t state_max, uint32_t head)
{
    const uint8_t *p = bank_at(b * MM_SLOT);
    uint32_t size, api;
    mb.st[b] = MBS_EMPTY; mb.len[b] = 0; mb.id[b] = 0; mb.ver[b] = 0;
    if (!p || mm_rd(p, 4) != magic) return 0;
    mb.st[b] = MBS_BAD;
    size = mm_rd(p + 12, 4);
    if (size < MM_HEAD + 20u || size > MM_SLOT) return 0;
    if (~mm_crc_run(mm_crc_run(0xFFFFFFFFu, p, 16), p + MM_HEAD, size - MM_HEAD) != mm_rd(p + 16, 4)) return 0;
    mb.st[b] = MBS_OTHER; mb.len[b] = size; mb.id[b] = (uint16_t)mm_rd(p + 8, 2); mb.ver[b] = (uint16_t)mm_rd(p + 10, 2);
    api = mm_rd(p + 4, 2);
    if (!api || api > api_max || mm_rd(p + 6, 2) > state_max) return 0;
#ifdef MM_XIP
    if (size < head + 8u || !mm_rd(p + 6, 2)) return 0;                       /* (a file for the browser game: no code in it) */
#else
    (void)head;
#endif
    return p;
}
#ifdef MM_XIP
static int mb_name_ok(const char *n)
{
    uint32_t k;
    for (k = 0; k < 21u && n[k]; k++) if (n[k] < ' ' || n[k] > 'Z') return 0;
    return k && k <= 20u;
}
#endif
static void mb_check(uint32_t i)
{
    const uint8_t *p = mb_whole(i, MM_MAGIC, MM_API, MM_STATE_MAX, sizeof(mm_head_t));
    uint32_t k;
    mb.h[i] = 0;
    if (!p) return;
#ifdef MM_XIP
    {
        const mm_head_t *h = (const mm_head_t *)(const void *)p;
        uint32_t size = mb.len[i];
        if (!mb_in(h->name, p, size, 2) || !mb_in(h->help, p, size, 7u * sizeof(char *)) || !mb_in(h->sign, p, size, 32)
            || !mb_in((const void *)(uintptr_t)h->start, p, size, 2) || !mb_in((const void *)(uintptr_t)h->tick, p, size, 2)
            || !mb_in((const void *)(uintptr_t)h->draw, p, size, 2)) return;
        if (!mb_in(h->help[0], p, size, 2)) return;
        for (k = 1; k < 7u; k++) if (h->help[k] && !mb_in(h->help[k], p, size, 2)) return;
        if (!mb_name_ok(h->name)) return;
        mb.h[i] = h;
    }
#else
    for (k = 0; k < sizeof MM_TWINS / sizeof MM_TWINS[0]; k++) if (MM_TWINS[k]->id == mb.id[i]) mb.h[i] = MM_TWINS[k];
    if (!mb.h[i]) return;
#endif
    mb.st[i] = MBS_OK;
}
/* the instrument in place j's slot (bank slot MM_NSLOT + j). Every address its header holds must lie inside its own
 * slot, and its counts be ones the room's controls can take */
static void ib_check(uint32_t j)
{
    uint32_t b = MM_NSLOT + j, k;
    const uint8_t *p = mb_whole(b, MI_MAGIC, MI_API, MI_STATE_MAX, sizeof(mi_head_t));
    const mi_head_t *h = 0;
    mb.ih[j] = 0;
    if (!p) return;
#ifdef MM_XIP
    {
        uint32_t size = mb.len[b];
        const void *const *f;
        h = (const mi_head_t *)(const void *)p;
        if (!mb_in(h->name, p, size, 2) || !mb_in(h->shortn, p, size, 2) || !mb_in(h->tag, p, size, 2) || !mb_in(h->seln, p, size, 2)
            || !mb_in(h->seqn, p, size, 2) || !mb_in(h->kinds, p, size, sizeof(char *)) || !mb_in(h->ksnd, p, size, 1)
            || !mb_in(h->presets, p, size, sizeof(char *)) || !mb_in(h->algos, p, size, sizeof(char *))) return;
        for (k = 0; k < 4u; k++) if (!mb_in(h->kn[k], p, size, 2) || !mb_in(h->sqn[k], p, size, 2)) return;
        if (h->nstep && (!mb_in(h->steps, p, size, sizeof(mi_step_t)) || !mb_in((const void *)(uintptr_t)h->lesson, p, size, 2))) return;
        if (h->ntopic && !mb_in(h->topics, p, size, sizeof(mi_topic_t))) return;
        f = (const void *const *)(const void *)&h->init;                      /* its nine functions, one after another in the header */
        for (k = 0; k < 9u; k++) if (!mb_in(f[k], p, size, 2)) return;
        if (!mb_name_ok(h->name)) return;
    }
#else
    for (k = 0; k < sizeof MI_TWINS / sizeof MI_TWINS[0]; k++) if (MI_TWINS[k]->id == mb.id[b]) h = MI_TWINS[k];
    if (!h) return;
#endif
    if (h->nkind < 2u || h->nkind > 8u || !h->npreset || !h->nalgo || h->nstep > 16u || h->ntopic > 8u || (h->id & IK_LOADED)) return;
    for (k = 0; k < h->ntopic; k++) if (!h->topics[k].n || h->topics[k].first + h->topics[k].n > h->nstep) return;
    mb.ih[j] = h;
    mb.st[b] = MBS_OK;
}
/* the slots as the save had them, made sound. What is kept for a loaded machine (ms.rec) is left alone here: a save
 * may be played where its machines are not (PLAY WITH MY FM-1: the browser's bank is its own) and go home again */
static void ms_sane(void)
{
    uint32_t i, k;
    if (ms.ver != 1u) {
        for (k = 0; k < sizeof ms; k++) ((uint8_t *)&ms)[k] = 0;
        ms.ver = 1;
        for (i = 0; i < MM_NSLOT; i++) ms.slot[i] = (uint8_t)i;
    }
    for (i = 0; i < MM_NSLOT; i++) if ((ms.slot[i] & 0x7Fu) > 2u) ms.slot[i] = (uint8_t)((ms.slot[i] & MS_LOADED) | i);
}
/* ... it goes when another machine than the one it was kept for is in the slot: when one is loaded, or walked up to */
static void ms_rec_for(uint32_t i)
{
    uint32_t k;
    if (ms.rec[i].id == mb.id[i]) return;
    for (k = 0; k < sizeof ms.rec[i]; k++) ((uint8_t *)&ms.rec[i])[k] = 0;
    ms.rec[i].id = mb.id[i];
}
static void is_sane(void)
{
    uint32_t j, k;
    if (is_.ver != 1u) {
        for (k = 0; k < sizeof is_; k++) ((uint8_t *)&is_)[k] = 0;
        is_.ver = 1;
        for (j = 0; j < MI_NSLOT; j++) { is_.act[j] = (uint16_t)j; is_.oid[j] = IK_NONE; }
    }
    for (j = 0; j < MI_NSLOT; j++) is_.slot[j] &= MS_LOADED;
}
static void mb_check_all(void) { uint32_t i; for (i = 0; i < MM_NSLOT; i++) mb_check(i); for (i = 0; i < MI_NSLOT; i++) ib_check(i); ms_sane(); is_sane(); }

/* a place of the music room: is it the loaded instrument that stands there; whose state that is (is_.act's kind) */
static int place_loaded(int j) { return (is_.slot[j] & MS_LOADED) && mb.st[MM_NSLOT + j] == MBS_OK && bank_runs(); }
static uint32_t place_kind(int j) { return place_loaded(j) ? IK_LOADED | mb.id[MM_NSLOT + j] : (uint32_t)j; }

/* a ledge: is it the loaded machine that stands there; else which of the firmware's own; its name and its colour */
static int ledge_loaded(int i) { return (ms.slot[i] & MS_LOADED) && mb.st[i] == MBS_OK && bank_runs(); }
static int ledge_which(int i) { return ms.slot[i] & 3u; }
static const char *ledge_name(int i) { return ledge_loaded(i) ? mb.h[i]->name : MACH_NAME[ledge_which(i)]; }
static int ledge_cap(int i) { return ledge_loaded(i) ? mb.h[i]->cap % 5 : ledge_which(i) * 2 % 5; }

/* a loaded machine in the cave: one cabinet for them all, an upright case on a foot, the machine's own sign on its
 * screen in the machine's colour, two knobs and a slot of light under it. Dark (before the power): its outline */
static void mm_cabinet(int32_t x, int32_t y, const mm_head_t *h, int on, int32_t lit, uint32_t tm)
{
    col_t c = on ? th->caps[h->cap % 5u] : th->rim;
    int32_t r, k;
    galpha(lit);
    grect(x - 14, y - 50, 28, 46, th->panel); gbox(x - 14, y - 50, 28, 46, c);
    grect(x - 18, y - 5, 36, 5, th->panel); gbox(x - 18, y - 5, 36, 5, c);
    gbox(x - 10, y - 46, 20, 20, on ? th->text : th->rim);
    if (on)
        for (r = 0; r < 16; r++) {
            uint32_t bits = (uint32_t)h->sign[2 * r] << 8 | h->sign[2 * r + 1];
            for (k = 0; k < 16; k++) if (bits >> (15 - k) & 1u) gp(x - 8 + k, y - 44 + r, c);
        }
    gdisc(x - 6, y - 17, 2, c); gdisc(x + 6, y - 17, 2, c);
    if (on) { galpha(lit * (110 + ipulse(tm, 1300) * 146 / 256) / 256); grect(x - 8, y - 11, 16, 2, c); }
    galpha(256);
}

/* ---------------------------------------------------------------- the bank, from the page (ac_remote.h)
 * mb_cmd: a command (CC 61). Its answer (CC 62) at once, or -1: it comes when the work is done (mb_poll)
 *    0             (answered by ac_remote.h with the bank's state, mb_info)
 *   16 + slot      a machine is coming for the slot: it is erased, then its pieces are taken (mb_chunk), in order
 *   32 + slot      that was all of it: checked; whole and for this firmware, it stands on its ledge from now on
 *   48 + 4 * slot + w   the ledge's slot: w 0..2 the firmware's own machine that stands there; 3 the loaded one again
 *   64 + slot      the slot emptied
 *   80 + place, 84 + place, 88 + place   the same three for a place of the music room: an instrument is coming; that
 *                  was all of it; the place's slot emptied
 *   96 + 2 * place + w   the place: w 0 the firmware's own instrument stands there, 1 the loaded one again
 * answers: 0 done, 1 not now or no such thing, 2 damaged on the way (nothing stands there), 3 the flash would not,
 * 4 whole but not a machine for this firmware (a newer API, or a file for the browser game) */
static void room_rekit(void);                    /* (ac_hall.h: what stands in the room's places changed) */
static int mb_cmd(uint32_t v)
{
    uint32_t s = v & 15u, op = v >> 4;
    if (v >= 96u) {                              /* a place of the room: the firmware's own, or the loaded one */
        s = (v - 96u) >> 1;
        if (s >= MI_NSLOT) return 1;
        if (v & 1u) { if (mb.st[MM_NSLOT + s] != MBS_OK) return 1; is_.slot[s] = MS_LOADED; }
        else is_.slot[s] = 0;
        room_rekit(); save_now();
        return 0;
    }
    if (v >= 80u) {                              /* a place's slot of the bank: as a ledge's, MM_NSLOT further on */
        s = MM_NSLOT + ((v - 80u) & 3u); op = v < 84u ? 1u : v < 88u ? 2u : v < 92u ? 4u : 0u;
        if (!op) return 1;
    } else if (s >= MM_NSLOT && op != 3u) return 1;
    if (op == 3u) {
        s = (v - 48u) >> 2;
        if (s >= MM_NSLOT) return 1;
        if ((v & 3u) == 3u) { if (mb.st[s] != MBS_OK) return 1; ms.slot[s] |= MS_LOADED; }
        else ms.slot[s] = (uint8_t)(v & 3u);
        save_now();
        return 0;
    }
    if (s >= MB_NSLOT || !bank_at(0)) return 1;
    if (op == 1u || op == 4u) {
        int was = s >= MM_NSLOT && place_loaded((int)(s - MM_NSLOT));
        mbw.open = 1; mbw.only = op == 4u; mbw.slot = (uint8_t)s; mbw.erase = 0; mbw.off = 0; mbw.ask = 1;
        if (s < MM_NSLOT) {
            mb.h[s] = 0;
            if (ms.slot[s] & MS_LOADED) { ms.slot[s] &= (uint8_t)~MS_LOADED; save_now(); }
        } else {                                 /* (the firmware's own instrument stands there from now, before a byte of its slot goes) */
            is_.slot[s - MM_NSLOT] = 0;
            if (was) { room_rekit(); save_now(); }
            mb.ih[s - MM_NSLOT] = 0;
        }
        mb.st[s] = MBS_EMPTY; mb.len[s] = 0; mb.id[s] = 0;
        return -1;
    }
    if (op == 2u) {
        if (!mbw.open || mbw.slot != s || mbw.erase < MM_SLOT / 0x1000u) return 1;
        mbw.open = 0;
        if (s < MM_NSLOT) mb_check(s); else ib_check(s - MM_NSLOT);
        if (mb.st[s] != MBS_OK) return mb.st[s] == MBS_OTHER ? 4 : 2;
        if (s < MM_NSLOT) { ms.slot[s] |= MS_LOADED; ms_rec_for(s); }
        else { is_.slot[s - MM_NSLOT] = MS_LOADED; room_rekit(); }
        save_now();
        return 0;
    }
    return 1;
}
/* a piece of the machine being sent (they come in order): 1 kept, 3 not */
static int mb_chunk(uint32_t s, const uint8_t *p, uint32_t n)
{
    if (!mbw.open || mbw.only || mbw.slot != s || mbw.erase < MM_SLOT / 0x1000u || !n || mbw.off + n > MM_SLOT) return 3;
    if (bank_write(s * MM_SLOT + mbw.off, p, n)) return 3;
    mbw.off += n;
    return 1;
}
/* every pass while a slot is being emptied: a sector at a time (50 ms each on the FM-1), then the answer */
static void mb_poll(void)
{
    if (!mbw.ask) return;
    if (mbw.erase < MM_SLOT / 0x1000u) {
        if (bank_erase(mbw.slot * MM_SLOT + mbw.erase * 0x1000u)) { mbw.ask = 0; mbw.open = 0; midi_send(0xBFu, 62u, 3u); return; }
        mbw.erase++;
        return;
    }
    mbw.ask = 0;
    if (mbw.only) mbw.open = 0;
    midi_send(0xBFu, 62u, 0u);
}
/* the bank's state for the page: 2, the machines' API, the ledges, a slot's KiB, the RAM a machine may ask for / 256,
 * loaded things run (0: not this time), there is a bank, the room's places, the instruments' API, the RAM one may
 * ask for / 256, six zeros; then 32 bytes a bank slot (the ledges', then the places'): its ms.slot or is_.slot, its
 * MBS_..., the number and version of what it holds (two bytes each, low first), its length (three), 0, its name (up
 * to 20 letters, then zeros) */
static uint32_t mb_info(void)
{
    uint8_t *o = mb_infob;
    uint32_t i, k;
    for (i = 0; i < sizeof mb_infob; i++) o[i] = 0;
    o[0] = 2; o[1] = (uint8_t)MM_API; o[2] = (uint8_t)MM_NSLOT; o[3] = (uint8_t)(MM_SLOT / 1024u); o[4] = (uint8_t)(MM_STATE_MAX / 256u);
    o[5] = (uint8_t)bank_runs(); o[6] = bank_at(0) != 0; o[7] = (uint8_t)MI_NSLOT; o[8] = (uint8_t)MI_API; o[9] = (uint8_t)(MI_STATE_MAX / 256u);
    for (i = 0; i < MB_NSLOT; i++) {
        uint8_t *q = o + 16u + 32u * i;
        const char *nm = i < MM_NSLOT ? (mb.h[i] ? mb.h[i]->name : 0) : mb.ih[i - MM_NSLOT] ? mb.ih[i - MM_NSLOT]->name : 0;
        q[0] = i < MM_NSLOT ? ms.slot[i] : is_.slot[i - MM_NSLOT]; q[1] = mb.st[i]; q[2] = (uint8_t)mb.id[i]; q[3] = (uint8_t)(mb.id[i] >> 8);
        q[4] = (uint8_t)mb.ver[i]; q[5] = (uint8_t)(mb.ver[i] >> 8);
        q[6] = (uint8_t)mb.len[i]; q[7] = (uint8_t)(mb.len[i] >> 8); q[8] = (uint8_t)(mb.len[i] >> 16);
        if (nm) for (k = 0; k < 20u && nm[k]; k++) q[10u + k] = (uint8_t)nm[k];
    }
    return sizeof mb_infob;
}
#ifdef AC_TEST_MOD                               /* (tests) GLOW CAPS as if loaded into slot AC_TEST_MOD - 1: a header and no code */
static void mb_test_load(void)
{
    static uint8_t img[40];
    uint32_t i, c;
    static const uint8_t HEAD[12] = {0x4D, 0x4D, 0x43, 0x48, (uint8_t)MM_API, 0, 0, 0, 16, 0, 1, 0};
    for (i = 0; i < 12u; i++) img[i] = HEAD[i];
    img[12] = sizeof img;
    c = ~mm_crc_run(mm_crc_run(0xFFFFFFFFu, img, 16), img + MM_HEAD, sizeof img - MM_HEAD);
    for (i = 0; i < 4u; i++) img[16u + i] = (uint8_t)(c >> (8u * i));
    for (i = 0; i < MM_SLOT / 0x1000u; i++) bank_erase((AC_TEST_MOD - 1u) * MM_SLOT + i * 0x1000u);
    bank_write((AC_TEST_MOD - 1u) * MM_SLOT, img, sizeof img);
}
#endif
#ifdef AC_TEST_INS                               /* (tests) THE SPRING as if loaded into place AC_TEST_INS - 1 of the music room */
static void ib_test_load(void)
{
    static uint8_t img[40];
    uint32_t i, c, b = MM_NSLOT + AC_TEST_INS - 1u;
    static const uint8_t HEAD[12] = {0x4D, 0x49, 0x4E, 0x53, (uint8_t)MI_API, 0, 0, 0, 16, 0, 1, 0};
    for (i = 0; i < 12u; i++) img[i] = HEAD[i];
    img[12] = sizeof img;
    c = ~mm_crc_run(mm_crc_run(0xFFFFFFFFu, img, 16), img + MM_HEAD, sizeof img - MM_HEAD);
    for (i = 0; i < 4u; i++) img[16u + i] = (uint8_t)(c >> (8u * i));
    for (i = 0; i < MM_SLOT / 0x1000u; i++) bank_erase(b * MM_SLOT + i * 0x1000u);
    bank_write(b * MM_SLOT, img, sizeof img);
}
#endif
