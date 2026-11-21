/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's three machines. All three run on the jukebox's clock, so what you do in them lands in the music.
 *   CRYSTAL ROAD      a song of the jukebox (or the player's own) comes down a road as crystals; the keys play it.
 *                     Three modes, a score and crystals for every song.
 *   JUICE WORKS       four taps of mushroom juice (KNOB 1-4), glasses put under them, moved and served with the
 *                     keys, tickets that say what to pour: many things to keep going at once.
 *   NIGHT BLOOM       new here: four knobs turn four petals round a flower, one to a ring, a click a twenty-fourth
 *                     of a turn, as the knob itself. Spores drift in slowly from beyond the screen; catch each on the
 *                     petal of its own colour. An arrow points off the screen to each knob as it joins. */

enum { MS_TITLE, MS_PLAY, MS_OVER };
static struct { uint8_t which, state; int32_t t, score, lives, combo, msg_t, flash; uint8_t msg; } mc;
static uint32_t best[3];
static uint8_t save_dirty;

static void mach_msg(int id) { mc.msg = (uint8_t)id; mc.msg_t = 700; }
static void num_str(char *b, int32_t v)
{
    char t[12]; int32_t n = 0, k = 0;
    if (v <= 0) t[n++] = '0';
    while (v > 0 && n < 10) { t[n++] = (char)('0' + v % 10); v /= 10; }
    while (n) b[k++] = t[--n];
    b[k] = 0;
}
static void mach_hud(void)
{
    char b[16]; int32_t i;
    num_str(b, mc.score);
    gtextr(234, 8, b, th->text, 2);
    for (i = 0; i < imin(mc.lives, 8); i++) gtext(8 + i * 9, 8, "\xE2\x99\xA5", th->caps[0], 1);
    if (mc.msg_t > 0) { galpha(imin(256, mc.msg_t * 256 / 300)); gfit(120, 40 - (700 - mc.msg_t) / 60, 232, TR(mc.msg), mc.msg == T_MISS ? th->caps[0] : th->pad, ts(2, 4)); galpha(256); }
}
/* the start (and the end) of a machine: its name; PLAY starts; HOME shows how it is played; REC goes back */
static void mach_card(const char *name, col_t c)
{
    char b[40]; const char *p;
    int32_t y, a = ts(1, 3), used, n = 0;
    gveil(th->bg, 24);
    used = gfit(120, 14 + 2 * ts(3, 4), 232, name, c, tsize ? 3 : 2);
    y = 14 + 2 * ts(3, 4) + 5 * used + 10;
    if (mc.state == MS_OVER) {                   /* how it went */
        y += gblock(4, y, 232, TR(T_GAMEOVER), th->caps[0], ts(2, 3), 1);
        num_str(b, mc.score); gtextc(120, y + 4, b, th->text, 4); y += 30;
    }
    while (a > 1 && y + gblock_h(232, TR(mc.state == MS_OVER ? T_PLAY_AGAIN : T_PLAY_START), a) + gblock_h(232, TR(T_HOME_CONTROLS), a)
           + (best[mc.which] ? TLH(a) : 0) + 12 > 238 - gblock_h(232, TR(T_REC_BACK), ts(1, 2))) a--;
    galpha(140 + ipulse((uint32_t)mc.t, 1600) * 116 / 256);
    y += gblock(4, y, 232, TR(mc.state == MS_OVER ? T_PLAY_AGAIN : T_PLAY_START), th->pad, a, 1) + 6;
    galpha(256);
    if (best[mc.which]) {
        for (p = TR(T_BEST); *p && n < 24; ) b[n++] = *p++;
        b[n++] = ' '; num_str(b + n, (int32_t)best[mc.which]);
        gfit(120, y + 2 * a, 232, b, th->mute, a);
    }
    y = gfoot(238, TR(T_REC_BACK), th->mute, th->bg, ts(1, 2));   /* at the foot: HOME, and under it REC */
    gfoot(y - 2, TR(T_HOME_CONTROLS), th->text, th->bg, a);
}
static void mach_over(void)
{
    mc.state = MS_OVER; mc.t = 0;
    if ((uint32_t)mc.score > best[mc.which]) { best[mc.which] = (uint32_t)mc.score; save_dirty = 1; }
    sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150);
}

/* ---------------------------------------------------------------- CRYSTAL ROAD */
/* MYSTIC RUN turned to face the player: the land of the run seen along a road, the song coming down it as crystals,
 * and Rock on a line of light, leaping to wherever on the keyboard a key is pressed. The keys play the song's melody
 * (one written for each tune, MELODY in ac_music.h; the player's own song: the notes kept at the dish): the tune's lead
 * rests, its arpeggio steps back, a note caught rings out in the melody's voice, a note missed is a sour one. A mode cuts the 16 steps of the keyboard's
 * ladder (key_play) into lanes: PEBBLE 4 (JUICE WORKS' key groups and colours, one note a beat, misses cost nothing),
 * ROCK 8 (a half beat, five hearts), CRYSTAL 16 (every note). The first song played to its end opens ROCK and CRYSTAL.
 * Every song keeps a best score and one to three crystals in each mode. */
#define GH_YH 84                                 /* the horizon */
#define GH_YS 190                                /* the line of light: where a crystal is played */
#define GH_LOOK 384                              /* a crystal is seen four beats ahead */
#define GH_HW 116                                /* half the road's width at the line */
#define GH_KB 208                                /* the top of the keyboard drawn at the foot */
#define GH_PERFECT 9                             /* ticks either side: PERFECT (MYSTIC RUN's own figure) */
enum { GH_PEBBLE, GH_ROCK, GH_CRYSTAL };
/* how close a key must be to its note (ticks either side, 96 a beat): wide in PEBBLE, kind in ROCK. A note caught early
 * still sounds on its beat (the game waits for it); only PERFECT is celebrated */
static const uint8_t GH_LANES[3] = {4, 8, 16}, GH_GROUP[3] = {4, 2, 1}, GH_WIN[3] = {40, 30, 22}, GH_HEART[3] = {0, 8, 16};
static const char *const GH_MODE[3] = {"PEBBLE", "ROCK", "CRYSTAL"};
static const col_t GH_ZONE[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
static uint16_t gh_best[JUKE_N][3];              /* (kept in the save) the best of each song in each mode */
static uint8_t gh_gem[JUKE_N];                   /* its crystals, two bits a mode */
static uint8_t gh_flags, gh_mode;                /* bit 0: ROCK and CRYSTAL are open; the mode last chosen */
static uint8_t gh_st[MUS_CHART_MAX];             /* each note of the song: 0 to come, 1 PERFECT, 2 GOOD, 3 missed */
static struct {
    uint32_t t0, track;                          /* the tick of the song's step 0; the tune */
    int32_t L, total, hits, done_s, pause_at, rock_x0, rock_x1, rock_t, judge_t, judge_x;
    uint8_t judge_perf, cleared, unlocked, newbest, gems, playing, played;   /* played: the mode of the song just over */
    int32_t perf_t, perf_x, pstreak;            /* the last PERFECT: when, where, how many in a row */
    uint8_t perf_lane, card_track;
    int32_t swap_t, swap_dir;                    /* the start card: when the song last changed, and which way */
    uint8_t demo_track, demo_mode;               /* what the start card's demo plays (255: nothing yet) */
    uint8_t practice;                            /* after a song: a key touched, the song plays again to practise with */
    struct { int32_t t, x; uint8_t lane; } bur[6];
    uint8_t nbur;
    int16_t lane_x[16];
    uint32_t lane_keys[16];
} gh;

static void gh_demo_stop(void) { mus_demo = 0; mus_solo = 0; gh.demo_track = 255; }
static int gh_open(void) { return gh_flags & 1u ? 3 : 1; }   /* how many modes can be chosen */
static int gh_gems_of(int t, int m) { return gh_gem[t % JUKE_N] >> (m * 2) & 3; }
static int32_t gh_crystals(void)                 /* crystals earned over every song and mode (of 54) */
{
    int32_t t, m, n = 0;
    for (t = 0; t < JUKE_N; t++) for (m = 0; m < 3; m++) n += gh_gems_of(t, m);
    return n;
}
static int gh_key_lane(int k, int L) { return (k * 15 / 26) * L / 16; }
/* where key k sits on a strip 240 px wide: the 16 keys of the bottom row 15 px apart, the 11 of the top row between */
static int32_t gh_key_x(int k)
{
    int i, w = 0;
    for (i = 0; i < k; i++) if (!KEY_IS_SHARP[i]) w++;
    return KEY_IS_SHARP[k] ? w * 15 : w * 15 + 7;
}
static col_t gh_col(int lane, int L) { return GH_ZONE[lane * 4 / L]; }
static void gh_lanes(int L)                      /* each lane's keys and the middle of them */
{
    int i, k;
    gh.L = L;
    for (i = 0; i < L; i++) {
        int32_t a = 999, b = -1;
        gh.lane_keys[i] = 0;
        for (k = 0; k < AC_NKEY; k++)
            if (gh_key_lane(k, L) == i) { gh.lane_keys[i] |= 1u << k; a = imin(a, gh_key_x(k)); b = imax(b, gh_key_x(k)); }
        gh.lane_x[i] = (int16_t)((a + b) / 2);
    }
}
static int gh_lane_at(int32_t s) { return (mus_chart[s] - 1) * gh.L / 16; }
static int32_t gh_now(void) { return (int32_t)((tr_now_q8() >> 8) - gh.t0); }
static int32_t gh_end_tick(void) { return (int32_t)mus_chart_n * 24 + 32; }

/* the road's perspective. u: across it (-256..256 edge to edge); z: 0 at the horizon .. 256 at the line (more: past) */
static void gh_proj(int32_t u, int32_t z, int32_t *x, int32_t *y, int32_t *s)
{
    int32_t p;
    z = iclamp(z, 0, 330);
    p = (z * z / 256 * 3 + z) / 4;               /* far things crawl, near things rush */
    *y = GH_YH + (GH_YS - GH_YH) * p / 256;
    *x = 120 + u * (12 + (GH_HW - 12) * p / 256) / 256;
    *s = 44 + 212 * p / 256;
}
static int32_t gh_lane_u(int lane) { return (gh.lane_x[lane] - 120) * 256 / GH_HW; }
/* a crystal standing at (x, y), s its size (Q8); wide: a cluster (a lane of many keys); dead: missed */
static void gh_crystal(int32_t x, int32_t y, int32_t s, col_t c, int32_t beat, int wide, int dead)
{
    int32_t hh = 30 * s / 256, hw = (wide ? 15 : 8) * s / 256, k, xy[6];
    if (hh < 2) { gp(x, y - 1, c); return; }
    if (!dead) { int32_t a0 = (int32_t)ga * 8; galpha(a0 * (50 + beat * 60 / 256) / 256); gdisc(x, y - hh / 2, hh / 2 + 3 + 4 * s / 256, c); galpha(a0); }
    for (k = 0; k < (wide ? 3 : 2); k++) {
        int32_t ox = wide ? (k - 1) * hw * 2 / 3 : k * hw / 2 - hw / 4, h2 = k == (wide ? 1 : 0) ? hh : hh * 6 / 10;
        xy[0] = x + ox - hw / 2; xy[1] = y; xy[2] = x + ox + (k - 1) * hw / 6; xy[3] = y - h2; xy[4] = x + ox + hw / 2; xy[5] = y;
        gpoly(xy, 3, dead ? th->panel : c);
        gpolyline(xy, 3, 1, dead ? th->mute : th->text);
    }
}
/* the keyboard at the foot of the screen: the FM-1's two rows as they lie under the hands. A lit key is filled in its
 * lane's colour, a glowing one outlined in it, a held one white */
static void gh_kb(int32_t y0, uint32_t lit, uint32_t dim, uint32_t down, int L)
{
    int k;
    grect(0, y0 - 4, 240, 240 - y0 + 4, th->bg);
    for (k = 0; k < AC_NKEY; k++) {
        int32_t x = gh_key_x(k), sh = KEY_IS_SHARP[k], bx = sh ? x - 4 : x - 6, by = sh ? y0 : y0 + 11, w = sh ? 9 : 13, h = sh ? 9 : 14;
        col_t c = gh_col(gh_key_lane(k, L), L);
        if (down >> k & 1u) { grect(bx, by + 1, w, h, th->text); continue; }
        if (lit >> k & 1u) { grect(bx, by, w, h, c); continue; }
        if (dim >> k & 1u) { galpha(70); grect(bx, by, w, h, c); galpha(256); gbox(bx, by, w, h, c); continue; }
        galpha(110); gbox(bx, by, w, h, th->mute); galpha(256);
    }
}
/* a grade crystal (of three): earned, filled; not, an outline */
static void gh_gem_draw(int32_t x, int32_t y, int32_t s, col_t c, int on)
{
    int32_t xy[8];
    xy[0] = x; xy[1] = y - s; xy[2] = x + s * 2 / 3; xy[3] = y; xy[4] = x; xy[5] = y + s; xy[6] = x - s * 2 / 3; xy[7] = y;
    if (on) { galpha(70); gdisc(x, y, s + 3, c); galpha(256); gpoly(xy, 4, c); gline(x, y - s, x, y + s, th->text); }
    else { galpha(120); gpolyline(xy, 4, 1, th->mute); galpha(256); }
}
static void gh_gems(int32_t cx, int32_t y, int32_t s, int n, col_t c)
{
    int k;
    for (k = 0; k < 3; k++) gh_gem_draw(cx + (k - 1) * (s * 2 + 4), y, s, c, k < n);
}
static void gh_lock(int32_t x, int32_t y, col_t c)   /* a padlock, 7 x 9, its top left at (x, y) */
{
    garc(x + 3, y + 3, 2, 3, 32768, 65536, c, 0);
    grect(x, y + 4, 7, 5, c);
}
/* CRYSTAL ROAD's songs go slowest first: OPAL TIDE, SPORE DRIFT, ... ROLL ON (YOUR SONG where its tempo puts it) */
static uint32_t gh_bpm(uint32_t t) { t %= JUKE_N; return t ? SONGS[t].bpm : take_bpm; }
static int32_t gh_song_pos(uint32_t t)           /* its place in that order */
{
    uint32_t k; int32_t n = 0;
    t %= JUKE_N;
    for (k = 0; k < JUKE_N; k++) if (gh_bpm(k) < gh_bpm(t) || (gh_bpm(k) == gh_bpm(t) && k < t)) n++;
    return n;
}
static uint32_t gh_song_step(uint32_t t, int32_t d)   /* the next song that way (d > 0 faster), round */
{
    int32_t want = (gh_song_pos(t) + (d > 0 ? 1 : JUKE_N - 1)) % JUKE_N;
    uint32_t k;
    for (k = 0; k < JUKE_N; k++) if (gh_song_pos(k) == want) return k;
    return t;
}
static const char *gh_song_name(uint32_t t) { return t % JUKE_N ? SONGS[t % JUKE_N].name : TR(T_YOURSONG); }

/* ---- playing */
/* a caught note, in the melody's voice, `early` ticks before its beat (it waits for the beat); sour: missed, a note that
 * does not fit */
static void gh_note_sound(int32_t s, int sour, int32_t early)
{
    int32_t p, part;
    uint32_t delay = early > 0 ? (uint32_t)early * 60000u / ((tr_bpm_q8 >> 8) * 96u) : 0u;
    if (mus_note_at(gh.track, (uint32_t)s, &p, &part) == MUS_NONE) return;
    if (sour) { sfx(p + 1, 0, 260, W_SAW, 2400, 2, 20); sfx(p - 6, 0, 260, W_SQUARE, 1300, 2, 20); return; }
    if (part == 2) take_voice_at(p, 0, delay); else mus_mel_voice(p, 0, 256, delay);
}
static void gh_end(int cleared)
{
    int32_t t = (int32_t)(gh.track % JUKE_N), pct = gh.total ? gh.hits * 100 / gh.total : 0, g;
    mus_solo = 0; gh.playing = 0;
    mc.state = MS_OVER; mc.t = 0; gh.cleared = (uint8_t)cleared; gh.played = gh_mode; gh.unlocked = 0; gh.newbest = 0;
    g = cleared ? (pct >= 95 ? 3 : pct >= 80 ? 2 : pct >= 60 ? 1 : 0) : 0;
    gh.gems = (uint8_t)g;
    if (mc.score > gh_best[t][gh_mode]) { gh_best[t][gh_mode] = (uint16_t)imin(mc.score, 65535); gh.newbest = 1; save_dirty = 1; }
    if (g > gh_gems_of(t, gh_mode)) { gh_gem[t] = (uint8_t)((gh_gem[t] & ~(3u << (gh_mode * 2))) | (uint32_t)g << (gh_mode * 2)); save_dirty = 1; }
    if (cleared && !(gh_flags & 1u)) { gh_flags |= 1u; gh.unlocked = 1; save_dirty = 1; }
    if (cleared) { snd_chime(1); if (gh.unlocked) snd_whoosh(); }
    else { sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150); }
}
static void gh_rock_to(int32_t x, int32_t now)   /* Rock leaps toward x from wherever he is */
{
    int32_t f = ease8(iclamp((now - gh.rock_t) * 256 / 16, 0, 256));
    gh.rock_x0 = gh.rock_x0 + (gh.rock_x1 - gh.rock_x0) * f / 256;
    gh.rock_x1 = x; gh.rock_t = now;
}
static void gh_catch(int32_t s, int perfect, int32_t now)
{
    int lane = gh_lane_at(s);
    gh_st[s] = (uint8_t)(perfect ? 1 : 2);
    mc.combo++; gh.hits++;
    mc.score += (perfect ? 20 : 10) + imin(mc.combo, 20);
    if (GH_HEART[gh_mode] && !(mc.combo % GH_HEART[gh_mode]) && mc.lives < 5) mc.lives++;   /* a run of hits: a heart back */
    gh_note_sound(s, 0, s * 24 - now);
    if (perfect) {                               /* right on the beat: a star bursts over the line, a chime over the note */
        gh.pstreak++; gh.perf_t = now; gh.perf_x = gh.lane_x[lane]; gh.perf_lane = (uint8_t)lane;
        sfx(84 + SC_PEN[imin(gh.pstreak, 4)], (uint32_t)imax(0, s * 24 - now) * 60000u / ((tr_bpm_q8 >> 8) * 96u), 180, W_SINE, 900, 1, 40);
    } else gh.pstreak = 0;
    gh.bur[gh.nbur % 6u].t = now; gh.bur[gh.nbur % 6u].x = gh.lane_x[lane]; gh.bur[gh.nbur % 6u].lane = (uint8_t)lane; gh.nbur++;
    gh.judge_t = now; gh.judge_x = gh.lane_x[lane]; gh.judge_perf = (uint8_t)perfect;
}
static void gh_miss(int32_t s)
{
    gh_st[s] = 3; mc.combo = 0; gh.pstreak = 0;
    gh_note_sound(s, 1, 0);                         /* a sour note where the melody should have been */
    if (gh_mode == GH_PEBBLE) return;
    mc.lives--; mc.flash = 300;
    sfx_ex(45, 0, 300, W_SAW, 2600, 2, 100, 0, 0, 65100);
    if (mc.lives <= 0) gh_end(0);
}
static void gh_start(void)
{
    uint32_t abs_ = tr_now_q8() >> 8, i;
    gh_demo_stop(); gh.practice = 0;
    gh.track = mus_track % JUKE_N;
    mus_set(MUS_JUKE, gh.track);                 /* (the song's own tempo: scores must compare) */
    gh_mode = (uint8_t)imin(gh_mode, gh_open() - 1);
    gh_lanes(GH_LANES[gh_mode]);
    gh.total = mus_chart_build(gh.track, GH_GROUP[gh_mode]);
    for (i = 0; i < MUS_CHART_MAX; i++) gh_st[i] = 0;
    gh.t0 = (abs_ / 384u + 2u) * 384u;           /* the rest of this bar and one more: the count-in */
    mus_step0 = gh.t0 / 24u;
    gh.hits = 0; gh.done_s = 0; gh.nbur = 0; gh.judge_t = -100000; gh.playing = 1; gh.perf_t = -100000; gh.pstreak = 0;
    gh.rock_x0 = gh.rock_x1 = 120; gh.rock_t = -100000;
    mc.lives = gh_mode == GH_PEBBLE ? 0 : 5;
    mus_solo = GH_GROUP[gh_mode];
#ifdef AC_TEST_SCENE                             /* (tests) the chart: how many notes, and on which of the 16 steps */
    {
        int32_t h[16] = {0}, k;
        for (i = 0; i < mus_chart_n; i++) if (mus_chart[i]) h[mus_chart[i] - 1]++;
        printf("%6u ms  CRYSTAL ROAD tune %u mode %u: %d notes of %u steps:", fm1_ms, gh.track, (uint32_t)gh_mode, gh.total, (uint32_t)mus_chart_n);
        for (k = 0; k < 16; k++) printf(" %d", h[k]);
        printf("\n");
    }
#endif
}
#ifdef AC_TEST_SCENE                             /* (tests) every song's chart in every mode: how many notes, where */
static void gh_test_charts(void)
{
    uint32_t t, m, i;
    for (t = 0; t < JUKE_N; t++)
        for (m = 0; m < 3u; m++) {
            int32_t h[16] = {0}, k, tot = mus_chart_build(t, GH_GROUP[m]);
            for (i = 0; i < mus_chart_n; i++) if (mus_chart[i]) h[mus_chart[i] - 1]++;
            printf("chart tune %u %-7s %3d notes:", t, GH_MODE[m], tot);
            for (k = 0; k < 16; k++) printf(" %2d", h[k]);
            printf("\n");
        }
    mus_chart_n = 0;
}
#endif
static void gh_pause(void) { if (gh.playing) { gh.pause_at = gh_now(); mus_solo = 0; } else if (mus_demo) gh_demo_stop(); }
/* back from a sheet or the settings: a fresh count-in, then the song again from the start of the bar it stopped in
 * (the notes already caught in that bar stay caught) */
static void gh_resume(void)
{
    uint32_t abs_ = tr_now_q8() >> 8, bar;
    if (!gh.playing) return;
    bar = gh.pause_at > 0 ? (uint32_t)gh.pause_at / 384u : 0u;
    gh.t0 = (abs_ / 384u + 2u) * 384u - bar * 384u;
    mus_step0 = gh.t0 / 24u;
    gh.rock_t = -100000; gh.judge_t = -100000;
    mus_solo = GH_GROUP[gh_mode];
}
static void road_tick(int32_t dt)
{
    int32_t now = gh_now(), n = (int32_t)mus_chart_n, win = GH_WIN[gh_mode], s, k;
    int32_t dtt = dt * (int32_t)(tr_bpm_q8 >> 8) * 96 / 60000, nowj = now - dtt / 2;   /* (a key went down, on average, half a frame ago) */
#ifdef AC_TEST_CATCH                             /* (tests) every note caught on its tick */
    for (s = imax(0, gh.done_s); s < n && s * 24 <= now; s++)
        if (mus_chart[s] && !gh_st[s]) { gh_rock_to(gh.lane_x[gh_lane_at(s)], now); gh_catch(s, 1, now); }
#endif
    for (k = 0; k < AC_NKEY; k++) {              /* each key pressed: the nearest note of its lane within reach */
        int32_t lane, best = -1, bd = win + 1;
        if (!(in.kdown >> k & 1u)) continue;
        lane = gh_key_lane(k, gh.L);
        gh_rock_to(gh.lane_x[lane], now);
        for (s = imax(0, (nowj - win) / 24); s <= (nowj + win) / 24 + 1 && s < n; s++) {
            int32_t d = iabs(s * 24 - nowj);
            if (mus_chart[s] && !gh_st[s] && gh_lane_at(s) == lane && d <= win && d < bd) { bd = d; best = s; }
        }
        if (best >= 0) gh_catch(best, bd <= GH_PERFECT, now);
        else {                                   /* nothing of its lane in reach: its own note, quietly */
            sfx(key_play(k) + 12, 0, 400, W_TRI, 1500, 6, 120);
            if (gh_mode != GH_PEBBLE && now >= 0) { mc.combo = 0; gh.pstreak = 0; }
        }
    }
    while (gh.done_s < n && gh.done_s * 24 + win < nowj) {   /* gone past the line untouched: missed */
        s = gh.done_s++;
        if (mus_chart[s] && !gh_st[s]) { gh_miss(s); if (mc.state != MS_PLAY) return; }
    }
    if (now >= gh_end_tick()) { gh_end(1); return; }
    for (s = imax(0, now / 24); s <= (now + 96) / 24 && s < n; s++)   /* the next notes' keys: glowing a beat ahead, lit half a beat */
        if (mus_chart[s] && !gh_st[s] && s * 24 >= now - 8) {
            if (s * 24 - now <= 48) led_key_want |= gh.lane_keys[gh_lane_at(s)];
            else led_key_dim_want |= gh.lane_keys[gh_lane_at(s)];
        }
    led_key_dim_want &= ~led_key_want;
    if (((now % 96) + 96) % 96 < 12) led_btn_want |= 1u << BTN_PLAY;   /* (PLAY's green light, on the beat) */
}

/* each song its own colour and its own road: so it is clear which tune is playing before a word is read */
static const uint8_t GH_SCOL[JUKE_N] = {9, 3, 0, 4, 1, 2};   /* (9: the light of the dish, th->pad) */
static col_t gh_song_col(uint32_t t) { t %= JUKE_N; return GH_SCOL[t] > 4 ? th->pad : th->caps[GH_SCOL[t]]; }
static int32_t gh_floor_div(int32_t a, int32_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
/* the road and everything on it, for song t; play: 0 the road alone (behind a card) */
static void road_scene(int32_t now, int play, uint32_t tm, uint32_t t)
{
    int32_t beat = 256 - imin(256, (((now % 96) + 96) % 96) * 256 / 40), i, x, y, s, x2, y2, s2, b, st, L = gh.L, n = (int32_t)mus_chart_n, b0 = gh_floor_div(now, 96);
    uint32_t lit = 0, dim = 0;
    col_t sc = gh_song_col(t);
    t %= JUKE_N;
    sky_draw();
    if (t == 0) {                                /* YOUR SONG: the dish it was recorded on, rising over the horizon, and its stars */
        for (i = 0; i < 3; i++) { galpha(40 + i * 20 + beat / 8); garc(120, GH_YH, 26 + i * 12, 26 + i * 12, 32768, 65536, sc, 0); }
        for (i = 0; i < 14; i++) {
            int32_t sx = (int32_t)(ihash((uint32_t)i * 7u + 1u) % 236u) + 2, sy = 8 + (int32_t)(ihash((uint32_t)i * 3u + 5u) % 60u), tw = ipulse(tm + (uint32_t)i * 300u, 1400);
            galpha(60 + tw * 3 / 4); gline(sx - 2, sy, sx + 2, sy, th->text); gline(sx, sy - 2, sx, sy + 2, th->text);
        }
        galpha(256);
    }
    far_layer(now / 8, 0, 256, GH_YH - 20, 0, th->far1, th->fill);
    far_layer(now / 4, 0, 256, GH_YH - 6, 1, th->far2, th->fill);
    grect(0, GH_YH, 240, 240 - GH_YH, th->ground);
    galpha(120); ghl(0, 240, GH_YH, sc); galpha(256);
    if (t == 3)                                  /* MYCELIUM NIGHTS: glowing threads in the dark ground, either side of the road */
        for (i = 0; i < 6; i++) {
            int32_t side = (i & 1) ? 1 : -1, k, px = 0, py = 0;
            for (k = 0; k <= 12; k++) {
                int32_t z = k * 22, u = side * (300 + (int32_t)(ihash((uint32_t)i) % 120u)) + (isin(k * 9000 + i * 7000 + now * 60) >> 9);
                gh_proj(u, z, &x, &y, &s);
                if (k) { galpha(110 + (ipulse(tm + (uint32_t)(i * 500 + k * 90), 2200) >> 1)); gline(px, py, x, y, sc); gline(px, py + 1, x, y + 1, sc); }
                px = x; py = y;
            }
        }
    galpha(256);
    for (b = b0 + 1; b * 96 < now + GH_LOOK; b++) {   /* the beats, coming down the road: a bar's brighter */
        int32_t z = 256 - (b * 96 - now) * 256 / GH_LOOK, bb = b & 3;
        gh_proj(-256, z, &x, &y, &s); gh_proj(256, z, &x2, &y2, &s2);
        galpha(bb ? 30 + z / 4 : 70 + z / 2);
        if (t == 4) {                            /* OPAL TIDE: the beats come in as waves */
            int32_t xx, amp = 2 + z / 40;
            for (xx = x; xx < x2; xx += 3) gline(xx, y + ((amp * isin(xx * 700 + now * 180)) >> 14), xx + 3, y + ((amp * isin((xx + 3) * 700 + now * 180)) >> 14), bb ? th->bands[bb] : sc);
        } else ghl(x, x2 + 1, y, bb ? (t == 2 ? sc : th->bands[bb]) : (t ? sc : th->rim));
    }
    for (i = 0; i <= L; i++) {                   /* the lanes: the four key groups' edges stand out; the road's edges in the song's colour */
        int32_t u = i == 0 ? -256 : i == L ? 256 : (gh_lane_u(i - 1) + gh_lane_u(i)) / 2, edge = i == 0 || i == L, grp = !(i % (L / 4));
        gh_proj(u, 0, &x, &y, &s); gh_proj(u, 256, &x2, &y2, &s2);
        galpha(edge ? 230 : grp ? 90 : 36);
        gline(x, y, x2, y2, edge ? sc : th->bands[1]);
    }
    galpha(256);
    for (b = b0 + 4; b >= b0 - 1; b--) {         /* by the road, on the beat: mushrooms, or the song's own things */
        int32_t z = 256 - (b * 96 + 48 - now) * 256 / GH_LOOK, side = (b & 1) ? 330 : -330;
        if (z < 8) continue;
        gh_proj(side, z, &x, &y, &s);
        galpha(70 + imin(z, 300) / 2);
        if (t == 2) gh_crystal(x, y, s * 7 / 8, th->caps[(uint32_t)b % 5u], beat, 0, 0);   /* CRYSTAL STEPS: crystals */
        else if (t == 5 && (b & 1)) {           /* ROLL ON: rock friends rolling along, a bounce on the beat */
            int32_t hop = beat * s / 1600;
            rock_draw(x, y - 6 * s / 256 - hop, s, now * 500 * (side > 0 ? -1 : 1), 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
        }
        else mush_draw(x, y, s * 230 / 256 + beat * s / 1800, th->caps[(uint32_t)b % 5u], th->stem, beat);
    }
    galpha(256);
    if (t == 1)                                  /* SPORE DRIFT: spores drifting up through it all */
        for (i = 0; i < 16; i++) {
            int32_t sy = 236 - (int32_t)(((uint32_t)now * 2u / 3u + (uint32_t)i * 61u) % 236u), sx = (int32_t)(ihash((uint32_t)i * 11u) % 240u) + (isin(sy * 300 + i * 4000) >> 11);
            galpha(60 + (i & 3) * 30); gdisc(sx, sy, 3, sc); galpha(256); gp(sx, sy, th->text);
        }
    if (t == 3)                                  /* MYCELIUM NIGHTS: fireflies */
        for (i = 0; i < 9; i++) {
            int32_t fx = (int32_t)(ihash((uint32_t)i * 5u + 2u) % 240u) + (isin((int32_t)tm * 3 + i * 9000) >> 10), fy = 96 + (int32_t)(ihash((uint32_t)i * 9u) % 90u) + (icos((int32_t)tm * 2 + i * 5000) >> 11);
            galpha(ipulse(tm + (uint32_t)i * 377u, 1700)); gdisc(fx, fy, 2, th->caps[2]); galpha(256);
        }
    if (play) {
        for (st = imin(n - 1, (now + GH_LOOK) / 24); st >= 0 && st >= now / 24 - 4; st--) {   /* the crystals, far to near */
            int32_t lane, dt = st * 24 - now, z;
            col_t c;
            if (!mus_chart[st] || gh_st[st] == 1 || gh_st[st] == 2) continue;
            lane = gh_lane_at(st); c = gh_col(lane, L);
            z = 256 - dt * 256 / GH_LOOK;
            gh_proj(gh_lane_u(lane), z, &x, &y, &s);
            if (gh_st[st] == 3) { if (-dt < 60) { galpha(imax(0, 256 + dt * 4)); gh_crystal(x, y, s, c, 0, L == 4, 1); galpha(256); } }
            else if (dt > -30) { galpha(imin(256, z * 3 + 40)); gh_crystal(x, y, s, c, beat, L == 4, 0); galpha(256); }
        }
        for (i = 0; i < 6 && i < gh.nbur; i++) { /* caught: a burst of shards and a ring of light */
            int32_t q = now - gh.bur[i].t, k;
            col_t c;
            if (q < 0 || q >= 34) continue;
            c = gh_col(gh.bur[i].lane, L); x = gh.bur[i].x;
            galpha(256 - q * 7);
            gcirc(x, GH_YS - 8, 6 + q, c);
            for (k = 0; k < 7; k++) {
                int32_t a = -28000 + k * 9300 + gh.bur[i].t * 300, d0 = 4 + q, d1 = 9 + q * 2;
                gline2(x + ((d0 * icos(a)) >> 14), GH_YS - 10 + ((d0 * isin(a)) >> 14), x + ((d1 * icos(a)) >> 14), GH_YS - 10 + ((d1 * isin(a)) >> 14), k & 1 ? th->text : c);
            }
            galpha(256);
        }
        if (now - gh.perf_t >= 0 && now - gh.perf_t < 44) {   /* PERFECT: a pillar of light up the lane, a star, two rings */
            int32_t q = now - gh.perf_t, k, px = gh.perf_x, a = 256 - q * 256 / 44;
            col_t c = gh_col(gh.perf_lane, L);
            galpha(a * 90 / 256); grect(px - 7 + q / 8, GH_YS - 110 + q, 14 - q / 4, 110 - q, c);
            galpha(a); grect(px - 1, GH_YS - 100 + q * 2, 3, 100 - q * 2, th->text);
            gcirc(px, GH_YS - 8, 10 + q * 2, th->text); gcirc(px, GH_YS - 8, 6 + q, c);
            for (k = 0; k < 8; k++) {
                int32_t ang = k * 8192 + q * 300, r0 = 6 + q / 2, r1 = 16 + q;
                gline2(px + ((r0 * icos(ang)) >> 14), GH_YS - 8 + ((r0 * isin(ang)) >> 14), px + ((r1 * icos(ang)) >> 14), GH_YS - 8 + ((r1 * isin(ang)) >> 14), k & 1 ? c : th->text);
            }
            galpha(256);
        }
    }
    galpha(150 + beat * 106 / 256); grect(4, GH_YS, 232, 2, now - gh.perf_t >= 0 && now - gh.perf_t < 20 ? th->text : th->rim); galpha(256);
    for (i = 0; i < L; i++) {                    /* each lane's keys, bracketed under the line */
        uint32_t m = gh.lane_keys[i]; int k; int32_t a = 999, bb = -1;
        for (k = 0; k < AC_NKEY; k++) if (m >> k & 1u) { a = imin(a, gh_key_x(k)); bb = imax(bb, gh_key_x(k)); }
        galpha(L == 16 ? 110 : 200);
        grect(a - 5, 201, bb - a + 11, 2, gh_col(i, L));
    }
    galpha(256);
    {   /* Rock, on the line: he leaps to the lane of the key pressed */
        int32_t f = ease8(iclamp((now - gh.rock_t) * 256 / 16, 0, 256)), hop = (L == 4 ? 16 : 9) * f * (256 - f) / 16384;
        int32_t spin = play && now - gh.perf_t >= 0 && now - gh.perf_t < 30 ? (now - gh.perf_t) * 65536 / 30 : 0;   /* (a PERFECT: a spin) */
        if (!play) f = 256;
        rock_draw(gh.rock_x0 + (gh.rock_x1 - gh.rock_x0) * f / 256, GH_YS - 8 - hop - (spin ? 6 : 0), 256, now * 300 + spin, (tm % 3100u) < 150u ? 40 : 256, th->rock, th->rock_rim, th->dark, th->moss, th->eye, th->pupil);
    }
    if (play) {
        for (st = imax(0, now / 24); st <= (now + 96) / 24 && st < n; st++)
            if (mus_chart[st] && !gh_st[st] && st * 24 >= now - 8) {
                if (st * 24 - now <= 48) lit |= gh.lane_keys[gh_lane_at(st)]; else dim |= gh.lane_keys[gh_lane_at(st)];
            }
    }
    gh_kb(GH_KB, lit, dim & ~lit, play ? in.keys : 0, L);
}
static void gh_hud(int32_t now)
{
    char b[16]; int32_t i, n = (int32_t)mus_chart_n, beat = 256 - imin(256, (((now % 96) + 96) % 96) * 256 / 40);
    grect(0, 0, 240, 2, th->panel); grect(0, 0, 240 * iclamp(now, 0, n * 24) / imax(1, n * 24), 2, gh_song_col(gh.track));
    galpha(150 + beat * 106 / 256); gtext(4, 7 - beat / 200, gh_song_name(gh.track), gh_song_col(gh.track), 1); galpha(256);   /* (on the song's beat) */
    gtext(4, 16, GH_MODE[gh_mode], gh_mode ? th->caps[0] : GH_ZONE[0], 1);
    for (i = 0; i < imin(mc.lives, 5); i++) gtext(4 + i * 9, 26, "\xE2\x99\xA5", th->caps[0], 1);
    num_str(b, mc.score); gtextr(236, 7, b, th->text, 2);
    if (mc.combo >= 2) { b[0] = 'X'; num_str(b + 1, mc.combo); gtextr(236, 21, b, th->pad, 1); }
    if (now - gh.judge_t >= 0 && now - gh.judge_t < 70) {   /* PERFECT / GOOD rising from where the note was caught */
        int32_t age = now - gh.judge_t;
        galpha(256 - age * 256 / 70);
        if (gh.judge_perf) {                     /* PERFECT: large, and how many in a row */
            gfit(iclamp(gh.judge_x, 60, 180), GH_YS - 46 - age / 5, 120, TR(T_PERFECT), th->text, 2);
            if (gh.pstreak >= 2) { b[0] = 'X'; num_str(b + 1, gh.pstreak); gtextc(iclamp(gh.judge_x, 60, 180), GH_YS - 30 - age / 5, b, gh_col(gh.perf_lane, gh.L), 1); }
        } else gfit(iclamp(gh.judge_x, 40, 200), GH_YS - 34 - age / 6, 120, TR(T_GOOD), th->pad, 1);
        galpha(256);
    }
    if (now < 0) {                               /* the count-in: the song's name, then its last bar's beats */
        int32_t left = (-now + 95) / 96;
        if (left <= 4) { char c2[2] = {(char)('0' + left), 0}; galpha(140 + ((-now) % 96) * 116 / 96); gtextc(120, 104, c2, th->text, 5); galpha(256); }
        else gfit(120, 108, 220, gh_song_name(gh.track), th->text, 2);
    }
}
static void road_draw(void)
{
    int32_t now = gh.playing || mc.state == MS_PLAY ? gh_now() : (int32_t)(tr_now_q8() >> 8);
    if (!gh.L) gh_lanes(GH_LANES[gh_mode]);
    road_scene(now, mc.state == MS_PLAY, (uint32_t)mc.t, mc.state == MS_PLAY ? gh.track : mc.state == MS_OVER ? gh.track : mus_track);
    if (mc.flash > 0) { galpha(mc.flash * 110 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
    if (mc.state == MS_PLAY) gh_hud(now);
}

/* ---- the cards. Before a song: the machine's name, the song (the ALGORITHM knob turns through the jukebox), the
 * three modes (KNOB 1; two locked until a song is finished), PLAY. After: how it went, and what it opened */
static void gh_card(void)
{
    char b[40];
    int32_t a = ts(1, 2), k, t = (int32_t)(mus_track % JUKE_N), y;
    col_t nc = th->caps[0];
    uint32_t tm = (uint32_t)mc.t;
    int32_t yf;
    gveil(th->bg, 25);
    yf = gfoot(238, TR(T_REC_BACK), th->mute, th->bg, ts(1, 2));   /* at the foot: HOME, and under it REC; the rest above */
    yf = gfoot(yf - 2, TR(T_HOME_CONTROLS), th->text, th->bg, a);
    yf -= 5 * a + 6;                             /* (the PLAY line's top) */
    if (mc.state == MS_OVER) {
        int32_t n;
        t = (int32_t)gh.track;
        gfit(120, 12, 232, gh_song_name((uint32_t)t), th->mute, 2);
        gfit(120, 30, 232, GH_MODE[gh.played], nc, 1);
        if (gh.cleared) gh_gems(120, 58, 11, tm > 300 ? (tm > 600 ? (tm > 900 ? gh.gems : imin(gh.gems, 2)) : imin(gh.gems, 1)) : 0, nc);
        else gfit(120, 52, 232, TR(T_GAMEOVER), th->caps[0], 2);
        num_str(b, mc.score); gtextc(120, 80, b, th->text, 4);
        num_str(b, gh.hits); for (n = 0; b[n]; n++) ;
        b[n++] = ' '; b[n++] = '/'; b[n++] = ' '; num_str(b + n, gh.total);
        gtextc(120, 110, b, th->mute, 1);
        if (gh.unlocked && tm < 3200) {         /* (for three seconds: what opened; then the modes to choose from) */
            int32_t al = iclamp(((int32_t)tm - 700) * 256 / 300, 0, 256);
            galpha(al);
            grect(18, 124, 204, imin(46, yf - 128), th->panel); gbox(18, 124, 204, imin(46, yf - 128), nc);
            gfit(120, 130, 196, TR(T_GH_NEWMODES), th->pad, 2);
            gtextc(120, 150, "ROCK   CRYSTAL", th->text, 2);
            galpha(256);
        } else if (gh.newbest) gfit(120, 136, 232, TR(T_NEWBEST), th->pad, 2);
        for (k = 0; k < gh_open() && gh_open() > 1 && !(gh.unlocked && tm < 3200); k++) {   /* the mode for the next go: KNOB 1 */
            int32_t x = 44 + k * 76;
            if (k == gh_mode) gbox(x - 32, yf - 18, 64, 13, nc);
            gfit(x, yf - 14, 60, GH_MODE[k], k == gh_mode ? th->text : th->mute, 1);
        }
        galpha(140 + ipulse(tm, 1600) * 116 / 256);
        gfit(120, yf, 232, TR(T_PLAY_AGAIN), th->pad, a);
        galpha(256);
    } else {
        /* everything that names the song moves on its beat; the song's colour; a new song slides in from the side its knob
         * turned, and its best score with it */
        int32_t bt = 256 - imin(256, (int32_t)((tr_now_q8() >> 8) % 96u) * 256 / 40), bob = bt > 170 ? 2 : bt > 60 ? 1 : 0;
        int32_t sw = iclamp(((int32_t)tm - gh.swap_t) * 256 / 220, 0, 256), off = (256 - ease8(sw)) * 36 / 256 * gh.swap_dir, bs;
        col_t scol = gh_song_col((uint32_t)t);
        gfit(120, 10 - bob, 232, MACH_NAME[0], nc, 3);
        /* the song's box: its name between the arrows; under it its tempo, four lights going round its bar on its beat
         * and the BPM; under that every song's tempo on one line, slow to fast, this one lit */
        grect(14, 32, 212, 56, th->panel); gbox(14, 32, 212, 56, scol);
        if (bt > 128) gbox(13, 31, 214, 58, scol);
        gfit(120, 36, 200, TR(T_GH_SONG), th->mute, 1);
        galpha(150 + bt * 106 / 256);
        gtext(20 - bt / 100, 48, "\xE2\x86\x90", scol, 2); gtextr(220 + bt / 100, 48, "\xE2\x86\x92", scol, 2);
        galpha(imin(256, 60 + sw));
        gfit(120 + off, 48 - bob, 170, gh_song_name((uint32_t)t), sw < 256 ? th->text : scol, 2);
        galpha(256);
        {
            uint32_t bi = (tr_now_q8() >> 8) / 96u % 4u, bpm = gh_bpm((uint32_t)t), j;
            int32_t m, w = 0, x0;
            char bb[12];
            num_str(bb, (int32_t)bpm); while (bb[w]) w++;
            bb[w++] = ' '; bb[w++] = 'B'; bb[w++] = 'P'; bb[w++] = 'M'; bb[w] = 0;
            w = gtextw(bb, 2);
            x0 = 120 - (46 + w) / 2;             /* (the lights and the number, centred together) */
            for (m = 0; m < 4; m++) {
                int on = (uint32_t)m == bi;
                galpha(on ? 110 + bt * 146 / 256 : 70);
                gdisc(x0 + 4 + m * 11, 68, on ? 4 : 3, m == 0 ? scol : on ? th->text : th->mute);
            }
            galpha(256);
            gtext(x0 + 46, 64, bb, scol, 2);
            ghl(30, 211, 81, th->mute);          /* the tempo line: 60 BPM at its left end, 130 at its right */
            for (j = 0; j < JUKE_N; j++) {
                int32_t tx = 30 + iclamp((int32_t)gh_bpm(j) - 60, 0, 70) * 180 / 70, me = j == (uint32_t)t;
                if (me) { galpha(120 + bt * 136 / 256); gdisc(tx, 81, 3, scol); galpha(256); }
                else grect(tx, 79, 1, 5, gh_song_col(j));
            }
        }
        for (k = 0; k < 3; k++) {                /* the difficulties: the chosen one boxed, its crystals; a locked one, a padlock */
            int32_t x = 44 + k * 76, on = k < gh_open();
            col_t c = on ? (k == gh_mode ? th->text : th->pad) : th->mute;
            if (k == gh_mode) { grect(x - 35, 93, 70, 36, th->panel); gbox(x - 35, 93, 70, 36, nc); }
            gfit(x, 98, 64, GH_MODE[k], c, 1);
            if (on) gh_gems(x, 116, 5, gh_gems_of(t, k), nc);
            else gh_lock(x - 3, 110, th->mute);
        }
        bs = gh_best[t][gh_mode];                /* the best of this song at this difficulty, large */
        { const char *p = TR(T_BEST); int32_t m = 0; while (*p && m < 24) b[m++] = *p++; b[m++] = ' ';
          if (bs) num_str(b + m, bs); else { b[m++] = '-'; b[m] = 0; } }
        galpha(imin(256, 40 + sw));
        gfit(120, 134 - (sw < 256 ? (256 - sw) / 40 : 0), 232, b, bs ? scol : th->mute, 2);
        galpha(256);
        gfit(120, 150, 232, TR(T_GH_MODE), th->mute, 1);
        galpha(140 + ipulse(tm, 1600) * 116 / 256);
        y = imin(164, yf);
        if (t == 0 && !take_n) gfit(120, y, 232, TR(T_GH_RECFIRST), th->caps[0], a);
        else gfit(120, y, 232, TR(T_PLAY_START), th->pad, a);
        galpha(256);
    }
}
/* the start card plays the song as a perfect player would at the chosen difficulty, round and round, from the next bar
 * line: the melody's notes in its own voice, the ones the difficulty leaves out softly, and the keys light as each is
 * played (glowing a beat before it) */
static void gh_demo_tick(void)
{
    uint32_t t = mus_track % JUKE_N, ab = tr_now_q8() >> 8, len;
    int32_t pos, s, n;
    if (mus_mode != MUS_JUKE || (t == 0u && !take_n)) { if (mus_demo) gh_demo_stop(); return; }
    if (!mus_demo || gh.demo_track != t || gh.demo_mode != gh_mode) {
        mus_demo = 0;
        gh_lanes(GH_LANES[gh_mode]);
        mus_chart_build(t, GH_GROUP[gh_mode]);   /* (mus_solo is off while it is built) */
        mus_step0 = (ab / 24u / 16u + 1u) * 16u;
        gh.demo_track = (uint8_t)t; gh.demo_mode = gh_mode;
        mus_demo = 1; mus_solo = GH_GROUP[gh_mode];
    }
    n = (int32_t)mus_chart_n; len = (uint32_t)(n + 16) * 24u;   /* (the song and the bar between) */
    if (!len) return;
    pos = (int32_t)(ab - mus_step0 * 24u) % (int32_t)len;   /* where in the loop (before its first bar: its last) */
    if (pos < 0) pos += (int32_t)len;
    for (s = 0; s < n; s++) {
        int32_t d;
        if (!mus_chart[s]) continue;
        d = s * 24 - pos;
        if (d < -(int32_t)len / 2) d += (int32_t)len; else if (d > (int32_t)len / 2) d -= (int32_t)len;
        if (d <= 0 && d > -(int32_t)GH_GROUP[gh_mode] * 16) led_key_want |= gh.lane_keys[gh_lane_at(s)];   /* being played (2/3 of its share of time) */
        else if (d > 0 && d <= 96) led_key_dim_want |= gh.lane_keys[gh_lane_at(s)];   /* next */
    }
    led_key_dim_want &= ~led_key_want;
#ifdef AC_TEST_SCENE                             /* (tests) each change of the demo's lit keys */
    { static uint32_t was; if (led_key_want != was) { printf("%6u ms  demo: keys lit %07X (tune %u, %s)%s\n", fm1_ms, led_key_want, t, GH_MODE[gh_mode], gh.practice ? " practice" : ""); was = led_key_want; } }
#endif
}
/* the cards' own controls (SELECT, PRESETS and ALGORITHM turn the song: arcave.c): KNOB 1-4 the difficulty. Returns 1
 * if PLAY may start */
static int gh_card_tick(void)
{
    int32_t d = in.enc[ENC_KNOB1] + in.enc[ENC_KNOB2] + in.enc[ENC_KNOB3] + in.enc[ENC_KNOB4], k;
    uint32_t t = mus_track % JUKE_N;
    if (t != gh.card_track) {                    /* the song changed: it slides in from the way the knob turned */
        gh.swap_dir = (gh_song_pos(t) - gh_song_pos(gh.card_track) + JUKE_N) % JUKE_N <= JUKE_N / 2 ? 1 : -1;
        gh.card_track = (uint8_t)t; gh.swap_t = mc.t;
        if (mc.state == MS_OVER) { mc.state = MS_TITLE; gh.swap_t = mc.t = 0; }   /* (after a song: the new song's card) */
    }
    if (mc.state != MS_OVER) gh.practice = 0;
    else if (in.kdown && !gh.practice) gh.practice = 1;   /* (after a song: a key touched, the song again, to practise) */
    for (k = 0; k < AC_NKEY; k++)                /* the keys play along, in the melody's voice */
        if (in.kdown >> k & 1u) mus_mel_voice(key_play(k) + 12, 0, 200, 0);
    if (d && mc.state == MS_OVER && mc.t < 3200) mc.t = 3200;   /* (the difficulties show at once) */
    if (d) {
        int m = iclamp(gh_mode + (d > 0 ? 1 : -1), 0, gh_open() - 1);
        if (m != gh_mode) { gh_mode = (uint8_t)m; gh_lanes(GH_LANES[m]); snd_ui(76 + m * 3); save_dirty = 1; gh.swap_t = mc.t - 120; }
        else snd_ui(60);
    }
    if (mc.state == MS_TITLE || gh.practice) gh_demo_tick(); else if (mus_demo) gh_demo_stop();
    if (t == 0u && !take_n) return 0;            /* (no song recorded yet) */
    return 1;
}
/* EDIT: CRYSTAL ROAD's page of the scores: every song, its crystals in each mode and its best (of any mode) */
static void gh_scores_draw(int32_t y0)
{
    int k, m;
    col_t nc = th->caps[0];
    gfit(120, y0, 232, MACH_NAME[0], nc, 2);
    for (m = 0; m < 3; m++) gfit(132 + m * 28, y0 + 18, 26, m == 0 ? "PEB" : m == 1 ? "ROCK" : "CRY", th->mute, 1);
    for (k = 0; k < JUKE_N; k++) {
        int32_t y = y0 + 32 + k * 25, bs = 0;
        char b[8];
        gfit(64, y + 2, 116, gh_song_name((uint32_t)k), k == 0 ? th->pad : th->text, 1);
        for (m = 0; m < 3; m++) {
            int g;
            for (g = 0; g < 3; g++) gh_gem_draw(124 + m * 28 + g * 8, y + 5, 4, nc, g < gh_gems_of(k, m));
            bs = imax(bs, gh_best[k][m]);
        }
        if (bs) { num_str(b, bs); gtextr(236, y + 2, b, th->pad, 1); } else gtextr(236, y + 2, "-", th->mute, 1);
        galpha(100); ghl(6, 234, y + 16, th->mute); galpha(256);
    }
}

/* ---------------------------------------------------------------- JUICE WORKS */
/* Four taps of mushroom juice over a counter with four bays. KNOB 1-4 open and close the taps (three clicks:
 * a trickle, a pour, a gush). The keys under a tap put a glass in its bay; pressed again (as they are let go) the
 * glass is served; held while a key under another tap is pressed, the glass moves there. Tickets hang between the
 * taps: each shows the drink to make, layer by layer, in a glass the very size and shape of the ones under the taps.
 * The first two drinks of more than one juice wait twice as long. Juice on the floor, a glass run over, a wrong drink
 * or a ticket left too long costs a drop of the five. */
#define JW_NTK 3
#define JW_UPX 6                                 /* px to a part, on a glass and on a ticket alike */
#define JW_CAP 6                                 /* parts a glass holds */
#define JW_FLOOR 204
#define JW_X(k) (30 + (k) * 60)
static const col_t JW_COL[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
#define JW_TOL 184                               /* how far off a mark still serves (Q8 parts): nearly three quarters of one */
static const uint16_t JW_RATE[4] = {0, 150, 330, 700};   /* parts (Q8) a second, by how far the tap is open */
typedef struct { uint8_t n, c[3]; int32_t v[3]; } jw_glass_t;   /* layers from the bottom: colour, parts (Q8) */
static struct {
    uint8_t valve[4], has[4], used[4];
    jw_glass_t g[4], out;
    int32_t puddle[4], splash[4], out_t, out_x, out_ok;
    struct { uint8_t on, n, c[3], u[3]; int32_t t, life; } tk[JW_NTK];
    int32_t spawn_t, served, multi;              /* multi: drinks of two juices or more asked for so far */
    uint32_t prev_keys, seed, nseed;
} jw;
static int jw_zone(int key) { return key * 4 / AC_NKEY; }
static int32_t jw_total(const jw_glass_t *g) { return (g->n > 0 ? g->v[0] : 0) + (g->n > 1 ? g->v[1] : 0) + (g->n > 2 ? g->v[2] : 0); }
static void jw_hurt(int k)                       /* a drop lost; k: the bay it happened in, or -1 */
{
    mc.combo = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
    if (k >= 0) jw.splash[k] = 500;
    sfx_ex(43, 0, 400, W_SAW, 3400, 2, 120, 0, 0, 65150);
    if (mc.lives <= 0) mach_over();
}
static void jw_ticket(void)
{
    uint32_t i, r, k;
    for (i = 0; i < JW_NTK; i++) if (!jw.tk[i].on) break;
    if (i == JW_NTK) return;
    r = ihash(jw.seed + jw.nseed * 7u); jw.nseed++;
    jw.tk[i].n = (uint8_t)(jw.served >= 12 && (r & 7u) < 2u ? 3 : jw.served >= 3 && (r & 7u) < 5u ? 2 : 1);
    for (k = 0; k < jw.tk[i].n; k++) {
        uint32_t c = ihash(r + k * 31u) % 4u, u;
        if (k && c == jw.tk[i].c[k - 1]) c = (c + 1u + ihash(r + k * 5u) % 3u) % 4u;   /* a layer is never its neighbour's colour */
        u = 1u + ihash(r + k * 13u + 3u) % (jw.tk[i].n == 1 ? 4u : jw.tk[i].n == 2 ? 3u : 2u);
        jw.tk[i].c[k] = (uint8_t)c; jw.tk[i].u[k] = (uint8_t)u;
    }
    jw.tk[i].life = imax(20000, 42000 - jw.served * 1100) + (jw.tk[i].n - 1) * 9000;
    if (jw.tk[i].n > 1 && jw.multi++ < 2) jw.tk[i].life *= 2;   /* the first two mixed drinks: twice the time */
    jw.tk[i].t = jw.tk[i].life; jw.tk[i].on = 1;
    sfx(79, 0, 120, W_SQUARE, 1400, 2, 120); sfx(86, 90, 200, W_SQUARE, 1400, 2, 120);   /* a bell: an order */
}
static void jw_serve(int z)
{
    jw_glass_t *g = &jw.g[z];
    int32_t i, k, best_i = -1, best_e = 0x7FFFFFFF;
    jw.has[z] = 0; jw.out = *g; jw.out_t = 500; jw.out_x = JW_X(z); jw.out_ok = 0;
    if (!g->n) { snd_ui(60); return; }           /* an empty glass back on the shelf: nothing lost */
    for (i = 0; i < JW_NTK; i++) {
        int32_t e = 0;
        if (!jw.tk[i].on || jw.tk[i].n != g->n) continue;
        for (k = 0; k < g->n; k++) {
            if (jw.tk[i].c[k] != g->c[k]) break;
            e = imax(e, iabs(g->v[k] - Q8(jw.tk[i].u[k])));
        }
        if (k == g->n && e <= JW_TOL && e < best_e) { best_e = e; best_i = i; }
    }
    if (best_i < 0) { jw_hurt(z); return; }      /* nobody asked for that */
    mc.combo++; jw.served++; jw.out_ok = 1;
    mc.score += 10 * g->n + (best_e < 64 ? 10 : 0) + jw.tk[best_i].t * 10 / jw.tk[best_i].life + imin(mc.combo, 10);
    mach_msg(best_e < 64 ? T_PERFECT : T_GOOD);
    jw.tk[best_i].on = 0;
    for (k = 0; k < g->n; k++) sfx(72 + SC_PEN[(g->c[k] + k) % 5], (uint32_t)k * 90u, 500, W_SINE, 2600, 3, 170);
    snd_chime(best_e < 64);
}
static void juice_tick(int32_t dt)
{
    uint32_t down = in.kdown, up = jw.prev_keys & ~in.keys, zheld = 0;
    int32_t k, i;
    for (k = 0; k < 4; k++) {                    /* the taps */
        int32_t d = in.enc[ENC_KNOB1 + k];
        if (d) {
            int v = iclamp(jw.valve[k] + d, 0, 3);
            if (v != jw.valve[k]) { jw.valve[k] = (uint8_t)v; sfx(48 + v * 5 + k * 2, 0, 90, W_SQUARE, 1300, 1, 60); }
        }
    }
    for (k = 0; k < AC_NKEY; k++) if (in.keys >> k & 1u) zheld |= 1u << jw_zone(k);
    for (k = 0; k < AC_NKEY; k++)                /* a key let go over a glass that was there when it went down: served */
        if (up >> k & 1u) {
            int z = jw_zone(k);
            if (jw.has[z] && !jw.used[z] && !(zheld >> z & 1u)) { jw_serve(z); jw.used[z] = 1; if (mc.state != MS_PLAY) return; }
        }
    for (k = 0; k < 4; k++) if (!(zheld >> k & 1u)) jw.used[k] = 0;
    for (k = 0; k < AC_NKEY; k++)
        if (down >> k & 1u) {
            int z = jw_zone(k), a = -1;
            if (!jw.has[z]) {
                for (i = 0; i < 4; i++) if (i != z && jw.has[i] && (zheld >> i & 1u)) a = i;
                if (a >= 0) {                    /* held there, pressed here: the glass moves */
                    jw.g[z] = jw.g[a]; jw.has[z] = 1; jw.has[a] = 0; jw.used[a] = 1; jw.used[z] = 1;
                    sfx(76, 0, 120, W_TRI, 2200, 2, 120); sfx(96, 0, 60, W_NOISE, 500, 1, 60);
                } else {                         /* a fresh glass */
                    jw.has[z] = 1; jw.g[z].n = 0; jw.used[z] = 1;
                    sfx(88 + z * 2, 0, 160, W_SINE, 1800, 1, 160);
                }
            }
        }
    jw.prev_keys = in.keys;
    for (k = 0; k < 4; k++) {                    /* what is open, flows */
        int32_t add = (int32_t)JW_RATE[jw.valve[k]] * dt / 1000;
        if (jw.splash[k] > 0) jw.splash[k] -= dt;
        if (!add) { jw.puddle[k] = imax(0, jw.puddle[k] - dt / 8); continue; }
        if (jw.has[k]) {
            jw_glass_t *g = &jw.g[k];
            int32_t was = jw_total(g), now;
            if (!g->n || (g->c[g->n - 1] != k && g->n < 3)) { g->c[g->n] = (uint8_t)k; g->v[g->n] = 0; g->n++; }
            g->v[g->n - 1] += add;
            now = was + add;
            if ((now >> 8) != (was >> 8) && (now >> 8) <= JW_CAP)   /* each part reached, a note higher: the ear can count */
                sfx_ex(60 + SC_PEN[(now >> 8) % 5] + 12 * ((now >> 8) / 5), 0, 220, W_TRI, 2400, 2, 150, 0, (k * 2 - 3) * 25, 0);
            if (now > Q8(JW_CAP) + 80) { jw.has[k] = 0; jw_hurt(k); if (mc.state != MS_PLAY) return; }   /* over the brim: lost */
        } else {
            jw.puddle[k] += add;
            if (jw.puddle[k] > 384) { jw.puddle[k] = 0; jw_hurt(k); if (mc.state != MS_PLAY) return; }
        }
    }
    for (i = 0; i < JW_NTK; i++)                 /* the tickets: each waits only so long */
        if (jw.tk[i].on) {
            jw.tk[i].t -= dt;
#ifdef AC_TEST_CATCH
            if (jw.tk[i].t < 2000) jw.tk[i].t = 2000;
#endif
            if (jw.tk[i].t <= 0) { jw.tk[i].on = 0; jw_hurt(-1); if (mc.state != MS_PLAY) return; }
        }
    jw.spawn_t -= dt;
    if (jw.spawn_t <= 0) { jw_ticket(); jw.spawn_t = imax(3800, 8500 - jw.served * 260); }
    if (jw.out_t > 0) jw.out_t -= dt;
    for (k = 0; k < AC_NKEY; k++) {              /* the keys under a glass are lit, the rest glow */
        if (jw.has[jw_zone(k)]) led_key_want |= 1u << k; else led_key_dim_want |= 1u << k;
    }
}
static void jw_glass_draw(int32_t x, int32_t y, int32_t hw, const jw_glass_t *g, col_t rim)   /* y: its foot */
{
    int32_t k, yy = y - 1, h = JW_CAP * JW_UPX + 4;
    for (k = 0; k < g->n; k++) {
        int32_t lh = imin(g->v[k] * JW_UPX >> 8, yy - (y - h));
        if (lh > 0) { grect(x - hw + 1, yy - lh, hw * 2 - 1, lh, JW_COL[g->c[k]]); yy -= lh; }
    }
    gline(x - hw, y - h, x - hw, y, rim); gline(x + hw, y - h, x + hw, y, rim); ghl(x - hw, x + hw + 1, y, rim);
    for (k = 1; k <= JW_CAP; k++) ghl(x - hw + 1, x - hw + (k % 3 ? 3 : 5), y - 1 - k * JW_UPX, rim);   /* a mark to a part */
}
static void juice_draw(void)
{
    uint32_t tm = (uint32_t)mc.t;
    int32_t k, i;
    gclear(th->bg);
    for (k = 0; k < 10; k++) {                   /* the cellar wall: slow bubbles of the four juices */
        int32_t q = (int32_t)((tm / 14u + (uint32_t)k * 97u) % 260u);
        galpha(26); gdisc(12 + (k * 53) % 220 + (tsin(tm, 9, k * 7000) >> 12), 250 - q, 3 + k % 4, JW_COL[k % 4]);
    }
    galpha(256);
    ghl(0, 240, 16, th->rim);                    /* the main pipe the four tanks hang from */
    for (k = 0; k < 4; k++) {
        int32_t x = JW_X(k), v = jw.valve[k], top = JW_FLOOR;
        col_t c = JW_COL[k];
        char nm[2] = {(char)('1' + k), 0};
        grect(x - 1, 16, 3, 6, th->rim);
        galpha(70 + (tsin(tm, 18, k * 15000) >> 9)); gdisc(x, 36, 24, c);          /* its glow */
        galpha(256);
        gellf(x, 40, 21, 18, th->bg, 1); grect(x - 21, 40, 43, 9, th->bg);         /* the tank: a cap of glass, juice in it */
        galpha(190); gellf(x, 40, 19, 15, c, 1); grect(x - 19, 40, 39, 7, c); galpha(256);
        garc(x, 40, 21, 18, 32768, 65536, th->text, 0); gline(x - 21, 40, x - 21, 48, th->text); gline(x + 21, 40, x + 21, 48, th->text); ghl(x - 21, x + 22, 48, th->text);
        gtextc(x + 1, 33, nm, th->bg, 2);
        grect(x - 3, 49, 7, 11, th->panel); gbox(x - 3, 49, 7, 11, th->text);      /* the tap, and how far it is open */
        for (i = 0; i < 3; i++) grect(x + 8 + i * 5, 52, 3, 6, i < v ? c : th->mute);
        if (jw.has[k]) top = JW_FLOOR - 1 - imin(JW_CAP * JW_UPX + 3, jw_total(&jw.g[k]) * JW_UPX >> 8);
        if (v) {                                 /* the stream */
            int32_t w = v + 1, y;
            for (y = 60; y < top; y += 4) grect(x - w / 2 + ((tsin(tm, 90, y * 900) > 9000) ? 1 : 0), y, w, imin(4, top - y), c);
            galpha(150); gdisc(x, top, 3 + v + (int32_t)((tm / 60u) & 1u), c); galpha(256);
        }
        if (jw.puddle[k] > 0) { galpha(200); gellf(x, JW_FLOOR, 4 + jw.puddle[k] / 22, 2, c, 1); galpha(256); }
        if (jw.has[k]) jw_glass_draw(x, JW_FLOOR, 13, &jw.g[k], th->text);
        else { galpha(70); gbox(x - 13, JW_FLOOR - 4, 27, 4, th->mute); galpha(256); }   /* where a glass goes */
        if (jw.splash[k] > 0)
            for (i = 0; i < 7; i++) {
                int32_t a = 34000 + i * 4700, r = (500 - jw.splash[k]) * 30 / 500 + 4;
                galpha(jw.splash[k] * 256 / 500); grect(x + ((r * icos(a)) >> 14), JW_FLOOR - 6 + ((r * isin(a)) >> 14), 3, 3, c);
            }
        galpha(256);
        grect(x - 24, JW_FLOOR + 10, 49, 3, jw.has[k] ? c : th->mute);             /* the keys under this tap */
        for (i = 0; i < 7; i++) grect(x - 24 + i * 7, JW_FLOOR + 15, 6, 9, (in.keys && jw_zone(in.hikey < 0 ? 0 : in.hikey) == k) ? c : th->panel);
        for (i = 0; i < 7; i++) gbox(x - 24 + i * 7, JW_FLOOR + 15, 6, 9, jw.has[k] ? c : th->mute);
    }
    ghl(0, 240, JW_FLOOR + 1, th->rim); ghl(0, 240, JW_FLOOR + 4, th->mute);
    for (i = 0; i < JW_NTK; i++)                 /* the tickets: the drink as it should stand in the glass */
        if (jw.tk[i].on) {
            int32_t x = 60 + i * 60, left = jw.tk[i].t * 256 / jw.tk[i].life, sway = tsin(tm, 14, i * 20000) >> 13;
            jw_glass_t g;
            g.n = jw.tk[i].n;
            for (k = 0; k < g.n; k++) { g.c[k] = jw.tk[i].c[k]; g.v[k] = Q8(jw.tk[i].u[k]); }
            if (left < 64 && (tm / 160u) & 1u) galpha(120);
            gline(x, 16, x + sway, 74, th->mute);   /* (the glass on it is the one under the taps: same width, same marks) */
            grect(x - 17 + sway, 74, 35, 66, th->panel); gbox(x - 17 + sway, 74, 35, 66, left < 64 ? th->caps[0] : th->text);
            grect(x - 15 + sway, 77, 31 * left / 256, 3, left < 64 ? th->caps[0] : left < 128 ? JW_COL[0] : JW_COL[2]);
            jw_glass_draw(x + sway, 135, 13, &g, th->text);
            galpha(256);
        }
    if (jw.out_t > 0) {                          /* a glass on its way out: up and off if it was right, dropped if not */
        int32_t q = 500 - jw.out_t;
        galpha(jw.out_t * 256 / 500);
        jw_glass_draw(jw.out_x, JW_FLOOR + (jw.out_ok ? -q * 40 / 500 : q * 30 / 500), 13, &jw.out, jw.out_ok ? th->pad : th->caps[0]);
        galpha(256);
    }
    if (mc.flash > 0) { galpha(mc.flash * 110 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
}

/* ---------------------------------------------------------------- NIGHT BLOOM */
#define NB_MAX 24
static const uint8_t NB_R[4] = {26, 42, 58, 74};   /* drawn in from the frame: the spores have room to be seen coming */
#define NB_ARC 7000                              /* a petal's half width */
/* a ring, its petal and its spores share one colour no other ring could be taken for: yellow, white, green, blue */
/* the four rings' knobs, in the order they join: KNOB 1 (right of the screen), SELECT (left, above), ALGORITHM (left,
 * below), and KNOB 2 only for whoever has caught fifty */
static const uint8_t NB_ENC[4] = {ENC_KNOB1, ENC_SELECT, ENC_ALGORITHM, ENC_KNOB2};
static const char *const NB_KNOB[4] = {"KNOB 1", "SELECT\nKNOB", "ALGORITHM\nKNOB", "KNOB 2"};
static const uint8_t NB_JOIN[4] = {0, 6, 28, 58};   /* spores caught before each ring joins (room to learn each knob) */
static const col_t NB_COL[4] = {AC_RGB(255, 214, 40), AC_RGB(255, 255, 255), AC_RGB(40, 224, 96), AC_RGB(48, 112, 255)};
/* where each knob's arrow points from (the FM-1's knobs are off the screen that way): its tip, its direction (angle
 * units), the middle of its words */
static const int16_t NB_TAG[4][5] = {{226, 28, -8192, 172, 46}, {14, 28, -24576, 58, 46}, {12, 212, 24576, 76, 172}, {232, 40, -2500, 170, 36}};
#define NB_TAG_MS 4200                           /* how long a joining knob's arrow stays */
static struct {
    int32_t ang[4], hit[4];
    struct { uint32_t born; int16_t a, curl; uint8_t c, on, past; } sp[NB_MAX];
    struct { int16_t x, y; uint8_t c; } fl[48];
    uint32_t nfl, last_beat, t0;
    int32_t rings, caught, ring_t;
} nb;
/* an arrow at (x, y) pointing along ang (bobbing toward where it points), and the words of knob k by it, centred on
 * (lx, ly), each line its own: for a new knob, here and in HOME's loop (ac_mtutor.h) */
static void nb_tag(int32_t x, int32_t y, int32_t ang, int32_t lx, int32_t ly, int k, int32_t a, uint32_t tm, int32_t sc)
{
    int32_t bob = (tsin(tm, 70, 0) + 16384) * 4 / 32768, len = 10 * sc + 6, dx = icos(ang), dy = isin(ang), i, xy[6], nl = 1, wmax = 0;
    const char *s = NB_KNOB[k], *p;
    char line[12];
    col_t c = NB_COL[k];
    if (a <= 0) return;
    x += (bob * dx) >> 14; y += (bob * dy) >> 14;
    galpha(a);
    for (i = -1; i <= 1; i++)                    /* the shaft, three px thick, and the head */
        gline(x - ((len * dx) >> 14) + ((i * -dy) >> 14), y - ((len * dy) >> 14) + ((i * dx) >> 14), x - ((6 * dx) >> 14) + ((i * -dy) >> 14), y - ((6 * dy) >> 14) + ((i * dx) >> 14), c);
    xy[0] = x + ((2 * dx) >> 14); xy[1] = y + ((2 * dy) >> 14);
    xy[2] = x - ((9 * dx + 6 * -dy) >> 14); xy[3] = y - ((9 * dy + 6 * dx) >> 14);
    xy[4] = x - ((9 * dx - 6 * -dy) >> 14); xy[5] = y - ((9 * dy - 6 * dx) >> 14);
    gpoly(xy, 3, c);
    for (p = s; *p; p++) if (*p == '\n') nl++;
    for (p = s; *p; ) {                          /* the words: a dark plate, then each line centred */
        int32_t m = 0;
        while (*p && *p != '\n' && m < 11) line[m++] = *p++;
        line[m] = 0; if (*p) p++;
        wmax = imax(wmax, gtextw(line, sc));
    }
    galpha(a * 200 / 256);
    grect(lx - wmax / 2 - 4, ly - nl * TLH(sc) / 2 - 3, wmax + 8, nl * TLH(sc) + 4, th->bg);
    galpha(a);
    for (p = s, i = 0; *p; i++) {
        int32_t m = 0;
        while (*p && *p != '\n' && m < 11) line[m++] = *p++;
        line[m] = 0; if (*p) p++;
        gtextc(lx, ly - nl * TLH(sc) / 2 + i * TLH(sc) + sc, line, c, sc);
    }
    galpha(256);
}
static void bloom_spawn(uint32_t now, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < NB_MAX; i++)
        if (!nb.sp[i].on) {
            nb.sp[i].on = 1; nb.sp[i].past = 0; nb.sp[i].born = now; nb.sp[i].c = (uint8_t)(ihash(seed) % (uint32_t)nb.rings);
            nb.sp[i].a = (int16_t)ihash(seed + 1u); nb.sp[i].curl = (int16_t)((int32_t)(ihash(seed + 2u) & 0x3FFFu) - 0x2000);
            return;
        }
}
/* a spore's place: it comes in slowly from beyond the screen's corners (seven beats on the way), easing a little as it
 * nears its own ring; there it lingers two thirds of a beat, then falls to the flower. It can be caught all the while it
 * is at its ring: from half a beat before it arrives until it leaves (NB_EARLY .. NB_LINGER) */
#define NB_IN 672
#define NB_FAR 182                               /* where it starts: past the corners (the farthest is 174 px out) */
#define NB_EARLY 48
#define NB_LINGER 64
#define NB_OUT 192
static void bloom_pos(uint32_t i, uint32_t now, int32_t *rad, int32_t *ang)
{
    int32_t age = (int32_t)(now - nb.sp[i].born), ring = NB_R[nb.sp[i].c], q;
    if (age <= NB_IN) { q = (NB_IN - age) * 256 / NB_IN; *rad = ring + (NB_FAR - ring) * (3 * q + q * q / 256) / 1024; }
    else if (age <= NB_IN + NB_LINGER) *rad = ring - 3 * (age - NB_IN) / NB_LINGER;
    else *rad = ring - 3 - (ring - 9) * imin(NB_OUT, age - NB_IN - NB_LINGER) / NB_OUT;
    *ang = nb.sp[i].a + nb.sp[i].curl * age / NB_IN;
}
static void bloom_tick(int32_t dt)
{
    uint32_t now = (tr_now_q8() >> 8) - nb.t0, i, beat2;
    int32_t k, hard = nb.caught >= 90;
    if (in.kdown) snd_keys(in.kdown, 1);         /* the keys play along (in the tune's key); the knobs do the catching */
    for (k = 0; k < 4; k++) {
        if (k < nb.rings && in.enc[NB_ENC[k]]) nb.ang[k] += in.enc[NB_ENC[k]] * 2731;   /* a click of the knob: a twenty-fourth of a turn */
        if (nb.hit[k] > 0) nb.hit[k] -= dt;
    }
    if (nb.ring_t > 0) nb.ring_t -= dt;
    /* the pace: one ring, a spore every other beat. A new ring starts at that pace too, and only as more are caught
     * does one come now and then between (more with three rings, most beats with four); past ninety caught, every
     * beat and then half beats too */
    beat2 = now / (hard ? 48u : 96u);
    if (beat2 != nb.last_beat) {
        uint32_t r = ihash(beat2 * 5u + nb.t0);
        int go;
        nb.last_beat = beat2;
        if (hard) go = !(beat2 & 1u) || r < (uint32_t)imin(36000, 4000 + (nb.caught - 90) * 600);
        else {
            static const uint16_t CAP[5] = {0, 0, 12000, 18000, 34000};
            int32_t into = nb.caught - NB_JOIN[nb.rings - 1];   /* caught since this ring joined */
            go = !(beat2 & 1u) || r < (uint32_t)imin(CAP[nb.rings], imax(0, into - 4) * 1500);
        }
        if (go && (nb.rings == 1 || nb.ring_t < NB_TAG_MS - 2400)) bloom_spawn(beat2 * (hard ? 48u : 96u), beat2 * 11u + nb.t0);   /* (a breather while a new ring arrives) */
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t age;
        if (!nb.sp[i].on) continue;
        age = (int32_t)(now - nb.sp[i].born);
        if (!nb.sp[i].past && age >= NB_IN - NB_EARLY) {   /* it is at its ring: is the petal there? */
            int32_t rad, ang, d;
            bloom_pos(i, now, &rad, &ang);
            d = (int32_t)(int16_t)(uint16_t)(ang - nb.ang[nb.sp[i].c]);
#ifdef AC_TEST_CATCH                             /* (tests) every spore is caught: the rings arriving one by one */
            if (age >= NB_IN) { d = 0; nb.ang[nb.sp[i].c] = ang; }
#endif
            if (iabs(d) < NB_ARC) {
                int32_t c = nb.sp[i].c;
                nb.sp[i].on = 0; mc.combo++; mc.score += 1 + mc.combo / 5; nb.hit[c] = 220; nb.caught++;
                sfx_ex(60 + 12 * (c / 2) + SC_PEN[(c * 2 + ((uint32_t)ang >> 13)) % 5], 0, 700, c & 1 ? W_TRI : W_SINE, 3200, 4, 170, 0, (icos(ang) * 60) >> 14, 0);
                if (nb.nfl < 48u) {              /* a flower opens where the garden has room */
                    uint32_t s = ihash(nb.nfl * 3u + nb.t0), e = nb.nfl % 4u;
                    nb.fl[nb.nfl].x = (int16_t)(e < 2u ? 6 + (int32_t)(s * 228u >> 16) : (e == 2u ? 6 + (int32_t)(s & 15u) : 218 + (int32_t)(s & 15u)));
                    nb.fl[nb.nfl].y = (int16_t)(e == 0u ? 28 + (int32_t)(s & 7u) : e == 1u ? 222 + (int32_t)(s & 7u) : 40 + (int32_t)(s * 170u >> 16));
                    nb.fl[nb.nfl].c = (uint8_t)c; nb.nfl++;
                }
                if (nb.rings < 4 && nb.caught >= NB_JOIN[nb.rings]) {   /* enough caught: the next ring, and its knob, join in */
                    nb.rings++; nb.ring_t = NB_TAG_MS; snd_chime(1);
                }
                continue;
            }
            if (age > NB_IN + NB_LINGER) { nb.sp[i].past = 1; mc.combo = 0; }   /* gone by its ring: it will reach the flower */
        }
        if (nb.sp[i].past && age >= NB_IN + NB_LINGER + NB_OUT) {       /* it reached the flower: a petal falls */
            nb.sp[i].on = 0; mc.lives--; mc.flash = 300; mach_msg(T_MISS);
            sfx_ex(43, 0, 400, W_SAW, 3200, 2, 120, 0, 0, 65200);
            if (mc.lives <= 0) { mach_over(); return; }
        }
    }
    led_btn_want |= (now % 96u < 14u) ? 1u << BTN_PLAY : 0u;
    led_key_want |= in.keys;
}
static void bloom_draw(void)
{
    uint32_t now = (tr_now_q8() >> 8) - nb.t0, i, tm = (uint32_t)mc.t;
    int32_t cx = 120, cy = 126, k, beat = 256 - imin(256, (int32_t)(now % 96u) * 256 / 50);
    sky_draw();
    for (i = 0; i < nb.nfl; i++) {               /* the garden so far: faint, flat little flowers (nothing like a spore's glow) */
        col_t c = NB_COL[nb.fl[i].c];
        int32_t x = nb.fl[i].x, y = nb.fl[i].y, s = 2 + ((tsin(tm, 20, (int32_t)i * 7000) + 16384) >> 14);
        galpha(80);
        gp(x - s, y, c); gp(x + s, y, c); gp(x, y - s, c); gp(x, y + s, c); gp(x, y, c);
        galpha(256);
    }
    for (k = 0; k < nb.rings; k++) {
        col_t c = NB_COL[k];
        int32_t fresh = (k == nb.rings - 1 && nb.ring_t > 0) ? nb.ring_t : 0, on_ = nb.hit[k] > 0 || (fresh && (fresh / 150) % 2);
        galpha(fresh ? 200 : 96); garc(cx, cy, NB_R[k], NB_R[k], 0, 65536, c, 1);
        galpha(256);
        garc(cx, cy, NB_R[k] - 1, NB_R[k] - 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, on_ ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 1, NB_R[k] + 1, nb.ang[k] - NB_ARC, nb.ang[k] + NB_ARC, on_ ? th->text : c, 1);
        garc(cx, cy, NB_R[k] + 3, NB_R[k] + 3, nb.ang[k] - NB_ARC + 900, nb.ang[k] + NB_ARC - 900, c, 1);
        if (nb.hit[k] > 0) { galpha(nb.hit[k]); gdisc(cx + ((NB_R[k] * icos(nb.ang[k])) >> 14), cy + ((NB_R[k] * isin(nb.ang[k])) >> 14), 9, c); galpha(256); }
    }
    for (i = 0; i < NB_MAX; i++) {
        int32_t rad, ang, x, y;
        col_t c;
        if (!nb.sp[i].on) continue;
        bloom_pos(i, now, &rad, &ang);
        c = NB_COL[nb.sp[i].c];
        x = cx + ((rad * icos(ang)) >> 14); y = cy + ((rad * isin(ang)) >> 14);
        galpha(nb.sp[i].past ? 50 : 130 + (tsin(tm, 50, (int32_t)i * 9000) >> 10)); gdisc(x, y, 7, c); galpha(256);
        gdisc(x, y, 3, nb.sp[i].past ? th->mute : c);
        if (!nb.sp[i].past) gp(x, y, th->text);
    }
    for (k = 0; k < 5; k++) {                    /* the flower: a petal for each life */
        int32_t a = k * 13107 + (int32_t)(tm * 2u), on = k < mc.lives;
        galpha(on ? 256 : 50);
        gdisc(cx + (((7 + beat / 64) * icos(a)) >> 14), cy + (((7 + beat / 64) * isin(a)) >> 14), 4, on ? th->caps[0] : th->mute);
    }
    galpha(256); gdisc(cx, cy, 4 + beat / 100, th->pad);
    if (nb.ring_t > 0) {                         /* a new ring: an arrow to its knob, in the ring's own colour */
        const int16_t *t = NB_TAG[nb.rings - 1];
        nb_tag(t[0], t[1], t[2], t[3], t[4], (int)nb.rings - 1, imin(256, imin(nb.ring_t, NB_TAG_MS - nb.ring_t + 150) * 256 / 300), tm, 2);
    }
    if (mc.flash > 0 && mc.msg == T_MISS && mc.msg_t > 0) { galpha(mc.flash * 110 / 300); grect(0, 0, 240, 240, th->caps[0]); galpha(256); }
}

/* ---------------------------------------------------------------- all three */
static void mach_start(void)
{
    uint32_t i, now = tr_now_q8() >> 8;
    mc.state = MS_PLAY; mc.score = 0; mc.combo = 0; mc.t = 0; mc.msg_t = 0; mc.flash = 0;
    if (mc.which == 0) gh_start();
    else if (mc.which == 1) {
        for (i = 0; i < sizeof jw; i++) ((uint8_t *)&jw)[i] = 0;
        mc.lives = 5; jw.seed = now * 31u + 7u; jw.spawn_t = 700; jw.prev_keys = in.keys;
#ifdef AC_TEST_CATCH
        jw.seed = 1;
#endif
    } else if (mc.which == 2) {
        for (i = 0; i < sizeof nb; i++) ((uint8_t *)&nb)[i] = 0;
        mc.lives = 5; nb.t0 = (now / 96u) * 96u; nb.rings = 1; nb.ring_t = NB_TAG_MS;
        for (i = 0; i < 4u; i++) nb.ang[i] = (int32_t)i * 16384 - 16384;
    }
#ifdef AC_TEST_CATCH                             /* (tests) lives enough to see a long way in */
    if (mc.which) mc.lives = 99;
#endif
    snd_chime(0);
}
static void mach_enter(int which)
{
    uint32_t i;
    mc.which = (uint8_t)which; mc.state = MS_TITLE; mc.t = 0; mc.msg_t = 0; mc.flash = 0; mc.score = 0; mc.lives = 0;
    gh.card_track = (uint8_t)(mus_track % JUKE_N); gh.swap_t = -10000; gh.demo_track = 255;
    (void)i;
#ifdef AC_TEST_SCENE
    if (which == 0) gh_test_charts();
#endif
}
/* -1: REC at its start or end, back to the Arcave; 1: HOME, how it is played (arcave.c opens the sheet) */
static int mach_tick(int32_t dt)
{
    mc.t += dt;
    if (mc.msg_t > 0) mc.msg_t -= dt;
    if (mc.flash > 0) mc.flash -= dt;
    if (BDOWN(BTN_HOME)) return 1;
    if (mc.state != MS_PLAY) {
        if (BDOWN(BTN_REC)) return -1;
        if (mc.which == 0) {                     /* (CRYSTAL ROAD: KNOB 1 the mode; no song recorded, no start) */
            int ok = gh_card_tick();
            if (BDOWN(BTN_PLAY)) { if (ok) mach_start(); else snd_ui(55); }
            else { led_play_breathe = (uint8_t)ok; led_breathe_want |= 1u << BTN_HOME | 1u << BTN_REC; }
            return 0;
        }
        if (BDOWN(BTN_PLAY)) mach_start();
        else { led_play_breathe = 1; led_breathe_want |= 1u << BTN_HOME | 1u << BTN_REC; }
        return 0;
    }
    if (mc.which == 0) road_tick(dt); else if (mc.which == 1) juice_tick(dt); else bloom_tick(dt);
    return 0;
}
static void mach_draw(void)
{
    if (mc.which == 0) { road_draw(); if (mc.state != MS_PLAY) gh_card(); return; }
    if (mc.which == 1) juice_draw(); else bloom_draw();
    if (mc.state == MS_PLAY) mach_hud();
    else mach_card(MACH_NAME[mc.which], th->caps[mc.which * 2 % 5]);
}
