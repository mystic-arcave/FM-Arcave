/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE's tunes. Each is read off the one clock (ac_synth.h): the bar and the sixteenth come from tr_tick, so
 * every tune, on every FM-1 that shares the clock, turns its bars over at the same moment. All of them stay in the
 * notes of C major pentatonic (A minor pentatonic is the same five), so two machines on different tunes still agree.
 * Runs in the audio interrupt. */

enum { MUS_OFF, MUS_INTRO, MUS_JUKE };
static volatile uint8_t mus_mode = MUS_OFF, mus_track;

static const uint8_t SC_PEN[5] = {0, 2, 4, 7, 9}, SC_MPEN[5] = {0, 3, 5, 7, 10};
typedef struct {
    const char *name;
    uint8_t bpm, root, minor, pad, lead, arp_wave, lead_wave;
    int8_t prog[4][3];
    const char *bass, *arp, *kick, *snare, *hat;
} song_t;
#define JUKE_N 6
static const song_t SONGS[JUKE_N] = {
    /* 0: the tune the player made at the dish; its tempo and its notes come from the save */
    {"", 90, 48, 0, 26, 0, W_TRI, W_SINE, {{0, 2, 4}, {0, 2, 4}, {-2, 0, 2}, {1, 3, 5}},
     "o..o..o.o..o..q.", "................", "x.......x.......", "................", "x.x.x.x.x.x.x.x."},
    {"SPORE DRIFT", 84, 48, 0, 34, 70, W_TRI, W_SINE, {{0, 2, 4}, {0, 2, 4}, {1, 3, 5}, {0, 2, 4}},
     "o.......o.....q.", "0.1.2.1.0.1.2.3.", "................", "................", "x...x...x...x..."},
    {"CRYSTAL STEPS", 108, 45, 1, 20, 90, W_SQUARE, W_TRI, {{0, 2, 4}, {-2, 0, 2}, {1, 3, 5}, {0, 2, 4}},
     "o..o..o.o..o..q.", "0.2.1.3.0.2.1.3.", "x...x...x...x...", "....x.......x...", "x.x.x.x.x.x.x.x."},
    {"MYCELIUM NIGHTS", 96, 45, 1, 30, 110, W_TRI, W_SAW, {{0, 2, 4}, {0, 2, 4}, {-1, 1, 3}, {-2, 0, 2}},
     "o.....o...o..q..", "0..1..2.0..1..3.", "x.....x...x.....", "....x.......x..x", "..x...x...x...xx"},
    {"OPAL TIDE", 72, 48, 0, 44, 60, W_SINE, W_TRI, {{0, 2, 4}, {2, 4, 6}, {1, 3, 5}, {-1, 1, 3}},
     "o...............", "0...2...1...3...", "x...............", "........x.......", "....x.......x..."},
    {"ROLL ON", 126, 48, 0, 16, 120, W_SQUARE, W_SQUARE, {{0, 2, 4}, {1, 3, 5}, {-2, 0, 2}, {-1, 1, 3}},
     "o.o.o.q.o.o.o.q.", "0123012301230123", "x...x...x...x.x.", "....x.......x...", "x.xxx.xxx.xxx.xx"},
};

/* the player's own tune (kept in the save; ac_game.c copies it here) */
#define TAKE_MAX 48
static volatile uint8_t take_n, take_bpm = 90;
static uint8_t take_ev[TAKE_MAX][2];             /* [step of 32, key] */
/* the song's four effects, 0..8 each (KNOB 1-4 at the dish): echo, reverb, distortion, chorus */
#define TAKE_FX_MAX 8
static volatile uint8_t take_fx[4] = {4, 0, 0, 0};
/* one of the player's notes, with the effects on it. irq: from the tune (the interrupt), else from the main loop */
static void take_voice(int32_t note, int irq)
{
    uint32_t e = take_fx[0], r = take_fx[1], d = take_fx[2], c = take_fx[3], k;
    uint32_t vol = 3000u * 256u / (256u + d * 20u), send = e ? 26u + e * 26u : 0u, rs = r * 28u;
    if (c) vol = vol * 4u / 5u;
    for (k = 0; k < (c ? 3u : 1u); k++) {        /* chorus: two more of it, a little off pitch, one each side */
        int32_t det = k == 1u ? 5 + (int32_t)c * 3 : k == 2u ? -5 - (int32_t)c * 3 : 0, pan = k == 1u ? 70 : k == 2u ? -70 : 0;
        uint32_t v = k ? vol * 11u / 20u : vol;
        if (irq) sy_now_fx(note, 700, W_TRI, v, 5, send, det, pan, 0, rs, d);
        else sfx_fx(note, 0, 700, W_TRI, v, 5, send, det, pan, 0, rs, d);
    }
}

static int32_t mus_deg(const song_t *s, int32_t d)
{
    const uint8_t *sc = s->minor ? SC_MPEN : SC_PEN;
    int32_t o = d >= 0 ? d / 5 : -((4 - d) / 5);
    return s->root + sc[d - o * 5] + 12 * o;
}
static int32_t key_note(int32_t key)             /* a key of the 27 -> a note of the scale: no key is wrong */
{
    int32_t d = key * 15 / 26;
    return 48 + SC_PEN[d % 5] + 12 * (d / 5);
}

static const uint8_t INTRO_CH[4][4] = {{48, 55, 64, 71}, {45, 52, 60, 67}, {41, 48, 57, 64}, {43, 50, 59, 64}};
static const uint8_t INTRO_PENT[9] = {60, 62, 64, 67, 69, 72, 74, 76, 79};

static void mus_intro(uint32_t step)
{
    uint32_t bar = step / 32u, s = step % 32u, k;
    const uint8_t *ch = INTRO_CH[bar % 4u];
    if (s == 0u) {
        for (k = 0; k < 4u; k++) {
            sy_now(ch[k], 5600, W_SINE, 1640, 1600, 0, -5, (int32_t)k * 20 - 30, 0);
            sy_now(ch[k], 5600, W_TRI, 720, 2000, 0, 6, 30 - (int32_t)k * 20, 0);
        }
        sy_now(ch[0] - 12, 1600, W_SINE, 5200, 20, 0, 0, 0, 0);
    }
    if (s == 20u) sy_now(ch[0] - 12, 900, W_SINE, 3200, 20, 0, 0, 0, 0);
    if (s == 24u) sy_now(ch[0], 700, W_SINE, 2300, 20, 0, 0, 0, 0);
    if (!(s & 3u)) {
        uint32_t seed = (bar / 16u) * 100u + (bar % 8u) * 8u + s / 4u;
        if (ihash(seed) < 30147u)                /* 0.46 */
            sy_now(INTRO_PENT[ihash(seed + 50u) * 9u >> 16], 1100, W_SINE, 2460, 8, 150, 0, 0, 0);
    }
}

static void mus_juke(uint32_t step)
{
    const song_t *g = &SONGS[mus_track % JUKE_N];
    uint32_t bar = step / 16u, s = step % 16u, phrase = bar % 16u, k;
    const int8_t *ch = g->prog[bar % 4u];
    int quiet = phrase >= 12u && mus_track != 0u;   /* every fourth four bars: the drums rest */
    char c;
    if (s == 0u && g->pad)
        for (k = 0; k < 3u; k++) {
            uint32_t beat_ms = 60000u / (tr_bpm_q8 >> 8);
            sy_now(mus_deg(g, ch[k] + 5), beat_ms * 4u + 600u, W_SINE, g->pad * 40u, beat_ms, 40, -4, (int32_t)k * 30 - 30, 0);
            sy_now(mus_deg(g, ch[k] + 5), beat_ms * 4u + 600u, W_TRI, g->pad * 16u, beat_ms, 0, 5, 30 - (int32_t)k * 30, 0);
        }
    c = g->bass[s];
    if (c == 'o') sy_now(mus_deg(g, ch[0]) - 12, 420, W_SINE, 5200, 6, 0, 0, 0, 0);
    else if (c == 'q') sy_now(mus_deg(g, ch[2]) - 12, 300, W_SINE, 4200, 6, 0, 0, 0, 0);
    c = g->arp[s];
    if (c >= '0' && c <= '3') {
        int32_t n = c == '3' ? mus_deg(g, ch[0] + 5) : mus_deg(g, ch[c - '0']);
        if (phrase >= 8u) n += (s & 4u) ? 12 : 0;   /* the second half of the phrase climbs */
        sy_now(n + 12, 260, g->arp_wave, g->arp_wave == W_SQUARE ? 900 : 1700, 4, 90, 0, (s & 2u) ? 28 : -28, 0);
    }
    if (!quiet) {
        if (g->kick[s] == 'x') sy_now(47, 230, W_SINE, 9000, 1, 0, 0, 0, 65010);
        if (g->snare[s] == 'x') { sy_now(100, 150, W_NOISE, 2300, 1, 40, 0, 0, 0); sy_now(57, 90, W_TRI, 2000, 1, 0, 0, 0, 65200); }
        if (g->hat[s] == 'x') sy_now(0, 45, W_HAT, (s & 3u) ? 700 : 1100, 1, 0, 0, 20, 0);
        if (phrase == 15u && s >= 12u && mus_track != 0u) sy_now(100, 90, W_NOISE, 1500 + (s - 12u) * 400u, 1, 30, 0, 0, 0);   /* a fill */
    }
    if (g->lead && !(s & 1u)) {
        uint32_t seed = mus_track * 977u + (bar / 8u % 4u) * 131u + (bar % 4u) * 16u + s;
        if (ihash(seed) < (uint32_t)g->lead * 200u)
            sy_now(mus_deg(g, (int32_t)(ihash(seed + 7u) * 8u >> 16) + ch[0] % 5 + 5) + 12, 520, g->lead_wave,
                   g->lead_wave == W_SINE ? 2300 : 1000, 6, 140, 0, 0, 0);
    }
    if (mus_track == 0u) {                       /* the player's own notes: two bars, round and round */
        uint32_t s32 = step % 32u;
        for (k = 0; k < take_n; k++)
            if (take_ev[k][0] == s32) take_voice(key_note(take_ev[k][1]) + 12, 1);
    }
}

static void mus_tick(uint32_t tick)
{
    if (tick % TICKS_STEP) return;
    if (mus_mode == MUS_INTRO) mus_intro(tick / TICKS_STEP);
    else if (mus_mode == MUS_JUKE) mus_juke(tick / TICKS_STEP);
}

static uint8_t tr_follow;                        /* another FM-1 (or the Link page) leads the tempo: leave it alone */
static void mus_set(uint32_t mode, uint32_t track)
{
    mus_track = (uint8_t)track;
    mus_mode = (uint8_t)mode;
    if (!tr_follow) {
        if (mode == MUS_INTRO) tr_bpm_q8 = 120u << 8;
        else if (mode == MUS_JUKE) tr_bpm_q8 = (uint32_t)(track == 0u ? take_bpm : SONGS[track % JUKE_N].bpm) << 8;
    }
}

/* ---- little sounds, from the main loop */
static void snd_jingle(void)
{
    static const uint8_t RUN[7] = {60, 64, 67, 71, 74, 79, 83}, END[4] = {72, 76, 79, 84};
    uint32_t i;
    for (i = 0; i < 7u; i++) sfx(RUN[i], 250u + i * 170u, 1400, W_TRI, 2950, 10, 130);
    sfx(48, 200, 4200, W_SINE, 1960, 1200, 0); sfx(55, 200, 4200, W_SINE, 1960, 1200, 0); sfx(64, 200, 4200, W_SINE, 1960, 1200, 0);
    sfx(36, 200, 2500, W_SINE, 4590, 50, 0);
    sfx(91, 2760, 300, W_SINE, 2290, 5, 130); sfx(96, 2880, 500, W_SINE, 2290, 5, 130);
    for (i = 0; i < 4u; i++) sfx(END[i], 3400u + i * 30u, 2000, W_SINE, 1960, 10, 130);
}
static void snd_chime(int big)
{
    static const uint8_t B[5] = {72, 76, 79, 84, 88};
    uint32_t i;
    if (big) for (i = 0; i < 5u; i++) sfx(B[i], i * 70u, 900, W_SINE, 3270, 8, 130);
    else { sfx(76, 0, 500, W_SINE, 2950, 8, 130); sfx(79, 90, 600, W_SINE, 2950, 8, 130); }
}
static void snd_whoosh(void)
{
    static const uint8_t N[9] = {60, 64, 67, 72, 76, 79, 84, 88, 91};
    uint32_t i;
    for (i = 0; i < 9u; i++) sfx(N[i], i * 45u, 500, W_TRI, 2620, 5, 130);
}
static void snd_key(int32_t key) { sfx(key_note(key), 0, 500, W_TRI, 4900, 12, 130); }
static void snd_ui(int32_t n) { sfx(n, 0, 120, W_TRI, 2600, 3, 40); }
