/* SPDX-License-Identifier: GPL-3.0-only */
/* UNDERSONG: a game for two FM-1s, down the well in the Arcave's floor. Under the Arcave sleeps the lower half of its
 * song, and no rock can wake it alone: two players, each on their own FM-1, each seeing the other's rock, keys and
 * dial on their own screen, play it awake together.
 *
 *   the story     four chapters, each a game for two on the shared clock, in this order (DU_CHAP):
 *     POTION CHOIR  eight rocks, no two alike, in another order on each screen. In turns: one sees which rock needs
 *                     the potion and says what it looks like; the other moves the potion there and pours
 *     TWIN ROAD       a song comes down the road, half of its notes yours, half your friend's, the big ones both at once
 *     ECHO STEPS      one plays a few notes; the other only sees the friend's rock and keys, and plays them back;
 *                     a bar counted in before each part (4, 3, 2, 1 on the beats), and they swap
 *     NOTE RALLY      one note, kept in the air: it comes down in a lane drawn at random each time, a knob rolls you
 *                     under it, a key sends it back on the beat
 *   ROCK-OFF      the one game against each other: the same song for both, who catches more (a key where no note
 *                 is costs points)
 *   afterwards    the story played through, its four games can be played again for the best score; TWIN ROAD then
 *                 on any of five songs
 *
 *   for three     the same five, for three FM-1s at once: in ECHO STEPS the other two echo together, TWIN ROAD is
 *                 split three ways, in POTION CHOIR the two who tell each see half of who needs it, NOTE RALLY's note
 *                 goes round the three, ROCK-OFF is all against all. Who is in the well when the list comes up plays;
 *                 a third can still come while the list is up. Anyone gone in the middle stops the game for all, with
 *                 an error that says who.
 *
 * The link. An FM-1 only sends MIDI over USB, so they talk through a relay: the Link page (web/link/), or
 * emulators in one browser. Everything is a control change on channel 16, as ARCAVE LINK's own messages are, and
 * (but "I am here") carries the sender's seat s:
 *   CC 110 n      "I am here", every 300 ms; n is a number drawn at the door: the lowest is seat 0 and leads, the
 *                 next seat 1, the next seat 2 (a fourth waits at the door)
 *   CC 112 k+32s  a key pressed (k 0..26)    CC 113 k+32s  a key let go (127: all)    CC 114 d+16s  the pipe's dial
 *   CC 118 c, CC 115 a, CC 116 b, CC 117 op+16s   a message with three numbers; the op comes last (DO_...)
 *   (CC 114 d is also the rock the potion is over, and your lane in NOTE RALLY)
 * The Link page passes the four of a message on together, so two players' messages never mix on the way to a third.
 * Seat 0 also says where its clock is four times a second (ac_sync.h, CC 102-107 with sender 5 and "follow"): the
 * others follow, so all hear the same bar. Each FM-1 judges its own player against that clock and tells the others
 * what happened, so the delay of the link never costs a note. */

enum { DM_ECHO, DM_ROAD, DM_PIPE, DM_DRUM, DM_DUEL, DM_N };
static const char *const DU_NAME[DM_N] = {"ECHO STEPS", "TWIN ROAD", "POTION CHOIR", "NOTE RALLY", "ROCK-OFF"};
/* the story's chapters in order: POTION CHOIR, TWIN ROAD, ECHO STEPS, NOTE RALLY */
static const uint8_t DU_CHAP[4] = {DM_PIPE, DM_ROAD, DM_ECHO, DM_DRUM};
static int du_chap(int m) { int i; for (i = 0; i < 4; i++) if (DU_CHAP[i] == m) return i; return 4; }
static const uint8_t DU_TRACK[DM_N] = {4, 1, 3, 5, 2};   /* OPAL TIDE, SPORE DRIFT, MYCELIUM NIGHTS, ROLL ON, CRYSTAL STEPS */
enum { DUS_LOBBY, DUS_MENU, DUS_CARD, DUS_COUNT, DUS_PLAY, DUS_RESULT, DUS_FINALE, DUS_ERROR };
enum { DO_HELLO = 1, DO_EPOCH, DO_PICK, DO_OPEN, DO_READY, DO_GO, DO_HIT, DO_MISS, DO_QUIT, DO_STRAY, DO_SONG, DO_POUR };
#define DU_YL 180                                /* the line the notes are played on */
#define DU_LOOK 288                              /* a note is seen three beats ahead */
#define DU_WIN 34                                /* ticks either side that still count */
#define DU_PERF 10
#define DU_NOTES 160
#define DU_PIPE_N 12                             /* (the dial's message: a rock of POTION CHOIR, a lane of NOTE RALLY) */
#define DU_ECHO_R 6                              /* ECHO STEPS: six rounds, */
#define DU_ECHO_LEN 1536                         /* each four bars: the call, a count, the echo, a count to the next call */
#define DU_POT_N 8                               /* POTION CHOIR: eight rocks, */
#define DU_POT_R 6                               /* six of them need the potion, one a round, */
#define DU_POT_BARS 8                            /* and a round is over after eight bars at the latest */
#define DU_RALLY_MAX 3600                        /* NOTE RALLY: the score that is three stars' worth and more */
static const uint8_t DU_ROAD_SONG[5] = {4, 1, 3, 2, 5};   /* TWIN ROAD's songs once the story is told, slowest first; [1] is the story's */
static uint16_t duo_road[4];                     /* (kept in the save) TWIN ROAD's best on tunes 2..5; SPORE DRIFT's is duo_best[DM_ROAD] */
typedef struct { uint8_t shape, eyes, top, mouth, col, awake; } du_ch_t;   /* a rock of the deep choir: how it looks */
static du_ch_t du_ch[DU_POT_N];
#define DU_PAGE_MS 4000                          /* the waiting screen: how long each of its lines stays */
#define DU_OUT_MS 320                            /* and how long it takes to go dark when REC is pressed there */
static const col_t DU_COL[4] = {AC_RGB(240, 96, 200), AC_RGB(110, 232, 210), AC_RGB(255, 150, 50), AC_RGB(150, 150, 150)};   /* seats 0, 1, 2 (and a fourth, waiting) */

static uint16_t duo_best[DM_N];                  /* (kept in the save) the best of each game; ROCK-OFF: your own */
static uint16_t trio_best[DM_N], trio_road[4];   /* (kept in the save) the same, played by three */
static uint8_t duo_prog;                         /* chapters cleared, 0..4 */

typedef struct { uint16_t step; uint8_t lane, own, hid, pair, st[3]; int8_t pitch; } du_note_t;   /* own: bit = seat; st: 0 to come, 1 PERFECT, 2 GOOD, 3 missed */
static du_note_t du_n[DU_NOTES];
static struct {
    uint8_t on, state, seat, nonce, np, gone;    /* np: the seats taken (2 or 3); gone: the seat that left (DUS_ERROR) */
    uint8_t ros[4], prog[4], pst[4], pnon[4];    /* each seat's number, chapters and state; the numbers heard */
    uint32_t plast[4];                           /* (when each of those was last heard) */
    uint8_t cur, mission, story, page, ready[4], n, latched, finale, stars, cleared;
    uint8_t ra, rb, rc, dial[4], lcol[4], msg;
    uint32_t pkeys[4], pk_down[4], last_rx, last_tx, t0;
    int32_t t, end_tick, beat, score, max, sc[4], msg_t, hop[4], rx[4], tx[4], lflash[4];
    uint8_t song, pr, bmode, bown, bfrom, bto, csing, pperm[3][DU_POT_N];   /* song: TWIN ROAD's (DU_ROAD_SONG); pr: POTION CHOIR's round; b...: NOTE RALLY's note */
    uint16_t rally, rbest, vh[2];
    int32_t pstart, pend, bt0, bt1, bready, csing_t;
    int32_t out;                                 /* > 0: REC at the door, the picture going dark (ms) before the Arcave again */
    uint16_t stray[4];                           /* ROCK-OFF: keys pressed where no note was (30 points each: no winning by hammering) */
    int16_t lx[4];
} du;

static int du_live(int i) { return du.pnon[i] && fm1_ms - du.plast[i] < 2500u; }
static int du_seat_live(int s)                   /* a seat's FM-1 still heard from */
{
    int i;
    if (s >= du.np) return 0;
    if (du.ros[s] == du.nonce) return 1;
    for (i = 0; i < 4; i++) if (du.pnon[i] == du.ros[s] && du_live(i)) return 1;
    return 0;
}
/* who is in: this FM-1 and the ones heard in the last moments, the lowest number first; the first three have seats */
static void du_roster(void)
{
    uint8_t v[5], t;
    int n = 0, i, j;
    v[n++] = du.nonce;
    for (i = 0; i < 4; i++) if (du_live(i)) v[n++] = du.pnon[i];
    for (i = 1; i < n; i++) for (j = i; j > 0 && v[j] < v[j - 1]; j--) { t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    du.np = (uint8_t)imin(n, 3); du.seat = 3;
    for (i = 0; i < 4; i++) du.ros[i] = i < du.np ? v[i] : 0;
    for (i = 0; i < du.np; i++) if (v[i] == du.nonce) du.seat = (uint8_t)i;
}
static int du_peer(void)                         /* all the seats taken, and all still there */
{
    int s;
    if (du.np < 2 || du.seat >= 3u) return 0;
    for (s = 0; s < du.np; s++) if (!du_seat_live(s)) return 0;
    return 1;
}
static int du_all(void) { return (1 << du.np) - 1; }   /* (a note for everyone) */
static int du_next(int s) { return (s + 1) % imax(du.np, 2); }   /* NOTE RALLY: who a seat sends the note to */
static int du_giver(int r) { return du.np == 3 ? (r + 1) % 3 : !(r & 1); }   /* POTION CHOIR: who pours in round r */
static int du_bits(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1); }
static int32_t du_now(void) { return (int32_t)((tr_now_q8() >> 8) - du.t0); }
static int du_lane(int k) { return KEY_ZONE[k]; }
static void du_tx(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    midi_send(0xBFu, 118u, c & 127u); midi_send(0xBFu, 115u, a & 127u); midi_send(0xBFu, 116u, b & 127u); midi_send(0xBFu, 117u, (op & 15u) | (du.seat & 3u) << 4);
}
static void du_dial_tx(uint32_t v) { midi_send(0xBFu, 114u, (v & 15u) | (du.seat & 3u) << 4); }
static uint32_t du_track(void) { return du.mission == DM_ROAD && !du.story ? DU_ROAD_SONG[du.song % 5u] : DU_TRACK[du.mission]; }
static uint16_t *du_bestp(int m, int story)      /* where a game's best is kept: TWIN ROAD has one for each song */
{
    uint32_t t = DU_ROAD_SONG[du.song % 5u];
    if (du.np == 3u) return m == DM_ROAD && !story && t != 1u ? &trio_road[t - 2u] : &trio_best[m];
    return m == DM_ROAD && !story && t != 1u ? &duo_road[t - 2u] : &duo_best[m];
}
static int32_t du_pipe_pitch(int d) { return mus_ladder(&SONGS[DU_TRACK[DM_PIPE]], 3 + d) + 12; }
static int du_pot_pos(int seat, int ch) { int p; for (p = 0; p < DU_POT_N; p++) if (du.pperm[seat][p] == ch) return p; return 0; }
/* the next rock in a seat's own order that has had no potion: d +1 or -1 from ch, or 0: ch itself if it has had none */
static int du_pot_free(int seat, int ch, int d)
{
    int p = du_pot_pos(seat, ch), k;
    for (k = 0; k < DU_POT_N; k++) {
        int c = du.pperm[seat][(p + (d ? d * (k + 1) : k) + 8 * DU_POT_N) % DU_POT_N];
        if (!du_ch[c].awake) return c;
    }
    return ch;
}

/* ---------------------------------------------------------------- the charts */
static void du_add(uint32_t step, int lane, int own, int hid, int pitch)
{
    du_note_t *n;
    if (du.n >= DU_NOTES) return;
    n = &du_n[du.n++];
    n->step = (uint16_t)step; n->lane = (uint8_t)lane; n->own = (uint8_t)own; n->hid = (uint8_t)hid; n->pair = 255;
    n->st[0] = n->st[1] = n->st[2] = 0; n->pitch = (int8_t)pitch;
}
static void du_build(uint32_t seed)
{
    const song_t *g = &SONGS[du_track()];
    uint32_t s, r, j, last = 0;
    int32_t p, d;
    int part;
    du.n = 0;
    if (du.mission == DM_ECHO) {                 /* six rounds of four bars: the call, a count, the echo, a count; the caller
                                                    changes (for three: the other two echo together) */
        static const uint8_t TPL[3][5] = {{0, 4, 8, 99, 99}, {0, 4, 8, 12, 99}, {0, 4, 6, 8, 12}};
        for (r = 0; r < 6u; r++) {               /* (in the order they are played: the judging counts on it) */
            uint32_t cnt = 0, e;
            int caller = (int)(r % (uint32_t)du.np);
            while (cnt < 5u && TPL[r / 2u][cnt] != 99u) cnt++;
            for (e = 0; e < 2u; e++)
                for (j = 0; j < cnt; j++) {
                    int lane = (int)(ihash(seed * 131u + r * 17u + j) % 4u);
                    du_add(r * 64u + e * 32u + TPL[r / 2u][j], lane, e ? du_all() & ~(1 << caller) : 1 << caller, (int)e, mus_ladder(g, 5 + lane * 2) + 12);
                    if (!e) du_n[du.n - 1].pair = (uint8_t)(du.n - 1u + cnt);
                }
        }
        last = 6u * 64u - 14u;
    } else if (du.mission == DM_ROAD || du.mission == DM_DUEL) {   /* the tune's own melody, round until sixteen bars */
        uint32_t n = mus_song_steps(du_track());
        last = 256u;
        for (s = 0; s < last; s++) {
            d = mus_note_at(du_track(), s % n, &p, &part);
            if (d == MUS_NONE) continue;
            du_add(s, d * 4 / 16, du.mission == DM_DUEL || s % 16u == 0u ? du_all() : 1 << ((s / 16u + s / n) % du.np), 0, p);
        }
    } else if (du.mission == DM_DRUM) {          /* NOTE RALLY: thirty-two bars; seat 0 serves */
        du.dial[0] = 1; du.dial[1] = 2; du.dial[2] = 0;
        du.bmode = 0; du.bown = 0; du.bready = 0; du.rally = du.rbest = 0; du.vh[0] = du.vh[1] = 0;
        last = 32u * 16u;
    } else {                                     /* POTION CHOIR: eight rocks, no two alike, in another order on each
                                                    screen; six of them, one a round, need the potion. A "note" is a
                                                    round: lane the rock that needs it, pitch the one that got it, pair the
                                                    bars it took, st[0] 1 right, 3 wrong */
        uint8_t tp[DU_POT_N];
        int k, sw;
        for (r = 0; r < DU_POT_N; r++) {
            du_ch_t *c = &du_ch[r];
            uint32_t tr = 0, f;
            int dup;
            do {
                f = ihash(seed * 53u + r * 11u + tr * 101u);
                c->shape = (uint8_t)(f & 3u); c->eyes = (uint8_t)(1u + (f >> 2) % 3u); c->top = (uint8_t)((f >> 5) % 5u);
                c->mouth = (uint8_t)((f >> 8) % 3u); c->col = (uint8_t)((f >> 11) % 5u); c->awake = 0;
                for (dup = 0, sw = 0, j = 0; j < r; j++) {   /* no two alike in more than three of the five things, no body four times */
                    const du_ch_t *o = &du_ch[j];
                    if ((o->shape == c->shape) + (o->eyes == c->eyes) + (o->top == c->top) + (o->mouth == c->mouth) + (o->col == c->col) > 3) dup = 1;
                    if (o->shape == c->shape && ++sw >= 3) dup = 1;
                }
            } while (dup && ++tr < 60u);
        }
        for (k = 0; k < 4; k++) {                /* the three screens' orders, and the order they are asked for */
            uint8_t *q = k < 3 ? du.pperm[k] : tp;
            for (j = 0; j < DU_POT_N; j++) q[j] = (uint8_t)j;
            for (j = DU_POT_N - 1u; j > 0u; j--) { uint32_t o = ihash(seed * 7u + (uint32_t)k * 1000u + j) % (j + 1u); sw = q[j]; q[j] = q[o]; q[o] = (uint8_t)sw; }
        }
        for (k = 1; k < 3; k++) {                /* (no two screens alike) */
            int o;
            for (o = 0; o < k; o++) {
                for (j = 0, sw = 1; j < DU_POT_N; j++) if (du.pperm[o][j] != du.pperm[k][j]) sw = 0;
                if (sw) { sw = du.pperm[k][0]; for (j = 0; j + 1u < DU_POT_N; j++) du.pperm[k][j] = du.pperm[k][j + 1u]; du.pperm[k][DU_POT_N - 1] = (uint8_t)sw; o = -1; }
            }
        }
        for (r = 0; r < DU_POT_R; r++) du_add(r, tp[r], du_all(), 0, 0);
        du.pr = 0; du.pstart = 0; du.pend = 0; du.csing_t = -9999;
        for (k = 0; k < 3; k++) du.dial[k] = du.pperm[k][0];
        last = 1u << 22;                         /* (it ends when the sixth round does) */
    }
    du.end_tick = (int32_t)(last + 6u) * 24;
    (void)g;
}
/* the score so far, and what a faultless pair would have. A note of one rock: 100 PERFECT, 60 GOOD. A note for both:
 * each one's own points and 100 more when both have it; ROCK-OFF: each rock for itself */
static void du_score(void)
{
    static const uint8_t PT[4] = {0, 100, 60, 0};
    int32_t i, s = 0, m = 0, k, p, hit, sum, q;
    for (i = 0; i < 4; i++) du.sc[i] = 0;
    if (du.mission == DM_PIPE) {                 /* the right rock: 100, and up to 100 more for being quick (three bars: all of it) */
        for (i = 0; i < du.n; i++) { m += 200; if (du_n[i].st[0] == 1u) s += 100 + iclamp(100 - ((int32_t)du_n[i].pair - 3) * 20, 0, 100); }
        du.score = s; du.max = imax(m, 1);
        return;
    }
    if (du.mission == DM_DRUM) { du.score = du.vh[0] * 100 + du.vh[1] * 60; du.max = DU_RALLY_MAX; return; }   /* every time the note is kept up */
    for (i = 0; i < du.n; i++) {                 /* (a note for two or three: 100 more when all have it, 40 each if not) */
        const du_note_t *n = &du_n[i];
        for (q = 0, k = 0, hit = 0, sum = 0; q < du.np; q++) if (n->own >> q & 1u) {
            p = PT[n->st[q] & 3u]; k++; sum += p; hit += p > 0;
            if (du.mission == DM_DUEL) du.sc[q] += p;
        }
        if (du.mission == DM_DUEL) m += 100;
        else if (k >= 2) { m += 100 * k + 100; s += hit == k ? sum + 100 : 40 * hit; }
        else { m += 100; s += sum; }
    }
    if (du.mission == DM_DUEL) {
        for (i = 0; i < du.np; i++) du.sc[i] = imax(0, du.sc[i] - 30 * du.stray[i]);
        s = du.sc[du.seat];
    }
    du.score = s; du.max = imax(m, 1);
}

/* ---------------------------------------------------------------- sounds */
static void du_sound(const du_note_t *n, uint32_t vol, int32_t early)
{
    uint32_t delay = early > 0 ? (uint32_t)early * 60000u / ((tr_bpm_q8 >> 8) * 96u) : 0u;
    mus_mel_voice(n->pitch, 0, vol, delay);
}
static void du_sour(int32_t p) { sfx(p + 1, 0, 220, W_SAW, 1500, 2, 20); sfx(p - 6, 0, 220, W_SQUARE, 800, 2, 20); }

/* a note's outcome, yours or your friend's. A call that was missed leaves nothing to echo: its echo goes with it */
static void du_set(int i, int seat, int q)
{
    du_note_t *n = &du_n[i];
    if (i >= du.n || n->st[seat]) return;
    n->st[seat] = (uint8_t)q;
    if (q == 3 && n->pair != 255u) {              /* (the echo goes for all who were to echo it) */
        int s;
        for (s = 0; s < 3; s++) if ((du_n[n->pair].own >> s & 1u) && !du_n[n->pair].st[s]) du_n[n->pair].st[s] = 3;
    }
    if (q != 3) { du.lflash[n->lane] = 240; du.lcol[n->lane] = (uint8_t)seat; du.tx[seat] = Q8(du.lx[n->lane]); du.hop[seat] = 240; }
}

/* ---------------------------------------------------------------- the link */
static int32_t du_ask;                           /* REC once in a game: the box, this long more */
static void du_abort(void)
{
    if (du.state == DUS_COUNT || du.state == DUS_PLAY || du.state == DUS_CARD) {
        mus_duo = 0; mus_set(MUS_JUKE, du_track());
    }
    du.state = DUS_MENU; du.t = 0;
}
static void du_open(int item)
{
    du.mission = (uint8_t)((item & 7) % DM_N); du.story = (uint8_t)(item >> 3 & 1);
    du.page = du.story ? 0u : 1u; du.ready[0] = du.ready[1] = du.ready[2] = du.ready[3] = 0;
    du.state = DUS_CARD; du.t = 0;
    mus_duo = 1; mus_set(MUS_JUKE, du_track());   /* its tune, the tempo with it: the clocks agree long before the start */
    snd_ui(79);
}
static void du_go(uint32_t t0)
{
    uint32_t i;
    du.t0 = t0; du.state = DUS_COUNT; du.t = 0; du.latched = 0; du.msg_t = 0; du.beat = -99;
    du_build(t0 / 96u);
    for (i = 0; i < 4u; i++) { du.stray[i] = 0; du.rx[i] = du.tx[i] = Q8(i == 1u ? 150 : i == 2u ? 120 : 90); du.hop[i] = 0; }
    mus_set(MUS_OFF, 0);                         /* a bar of clicks, then the tune from its own bar one */
    if (du.mission == DM_PIPE || du.mission == DM_DRUM) du_dial_tx(du.dial[du.seat]);
}
/* POTION CHOIR: the potion is poured over a rock (127: over none, the time ran out). Both FM-1s hear of it from
 * the one who poured, with the bars it took: the next round starts a bar after that one ends */
static void du_pour(int r, int choice, int bars)
{
    du_note_t *n = &du_n[r];
    int ok = choice == n->lane;
    if (r >= du.n || n->st[0]) return;
    n->st[0] = ok ? 1 : 3; n->pitch = (int8_t)choice; n->pair = (uint8_t)bars;
    du.pend = du.pstart + (bars + 1) * 384;
    if (ok) {
        int32_t p = du_pipe_pitch(choice);
        du_ch[choice].awake = 1;
        sfx(p, 0, 900, W_TRI, 3000, 10, 140); sfx(p + 7, 120, 800, W_TRI, 2400, 10, 140); sfx(p + 12, 240, 900, W_SINE, 2200, 10, 140);
        du.msg = bars <= 3 ? T_PERFECT : T_GOOD;
    } else { du_sour(du_pipe_pitch(choice < DU_POT_N ? choice : 0)); du.msg = T_MISS; }
    du.msg_t = 700;
}
/* NOTE RALLY: a seat sends the note toward a lane (q 0: a serve; 1, 2: it was kept up, PERFECT or GOOD); it leaves at
 * t0 and lands at t1, on a beat, where the other seat has to be */
/* where the note goes when seat sends it, to land at t1: a lane drawn at random, never the one it is leaving from, so
 * the one it goes to watches only for where it is coming down (whoever sends it says where, so all agree) */
static int du_rally_lane(int seat, int32_t t1)
{
    int from = du.bmode ? du.bto : (du.dial[seat] & 3), l = (int)(ihash((uint32_t)t1 * 7u + (uint32_t)seat * 131u + 17u) % 3u);
    return l >= from ? l + 1 : l;
}
static void du_rally_hit(int seat, int lane, int q, int32_t t0, int32_t t1)
{
    if (q) { du.rally++; du.vh[q == 1 ? 0 : 1]++; }
    du.bfrom = du.bmode ? du.bto : (uint8_t)(du.dial[seat] & 3u);
    du.bmode = 1; du.bown = (uint8_t)du_next(seat); du.bto = (uint8_t)(lane & 3); du.bt0 = t0; du.bt1 = t1;
    du.hop[seat] = 240;
    mus_mel_voice(mus_ladder(&SONGS[du_track()], 5 + (lane & 3) * 2) + 12, 0, seat == du.seat ? 256 : 170, 0);
}
static void du_rally_drop(int seat, int32_t now)  /* it fell on seat's side: that seat serves again */
{
    if (du.rally > du.rbest) du.rbest = du.rally;
    du.rally = 0; du.bmode = 0; du.bown = (uint8_t)seat; du.bready = now + 192;
    du_sour(55);
    if (seat == du.seat) { du.msg = T_MISS; du.msg_t = 500; }
}
static void du_op(int from, uint32_t op, uint32_t a, uint32_t b, uint32_t c)   /* from: the seat that sent it */
{
    switch (op) {
    case DO_HELLO: du.prog[from] = (uint8_t)imin((int32_t)a, 4); du.pst[from] = (uint8_t)b; break;
    case DO_EPOCH:                               /* the leader's clock in whole (ticks / 32): LINK only keeps them
                                                    together modulo 2^19 ticks, and the games count bars from a start */
        if (from == 0 && du.seat != 0u) {
            int32_t d = (int32_t)((a << 7 | b | c << 14) << 5) - (int32_t)(tr_tick & 0x3FFFFFFu), k;
            k = (d + (d >= 0 ? 1 << 18 : -(1 << 18))) / (1 << 19);
            if (k && du.state != DUS_COUNT && du.state != DUS_PLAY) tr_tick += (uint32_t)(k * (1 << 19));
        }
        break;
    case DO_PICK: if (du.state == DUS_MENU) du.cur = (uint8_t)a; break;
    case DO_OPEN:
        if (a == 127u) { if (du.state == DUS_CARD) du_abort(); }
        else if (du.state == DUS_MENU || du.state == DUS_CARD) du_open((int)a);
        break;
    case DO_READY: du.ready[from] = (uint8_t)(a & 1u); if (a) snd_ui(88); break;
    case DO_GO:
        if (from == 0 && (du.state == DUS_CARD || du.state == DUS_MENU)) {
            uint32_t own = tr_tick / 96u, v = (own & ~16383u) | (b | c << 7);
            if (v + 8192u < own) v += 16384u; else if (v > own + 8192u) v -= 16384u;
            du.mission = (uint8_t)((a & 7u) % DM_N); du.story = (uint8_t)(a >> 3 & 1u);
            if (du.mission == DM_ROAD && !du.story) du.song = (uint8_t)((a >> 4 & 7u) % 5u);
            du_go(v * 96u);
        }
        break;
    case DO_HIT:
        if (du.mission == DM_DRUM) {             /* NOTE RALLY: the note is on its way on; a: lane and how well, b c: the beat it lands on */
            if (du.state == DUS_PLAY && du.bown == from) du_rally_hit(from, (int)(a & 3u), (int)(a >> 2 & 3u), du.bmode ? du.bt1 : du_now(), (int32_t)(b | c << 7) * 96);
        } else if (du.mission != DM_PIPE && (du.state == DUS_PLAY || du.state == DUS_RESULT)) {
            uint32_t i = a | c << 7;
            if (i < du.n && !du_n[i].st[from]) { du_set((int)i, from, b == 1u ? 1 : 2); du_sound(&du_n[i], 150, 0); }
        }
        break;
    case DO_MISS:
        if (du.mission == DM_DRUM) { if (du.state == DUS_PLAY && du.bmode && du.bown == from) du_rally_drop(from, du_now()); }
        else if (du.mission != DM_PIPE && (du.state == DUS_PLAY || du.state == DUS_RESULT)) du_set((int)(a | c << 7), from, 3);
        break;
    case DO_SONG:                                /* TWIN ROAD's card: someone turned to another song */
        if (du.state == DUS_CARD && du.mission == DM_ROAD && !du.story) {
            du.song = (uint8_t)(a % 5u); du.ready[0] = du.ready[1] = du.ready[2] = du.ready[3] = 0;
            mus_set(MUS_JUKE, du_track()); snd_ui(84);
        }
        break;
    case DO_POUR:
        if (du.state == DUS_PLAY && du.mission == DM_PIPE && a == du.pr && !du.pend) du_pour((int)a, (int)b, iclamp((int32_t)c, 1, DU_POT_BARS));
        break;
    case DO_STRAY: if (du.state == DUS_PLAY) du.stray[from]++; break;
    case DO_QUIT:
        if (a) {                                 /* gone back up to the Arcave: not heard from again */
            int i;
            for (i = 0; i < 4; i++) if (du.pnon[i] == du.ros[from]) du.plast[i] = fm1_ms - 5000u;
        } else if (du.state == DUS_COUNT || du.state == DUS_PLAY) du_abort();
        break;
    }
}
/* a message of the link came in (ac_sync.h reads the MIDI and hands these over). All but "I am here" say whose */
static void duo_rx(uint32_t cc, uint32_t v)
{
    int s, i, k;
    if (!du.on) return;
    du.last_rx = fm1_ms;
    if (cc == 110u) {                            /* an FM-1 in the well: a number not heard yet takes a free place */
        int fr = -1;
        if (v == du.nonce) du.nonce = (uint8_t)(1u + (ac_rnd() ^ fm1_ms ^ tr_now_q8()) % 126u);   /* the same number: draw again */
        for (i = 0; i < 4; i++) { if (du.pnon[i] == v) break; if (fr < 0 && !du_live(i)) fr = i; }
        if (i == 4) { if (fr < 0) return; i = fr; du.pnon[i] = (uint8_t)v; }
        du.plast[i] = fm1_ms;
        return;
    }
    if (cc == 113u && v == 127u) { for (s = 0; s < 4; s++) du.pkeys[s] = 0; return; }
    s = cc == 112u || cc == 113u ? (int)(v >> 5 & 3u) : (int)(v >> 4 & 3u);
    if ((cc == 112u || cc == 113u || cc == 114u || cc == 117u) && s == du.seat) return;   /* (not from anyone else) */
    if (cc == 112u) { k = (int)(v & 31u); if (k < AC_NKEY) { du.pkeys[s] |= 1u << k; du.pk_down[s] |= 1u << k; } }
    else if (cc == 113u) { k = (int)(v & 31u); if (k < AC_NKEY) du.pkeys[s] &= ~(1u << k); }
    else if (cc == 114u) du.dial[s] = (uint8_t)((v & 15u) % DU_PIPE_N);
    else if (cc == 115u) du.ra = (uint8_t)v;
    else if (cc == 116u) du.rb = (uint8_t)v;
    else if (cc == 118u) du.rc = (uint8_t)v;
    else if (cc == 117u) du_op(s, v & 15u, du.ra, du.rb, du.rc);
}
/* every pass of the main loop: "I am here", and the leader's clock in whole */
static void duo_poll(void)
{
    uint32_t now = fm1_ms;
    if (!du.on || now - du.last_tx < 300u) return;
    du.last_tx = now;
    midi_send(0xBFu, 110u, du.nonce);
    du_tx(DO_HELLO, duo_prog, du.state, 0);
    if (du.seat == 0u && du_peer()) { uint32_t v = (tr_tick & 0x3FFFFFFu) >> 5; du_tx(DO_EPOCH, v >> 7, v, v >> 14); }
}
static int duo_leads(void) { return du.on && du.seat == 0u && du_peer(); }   /* (ac_sync.h: this FM-1 says where its clock is) */
static int duo_follows(void) { return du.on && (du.seat == 1u || du.seat == 2u); }

static void duo_enter(void)
{
    uint32_t i, k;
    for (i = 0; i < sizeof du; i++) ((uint8_t *)&du)[i] = 0;
    du.on = 1; du.state = DUS_LOBBY;
    du.nonce = (uint8_t)(1u + (ac_rnd() ^ fm1_ms ^ (fm1_ms >> 7) ^ tr_now_q8()) % 126u);
    du.np = 1; du.ros[0] = du.nonce;
    du.last_rx = fm1_ms - 5000u;
    for (i = 0; i < 4u; i++) {                   /* each lane's middle on the keyboard drawn at the foot */
        int32_t a = 999, b = -1;
        for (k = 0; k < AC_NKEY; k++) if (du_lane((int)k) == (int)i) { a = imin(a, gh_key_x((int)k)); b = imax(b, gh_key_x((int)k)); }
        du.lx[i] = (int16_t)((a + b) / 2);
    }
    du.rx[0] = du.tx[0] = Q8(90); du.rx[1] = du.tx[1] = Q8(150); du.rx[2] = du.tx[2] = Q8(120);
    du.song = 1;
    mus_solo = 0; mus_demo = 0; mus_duo = 0;
    snd_whoosh();
}
static void duo_leave(void)
{
    if (du.on) { du_tx(DO_QUIT, 1, 0, 0); midi_send(0xBFu, 113u, 127u); }   /* (127: all keys let go) */
    du.on = 0; mus_duo = 0;
}
static int du_items(uint8_t *it)                 /* what the two can choose: the story's next chapter, or (the story told)
                                                    its four games; and ROCK-OFF. 8: as the story */
{
    int n = 0, i, sp = duo_prog;
    for (i = 0; i < du.np; i++) if (i != du.seat) sp = imin(sp, du.prog[i]);
    if (sp < 4) it[n++] = (uint8_t)(DU_CHAP[sp] | 8);
    else for (i = 0; i < 4; i++) it[n++] = (uint8_t)i;
    it[n++] = DM_DUEL;
    return n;
}

/* ---------------------------------------------------------------- a frame's doing. -1: REC, back up to the Arcave */
static void du_latch(void)
{
    int pct;
    du.latched = 1;
    du_score();
    pct = du.score * 100 / du.max;
    du.stars = (uint8_t)(pct >= 92 ? 3 : pct >= 75 ? 2 : pct >= 55 ? 1 : 0);
    du.cleared = du.stars > 0u; du.finale = 0;
    if (du.mission == DM_DUEL) {                 /* (the most, or as many as the most) */
        int s;
        for (s = 0, du.cleared = 1; s < du.np; s++) if (du.sc[s] > du.sc[du.seat]) du.cleared = 0;
    }
    if (du.score > (int32_t)*du_bestp(du.mission, du.story)) { *du_bestp(du.mission, du.story) = (uint16_t)imin(du.score, 65535); save_dirty = 1; }
    if (du.mission < 4u && du.cleared && duo_prog <= du_chap(du.mission)) {
        duo_prog = (uint8_t)(du_chap(du.mission) + 1); save_dirty = 1;
        if (duo_prog >= 4u) du.finale = 1;
    }
#ifdef DINGHY_HOST
    printf("%6u ms  UNDERSONG%s %s%s%s: %d of %d, %u stars, chapters %u\n", fm1_ms, du.np == 3u ? " FOR THREE" : "", DU_NAME[du.mission], du.mission == DM_ROAD && !du.story ? " on " : "",
           du.mission == DM_ROAD && !du.story ? SONGS[du_track()].name : "", (int)du.score, (int)du.max, du.stars, duo_prog);
#endif
    if (du.cleared) snd_chime(1);
    else { sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150); }
    tix_pay(2 + (int32_t)du.stars * 6, th->caps[3]);   /* the tickets: two for playing with friends, six a star */
}
static void du_play_tick(int32_t now)
{
    int me = du.seat, i, k;
    if (du.mission == DM_PIPE) {
        int r = du.pr, giver = du_giver(r);
        int32_t el = now - du.pstart, b = now >= 0 ? now / 96 : -1;
        if (b != du.beat && now >= 0) {          /* the rocks that have their voice back sing, one on every beat */
            int nw = 0, w;
            du.beat = b;
            for (i = 0; i < DU_POT_N; i++) nw += du_ch[i].awake;
            if (nw) {
                for (i = 0, w = b % nw; i < DU_POT_N; i++) if (du_ch[i].awake && !w--) break;
                du.csing = (uint8_t)i; du.csing_t = now;
                sfx(du_pipe_pitch(i), 0, 260, W_TRI, 1200, 3, 80);
            }
        }
        if (r >= DU_POT_R) return;
        if (du.pend) {                           /* poured: a bar to see how it went, then the next one who needs it */
            if (now >= du.pend) {
                du.pr++; du.pstart = du.pend; du.pend = 0;
                if (du.pr >= DU_POT_R) du.end_tick = now + 96;
                else {
                    for (i = 0; i < du.np; i++) du.dial[i] = (uint8_t)du_pot_free(i, du.dial[i], 0);
                    du_dial_tx(du.dial[me]);
                    snd_ui(du.pr & 1 ? 81 : 86);
                }
            }
            return;
        }
        if (el < 0) return;
        if (me == giver) {                       /* yours to pour: a knob moves the potion, any key pours it */
            int d = in.any_knob > 0 ? 1 : in.any_knob < 0 ? -1 : 0, pour = in.kdown && el >= 48, v = du.dial[me];
            if (d) v = du_pot_free(me, v, d);
#ifdef AC_TEST_CATCH
            if (el >= 400) { v = du_n[r].lane; pour = 1; }
#endif
            if (v != du.dial[me]) { du.dial[me] = (uint8_t)v; du_dial_tx((uint32_t)v); sfx(du_pipe_pitch(v), 0, 200, W_TRI, 2200, 4, 60); }
            if (el >= DU_POT_BARS * 384) { v = 127; pour = 1; }   /* too late: nobody gets it */
            if (pour) {
                int bars = iclamp(el / 384 + 1, 1, DU_POT_BARS);
                du_tx(DO_POUR, (uint32_t)r, (uint32_t)v, (uint32_t)bars); du_pour(r, v, bars);
            }
        } else {
#ifdef AC_TEST_DUO
            if (el >= 300) du_pour(r, du_n[r].lane, 1);
#endif
            if (el >= DU_POT_BARS * 384 + 192) du_pour(r, 127, DU_POT_BARS);   /* (no word from your friend: as if the time ran out) */
        }
        return;
    }
    if (du.mission == DM_DRUM) {
        int d = in.any_knob > 0 ? 1 : in.any_knob < 0 ? -1 : 0, v = iclamp((du.dial[me] & 3) + d, 0, 3), seat, key = -1;
        for (k = 0; k < AC_NKEY; k++) if (in.kdown >> k & 1u) { key = k; break; }
        for (seat = 0; seat < du.np; seat++) {   /* (tests) a seat that plays by itself: under the note, back on the beat */
            int bot = 0, lane;
#ifdef AC_TEST_CATCH
            if (seat == me) bot = 1;
#endif
#ifdef AC_TEST_DUO
            if (seat != me) bot = 1;
#endif
            if (!bot || du.bown != seat) continue;
            if (!du.bmode) {
                if (now >= du.bready + 96 && now < du.end_tick - 480) {
                    int32_t t1 = (now + 95) / 96 * 96 + 384;
                    lane = du_rally_lane(seat, t1);
                    if (seat == me) du_tx(DO_HIT, (uint32_t)lane, (uint32_t)(t1 / 96) & 127u, (uint32_t)(t1 / 96) >> 7);
                    du_rally_hit(seat, lane, 0, now, t1);
                }
            } else {
                if (du.bt1 - now < 150) { if (seat == me) v = du.bto; else du.dial[seat] = du.bto; }
                if (du.bt1 - now <= 1 && (seat != me || du.dial[me] == du.bto)) {
                    int32_t t1 = du.bt1 + (du.rally + 1 >= 4 ? 192 : 384);
                    lane = du_rally_lane(seat, t1);
                    if (seat == me) du_tx(DO_HIT, (uint32_t)lane | 4u, (uint32_t)(t1 / 96) & 127u, (uint32_t)(t1 / 96) >> 7);
                    du_rally_hit(seat, lane, 1, du.bt1, t1);
                    if (seat == me) key = -1;
                }
            }
        }
        if (v != (du.dial[me] & 3)) { du.dial[me] = (uint8_t)v; du_dial_tx((uint32_t)v); sfx(84, 0, 30, W_SQUARE, 700, 1, 0); }
        for (i = 0; i < du.np; i++) du.tx[i] = Q8(du.lx[du.dial[i] & 3]);
        if (du.bown == me && !du.bmode) {        /* yours to serve: any key; where it goes is drawn at random */
            if (key >= 0 && now >= du.bready && now < du.end_tick - 480) {
                int32_t t1 = (now + 95) / 96 * 96 + 384;
                int lane = du_rally_lane(me, t1);
                du_tx(DO_HIT, (uint32_t)lane, (uint32_t)(t1 / 96) & 127u, (uint32_t)(t1 / 96) >> 7);
                du_rally_hit(me, lane, 0, now, t1);
            }
        } else if (du.bown == me) {              /* coming to you: under it, and a key as it lands */
            int32_t dd = du.bt1 - now;
            if (key >= 0 && iabs(dd) <= DU_WIN && (du.dial[me] & 3) == du.bto) {
                int q = iabs(dd) <= DU_PERF ? 1 : 2;
                int32_t t1 = du.bt1 + (du.rally + 1 >= 4 ? 192 : 384);
                int lane = du_rally_lane(me, t1);
                du_tx(DO_HIT, (uint32_t)lane | (uint32_t)q << 2, (uint32_t)(t1 / 96) & 127u, (uint32_t)(t1 / 96) >> 7);
                du_rally_hit(me, lane, q, du.bt1, t1);
                du.msg = q == 1 ? T_PERFECT : T_GOOD; du.msg_t = 400;
            } else if (dd < -DU_WIN) { du_tx(DO_MISS, 0, 0, 0); du_rally_drop(me, now); }
        }
#ifdef AC_TEST_DUO
        else if (du.bmode && du.bt1 - now < -DU_WIN) du_rally_drop(du.bown, now);
#endif
        led_key_want |= 0x7FFFFFFu;
        return;
    }
    if (du.mission == DM_ECHO && now >= 0 && now < DU_ECHO_R * DU_ECHO_LEN) {   /* whose turn: the keys light for it; in the
                                                    bar before it they blink the count, a tick on every beat, the last high */
        int r = now / DU_ECHO_LEN, ph = now % DU_ECHO_LEN / 384, q = now % 384, caller = r % du.np, echo = du_all() & ~(1 << caller);
        int tm = ph == 0 ? 1 << caller : ph == 2 ? echo : 0, nm = ph == 1 ? echo : ph == 3 && r + 1 < DU_ECHO_R ? 1 << ((r + 1) % du.np) : 0;
        int cue = (r * 4 + ph) * 4 + q / 96;
        if ((tm >> me & 1) || ((nm >> me & 1) && q % 96 < 36)) led_key_want |= 0x7FFFFFFu;
        if (cue != du.beat) {
            du.beat = cue;
            if (nm >> me & 1) sfx(q / 96 == 3 ? 96 : 89, 0, 60, W_SQUARE, 1100, 1, 0);
            else if (nm) sfx(84, 0, 30, W_SQUARE, 450, 1, 0);   /* (a friend's count, softly: the beat all the same) */
            if (q < 96 && (tm >> me & 1)) sfx(100, 0, 220, W_SINE, 1500, 1, 60);
        }
    }
    for (k = 0; k < AC_NKEY; k++) {
        int best = -1, lane = du_lane(k);
        int32_t bd = DU_WIN + 1, early = 0;
        if (!(in.kdown >> k & 1u)) continue;
        for (i = 0; i < du.n; i++) {
            const du_note_t *n = &du_n[i];
            int32_t d = n->step * 24 - now;
            if (!(n->own >> me & 1u) || n->st[me] || n->lane != lane) continue;
            if (iabs(d) < bd) { bd = iabs(d); best = i; early = d; }
        }
        du.tx[me] = Q8(du.lx[lane]); du.hop[me] = 240;
        if (best >= 0) {
            int q = bd <= DU_PERF ? 1 : 2;
            du_set(best, me, q); du_tx(DO_HIT, (uint32_t)best & 127u, (uint32_t)q, (uint32_t)best >> 7);
            du_sound(&du_n[best], 256, early);
            du.msg = q == 1 ? T_PERFECT : T_GOOD; du.msg_t = 500;
        } else {
            sfx(mus_ladder(&SONGS[du_track()], 5 + lane * 2) + 12, 0, 240, W_TRI, 1500, 4, 40);
            if (du.mission == DM_DUEL && now >= 0) { du.stray[me]++; du_tx(DO_STRAY, 0, 0, 0); }
        }
    }
    for (i = 0; i < du.n; i++) {
        du_note_t *n = &du_n[i];
        int32_t d = n->step * 24 - now;
        if (d > DU_WIN) break;
#ifdef AC_TEST_CATCH                             /* (tests) your notes play themselves */
        if ((n->own >> me & 1u) && !n->st[me] && d <= 1) { du_set(i, me, 1); du_tx(DO_HIT, (uint32_t)i & 127u, 1, (uint32_t)i >> 7); du_sound(n, 256, 0); }
#endif
#ifdef AC_TEST_DUO                               /* (tests) friends who never miss */
        for (k = 0; k < du.np; k++) if (k != me && (n->own >> k & 1u) && !n->st[k] && d <= 1) { du_set(i, k, 1); du_sound(n, 150, 0); }
#endif
        if ((n->own >> me & 1u) && !n->st[me] && d < -DU_WIN) {
            du_set(i, me, 3); du_tx(DO_MISS, (uint32_t)i & 127u, 0, (uint32_t)i >> 7);
            du_sour(n->pitch);
            du.msg = T_MISS; du.msg_t = 500;
        }
    }
}
/* someone has gone: the game stops for everyone, and says who */
static void du_lost(int s)
{
    int k;
    if (du.state == DUS_COUNT || du.state == DUS_PLAY || du.state == DUS_CARD) du_abort();
    du.gone = (uint8_t)s; du.state = DUS_ERROR; du.t = 0;
    for (k = 0; k < 4; k++) du.pkeys[k] = 0;
    sfx(55, 0, 300, W_TRI, 2400, 4, 100); sfx(48, 160, 500, W_TRI, 2400, 4, 100); sfx(43, 400, 700, W_SQUARE, 900, 4, 60);
}
#ifndef AC_TEST_NP
#define AC_TEST_NP 2                             /* (tests) how many play: you and AC_TEST_NP - 1 friends */
#endif
static int duo_tick(int32_t dt)
{
    int me, k, n, s, busy = 0;
    uint8_t it[8];
    if (du.state == DUS_RESULT) tix_tick(dt); else tix.on = 0;   /* (the tickets come out over the result; the game runs on) */
#ifdef AC_TEST_DUO                               /* (tests) friends always there, the seats after yours, ready when you are */
    if (duo_prog < AC_TEST_DUO) duo_prog = AC_TEST_DUO;   /* (that many chapters cleared on all; 4: the story told) */
    du.nonce = 1; du.last_rx = fm1_ms;
    for (s = 1; s < AC_TEST_NP; s++) {
        du.pnon[s - 1] = (uint8_t)(100 + s); du.plast[s - 1] = fm1_ms;
        du.prog[s] = (uint8_t)imax(AC_TEST_DUO, duo_prog); du.pst[s] = du.state == DUS_LOBBY ? DUS_MENU : du.state;
    }
    if (du.state == DUS_CARD && du.ready[0]) for (s = 1; s < 4; s++) du.ready[s] = 1;
#endif
    du.t += dt;
    if (du.out) { du.out += dt; return du.out > DU_OUT_MS ? -1 : 0; }   /* back up: dark first, the Arcave fades in by itself */
    if (du.msg_t > 0) du.msg_t -= dt;
    for (k = 0; k < 4; k++) {
        if (du.hop[k] > 0) du.hop[k] -= dt;
        du.rx[k] += (du.tx[k] - du.rx[k]) * imin(dt, 70) / 70;
    }
    for (k = 0; k < 4; k++) if (du.lflash[k] > 0) du.lflash[k] -= dt;
    /* who is here. At the door and in the list, whoever comes; from the card on, the seats stay as they are, and
       anyone of them gone stops it all */
    if (du.state == DUS_LOBBY) du_roster();
    else if (du.state != DUS_ERROR) {
        int lost = -1;
        for (s = 0; s < du.np; s++) if (!du_seat_live(s)) lost = s;
        if (lost >= 0) du_lost(lost);
        else if (du.state == DUS_MENU) du_roster();
    }
    me = du.seat;
    if (du.state == DUS_MENU)                    /* the others somewhere else still (a result, a game of their own): wait */
        for (s = 0; s < du.np; s++) if (s != me && du.pst[s] != DUS_MENU) busy = 1;
    /* what you do, to the others' screens */
    for (k = 0; k < AC_NKEY; k++) {
        if (!busy && (in.kdown >> k & 1u)) midi_send(0xBFu, 112u, (uint32_t)k | (uint32_t)(me & 3) << 5);
        if (!busy && (in.kup >> k & 1u)) midi_send(0xBFu, 113u, (uint32_t)k | (uint32_t)(me & 3) << 5);
        for (s = 0; s < 4; s++) if (s != me && (du.pk_down[s] >> k & 1u)) {   /* and what they do, on yours */
            du.hop[s] = 240;
            if (du.state == DUS_PLAY && du.mission != DM_DRUM && du.mission != DM_PIPE) du.tx[s] = Q8(du.lx[du_lane(k)]);
            if (du.state != DUS_PLAY && du.state != DUS_COUNT) sfx(key_play(k) + 12, 0, 400, W_SINE, 1700, 6, 120);
        }
    }
    for (s = 0; s < 4; s++) du.pk_down[s] = 0;
    led_key_want |= in.keys;
    if (in.kdown) du.hop[me & 3] = 240;
    if (du.state != DUS_PLAY && du.state != DUS_COUNT) snd_keys(in.kdown, 0);

    if (du.state != DUS_COUNT && du.state != DUS_PLAY) du_ask = 0;
    else if (du_ask > 0) { du_ask -= dt; led_breathe_want |= 1u << BTN_REC; }
    switch (du.state) {
    case DUS_LOBBY:
        if (du_peer()) { du.state = DUS_MENU; du.t = 0; du.cur = 0; snd_chime(1); }
        else { led_breathe_want |= 1u << BTN_REC; if (BDOWN(BTN_REC)) { du.out = 1; snd_ui(67); } }
        break;
    case DUS_MENU:
        if (!du_peer()) { du.state = DUS_LOBBY; du.t = 0; break; }   /* (a fourth came in with a lower number: this one waits) */
        n = du_items(it);
        if (du.cur >= n) du.cur = 0;
        led_breathe_want |= 1u << BTN_REC;
        if (busy) { if (BDOWN(BTN_REC)) return -1; break; }
        if (in.any_knob) { du.cur = (uint8_t)((du.cur + n + (in.any_knob > 0 ? 1 : -1)) % n); du_tx(DO_PICK, du.cur, 0, 0); snd_ui(84); }
        led_play_breathe = 1;
        if (BDOWN(BTN_PLAY)) { du_tx(DO_OPEN, it[du.cur], 0, 0); du_open(it[du.cur]); }
        else if (BDOWN(BTN_REC)) return -1;
        break;
    case DUS_CARD:
        led_breathe_want |= 1u << BTN_REC;
        if (!du.ready[me]) led_play_breathe = 1;
        if (du.mission == DM_ROAD && !du.story && du.page && in.any_knob) {   /* TWIN ROAD, the story told: a knob turns to another song */
            du.song = (uint8_t)((du.song + 5 + (in.any_knob > 0 ? 1 : -1)) % 5);
            du.ready[0] = du.ready[1] = du.ready[2] = du.ready[3] = 0;
            du_tx(DO_SONG, du.song, 0, 0); mus_set(MUS_JUKE, du_track()); snd_ui(84);
        }
        if (BDOWN(BTN_PLAY)) {
            if (du.page == 0u) { du.page = 1; snd_ui(84); }
            else if (!du.ready[me]) { du.ready[me] = 1; du_tx(DO_READY, 1, 0, 0); snd_ui(88); }
        } else if (BDOWN(BTN_REC)) { du_tx(DO_OPEN, 127, 0, 0); du_abort(); break; }
        for (s = 0, k = 1; s < du.np; s++) k &= du.ready[s];
        if (me == 0 && k) {                      /* all ready: the leader names the beat it starts on, five or six from now */
            uint32_t b = tr_tick / 96u + 6u;
            du_tx(DO_GO, du.mission | (uint32_t)du.story << 3 | (uint32_t)du.song << 4, b & 127u, b >> 7 & 127u);
            du_go(b * 96u);
        }
        break;
    case DUS_COUNT: {
        int32_t now = du_now(), b = now < 0 ? -((-now + 95) / 96) : 0;
        if (BDOWN(BTN_REC)) {                    /* REC once: the box; again while it is up: up to the Arcave (the others are told) */
            if (du_ask > 0) { du_ask = 0; return -1; }
            du_ask = 4000; snd_ui(74);
        } else if (BDOWN(BTN_PLAY)) du_ask = 0;
        if (b != du.beat && now < 0 && b >= -4) {
            du.beat = b;
            sfx_ex(47, 0, 230, W_SINE, 8000, 1, 0, 0, 0, 65010); sfx(b == -4 ? 84 : 79, 0, 60, W_SQUARE, 1300, 1, 0);
        }
        if (now >= -20) {
            mus_step0 = du.t0 / 24u; mus_duo = 1; mus_set(MUS_JUKE, du_track());
            du.state = DUS_PLAY; du.t = 0;
        }
        break; }
    case DUS_PLAY: {
        int32_t now = du_now();
        if (BDOWN(BTN_REC)) {                    /* REC once: the box; again while it is up: up to the Arcave (the others are told) */
            if (du_ask > 0) { du_ask = 0; return -1; }
            du_ask = 4000; snd_ui(74);
        } else if (BDOWN(BTN_PLAY)) du_ask = 0;
        du_play_tick(now);
        if (now >= du.end_tick) { du.state = DUS_RESULT; du.t = 0; du.latched = 0; mus_duo = 0; }
        break; }
    case DUS_RESULT:
        if (!du.latched) { if (du.t > 900) du_latch(); break; }   /* (a moment for the friend's last notes to arrive) */
        led_play_breathe = 1;
        if (BDOWN(BTN_PLAY)) {
            du.t = 0; du.cur = 0;
            if (du.finale) { du.state = DUS_FINALE; du.finale = 0; snd_jingle(); } else { du.state = DUS_MENU; snd_ui(79); }
        }
        break;
    case DUS_FINALE:
        led_play_breathe = 1;
        if (BDOWN(BTN_PLAY) && du.t > 1200) { du.state = DUS_MENU; du.t = 0; du.cur = 0; snd_ui(79); }
        break;
    case DUS_ERROR:                              /* PLAY: on with whoever is left (or to the door); REC: back up */
        led_play_breathe = 1; led_breathe_want |= 1u << BTN_REC;
        if (BDOWN(BTN_PLAY) && du.t > 600) { du_roster(); du.state = du_peer() ? DUS_MENU : DUS_LOBBY; du.t = 0; du.cur = 0; snd_ui(79); }
        else if (BDOWN(BTN_REC)) return -1;
        break;
    }
    return 0;
}

/* ---------------------------------------------------------------- the pictures */
static void du_bg(uint32_t tm)                   /* the well: rings of the five colours rising out of the dark */
{
    int32_t k;
    gclear(th->bg);
    for (k = 0; k < 6; k++) {
        int32_t r = 40 + (int32_t)((tm / 40u + (uint32_t)k * 46u) % 276u);
        galpha(imax(0, 70 - r / 5));
        garc(120, 268, r, r * 3 / 4, 32768, 65536, th->caps[k % 5], 0);
    }
    galpha(256);
}
static void du_rock(int32_t x, int32_t y, int seat, int32_t z)
{
    int32_t h = du.hop[seat], up = h > 0 ? (isin((240 - h) * 32768 / 240) * 12) >> 14 : 0;
    rock_draw(x, y - up, z, (du.rx[seat] >> 8) * 1159, 256, th->rock, DU_COL[seat], 1, DU_COL[seat], th->eye, th->pupil);
}
static const char *du_who(int seat)              /* YOU, FRIEND; for three: YOU, PLAYER 2 and so on */
{
    static char b[4][24];
    const char *q;
    int p = 0;
    if (seat == du.seat) return TR(T_DU_YOU);
    if (du.np < 3u) return TR(T_DU_FRIEND);
    for (q = TR(T_DU_PLAYER); *q && p < 20; ) b[seat & 3][p++] = *q++;
    b[seat & 3][p++] = ' '; b[seat & 3][p++] = (char)('1' + seat); b[seat & 3][p] = 0;
    return b[seat & 3];
}
static void du_tick_mark(int32_t x, int32_t y, col_t c) { gline2(x - 4, y, x - 1, y + 3, c); gline2(x - 1, y + 3, x + 5, y - 4, c); }
static void du_hud(void)
{
    char b[16];
    du_score();
    if (du.mission == DM_DUEL) {                 /* yours on the left, the others' along to the right */
        int s, j = 0;
        num_str(b, du.sc[du.seat]); gtext(6, 6, b, DU_COL[du.seat], 2);
        for (s = 0; s < du.np; s++) if (s != du.seat) {
            num_str(b, du.sc[s]);
            if (du.np == 3u && j++ == 0) gtextc(132, 6, b, DU_COL[s], 2); else gtextr(234, 6, b, DU_COL[s], 2);
        }
    } else { num_str(b, du.score); gtextr(234, 6, b, th->text, 2); }
    if (du.msg_t > 0) {
        galpha(imin(256, du.msg_t * 256 / 250));
        gfit(120, (du.mission == DM_PIPE ? 190 : du.mission == DM_ECHO ? 150 : 50) - (500 - du.msg_t) / 50, 200, TR(du.msg), du.msg == T_MISS ? th->caps[0] : th->pad, ts(1, 2));
        galpha(256);
    }
}
static void du_kb(void)                          /* the keyboard at the foot: your keys, and over them the others' */
{
    int k, s, j = 0;
    gh_kb(212, 0, 0x7FFFFFFu, in.keys, 4);
    for (s = 0; s < du.np; s++) if (s != du.seat) {
        for (k = 0; k < AC_NKEY; k++)
            if (du.pkeys[s] >> k & 1u) grect(gh_key_x(k) - 4, 203 - j * 5, 9, 4, DU_COL[s]);
        j++;
    }
}
/* ECHO STEPS, where the round is: r the round, ph its bar (0 the call, 1 the count, 2 the echo, 3 the count to the next
 * call), q the tick in the bar; who plays this bar (tm) and who plays the next one (nm), as bits of seats; p the
 * beat's pulse, 256 on the beat and gone by the next; c the colour of it all: yours when you are in it */
typedef struct { int r, ph, q, caller, tm, nm, who; int32_t p; col_t c; } du_ech_t;
static int du_first(int m) { int s; for (s = 0; s < 3; s++) if (m >> s & 1) return s; return -1; }
static du_ech_t du_ech(int32_t now)
{
    du_ech_t e;
    int32_t f;
    int me = du.seat, echo, m;
    e.r = now / DU_ECHO_LEN; e.ph = now % DU_ECHO_LEN / 384; e.q = now % 384; e.caller = e.r % du.np;
    echo = du_all() & ~(1 << e.caller);
    e.tm = e.ph == 0 ? 1 << e.caller : e.ph == 2 ? echo : 0;
    e.nm = e.ph == 1 ? echo : e.ph == 3 && e.r + 1 < DU_ECHO_R ? 1 << ((e.r + 1) % du.np) : 0;
    f = e.q % 96; e.p = (96 - f) * (96 - f) * 256 / 9216;
    m = e.tm ? e.tm : e.nm;
    e.who = (m >> me & 1) ? me : du_first(m);
    e.c = e.who >= 0 ? DU_COL[e.who] : th->mute;
    return e;
}
/* a line said large in the middle of the road: one line at three times if it fits, else up to two lines at twice,
 * on a shadow so the lanes do not cut through it */
static void du_say(int32_t y, const char *s, col_t c)
{
    int32_t sc = 2;
    if (gtextw(s, 3) <= 228) { gtextc(121, y + 1, s, th->bg, 3); gtextc(120, y, s, c, 3); return; }
    if (gtext_wrap(0, 0, 228, s, c, 2, TLH(2), -1, 1, 0, 0, 0, 0) > 2) sc = 1;
    gtext_wrap(7, y + 1, 228, s, th->bg, sc, TLH(sc), -1, 1, 0, 1, 0, 0);
    gtext_wrap(6, y, 228, s, c, sc, TLH(sc), -1, 1, 0, 1, 0, 0);
}
/* behind the notes: the road breathing on every beat in the colour of whoever plays (or plays next), the part said
 * large in the middle, and in a count the number, huge for the one about to play */
static void du_echo_back(int32_t now)
{
    du_ech_t e = du_ech(now);
    int me = du.seat, b = e.q / 96, id, mine = e.tm >> me & 1, next = e.nm >> me & 1;
    char num[2];
    galpha(((mine ? 58 : 28) + (b == 0 ? 18 : 0)) * e.p / 256);
    grect(0, 34, 240, DU_YL - 24, e.c);
    galpha(256);
    if (e.nm) {                                  /* the count: 4, 3, 2, 1 on the beats */
        int32_t sc = next ? (e.p > 170 ? 10 : 9) : 5, w;
        num[0] = (char)('4' - b); num[1] = 0;
        w = gtextw(num, sc);
        galpha(next ? 140 + e.p * 116 / 256 : 70 + e.p * 70 / 256);
        gtext(120 - w / 2 + 1, 128 - sc * 7 / 2 + 1, num, th->bg, sc);
        gtext(120 - w / 2, 128 - sc * 7 / 2, num, e.c, sc);
        galpha(256);
    }
    id = e.ph == 0 ? (e.caller == me ? T_DU_CALL : T_DU_WATCH)
       : e.ph == 1 ? (next ? T_DU_GETREADY : T_DU_NEXT_FR)
       : e.ph == 2 ? (mine ? T_DU_ECHO : T_DU_LISTEN)
       : !e.nm ? -1 : next ? T_DU_NEXT_YOU : T_DU_NEXT_FR;
    if (id >= 0) du_say(50, TR(id), next ? ((e.q / 48) & 1 ? th->text : DU_COL[me]) : e.c);   /* (yours next: it blinks with the count) */
}
/* over everything: the frame in the colour of whoever plays (thick when it is you), a ring beating round each rock
 * that plays, the line under the rocks flashing on the beat, the rounds and the bar's four beats at the top, YOUR TURN */
static void du_echo_front(int32_t now)
{
    du_ech_t e = du_ech(now);
    int me = du.seat, b = e.q / 96, i, k, mine = e.tm >> me & 1, m = e.tm ? e.tm : e.nm;
    if (e.tm) {
        galpha(mine ? 150 + e.p * 106 / 256 : 100 + e.p * 120 / 256);
        for (k = 0; k < (mine ? 4 : 2); k++) gbox(k, 34 + k, 240 - 2 * k, DU_YL - 22 - 2 * k, e.c);
        galpha(256);
    }
    for (i = 0; i < du.np; i++) if (m >> i & 1) {   /* each rock that plays (or is next) gets a ring on every beat */
        int32_t x = du.rx[i] >> 8, rr = 16 + (256 - e.p) * 12 / 256;
        galpha(e.p * (e.tm ? 220 : 150) / 256);
        gcirc(x, DU_YL - 10, rr, DU_COL[i]); gcirc(x, DU_YL - 10, rr + 1, DU_COL[i]);
        galpha(256);
    }
    galpha(90 + e.p * 166 / 256);                /* the line the notes are played on, flashing on the beat */
    grect(0, DU_YL + 9, 240, b == 0 ? 4 : 2, e.c);
    galpha(256);
    for (i = 0; i < DU_ECHO_R; i++) { if (i <= e.r) grect(6 + i * 9, 8, 6, 6, i == e.r ? th->text : th->mute); else gbox(6 + i * 9, 8, 6, 6, th->mute); }
    for (i = 0; i < 4; i++) {                    /* the bar's four beats, the one now beating */
        int32_t x = 87 + i * 22, y = 11;
        if (i == b) { gdisc(x, y, 5 + e.p * 3 / 256, e.c); }
        else if (i < b) gdisc(x, y, 4, e.c);
        else gcirc(x, y, 4, th->mute);
    }
    if (mine && e.q < 120) { galpha(230 - e.q * 230 / 120); gfit(120, 104, 232, TR(T_TU_TURN), DU_COL[me], 4); galpha(256); }
}
/* the road of ECHO STEPS, TWIN ROAD and ROCK-OFF: four lanes in the keyboard's four colours, the notes coming down to
 * the line the two rocks stand on */
static void du_field(int32_t now)
{
    int i, k, me = du.seat, echo = du.mission == DM_ECHO && now >= 0 && now < DU_ECHO_R * DU_ECHO_LEN;
    for (i = 0; i < 4; i++) {
        galpha(34); grect(du.lx[i], 34, 1, DU_YL - 34, GH_ZONE[i]);
        if (du.lflash[i] > 0) { galpha(du.lflash[i] * 150 / 240); grect(du.lx[i] - 20, 34, 41, DU_YL - 24, DU_COL[du.lcol[i]]); }
    }
    galpha(256);
    ghl(0, 240, DU_YL + 10, th->mute);
    if (echo) du_echo_back(now);
    for (i = 0; i < du.n; i++) {
        const du_note_t *n = &du_n[i];
        int32_t d = n->step * 24 - now, x = du.lx[n->lane], y = DU_YL - d * 146 / DU_LOOK;
        int mine = n->own >> me & 1, both = du_bits(n->own) >= 2 && du.mission != DM_DUEL, w = mine ? me : du_first(n->own);
        col_t c = GH_ZONE[n->lane];
        if (d > DU_LOOK) break;
        if (d < -70) continue;
        if (du.mission == DM_ECHO && (!mine || n->hid)) continue;   /* (your friend's call is not shown: watch the rock) */
        if (n->st[w] == 1u || n->st[w] == 2u) continue;
        galpha(n->st[w] == 3u ? 70 : 256);
        if (both) { gdisc(x, y, 10, c); gcirc(x, y, 12, th->text); gcirc(x, y, 13, th->text); }
        else if (mine) { gdisc(x, y, 7, c); gcirc(x, y, 9, DU_COL[me]); }
        else { gcirc(x, y, 7, c); gcirc(x, y, 6, c); }
    }
    galpha(256);
    for (k = 0; k < du.np; k++) if (k != me) du_rock(du.rx[k] >> 8, DU_YL, k, 210);
    du_rock(du.rx[me] >> 8, DU_YL, me, 256);
    if (echo) du_echo_front(now);
    du_kb();
}
/* NOTE RALLY: the four lanes between the rocks, yours at the foot and the others' at the top; the note flies from
 * one to the next, a ring closing on the lane where it lands (for three, between the two at the top it arcs across) */
static int32_t du_rally_y(int s) { return s == du.seat ? DU_YL : du.np == 3u && s == du_next(du_next(du.seat)) ? 64 : 56; }
static void du_rally(int32_t now)
{
    int i, me = du.seat;
    char b[8];
    int32_t ytop = 62, ybot = DU_YL - 12;
    for (i = 0; i < 4; i++) { galpha(40); grect(du.lx[i], 50, 1, DU_YL - 40, GH_ZONE[i]); galpha(256); gdisc(du.lx[i], 50, 2, GH_ZONE[i]); }
    ghl(0, 240, DU_YL + 10, th->mute); galpha(120); ghl(0, 240, 44, th->mute); galpha(256);
    gbox(40, 30, 160, 5, th->mute);              /* how much of the song is left */
    grect(42, 32, 156 * iclamp(du.end_tick - now, 0, du.end_tick) / imax(du.end_tick, 1), 1, th->pad);
    for (i = 0; i < du.np; i++) if (i != me) du_rock(du.rx[i] >> 8, du_rally_y(i), i, 180);
    du_rock(du.rx[me] >> 8, DU_YL, me, 256);
    if (du.bmode) {
        int from = (du.bown + du.np - 1) % du.np;   /* (who sent it: the one before) */
        int32_t f = iclamp((now - du.bt0) * 256 / imax(du.bt1 - du.bt0, 1), 0, 300), tome = du.bown == me;
        int32_t y0 = from == me ? ybot : ytop, y1 = tome ? ybot : ytop, x = du.lx[du.bfrom] + (du.lx[du.bto] - du.lx[du.bfrom]) * f / 256;
        int32_t y = y0 + (y1 - y0) * f / 256 - (y0 == y1 ? (isin(iclamp(f, 0, 256) * 128) * 30) >> 14 : 0);
        int32_t r = 6 + ((isin(iclamp(f, 0, 256) * 128) * 6) >> 14), rr = 9 + imax(0, 256 - f) * 26 / 256;
        if (y0 == y1) y += 20;                   /* (between the two at the top: a little lower, an arc over) */
        col_t c = GH_ZONE[du.bto];
        galpha(tome ? 256 : 130);
        garc(du.lx[du.bto], y1, rr, rr, 0, 65536, c, tome); gcirc(du.lx[du.bto], y1, 9, th->text);
        galpha(60); gdisc(x, y, r + 5, c);
        galpha(256); gdisc(x, y, r, c); gtext(x - 3, y - 4, "\xE2\x99\xAA", th->bg, 1);
    } else {                                     /* waiting to be served: over the rock that has it */
        int s = du.bown;
        int32_t x = du.rx[s] >> 8, y = (s == me ? DU_YL - 26 : du_rally_y(s) + 20) + (tsin(fm1_ms, 50, 0) >> 12);
        galpha(60); gdisc(x, y, 11, DU_COL[s]); galpha(256); gdisc(x, y, 6, th->text); gtext(x - 3, y - 4, "\xE2\x99\xAA", th->bg, 1);
        if (s == me && now >= du.bready) { galpha(150 + ipulse(fm1_ms, 900) * 106 / 256); gfit(120, 108, 232, TR(T_DU_SERVE), DU_COL[me], ts(1, 2)); galpha(256); }
    }
    if (du.rally) { b[0] = 'x'; num_str(b + 1, du.rally); gtext(6, 6, b, du.rally >= 4 ? th->pad : th->text, 2); }
    du_kb();
}
/* a rock of the deep choir, standing on y + 16: four bodies, one to three eyes, five things on its head, three mouths,
 * its rim one of the five colours */
#define DC_SHAPE 1                               /* (what of a rock is shown: all of it, or for three, half to each who tells) */
#define DC_EYES 2
#define DC_COL 4
#define DC_TOP 8
#define DC_MOUTH 16
#define DC_ALL 31
static void du_char_m(int32_t x, int32_t y, const du_ch_t *c, int show)
{
    col_t rim = show & DC_COL ? th->caps[c->col % 5u] : th->mute, body = th->rock;
    int32_t ty, ey, ew, i, n = show & DC_EYES ? c->eyes : 1, er = n == 1 ? 5 : 3, xy[6];
    if (!(show & DC_SHAPE)) galpha(90);           /* (the body not told: only faintly there) */
    switch (show & DC_SHAPE ? c->shape : 0) {
    case 0: gellf(x, y, 17, 16, rim, 0); gellf(x, y, 15, 14, body, 0); ty = y - 15; ey = y - 3; ew = 9; break;              /* round */
    case 1: gellf(x, y - 12, 12, 12, rim, 0); grect(x - 12, y - 12, 25, 29, rim);                                          /* tall */
            gellf(x, y - 12, 10, 10, body, 0); grect(x - 10, y - 12, 21, 27, body); ty = y - 23; ey = y - 9; ew = 6; break;
    case 2: gellf(x, y + 5, 26, 12, rim, 0); gellf(x, y + 5, 24, 10, body, 0); ty = y - 6; ey = y + 2; ew = 12; break;      /* flat */
    default:                                                                                                               /* pointed */
        xy[0] = x; xy[1] = y - 24; xy[2] = x + 21; xy[3] = y + 17; xy[4] = x - 21; xy[5] = y + 17; gpoly(xy, 3, rim);
        xy[1] = y - 19; xy[2] = x + 17; xy[3] = y + 15; xy[4] = x - 17; xy[5] = y + 15; gpoly(xy, 3, body);
        ty = y - 23; ey = y + 3; ew = 7; break;
    }
    galpha(256);
    if (!(show & DC_SHAPE) && (show & DC_COL)) { gdisc(x + 24, y + 10, 6, rim); gcirc(x + 24, y + 10, 6, th->text); }   /* (its colour, plain) */
    if (show & DC_EYES) for (i = 0; i < n; i++) {
        int32_t ex = x + (2 * i - (n - 1)) * ew / 2;
        gdisc(ex, ey, er, th->eye); gdisc(ex, ey, er > 3 ? 2 : 1, th->pupil);
    } else gtextc(x, ey - 4, "?", th->mute, 1);
    ey += er + 4;
    if (!(show & DC_MOUTH)) ;
    else if (c->mouth == 0) garc(x, ey - 2, 4, 3, 0, 32768, th->eye, 1);                                   /* a smile */
    else if (c->mouth == 1) { gdisc(x, ey, 3, th->eye); gdisc(x, ey, 1, th->pupil); }                                             /* an O */
    else { ghl(x - 5, x + 6, ey - 1, th->eye); grect(x - 4, ey, 2, 3, th->text); grect(x + 3, ey, 2, 3, th->text); }   /* two teeth */
    switch (show & DC_TOP ? c->top : 0) {
    case 1: grect(x - 1, ty - 5, 3, 6, th->stem); gellf(x, ty - 4, 9, 7, rim, 1); gp(x - 4, ty - 7, th->text); gp(x + 3, ty - 8, th->text); break;   /* a mushroom */
    case 2:                                                                                                                /* two crystals */
        xy[0] = x - 8; xy[1] = ty + 3; xy[2] = x - 4; xy[3] = ty - 10; xy[4] = x; xy[5] = ty + 3; gpoly(xy, 3, rim);
        xy[0] = x + 1; xy[2] = x + 5; xy[3] = ty - 6; xy[4] = x + 9; gpoly(xy, 3, th->text); break;
    case 3: gline2(x, ty + 1, x, ty - 7, th->moss); gellf(x - 5, ty - 8, 5, 2, th->moss, 0); gellf(x + 5, ty - 10, 5, 2, th->moss, 0); break;   /* a sprout */
    case 4: gdisc(x - 7, ty + 2, 4, th->moss); gdisc(x, ty - 1, 5, th->moss); gdisc(x + 7, ty + 2, 4, th->moss); break;        /* moss */
    default: break;                                                                                                        /* nothing */
    }
}
static void du_char(int32_t x, int32_t y, const du_ch_t *c) { du_char_m(x, y, c, DC_ALL); }
static void du_bottle(int32_t x, int32_t y, col_t c)
{
    grect(x - 2, y - 10, 5, 2, th->stem); grect(x - 2, y - 8, 5, 5, th->text);
    gdisc(x, y, 5, c); gcirc(x, y, 5, th->text); gp(x - 2, y - 2, th->text);
}
/* POTION CHOIR: the eight rocks, in your own order. The one who tells sees who needs the potion; the one who pours
 * moves the bottle, and both see where it is */
static void du_potion(int32_t now)
{
    int me = du.seat, r = imin(du.pr, DU_POT_R - 1), giver = du_giver(r), p, k, over = du.pr >= DU_POT_R;
    int32_t el = now - du.pstart, a;
    const du_note_t *n = &du_n[r];
    if (du.np == 3u && me != giver && !over && now >= 0) {   /* for three, the two who tell: half of the one who needs it */
        int half = me == (giver + 1) % 3 ? DC_SHAPE | DC_EYES : DC_COL | DC_TOP | DC_MOUTH;
        const char *s = TR(half & DC_EYES ? T_DU_CARDA : T_DU_CARDB);
        du_char_m(120, 104, &du_ch[n->lane], half);
        gzoom(120, 104, 640);                    /* (large: the one picture on this screen) */
        for (k = 0; k < DU_POT_R; k++) {
            if (k < du.n && du_n[k].st[0]) grect(6 + k * 10, 8, 7, 7, du_n[k].st[0] == 1u ? th->pad : th->caps[0]);
            else gbox(6 + k * 10, 8, 7, 7, k == r ? th->text : th->mute);
        }
        {   /* who pours, and how long it has had */
            int used = du.pend ? n->pair : iclamp(el / 384, 0, DU_POT_BARS);
            for (k = 0; k < DU_POT_BARS; k++) { if (k >= used) grect(74 + k * 12, 24, 9, 4, k < 3 ? th->pad : th->mute); else gbox(74 + k * 12, 24, 9, 4, th->mute); }
            gbox(30, 36, 180, 134, DU_COL[me]);
            du_bottle(196, 50 + (tsin(fm1_ms, 60, 0) >> 12), DU_COL[giver]);
            gtextr(186, 44, du_who(giver), DU_COL[giver], 1);
        }
        if (du.pend) {                           /* poured: right or wrong */
            if (n->st[0] == 1u) { du_tick_mark(120, 60, th->pad); du_tick_mark(121, 60, th->pad); }
            else gtextc(120, 50, "X", th->caps[0], 3);
        }
        a = ts(1, 2);
        while (a > 1 && gblock_h(232, s, a) > 34) a--;
        gblock(4, 238 - gblock_h(232, s, a) - 2, 232, s, DU_COL[me], a, 1);
        return;
    }
    for (k = 0; k < DU_POT_R; k++) {             /* the six rounds: right, wrong, to come */
        if (k < du.n && du_n[k].st[0]) grect(6 + k * 10, 8, 7, 7, du_n[k].st[0] == 1u ? th->pad : th->caps[0]);
        else gbox(6 + k * 10, 8, 7, 7, k == r ? th->text : th->mute);
    }
    if (!over && now >= 0) {                     /* the bars going by: the first three still give everything */
        int used = du.pend ? n->pair : iclamp(el / 384, 0, DU_POT_BARS);
        for (k = 0; k < DU_POT_BARS; k++) { if (k >= used) grect(74 + k * 12, 24, 9, 4, k < 3 ? th->pad : th->mute); else gbox(74 + k * 12, 24, 9, 4, th->mute); }
    }
    for (p = 0; p < DU_POT_N; p++) {
        int c = du.pperm[me][p];
        int32_t x = 30 + (p % 4) * 60, y = 80 + (p / 4) * 68, up = 0;
        if (du_ch[c].awake) {
            if (c == du.csing && now - du.csing_t < 48) up = (isin((now - du.csing_t) * 32768 / 48) * 7) >> 14;
            galpha(50); gdisc(x, y, 26, th->caps[du_ch[c].col % 5u]); galpha(256);
            gtext(x + 14, y - 30 - up, "\xE2\x99\xAA", th->caps[du_ch[c].col % 5u], 1);
        }
        du_char(x, y - up, &du_ch[c]);
        if (over || now < 0) continue;
        if (!du.pend && me != giver && c == n->lane) {   /* yours to tell: this one needs it */
            galpha(140 + ipulse(fm1_ms, 700) * 116 / 256);
            gbox(x - 29, y - 36, 58, 56, th->text); gbox(x - 28, y - 35, 56, 54, th->text);
            galpha(256); gtext(x + 20, y - 34, "!", th->text, 2);
        }
        if (!du.pend && c == du.dial[giver]) {   /* the potion, over the rock it would be poured on */
            gbox(x - 27, y - 34, 54, 52, DU_COL[giver]);
            du_bottle(x - 18, y - 26 + (tsin(fm1_ms, 60, 0) >> 12), DU_COL[giver]);
        }
        if (du.pend) {                           /* poured: right or wrong, and who it was for */
            if (c == n->pitch) { if (n->st[0] == 1u) du_tick_mark(x, y - 30, th->pad); else gtextc(x, y - 36, "X", th->caps[0], 2); }
            if (n->st[0] != 1u && c == n->lane) { gbox(x - 29, y - 36, 58, 56, th->caps[0]); gtext(x + 20, y - 34, "!", th->caps[0], 2); }
        }
    }
    if (!over && now >= 0) {
        const char *s = TR(me == giver ? T_DU_GIVE : T_DU_TELL);
        a = ts(1, 2);
        while (a > 1 && gblock_h(232, s, a) > 34) a--;
        gblock(4, 238 - gblock_h(232, s, a) - 2, 232, s, DU_COL[me == giver ? giver : me], a, 1);
    }
}
static void du_play_draw(int32_t now)
{
    if (du.mission == DM_PIPE) du_potion(now);
    else if (du.mission == DM_DRUM) du_rally(now);
    else du_field(now);
    du_hud();
}
static void du_pair(int32_t y, int ready_marks)  /* the rocks side by side, named; a tick over each one that is ready.
                                                    In the list, two: a place for a third, who can still come */
{
    int s, n = du.np >= 2u ? du.np : 2, slot = du.state == DUS_MENU && du.np == 2u;
    for (s = 0; s < n + slot; s++) {
        int32_t x = n + slot == 3 ? 46 + s * 74 : s ? 164 : 76;
        if (s == 2 && slot) {
            galpha(50 + ipulse(fm1_ms, 1800) * 60 / 256); gcirc(x, y, 10, DU_COL[2]); gtextc(x, y - 4, "+", DU_COL[2], 2); galpha(256);
            continue;
        }
        if (s != du.seat && !du_seat_live(s)) {
            galpha(90 + ipulse(fm1_ms, 1400) * 100 / 256); gcirc(x, y, 10, th->mute); gtextc(x, y - 4, "?", th->mute, 2); galpha(256);
        } else du_rock(x, y, s, 256);
        gfit(x, y + 16, n == 3 ? 72 : 110, du_who(s), DU_COL[s], ts(1, 2));
        if (ready_marks && du.ready[s]) du_tick_mark(x + 18, y - 4, th->pad);
    }
}
static void du_stars(int32_t cx, int32_t y, int n)
{
    int k;
    for (k = 0; k < 3; k++) gh_gem_draw(cx + (k - 1) * 22, y, 8, th->pad, k < n);
}
/* the waiting screen: Rock falling down the well, which never ends: rings of its stones come up out of the dark
 * and past, turning against each other, the well bending slowly; a friend's place beside Rock, still empty. Over it,
 * one after another and round again: what this is, how two FM-1s are joined, how to go back */
static const uint16_t DU_FPAGE[5] = {T_DU_ABOUT, T_DU_CONNECT, T_DU_PAGE, T_DU_JOIN, T_DU_WAIT};
/* the Link page's address, broken after a slash wherever the line would be wider than the words (at scale a); its
 * height, and drawn (centred) when draw is set. The font has capitals only: this address takes them */
static int32_t du_addr(int32_t y, int32_t a, col_t c, int draw)
{
    static const char ADDR[] = "GITHUB.COM/MYSTIC-ARCAVE/FM-ARCAVE";
    char ln[sizeof ADDR];
    int32_t h = 0, i = 0, n, e, k;
    while (ADDR[i]) {
        for (n = 0, e = i; ADDR[e];) {           /* as many of its parts as fit (one at least) */
            while (ADDR[e] && ADDR[e] != '/') e++;
            if (ADDR[e]) e++;
            for (k = i; k < e; k++) ln[k - i] = ADDR[k];
            ln[e - i] = 0;
            if (n && gtextw(ln, a) > 224) break;
            n = e - i;
        }
        for (k = 0; k < n; k++) ln[k] = ADDR[i + k];
        ln[n] = 0;
        if (draw) gfit(120, y + h, 232, ln, c, a);
        h += TLH(a); i += n;
    }
    return h;
}
static int32_t du_fpage_h(int pg, int32_t a)     /* a page of the waiting screen's words: its height at scale a */
{
    int32_t h = gblock_h(224, TR(DU_FPAGE[pg]), a);
    if (pg >= 1 && pg <= 3) h += TLH(a);         /* how to join: three steps, numbered */
    if (pg == 2) h += 4 + du_addr(0, a, 0, 0);
    if (pg == 4) h += 4 + gblock_h(224, TR(T_REC_BACK), a);
    return h;
}
static int32_t du_fpage_a(int pg)                /* its scale: what is said, unless that will not fit under Rock */
{
    int32_t a = ts(1, 2);
    while (a > 1 && du_fpage_h(pg, a) > 104) a--;
    return a;
}
static int32_t du_flift(int pg)                  /* how far the well goes up to make room for a page's words */
{
    return iclamp(du_fpage_h(pg, du_fpage_a(pg)) - 70, 0, 30);
}
static void du_fall(uint32_t tm)
{
    int32_t t = du.t, in8 = smooth8(0, 700, t), me = du.seat, k, j, xy[24];
    int32_t fq = t > 600 ? (t - 600) % DU_PAGE_MS : 0, fpg = t > 600 ? (t - 600) / DU_PAGE_MS % 5 : 0;
    int32_t fo = smooth8(DU_PAGE_MS - 380, DU_PAGE_MS, fq);
    int32_t lift = (du_flift((int)fpg) * (256 - fo) + du_flift((int)(fpg + 1) % 5) * fo) / 256, cy0 = 106 - lift;
    int32_t vx = 120 + ((tsin(tm, 7, 0) * 20) >> 14), vy = cy0 + ((tsin(tm, 5, 16384) * 13) >> 14);   /* the far end */
    int32_t nx = 120 - (vx - 120) / 2, ny = cy0 - (vy - cy0) / 2;                                 /* and the near one */
    uint32_t u = (uint32_t)((uint64_t)tm * 9u * 1024u / 3400u), id0 = u >> 10;
    gclear(th->bg);
    for (k = 0; k < 9; k++) {                    /* the rings, the far ones first */
        int32_t p = (int32_t)((u & 1023u) + (uint32_t)k * 1024u) / 9, pp = (p * p) >> 10, r = 5 + ((pp * 215) >> 10);
        int32_t cx = vx + (((nx - vx) * pp) >> 10), cy = vy + (((ny - vy) * pp) >> 10), pass;
        uint32_t id = id0 - (uint32_t)k;
        int32_t tw = (int32_t)(id * 1800u & 65535u) + (int32_t)((tm * 5u) & 65535u) * ((id & 1u) ? 1 : -1);
        galpha((26 + ((pp * 210) >> 10)) * in8 / 256);
        for (pass = 0; pass < (p > 760 ? 2 : 1); pass++) {
            for (j = 0; j < 12; j++) {
                int32_t rr = (r + pass) * (236 + (int32_t)(ihash(id * 16u + (uint32_t)j) % 40u)) / 256, a = j * 5461 + tw;
                xy[2 * j] = cx + ((icos(a) * rr) >> 14); xy[2 * j + 1] = cy + ((isin(a) * rr) >> 14);
            }
            gpolyline(xy, 12, 1, th->caps[id % 5u]);
        }
    }
    for (k = 0; k < 26; k++) {                   /* dust going by, faster the nearer it is */
        uint32_t per = 1400u + ihash((uint32_t)k + 50u) % 2200u;
        int32_t p = (int32_t)(((tm + ihash((uint32_t)k + 99u) * 7u) % per) * 1024u / per), pp = (p * p) >> 10;
        int32_t a = (int32_t)ihash((uint32_t)k * 7u + 1u), r0 = 6 + ((pp * 190) >> 10), r1 = r0 - 1 - ((pp * 12) >> 10);
        int32_t cx = vx + (((nx - vx) * pp) >> 10), cy = vy + (((ny - vy) * pp) >> 10);
        galpha(((pp * 200) >> 10) * in8 / 256);
        gline(cx + ((icos(a) * r0) >> 14), cy + ((isin(a) * r0) >> 14), cx + ((icos(a) * r1) >> 14), cy + ((isin(a) * r1) >> 14), k % 3 ? th->mute : th->text);
    }
    for (k = 0; k < 5; k++) {                    /* and notes, out of the song below */
        int32_t p = (int32_t)(((tm + (uint32_t)k * 900u) % 4500u) * 1024u / 4500u), pp = (p * p) >> 10;
        int32_t a = (int32_t)ihash((uint32_t)k * 31u + (tm + (uint32_t)k * 900u) / 4500u * 7u), r = 10 + ((pp * 150) >> 10);
        galpha(imin(256, pp / 2 + 30) * in8 / 256);
        gtext(vx + (((nx - vx) * pp) >> 10) + ((icos(a) * r) >> 14) - 3, vy + (((ny - vy) * pp) >> 10) + ((isin(a) * r) >> 14) - 4,
              "\xE2\x99\xAA", k & 1 ? DU_COL[k >> 1 & 1] : th->caps[k % 5], p > 640 ? 2 : 1);
    }
    {   /* Rock, turning as it falls (it came in from above, large), and the place a friend will fall in */
        int32_t rx = 120 + ((tsin(tm, 11, 0) * 7) >> 14), ry = cy0 + ((tsin(tm, 9, 9000) * 5) >> 14);
        int32_t z = 430 + (256 - in8) * 700 / 256, h = du.hop[me], up = h > 0 ? (isin((240 - h) * 32768 / 240) * 20) >> 14 : 0;
        int32_t g = (int32_t)((tm * 13u) & 65535u), gx = rx + ((icos(g) * 58) >> 14), gy = ry + ((isin(g) * 36) >> 14);
        if (isin(g) < 0) {                       /* (round behind Rock) */
            galpha((60 + ipulse(tm, 1400) * 90 / 256) * in8 / 256);
            gcirc(gx, gy, 8, DU_COL[!me]); gtextc(gx, gy - 3, "?", DU_COL[!me], 1);
        }
        galpha(34 * in8 / 256); gdisc(rx, ry, 36, DU_COL[me]); galpha(256);
        rock_draw(rx, ry + 6 - up, z, (int32_t)(tm * 9u), (tm % 3700u) < 160u ? iabs((int32_t)(tm % 3700u) - 80) * 256 / 80 : 256,
                  th->rock, DU_COL[me], 1, DU_COL[me], th->eye, th->pupil);
        if (isin(g) >= 0) {                      /* (and round in front) */
            galpha((110 + ipulse(tm, 1400) * 120 / 256) * in8 / 256);
            gcirc(gx, gy, 11, DU_COL[!me]); gtextc(gx, gy - 5, "?", DU_COL[!me], 2);
        }
    }
    galpha(in8);
    gfit(120, 12, 232, "UNDERSONG", th->caps[(tm / 500u) % 5u], 3);
    if (t > 600) {                               /* the words: each comes up into a band of dark, stays, and goes on up and out */
        int32_t q = fq, pg = fpg, ai = smooth8(0, 420, q), ao = fo, al = imin(ai, 256 - ao);
        int32_t dy = (256 - ai) * 10 / 256 - ao * 8 / 256, a = du_fpage_a((int)pg), hh, top, y, w;
        const char *s = TR(DU_FPAGE[pg]);
        char num[4] = "1/3";
        hh = gblock_h(224, s, a);
        top = pg >= 1 && pg <= 3 ? TLH(a) : 0;
        y = 220 - du_fpage_h((int)pg, a);
        galpha(al * 232 / 256); grect(0, y - 8, 240, 228 - (y - 8), th->bg);
        galpha(al * 220 / 256); w = 20 + al * 90 / 256;
        ghl(120 - w, 120 + w, y - 8, DU_COL[me]); ghl(120 - w, 120 + w, 227, DU_COL[!me]);
        galpha(al);
        if (top) { num[0] = (char)('0' + pg); gtextc(120, y + dy, num, DU_COL[me], a); }
        gblock(8, y + top + dy, 224, s, pg == 4 ? th->pad : th->text, a, 1);
        if (pg == 2) du_addr(y + top + hh + 4 + dy, a, th->pad, 1);
        if (pg == 4) gblock(8, y + hh + 4 + dy, 224, TR(T_REC_BACK), th->mute, a, 1);
        galpha(in8);
        for (k = 0; k < 5; k++) gdisc(100 + k * 10, 234, k == pg ? 2 : 1, k == pg ? th->pad : th->mute);
    }
    galpha(256);
    if (t < 500) gveil(th->bg, (uint32_t)(32 - smooth8(0, 500, t) / 8));   /* out of the dark the dive ended in */
}
static void duo_draw(void)
{
    uint32_t tm = fm1_ms;
    char b[40];
    int32_t y, a, i, n;
    uint8_t it[8];
    du_bg(tm);
    switch (du.state) {
    case DUS_LOBBY:
        du_fall(tm);
        if (du.out) gfade(du.out * 256 / DU_OUT_MS);
        break;
    case DUS_MENU:
        gfit(120, 12, 232, "UNDERSONG", th->caps[(tm / 500u) % 5u], 3);
        if (du.np == 3u) gfit(120, 32, 232, TR(T_DU_TRIO), DU_COL[2], 1);
        du_pair(56, 0);
        n = du_items(it);
        a = n > 3 ? 2 : ts(2, 3);
        y = 96;
        for (i = 0; i < n; i++) {
            int m = it[i] & 7, sel = i == du.cur;
            int32_t h = 5 * a + 10, p = 0, sc = a;
            const char *q;
            if (sel) { grect(6, y, 228, h, th->panel); gbox(6, y, 228, h, DU_COL[du.seat]); }
            if (it[i] & 8u) {                    /* the story: its chapter */
                for (q = TR(T_DU_STORY); *q && p < 24; ) b[p++] = *q++;
                b[p++] = ' '; b[p++] = (char)('1' + m); b[p++] = '/'; b[p++] = '4'; b[p] = 0;
                gfit(120, y + 5, 216, b, sel ? th->text : th->mute, a);
            } else {
                int32_t bs = *du_bestp(m, 0);
                while (sc > 1 && gtextw(DU_NAME[m], sc) > (bs ? 130 : 216)) sc--;
                gtext(14, y + 5 + (a - sc) * 2, DU_NAME[m], sel ? th->text : th->mute, sc);
                if (bs) { num_str(b, bs); gtextr(226, y + 5 + (a - 1) * 2, b, th->pad, imin(a, 2)); }
            }
            y += h + 3;
        }
        {   /* (the others not back from what they were doing: wait for them) */
            int s, busy = 0;
            for (s = 0; s < du.np; s++) if (s != du.seat && du.pst[s] != DUS_MENU) busy = 1;
            if (busy) { galpha(150 + ipulse(tm, 1500) * 106 / 256); gfoot(238, TR(T_DU_BUSY), th->pad, th->bg, 1); galpha(256); }
            else gfoot(238, TR(T_DU_CHOOSE), th->mute, th->bg, 1);
        }
        break;
    case DUS_CARD: {
        const char *s = du.page == 0u ? TR(T_DU_S1 + du.mission) : TR((du.np == 3u ? T_DU_T1 : T_DU_H1) + du.mission);
        int song = du.mission == DM_ROAD && !du.story && du.page;   /* (TWIN ROAD, the story told: its song, a knob turns to another) */
        int32_t foot = du.page ? (song ? 118 : 150) : 214;   /* (how it is played: the two rocks under it, ticked as each is ready) */
        gfit(120, 14, 232, DU_NAME[du.mission], th->caps[du.mission % 5u], 3);
        a = ts(2, 3);
        while (a > 1 && 38 + gblock_h(228, s, a) > foot) a--;
        gblock(6, 38, 228, s, th->text, a, 1);
        if (song) {
            int32_t bob = tsin(tm, 60, 0) >> 13, w, sc = 2;
            const char *nm = SONGS[du_track()].name;
            while (sc > 1 && gtextw(nm, sc) > 190) sc--;
            w = gtextw(nm, sc);
            grect(6, 121, 228, 22, th->panel); gbox(6, 121, 228, 22, DU_COL[du.seat]);
            gtext(120 - w / 2, 127 + (2 - sc) * 2, nm, th->text, sc);
            gtext(12 - bob, 127, "\xE2\x86\x90", th->pad, 2); gtext(218 + bob, 127, "\xE2\x86\x92", th->pad, 2);
            if (*du_bestp(DM_ROAD, 0)) { num_str(b, *du_bestp(DM_ROAD, 0)); gtextr(232, 112, b, th->pad, 1); }
        }
        if (du.page) du_pair(166, 1);
        galpha(150 + ipulse(tm, 1500) * 106 / 256);
        if (du.page == 0u) gfit(120, 224, 232, TR(T_PRESSPLAY), th->pad, ts(1, 2));
        else if (du.ready[du.seat]) gfit(120, 224, 232, TR(T_DU_READY), th->pad, ts(1, 2));
        else gfit(120, 224, 232, TR(du.np == 3u ? T_DU_ALL : T_DU_BOTH), th->pad, ts(1, 2));
        galpha(256);
        break; }
    case DUS_COUNT: {
        int32_t now = du_now(), c = (-now + 95) / 96;
        du_play_draw(now);
        if (c >= 1 && c <= 4) { b[0] = (char)('0' + c); b[1] = 0; gtextc(120, 84, b, th->text, 8); }
        break; }
    case DUS_PLAY: du_play_draw(du_now()); break;
    case DUS_RESULT:
        gfit(120, 14, 232, DU_NAME[du.mission], th->caps[du.mission % 5u], 3);
        du_score();
        if (du.mission == DM_DUEL) {
            int s, top = 0, ntop = 0;
            for (s = 0; s < du.np; s++) {
                int32_t x = du.np == 3u ? 46 + s * 74 : s ? 172 : 68;
                du_rock(x, 66, s, 256);
                gfit(x, 82, du.np == 3u ? 72 : 110, du_who(s), DU_COL[s], ts(1, 2));
                num_str(b, du.sc[s]); gfit(x, 110, du.np == 3u ? 70 : 100, b, DU_COL[s], 3);
                top = imax(top, du.sc[s]);
            }
            for (s = 0; s < du.np; s++) ntop += du.sc[s] == top;
            if (du.latched) gfit(120, 150, 232, TR(du.sc[du.seat] < top ? T_DU_LOSE : ntop > 1 ? T_DU_DRAW : T_DU_WIN), th->pad, ts(2, 3));
        } else {
            du_pair(60, 0);
            num_str(b, du.score); gtextc(120, 98, b, th->text, 5);
            if (du.latched) {
                du_stars(120, 142, du.stars);
                a = ts(2, 3);
                while (a > 1 && gblock_h(232, TR(du.cleared ? T_DU_CLEAR : T_DU_RETRY), a) > 40) a--;
                gblock(4, 158, 232, TR(du.cleared ? T_DU_CLEAR : T_DU_RETRY), du.cleared ? th->pad : th->caps[0], a, 1);
            }
        }
        if (du.latched) {
            n = 0;
            { const char *q; for (q = TR(T_BEST); *q && n < 24; ) b[n++] = *q++; }
            b[n++] = ' '; num_str(b + n, *du_bestp(du.mission, du.story));
            gfit(120, 206, 232, b, th->mute, 1);
            galpha(150 + ipulse(tm, 1500) * 106 / 256);
            gfit(120, 224, 232, TR(T_PRESSPLAY), th->pad, ts(1, 2));
            galpha(256);
        }
        break;
    case DUS_FINALE:
        for (i = 0; i < 9; i++) {                /* notes rise out of the well */
            int32_t q = (int32_t)((tm / 4u + (uint32_t)i * 211u) % 1000u);
            galpha(256 - q * 256 / 1000);
            gtext(14 + i * 26 + (tsin(tm, 20, i * 9000) >> 12), 236 - q * 236 / 1000, "\xE2\x99\xAA", th->caps[i % 5], 2);
        }
        galpha(256);
        gfit(120, 14, 232, "UNDERSONG", th->caps[(tm / 300u) % 5u], 4);
        du_pair(60, 0);
        a = ts(2, 3);
        while (a > 1 && 92 + gblock_h(228, TR(T_DU_FINALE), a) > 216) a--;
        gblock(6, 92, 228, TR(T_DU_FINALE), th->text, a, 1);
        galpha(150 + ipulse(tm, 1500) * 106 / 256);
        gfit(120, 226, 232, TR(T_PRESSPLAY), th->pad, ts(1, 2));
        galpha(256);
        break;
    case DUS_ERROR: {                            /* someone gone: who, in their colour, faded and crossed out */
        int32_t sh = du.t < 400 ? ((tsin(tm, 30, 0) * (400 - du.t) / 400) >> 12) : 0;
        galpha(200); grect(0, 0, 240, 240, th->bg); galpha(256);
        grect(0, 0, 240, 4, th->caps[0]); grect(0, 236, 240, 4, th->caps[0]);
        gfit(120 + sh, 18, 232, TR(T_DU_LOSTT), th->caps[0], 3);
        galpha(110); du_rock(120, 92, du.gone, 330); galpha(256);
        gline2(96, 68, 144, 116, th->caps[0]); gline2(144, 68, 96, 116, th->caps[0]);
        gfit(120, 122, 232, du_who(du.gone), DU_COL[du.gone & 3], ts(2, 3));
        a = ts(1, 2);
        while (a > 1 && gblock_h(228, TR(T_DU_LEFT), a) > 56) a--;
        gblock(6, 150, 228, TR(T_DU_LEFT), th->text, a, 1);
        if (du.t > 600) { galpha(150 + ipulse(tm, 1500) * 106 / 256); gfit(120, 224, 232, TR(T_PRESSPLAY), th->pad, ts(1, 2)); galpha(256); }
        break; }
    }
    if (du.state == DUS_RESULT) tix_draw();
    if (du_ask > 0 && (du.state == DUS_COUNT || du.state == DUS_PLAY)) rec_box_draw(0, tm);
}
