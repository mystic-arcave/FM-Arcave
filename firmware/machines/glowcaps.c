/* SPDX-License-Identifier: GPL-3.0-only */
/* GLOW CAPS: the first mystic machine that is loaded, not built in (firmware/src/arcave/mm.h; docs/MACHINES.md). Small on
 * purpose: it is also the example to start a machine of your own from.
 *
 * Sixteen mushroom caps stand in a row, one for each key of the FM-1's bottom row, in the keyboard's four colours. On
 * the jukebox's beat a cap starts to glow, a beat and a half before its turn, and a ring closes on it: press its key
 * as the ring meets the cap and it sings its note of the tune, and Rock hops over to it. Let a cap's turn go by and a
 * heart is gone. They come every other beat, then every beat, then two at a time, then on the half beats. The keys'
 * own lights show the same: glowing when a cap is coming, lit when it is time. */
#include "../src/arcave/mm.h"

#define GC_NCAP 16
#define GC_LIVE 4                                /* caps that can be coming at once */
#define GC_LEAD 384                              /* a cap is shown this long before its turn (256 a beat) */
#define GC_WIN 80                                /* ... and may be pressed this long before and after it */
#define GC_NSP 12

typedef struct { int32_t at; uint8_t on, cap, rsv[2]; } gc_live_t;   /* at: the clock at its turn */
typedef struct { int16_t x, y, vx, vy, t; uint8_t col, rsv; } gc_spore_t;
typedef struct {
    gc_live_t live[GC_LIVE];
    gc_spore_t sp[GC_NSP];
    int32_t next;                                /* the clock at the next cap's turn (0: none set yet) */
    int32_t pops, rock, from, hop_t, last;       /* Rock: on which cap, hopped from which, how long ago; last: the cap before */
    int32_t pop_t[GC_NCAP];                      /* each cap: ms left of its pop */
    uint32_t seed;
} gc_t;

static const uint8_t GC_KEY[GC_NCAP] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the bottom row */
static int32_t gc_x(int32_t i) { return 15 + i * 14; }
static int32_t gc_y(int32_t i) { return 186 - i * (15 - i) / 4; }   /* a low arch: the middle caps stand higher */

static uint32_t gc_rnd(gc_t *s) { s->seed = s->seed * 1664525u + 1013904223u; return s->seed >> 16; }
static void gc_puff(gc_t *s, int32_t i, uint32_t col)
{
    int32_t k, n = 0;
    for (k = 0; k < GC_NSP && n < 4; k++)
        if (s->sp[k].t <= 0) {
            s->sp[k].x = (int16_t)(gc_x(i) * 16); s->sp[k].y = (int16_t)((gc_y(i) - 6) * 16);
            s->sp[k].vx = (int16_t)((int32_t)(gc_rnd(s) % 33u) - 16); s->sp[k].vy = (int16_t)(-20 - (int32_t)(gc_rnd(s) % 24u));
            s->sp[k].t = 600; s->sp[k].col = (uint8_t)col; n++;
        }
}
static void gc_add(gc_t *s, int32_t at)
{
    int32_t k, cap, tries = 0;
    do { cap = (int32_t)(gc_rnd(s) % GC_NCAP); } while ((cap == s->last || cap == s->rock) && ++tries < 6);
    for (k = 0; k < GC_LIVE; k++) if (s->live[k].on && s->live[k].cap == cap) return;   /* (one turn a cap at a time) */
    for (k = 0; k < GC_LIVE; k++)
        if (!s->live[k].on) { s->live[k].on = 1; s->live[k].cap = (uint8_t)cap; s->live[k].at = at; s->last = cap; return; }
}

static void gc_start(void *st, mm_ctx_t *c, const mm_api_t *a)
{
    gc_t *s = st;
    (void)a;
    s->seed = c->ms * 2654435761u + 1u; s->rock = 7; s->from = 7; s->hop_t = 1000; s->last = -1;
}
static void gc_tick(void *st, mm_ctx_t *c, const mm_api_t *a)
{
    gc_t *s = st;
    int32_t now = (int32_t)c->beat_q8, k, i, step = s->pops < 8 ? 512 : s->pops < 48 ? 256 : 128;
    if (!s->next || s->next - now > 2048 || now - s->next > 2048) {   /* the first cap, or the tune was changed: two beats on, on a beat */
        for (k = 0; k < GC_LIVE; k++) s->live[k].on = 0;
        s->next = ((now >> 8) + 3) << 8;
    }
    while (s->next - now <= GC_LEAD) {
        gc_add(s, s->next);
        if (s->pops >= 24 && s->pops < 48 && !(gc_rnd(s) & 3u)) gc_add(s, s->next);   /* now and then two together */
        s->next += step;
    }
    s->hop_t += c->dt;
    for (i = 0; i < GC_NCAP; i++) if (s->pop_t[i] > 0) s->pop_t[i] -= c->dt;
    for (k = 0; k < GC_NSP; k++)
        if (s->sp[k].t > 0) {
            s->sp[k].t = (int16_t)(s->sp[k].t - c->dt);
            s->sp[k].x = (int16_t)(s->sp[k].x + s->sp[k].vx * c->dt / 16); s->sp[k].y = (int16_t)(s->sp[k].y + s->sp[k].vy * c->dt / 16);
        }
    for (i = 0; i < GC_NCAP; i++) {              /* the keys pressed: a cap whose turn it is sings; any other key plays quietly */
        int32_t key = GC_KEY[i], hit = -1;
        if (!(c->kdown >> key & 1u)) continue;
        for (k = 0; k < GC_LIVE; k++)
            if (s->live[k].on && s->live[k].cap == i && s->live[k].at - now <= GC_WIN && now - s->live[k].at <= GC_WIN) hit = k;
        if (hit < 0) { a->sfx(a->key_note(key), 0, 160, MM_TRI, 1400, 4, 40); continue; }
        k = s->live[hit].at - now;
        s->live[hit].on = 0;
        c->score += (k < 0 ? -k : k) <= GC_WIN / 3 ? 20 : 10;
        s->pops++; s->pop_t[i] = 320;
        s->from = s->rock; s->rock = i; s->hop_t = 0;
        a->key_voice(key, a->key_note(key));
        gc_puff(s, i, (uint32_t)(MMC_ZONE0 + c->zone[key]));
        if (s->pops == 8 || s->pops == 24 || s->pops == 48) a->say(s->pops == 24 ? "TWO AT A TIME" : "FASTER");
    }
    for (k = 0; k < GC_LIVE; k++) {              /* the lights; and a turn gone by */
        int32_t d;
        if (!s->live[k].on) continue;
        d = s->live[k].at - now;
        if (d < -GC_WIN) {
            s->live[k].on = 0;
            a->sfx(43, 0, 220, MM_SAW, 2200, 2, 60);
            if (--c->lives <= 0) { a->over(); return; }
            continue;
        }
        if (d <= GC_WIN) c->led_keys |= 1u << GC_KEY[s->live[k].cap]; else c->led_glow |= 1u << GC_KEY[s->live[k].cap];
    }
}
static void gc_draw(void *st, mm_ctx_t *c, const mm_api_t *a)
{
    gc_t *s = st;
    int32_t now = (int32_t)c->beat_q8, i, k, ph = now & 255, beat = ph < 64 ? 64 - ph : 0;   /* beat: 64 on it, 0 a quarter on */
    a->alpha(256);
    a->rect(0, 0, 240, 240, c->col[MMC_BG]);
    for (i = 0; i < 26; i++) {                   /* spores hanging in the dark, each breathing in its own time */
        uint32_t h = a->hash((uint32_t)i * 97u + 5u);
        a->alpha(40 + (a->sin((int32_t)(c->ms * 9u + h)) + 16384) * 60 / 32768);
        a->rect((int32_t)(h % 240u), (int32_t)((h >> 8) % 150u), 1, 1, c->col[MMC_CAP0 + i % 5]);
    }
    a->alpha(256);
    a->rect(0, 206, 240, 34, c->col[MMC_PANEL]);
    a->line(0, 206, 239, 206, c->col[MMC_RIM]);
    for (i = 0; i < GC_NCAP; i++) {
        int32_t x = gc_x(i), y = gc_y(i), key = GC_KEY[i], r = 6, d = 9999, pop = s->pop_t[i] > 0 ? s->pop_t[i] : 0;
        mm_col_t col = c->col[MMC_ZONE0 + c->zone[key]];
        if (c->state == MM_PLAY) { for (k = 0; k < GC_LIVE; k++) if (s->live[k].on && s->live[k].cap == i) d = s->live[k].at - now; }
        else if ((int32_t)(((uint32_t)now >> 8) * 5u % GC_NCAP) == i) d = 256 - ph;   /* (under the cards: one cap after another, for show) */
        a->rect(x - 1, y, 3, 206 - y, c->col[MMC_MUTE]);                              /* its stem */
        if (d != 9999 && d > -GC_WIN) {          /* its turn is coming: brighter, larger, the ring closing on it */
            int32_t near = d < 0 ? 0 : d;
            r = 7 + (GC_LEAD - (near > GC_LEAD ? GC_LEAD : near)) * 2 / GC_LEAD;
            a->alpha(110 + (GC_LEAD - (near > GC_LEAD ? GC_LEAD : near)) * 146 / GC_LEAD);
            a->arc(x, y - 2, r + 2 + near * 22 / GC_LEAD, r + 2 + near * 22 / GC_LEAD, 0, 65536, c->col[MMC_TEXT], near <= GC_WIN);
            a->alpha(256);
        } else a->alpha(pop ? 256 : 120 + beat);
        r += pop / 60;
        a->disc(x, y, r, col);
        a->alpha(256);
        a->rect(x - r - 1, y + 1, 2 * r + 3, r + 1, c->col[MMC_BG]);                  /* (a dome: the disc's lower half goes) */
        a->rect(x - 1, y + 1, 3, r + 1, c->col[MMC_MUTE]);
        if (c->keys >> key & 1u) a->arc(x, y, r + 1, r + 1, 32768, 65536, c->col[MMC_TEXT], 0);   /* the key held: a rim of light */
    }
    for (k = 0; k < GC_NSP; k++)
        if (s->sp[k].t > 0) { a->alpha(s->sp[k].t * 256 / 600); a->rect(s->sp[k].x / 16, s->sp[k].y / 16, 2, 2, c->col[s->sp[k].col]); }
    a->alpha(256);
    {   /* Rock, on the cap he last reached: a hop of a fifth of a second from the one before */
        int32_t t = s->hop_t > 200 ? 200 : s->hop_t, x = gc_x(s->from) + (gc_x(s->rock) - gc_x(s->from)) * t / 200,
                y = gc_y(s->from) + (gc_y(s->rock) - gc_y(s->from)) * t / 200 - 16 - t * (200 - t) / 500;
        a->rock(x, y, 420, (int32_t)c->ms * 6, 256);
    }
}

static const char *const GC_HELP[7] = {
    "A CAP GLOWS BEFORE ITS TURN, AND A RING CLOSES ON IT. PRESS ITS KEY AS THE RING MEETS THE CAP: IT SINGS, AND ROCK HOPS OVER. LET A TURN GO BY AND A HEART IS GONE. THE KEYS' OWN LIGHTS SHOW WHICH.",
    "EIN HUT GLÜHT, BEVOR ER DRAN IST, UND EIN RING ZIEHT SICH ZU. DRÜCK SEINE TASTE, WENN DER RING DEN HUT TRIFFT: ER SINGT, UND ROCK HÜPFT HIN. VERPASST DU IHN, IST EIN HERZ WEG. DIE LICHTER DER TASTEN ZEIGEN, WELCHE.",
    "EN HAT GLØDER, FØR DET ER DENS TUR, OG EN RING LUKKER SIG OM DEN. TRYK PÅ DENS TANGENT, NÅR RINGEN RAMMER HATTEN: DEN SYNGER, OG ROCK HOPPER HEN. MISSER DU DEN, MISTER DU ET HJERTE. TANGENTERNES LYS VISER HVILKEN.",
    "UN CHAPEAU LUIT AVANT SON TOUR, ET UN ANNEAU SE RESSERRE. APPUIE SUR SA TOUCHE QUAND L'ANNEAU LE TOUCHE : IL CHANTE, ET ROCK SAUTE DESSUS. UN TOUR MANQUÉ, UN CŒUR PERDU. LES LUMIÈRES DES TOUCHES MONTRENT LAQUELLE.",
    "UN SOMBRERO BRILLA ANTES DE SU TURNO Y UN ANILLO SE CIERRA. PULSA SU TECLA CUANDO EL ANILLO LO TOQUE: CANTA, Y ROCK SALTA HASTA ÉL. SI SE TE PASA, PIERDES UN CORAZÓN. LAS LUCES DE LAS TECLAS DICEN CUÁL.",
    "BİR ŞAPKA SIRASI GELMEDEN PARLAR VE BİR HALKA ÜSTÜNE KAPANIR. HALKA ŞAPKAYA DEĞİNCE TUŞUNA BAS: ŞARKI SÖYLER, ROCK ÜSTÜNE ZIPLAR. KAÇIRIRSAN BİR KALP GİDER. TUŞLARIN IŞIKLARI HANGİSİ OLDUĞUNU GÖSTERİR.",
    "EEN HOED GLOEIT VOOR ZIJN BEURT EN EEN RING SLUIT ZICH EROM. DRUK OP ZIJN TOETS ALS DE RING DE HOED RAAKT: HIJ ZINGT EN ROCK SPRINGT ERHEEN. MIS JE HEM, DAN BEN JE EEN HART KWIJT. DE LICHTJES VAN DE TOETSEN WIJZEN HEM AAN.",
};
static const uint8_t GC_SIGN[32] = {
    0x07, 0xE0, 0x1F, 0xF8, 0x3F, 0xFC, 0x7B, 0xDE, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFE,
    0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x07, 0xE0, 0x07, 0xE0, 0x00, 0x00,
};
MM_MACHINE(glowcaps) = {
    MM_MAGIC, MM_API, sizeof(gc_t), 16, 1, 0, 0,
    3, 5, 40, 0,                                 /* the fourth of the five colours; five hearts; a ticket for every 40 points */
    "GLOW CAPS", GC_HELP, GC_SIGN,
    gc_start, gc_tick, gc_draw,
};
