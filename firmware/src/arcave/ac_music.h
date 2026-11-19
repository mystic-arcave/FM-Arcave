/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's tunes. Each is read off the one clock (ac_synth.h): the bar and the sixteenth come from tr_tick, so
 * every tune, on every FM-1 that shares the clock, turns its bars over at the same moment. Each jukebox tune has its
 * own key (SONG_KEY), and the keyboard plays in it (key_play). With LINK on, every tune goes back to the one shared
 * key, C major pentatonic (A minor pentatonic is the same five), so FM-1s on different tunes still agree. The little
 * sounds of the games stay in C major pentatonic, which sits inside every one of the keys. Runs in the audio interrupt. */

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

/* each tune's own key (its root; SONGS[].root is its place in the shared key, used with LINK on): the player's own and
 * SPORE DRIFT in C major, CRYSTAL STEPS in A minor, MYCELIUM NIGHTS in D minor, OPAL TIDE in F major, ROLL ON in G major */
static const uint8_t SONG_KEY[JUKE_N] = {48, 48, 45, 50, 53, 55};
static volatile uint8_t mus_shared;              /* LINK is on: every tune in the shared key (arcave.c sets it) */
static int32_t song_root(const song_t *s) { int32_t t = (int32_t)(s - SONGS); return mus_shared ? s->root : SONG_KEY[t]; }

/* the player's own tune (kept in the save; ac_game.c copies it here) */
#define TAKE_MAX 48
static volatile uint8_t take_n, take_bpm = 90;
static uint8_t take_ev[TAKE_MAX][2];             /* [step of 32, key] */
/* the song's four effects, 0..8 each (KNOB 1-4 at the dish): echo, reverb, distortion, chorus */
#define TAKE_FX_MAX 8
static volatile uint8_t take_fx[4] = {4, 0, 0, 0};
/* one of the player's notes, with the effects on it. irq: from the tune (the interrupt), else from the main loop, after
 * delay ms */
static void take_voice_at(int32_t note, int irq, uint32_t delay)
{
    uint32_t e = take_fx[0], r = take_fx[1], d = take_fx[2], c = take_fx[3], k;
    uint32_t vol = 3000u * 256u / (256u + d * 20u), send = e ? 26u + e * 26u : 0u, rs = r * 28u;
    if (c) vol = vol * 4u / 5u;
    for (k = 0; k < (c ? 3u : 1u); k++) {        /* chorus: two more of it, a little off pitch, one each side */
        int32_t det = k == 1u ? 5 + (int32_t)c * 3 : k == 2u ? -5 - (int32_t)c * 3 : 0, pan = k == 1u ? 70 : k == 2u ? -70 : 0;
        uint32_t v = k ? vol * 11u / 20u : vol;
        if (irq) sy_now_fx(note, 700, W_TRI, v, 5, send, det, pan, 0, rs, d);
        else sfx_fx(note, delay, 700, W_TRI, v, 5, send, det, pan, 0, rs, d);
    }
}
static void take_voice(int32_t note, int irq) { take_voice_at(note, irq, 0); }

static int32_t mus_deg(const song_t *s, int32_t d)
{
    const uint8_t *sc = s->minor ? SC_MPEN : SC_PEN;
    int32_t o = d >= 0 ? d / 5 : -((4 - d) / 5);
    return song_root(s) + sc[d - o * 5] + 12 * o;
}
static int32_t key_note(int32_t key)             /* a key of the 27 -> a note of C major pentatonic: the player's own
                                                    song, recorded and played back in it (saved songs keep their notes) */
{
    int32_t d = key * 15 / 26;
    return 48 + SC_PEN[d % 5] + 12 * (d / 5);
}
/* a key of the 27 -> a note of the tune that is playing, in its own key: the lowest key its home note (around C3), each
 * key to the right the same or the next note up, over three octaves; no key is wrong. No tune playing (the intro, the
 * dark cave): C major pentatonic */
static int32_t key_play(int32_t key)
{
    const song_t *s = &SONGS[mus_mode == MUS_JUKE ? mus_track % JUKE_N : 0];
    int32_t r = song_root(s), base = 48 + ((r - 48) % 12 + 12) % 12, d = key * 15 / 26;
    const uint8_t *sc = s->minor ? SC_MPEN : SC_PEN;
    if (base > 54) base -= 12;
    return base + sc[d % 5] + 12 * (d / 5);
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

/* The tune's two melodic parts, a step at a time: the arpeggio and the lead line. Each returns the note's scale step
 * (for mus_deg) or MUS_NONE. mus_juke plays them, and CRYSTAL ROAD (ac_mach.h) reads its chart from them, so the two
 * can never drift apart. */
#define MUS_NONE (-99)
static int32_t mus_arp_dd(const song_t *g, uint32_t step)
{
    uint32_t bar = step / 16u, s = step % 16u;
    const int8_t *ch = g->prog[bar % 4u];
    char c = g->arp[s];
    int32_t d;
    if (c < '0' || c > '3') return MUS_NONE;
    d = c == '3' ? ch[0] + 5 : ch[c - '0'];
    if (bar % 16u >= 8u && (s & 4u)) d += 5;     /* the second half of the phrase climbs */
    return d;
}
static int32_t mus_lead_dd(const song_t *g, uint32_t track, uint32_t step)
{
    uint32_t bar = step / 16u, s = step % 16u, seed;
    const int8_t *ch = g->prog[bar % 4u];
    if (!g->lead || (s & 1u)) return MUS_NONE;
    seed = track * 977u + (bar / 8u % 4u) * 131u + (bar % 4u) * 16u + s;
    if (ihash(seed) >= (uint32_t)g->lead * 200u) return MUS_NONE;
    return (int32_t)(ihash(seed + 7u) * 8u >> 16) + ch[0] % 5 + 5;
}

/* CRYSTAL ROAD plays a song from its own bar 0: mus_step0 is the step it starts on (a multiple of 16, so the bar lines
 * stay where LINK has them). While mus_solo is set (the notes caught at a time: 4 one a beat, 2 a half beat, 1 all),
 * the song's chart notes are left out of the tune for the player's keys to put back. mus_chart holds the chart: a
 * step's scale step on the keyboard + 1 (0: no note), for the song's mus_chart_n steps */
#define MUS_CHART_MAX 512
static volatile uint32_t mus_step0;
static volatile uint8_t mus_solo;
static volatile uint8_t mus_demo;                /* the start card: the chart played by the tune itself, perfectly, round and round */
static uint8_t mus_chart[MUS_CHART_MAX];
static volatile uint16_t mus_chart_n;
/* the demo loops the whole song, the tune under it too, with a bar between (the drums rest, then a fill: a count-in):
 * step ss -> its step in the song (mus_chart_n and over: that bar between) */
static uint32_t mus_cs(uint32_t ss)
{
    uint32_t n = mus_chart_n;
    if (!mus_demo || !n) return ss;
    if ((int32_t)ss < 0) return (int32_t)ss >= -16 ? n + 16u - (uint32_t)(-(int32_t)ss) : n;   /* (before its first bar: the bar between) */
    return ss % (n + 16u);
}
static int mus_charted(uint32_t ss)              /* step ss of the song is the player's to play */
{
    ss = mus_cs(ss);
    return mus_solo && ss < mus_chart_n && mus_chart[ss];
}
static int mus_in_song(uint32_t ss) { return mus_solo && (mus_demo || mus_cs(ss) < mus_chart_n); }   /* a CRYSTAL ROAD song (or its demo) is under way */
static int32_t mus_take_first(uint32_t s32)      /* the player's own song: its first note at this step of 32, or -1 */
{
    uint32_t k;
    for (k = 0; k < take_n; k++) if (take_ev[k][0] == s32) return (int32_t)k;
    return -1;
}

/* ---- CRYSTAL ROAD's melodies: one written for each jukebox tune, to be played on the keys. A bar is 16 steps; each
 * note is a step of the keyboard's ladder (key_play: 0..f, '.' a rest), so the key pressed is the note heard. A hook,
 * the hook again with a turn, (eight bars) something new, the hook to close. Over the tune's own chords */
static const char *const MELODY[JUKE_N][8] = {
    {0},
    {"a.a.9.7.8...5...", "a.a.9.7.c...a...", "b.b.a.8.6...8...", "a.9.7.8.a......."},                          /* SPORE DRIFT */
    {"a.a.c.a.9.8.a...", "8.9.a.9.8.6.5...", "a.a.c.a.d.c.a...", "b.a.9.8.9...a...",                              /* CRYSTAL STEPS */
     "c.cd.c.a.c.d.e..", "d.c.a.9.a.8.9...", "a.a.c.a.9.8.a...", "8.9ab.a.9.8.a..."},
    {"8..8..9.a...8.6.", "5..6..8.7.......", "9..9..b.a...9.6.", "8..7..5.3.......",                              /* MYCELIUM NIGHTS */
     "8..8..9.a...d.c.", "a..b..a.8.......", "9..9..b.c.b.9.6.", "8.7.5.6.5......."},
    {"7...8.9.a...8...", "9...a.b.c...9...", "8...7.6.5...6...", "7...6...5......."},                          /* OPAL TIDE */
    {"5.7.8.7.a...8...", "6.8.9.8.b...9...", "3.5.7.5.8...7...", "4.6.8.6.9.8.6.4.",                              /* ROLL ON */
     "5.78.7.a.a.8.7..", "6.89.8.b.b.9.8..", "3.57.5.8.8.7.5..", "4.6.8.9.a...a..."},
};
static uint32_t mus_song_bars(uint32_t track)    /* 4 bars if slower than 90 BPM, 8 if not */
{
    uint32_t bpm = track % JUKE_N == 0u ? take_bpm : SONGS[track % JUKE_N].bpm;
    return bpm >= 90u ? 8u : 4u;
}
static uint32_t mus_song_steps(uint32_t track) { return mus_song_bars(track) * 16u; }
static int32_t mus_ladder(const song_t *g, int32_t d)   /* a step of the ladder -> its note, in the tune's own key */
{
    int32_t r = song_root(g), base = 48 + ((r - 48) % 12 + 12) % 12;
    const uint8_t *sc = g->minor ? SC_MPEN : SC_PEN;
    if (base > 54) base -= 12;
    return base + sc[d % 5] + 12 * (d / 5);
}
/* a song's note at step ss: its step on the ladder (0..15), or MUS_NONE. pitch: the note as it sounds (an octave over
 * the key); part: 1 a melody, 2 the player's own song */
static int32_t mus_note_at(uint32_t track, uint32_t ss, int32_t *pitch, int *part)
{
    const song_t *g = &SONGS[track % JUKE_N];
    const char *bar;
    char c;
    int32_t d;
    if (track % JUKE_N == 0u) {
        int32_t k = mus_take_first(ss % 32u), key;
        if (k < 0) return MUS_NONE;
        key = take_ev[k][1];
        *pitch = key_note(key) + 12; *part = 2;
        return key * 15 / 26;
    }
    bar = MELODY[track % JUKE_N][(ss / 16u) % mus_song_bars(track)];
    c = bar ? bar[ss % 16u] : '.';
    d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : MUS_NONE;
    if (d == MUS_NONE) return MUS_NONE;
    *pitch = mus_ladder(g, d) + 12; *part = 1;
    return d;
}
/* the melody's own voice: a bright square with a bell an octave over it, nearly dry (a little echo blurs the beat),
 * so it stands out of the mix. irq: from the tune (the interrupt); vol: 256 a note the player caught, less one the tune
 * plays for them; delay: ms to wait (a key pressed a little early sounds on the beat) */
static void mus_mel_voice(int32_t p, int irq, uint32_t vol, uint32_t delay)
{
    if (irq) { sy_now(p, 340, W_SQUARE, 2800u * vol / 256u, 2, 24, 0, 0, 0); sy_now(p + 12, 260, W_SINE, 1500u * vol / 256u, 2, 30, 0, 0, 0); }
    else { sfx(p, delay, 340, W_SQUARE, 2800u * vol / 256u, 2, 24); sfx(p + 12, delay, 260, W_SINE, 1500u * vol / 256u, 2, 30); }
}

static void mus_juke(uint32_t step)
{
    const song_t *g = &SONGS[mus_track % JUKE_N];
    uint32_t bar, s, phrase, k, ss = step - mus_step0;
    const int8_t *ch;
    int quiet, solo, insong;
    int32_t d;
    char c;
    step = ss;                                   /* (the tune counts its bars from mus_step0: wrapping keeps them whole) */
    if (mus_demo && mus_chart_n) {               /* the demo: the tune goes round with the song, the bar between it the tune's last */
        uint32_t c = mus_cs(ss);
        step = c < mus_chart_n ? c : 15u * 16u + (c - mus_chart_n);
    }
    bar = step / 16u; s = step % 16u; phrase = bar % 16u; ch = g->prog[bar % 4u];
    quiet = phrase >= 12u && mus_track != 0u;    /* every fourth four bars: the drums rest */
    solo = mus_charted(ss);
    insong = mus_in_song(ss);                    /* (the melody is the player's: the tune's own lead rests, the arpeggio steps back) */
    if (s == 0u && g->pad)
        for (k = 0; k < 3u; k++) {
            uint32_t beat_ms = 60000u / (tr_bpm_q8 >> 8);
            sy_now(mus_deg(g, ch[k] + 5), beat_ms * 4u + 600u, W_SINE, g->pad * 40u, beat_ms, 40, -4, (int32_t)k * 30 - 30, 0);
            sy_now(mus_deg(g, ch[k] + 5), beat_ms * 4u + 600u, W_TRI, g->pad * 16u, beat_ms, 0, 5, 30 - (int32_t)k * 30, 0);
        }
    c = g->bass[s];
    if (c == 'o') sy_now(mus_deg(g, ch[0]) - 12, 420, W_SINE, 5200, 6, 0, 0, 0, 0);
    else if (c == 'q') sy_now(mus_deg(g, ch[2]) - 12, 300, W_SINE, 4200, 6, 0, 0, 0, 0);
    d = mus_arp_dd(g, step);
    if (d != MUS_NONE)
        sy_now(mus_deg(g, d) + 12, 260, g->arp_wave, (g->arp_wave == W_SQUARE ? 900u : 1700u) * (insong ? 5u : 10u) / 10u, 4, 90, 0, (s & 2u) ? 28 : -28, 0);
    if (!quiet) {
        if (g->kick[s] == 'x') sy_now(47, 230, W_SINE, 9000, 1, 0, 0, 0, 65010);
        if (g->snare[s] == 'x') { sy_now(100, 150, W_NOISE, 2300, 1, 40, 0, 0, 0); sy_now(57, 90, W_TRI, 2000, 1, 0, 0, 0, 65200); }
        if (g->hat[s] == 'x') sy_now(0, 45, W_HAT, (s & 3u) ? 700 : 1100, 1, 0, 0, 20, 0);
        if (phrase == 15u && s >= 12u && mus_track != 0u) sy_now(100, 90, W_NOISE, 1500 + (s - 12u) * 400u, 1, 30, 0, 0, 0);   /* a fill */
    }
    d = mus_lead_dd(g, mus_track, step);
    if (d != MUS_NONE && !insong)
        sy_now(mus_deg(g, d) + 12, 520, g->lead_wave, g->lead_wave == W_SINE ? 2300 : 1000, 6, 140, 0, 0, 0);
    if (solo && mus_demo && mus_track != 0u) {   /* the demo: the player's note, played as a perfect player would */
        int32_t p, part;
        if (mus_note_at(mus_track, mus_cs(ss), &p, &part) != MUS_NONE) mus_mel_voice(p, 1, 256, 0);   /* (solo: inside the song) */
    }
    if (insong && !solo && mus_track != 0u && mus_cs(ss) < mus_chart_n) {   /* a note of the melody this mode leaves out: the tune plays it, softly */
        int32_t p, part;
        if (mus_note_at(mus_track, mus_cs(ss), &p, &part) != MUS_NONE) mus_mel_voice(p, 1, 80, 0);
    }
    if (mus_track == 0u && !(mus_demo && mus_cs(ss) >= mus_chart_n)) {   /* the player's own notes: two bars, round and round (not in the demo's bar between) */
        uint32_t s32 = step % 32u;
        int32_t first = solo && !mus_demo ? mus_take_first(s32) : -1;   /* (the one the player plays is left out) */
        for (k = 0; k < take_n; k++)
            if (take_ev[k][0] == s32 && (int32_t)k != first) take_voice(key_note(take_ev[k][1]) + 12, 1);
    }
}

/* the chart of a song for one mode: the first note in each `group` steps (4: one a beat, 2: a half beat, 1: all).
 * Returns how many notes */
static int32_t mus_chart_build(uint32_t track, uint32_t group)
{
    uint32_t n = mus_song_steps(track), ss, j;
    int32_t cnt = 0, p, part, d;
    mus_solo = 0;
    for (ss = 0; ss < n; ss++) {
        d = mus_note_at(track, ss, &p, &part);
        for (j = ss - ss % group; d != MUS_NONE && j < ss; j++) if (mus_note_at(track, j, &p, &part) != MUS_NONE) d = MUS_NONE;
        mus_chart[ss] = (uint8_t)(d == MUS_NONE ? 0 : d + 1);
        if (d != MUS_NONE) cnt++;
    }
    mus_chart_n = (uint16_t)n;
    return cnt;
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
static void snd_key(int32_t key) { sfx(key_play(key), 0, 500, W_TRI, 4900, 12, 130); }
static void snd_keys(uint32_t down, int32_t up12)   /* every key just pressed, its note (up12: an octave up) */
{
    int32_t k;
    for (k = 0; k < AC_NKEY; k++)
        if (down >> k & 1u) {
            sfx(key_play(k) + (up12 ? 12 : 0), 0, 600, W_TRI, 3600, 6, 160);
#ifdef AC_TEST_SCENE
            printf("%6u ms  key %d plays %d (tune %u)\n", fm1_ms, k, key_play(k) + (up12 ? 12 : 0), (uint32_t)mus_track);   /* (tests) */
#endif
        }
}
static void snd_ui(int32_t n) { sfx(n, 0, 120, W_TRI, 2600, 3, 40); }
