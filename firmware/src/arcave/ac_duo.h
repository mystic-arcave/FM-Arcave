/* SPDX-License-Identifier: GPL-3.0-only */
/* UNDERSONG: a game for two FM-1s, down the well in the Arcave's floor. Under the Arcave sleeps the lower half of its
 * song, and no rock can wake it alone: two players, each on their own FM-1, each seeing the other's rock, keys and
 * dial on their own screen, play it awake together.
 *
 *   the story     four chapters, each a game for two on the shared clock:
 *     ECHO STEPS    one plays a few notes; the other only sees the friend's rock and keys, and plays them back
 *     TWIN ROAD     a song comes down the road, half of its notes yours, half your friend's, the big ones both at once
 *     PIPE WHISPER  each turns a pipe to a mark only the other can see: you have to tell each other
 *     TWIN DRUM     any key strikes: white rings both at once, a coloured ring only that rock
 *   ROCK-OFF      the one game against each other: the same song for both, who catches more (a key where no note
 *                 is costs points)
 *   afterwards    the story played through, its four games can be played again for the best score
 *
 * The link. An FM-1 only sends MIDI over USB, so the two talk through a relay: the Link page (web/link/), or two
 * emulators in one browser. Everything is a control change on channel 16, as ARCAVE LINK's own messages are:
 *   CC 110 n      "I am here", every 300 ms; n is a number drawn at the door: the lower one is seat 0 and leads
 *   CC 112 k      a key pressed (0..26)       CC 113 k   a key let go (127: all)       CC 114 d   the pipe's dial
 *   CC 118 c, CC 115 a, CC 116 b, CC 117 op   a message with three numbers; the op comes last (DO_...)
 * Seat 0 also says where its clock is four times a second (ac_sync.h, CC 102-107 with sender 5 and "follow"): seat 1
 * follows, so both hear the same bar. Each FM-1 judges its own player against that clock and tells the other what
 * happened, so the delay of the link never costs a note. */

enum { DM_ECHO, DM_ROAD, DM_PIPE, DM_DRUM, DM_DUEL, DM_N };
static const char *const DU_NAME[DM_N] = {"ECHO STEPS", "TWIN ROAD", "PIPE WHISPER", "TWIN DRUM", "ROCK-OFF"};
static const uint8_t DU_TRACK[DM_N] = {4, 1, 3, 5, 2};   /* OPAL TIDE, SPORE DRIFT, MYCELIUM NIGHTS, ROLL ON, CRYSTAL STEPS */
enum { DUS_LOBBY, DUS_MENU, DUS_CARD, DUS_COUNT, DUS_PLAY, DUS_RESULT, DUS_FINALE };
enum { DO_HELLO = 1, DO_EPOCH, DO_PICK, DO_OPEN, DO_READY, DO_GO, DO_HIT, DO_MISS, DO_QUIT, DO_STRAY };
#define DU_YL 180                                /* the line the notes are played on */
#define DU_LOOK 288                              /* a note is seen three beats ahead */
#define DU_WIN 34                                /* ticks either side that still count */
#define DU_PERF 10
#define DU_NOTES 160
#define DU_PIPE_N 12                             /* places on a pipe */
static const col_t DU_COL[2] = {AC_RGB(240, 96, 200), AC_RGB(110, 232, 210)};   /* seat 0, seat 1 */

static uint16_t duo_best[DM_N];                  /* (kept in the save) the best of each game; ROCK-OFF: your own */
static uint8_t duo_prog;                         /* chapters cleared, 0..4 */

typedef struct { uint16_t step; uint8_t lane, own, hid, pair, st[2]; int8_t pitch; } du_note_t;   /* own: bit = seat; st: 0 to come, 1 PERFECT, 2 GOOD, 3 missed */
static du_note_t du_n[DU_NOTES];
static struct {
    uint8_t on, state, seat, nonce, pn, peer_prog, peer_state, was_peer;
    uint8_t cur, mission, story, page, ready[2], n, latched, finale, stars, cleared;
    uint8_t ra, rb, rc, dial[2], lcol[4], msg;
    uint32_t pkeys, pk_down, last_rx, last_tx, t0;
    int32_t t, end_tick, beat, score, max, sc[2], msg_t, hop[2], rx[2], tx[2], lflash[4], jud_t[2];
    uint8_t jud_ok[2];
    uint16_t stray[2];                           /* ROCK-OFF: keys pressed where no note was (30 points each: no winning by hammering) */
    int16_t lx[4];
} du;

static int du_peer(void) { return du.pn && fm1_ms - du.last_rx < 2500u; }
static int32_t du_now(void) { return (int32_t)((tr_now_q8() >> 8) - du.t0); }
static int du_lane(int k) { return (k * 15 / 26) / 4; }
static void du_tx(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    midi_send(0xBFu, 118u, c & 127u); midi_send(0xBFu, 115u, a & 127u); midi_send(0xBFu, 116u, b & 127u); midi_send(0xBFu, 117u, op);
}
static int32_t du_pipe_pitch(int d) { return mus_ladder(&SONGS[DU_TRACK[DM_PIPE]], 3 + d) + 12; }
static int du_target(int r, int seat) { return seat ? du_n[r].pitch : du_n[r].lane; }

/* ---------------------------------------------------------------- the charts */
static void du_add(uint32_t step, int lane, int own, int hid, int pitch)
{
    du_note_t *n;
    if (du.n >= DU_NOTES) return;
    n = &du_n[du.n++];
    n->step = (uint16_t)step; n->lane = (uint8_t)lane; n->own = (uint8_t)own; n->hid = (uint8_t)hid; n->pair = 255;
    n->st[0] = n->st[1] = 0; n->pitch = (int8_t)pitch;
}
static const char *const DU_DRUM[8] = {          /* x both, a one rock, b the other; the second time round they swap */
    "x...x...x...x...", "x...x...x.x.x...", "a...b...a...b...", "a.a.b.b.x...x...",
    "x..x..x.x..x..x.", "a..a..x.b..b..x.", "a.b.a.b.a.b.x...", "x.x.a.b.x.x.x...",
};
static void du_build(uint32_t seed)
{
    const song_t *g = &SONGS[DU_TRACK[du.mission]];
    uint32_t s, r, j, last = 0;
    int32_t p, d;
    int part;
    du.n = 0;
    if (du.mission == DM_ECHO) {                 /* six rounds of two bars: the call, then the echo; the caller changes */
        static const uint8_t TPL[3][5] = {{0, 4, 8, 99, 99}, {0, 4, 8, 12, 99}, {0, 4, 6, 8, 12}};
        for (r = 0; r < 6u; r++) {               /* (in the order they are played: the judging counts on it) */
            uint32_t cnt = 0, e;
            int caller = (int)(r & 1u);
            while (cnt < 5u && TPL[r / 2u][cnt] != 99u) cnt++;
            for (e = 0; e < 2u; e++)
                for (j = 0; j < cnt; j++) {
                    int lane = (int)(ihash(seed * 131u + r * 17u + j) % 4u);
                    du_add(r * 32u + e * 16u + TPL[r / 2u][j], lane, e ? 2 >> caller : 1 << caller, (int)e, mus_ladder(g, 5 + lane * 2) + 12);
                    if (!e) du_n[du.n - 1].pair = (uint8_t)(du.n - 1u + cnt);
                }
        }
        last = 6u * 32u;
    } else if (du.mission == DM_ROAD || du.mission == DM_DUEL) {   /* the tune's own melody, round until sixteen bars */
        uint32_t n = mus_song_steps(DU_TRACK[du.mission]);
        last = 256u;
        for (s = 0; s < last; s++) {
            d = mus_note_at(DU_TRACK[du.mission], s % n, &p, &part);
            if (d == MUS_NONE) continue;
            du_add(s, d * 4 / 16, du.mission == DM_DUEL || s % 16u == 0u ? 3 : 1 << ((s / 16u + s / n) & 1u), 0, p);
        }
    } else if (du.mission == DM_DRUM) {
        for (s = 0; s < 256u; s++) {
            char c = DU_DRUM[(s / 16u) % 8u][s % 16u];
            if (c != '.') du_add(s, 0, c == 'x' ? 3 : 1 << ((c == 'b') ^ (s >= 128u)), 0, 41);
        }
        last = 256u;
    } else {                                     /* PIPE WHISPER: eight chords, two bars each; a note is a round, its lane
                                                    seat 0's mark and its pitch seat 1's */
        int prev[2] = {5, 5}, k;
        for (r = 0; r < 8u; r++) {
            int t[2];
            for (k = 0; k < 2; k++) {
                t[k] = (int)(ihash(seed * 97u + r * 13u + (uint32_t)k * 7u) % DU_PIPE_N);
                while (iabs(t[k] - prev[k]) < 2) t[k] = (t[k] + 5) % DU_PIPE_N;
                prev[k] = t[k];
            }
            du_add((r + 1u) * 32u, t[0], 3, 0, t[1]);
        }
        last = 8u * 32u + 4u;
        du.dial[0] = du.dial[1] = 5;
    }
    du.end_tick = (int32_t)(last + 6u) * 24;
    (void)g;
}
/* the score so far, and what a faultless pair would have. A note of one rock: 100 PERFECT, 60 GOOD. A note for both:
 * each one's own points and 100 more when both have it; ROCK-OFF: each rock for itself */
static void du_score(void)
{
    static const uint8_t PT[4] = {0, 100, 60, 0};
    int32_t i, s = 0, m = 0, a, b;
    du.sc[0] = du.sc[1] = 0;
    for (i = 0; i < du.n; i++) {
        const du_note_t *n = &du_n[i];
        a = PT[n->st[0] & 3u]; b = PT[n->st[1] & 3u];
        if (du.mission == DM_DUEL) { du.sc[0] += a; du.sc[1] += b; m += 100; }
        else if (n->own == 3u) { m += 300; if (a && b) s += a + b + 100; else if (a || b) s += du.mission == DM_PIPE ? 100 : 40; }
        else { m += 100; s += n->own == 1u ? a : b; }
    }
    if (du.mission == DM_DUEL) {
        for (i = 0; i < 2; i++) du.sc[i] = imax(0, du.sc[i] - 30 * du.stray[i]);
        s = du.sc[du.seat];
    }
    du.score = s; du.max = imax(m, 1);
}

/* ---------------------------------------------------------------- sounds */
static void du_sound(const du_note_t *n, uint32_t vol, int32_t early)
{
    uint32_t delay = early > 0 ? (uint32_t)early * 60000u / ((tr_bpm_q8 >> 8) * 96u) : 0u;
    if (du.mission == DM_DRUM) {
        sfx_ex(n->pitch, delay, 280, W_SINE, 8600u * vol / 256u, 1, 0, 0, 0, 65080);
        sfx(98, delay, 70, W_NOISE, 1500u * vol / 256u, 1, 30);
    } else mus_mel_voice(n->pitch, 0, vol, delay);
}
static void du_sour(int32_t p) { sfx(p + 1, 0, 220, W_SAW, 1500, 2, 20); sfx(p - 6, 0, 220, W_SQUARE, 800, 2, 20); }

/* a note's outcome, yours or your friend's. A call that was missed leaves nothing to echo: its echo goes with it */
static void du_set(int i, int seat, int q)
{
    du_note_t *n = &du_n[i];
    if (i >= du.n || n->st[seat]) return;
    n->st[seat] = (uint8_t)q;
    if (q == 3 && n->pair != 255u && !du_n[n->pair].st[!seat]) du_n[n->pair].st[!seat] = 3;
    if (du.mission == DM_PIPE) { du.jud_t[seat] = 900; du.jud_ok[seat] = q != 3; return; }
    if (q != 3) { du.lflash[n->lane] = 240; du.lcol[n->lane] = (uint8_t)seat; du.tx[seat] = Q8(du.lx[n->lane]); du.hop[seat] = 240; }
}

/* ---------------------------------------------------------------- the link */
static void du_abort(void)
{
    if (du.state == DUS_COUNT || du.state == DUS_PLAY || du.state == DUS_CARD) {
        mus_duo = 0; mus_set(MUS_JUKE, DU_TRACK[du.mission]);
    }
    du.state = DUS_MENU; du.t = 0;
}
static void du_open(int item)
{
    du.mission = (uint8_t)((item & 7) % DM_N); du.story = (uint8_t)(item >> 3 & 1);
    du.page = du.story ? 0u : 1u; du.ready[0] = du.ready[1] = 0;
    du.state = DUS_CARD; du.t = 0;
    mus_duo = 1; mus_set(MUS_JUKE, DU_TRACK[du.mission]);   /* its tune, the tempo with it: the clocks agree long before the start */
    snd_ui(79);
}
static void du_go(uint32_t t0)
{
    uint32_t i;
    du.t0 = t0; du.state = DUS_COUNT; du.t = 0; du.latched = 0; du.msg_t = 0; du.beat = -99;
    du.stray[0] = du.stray[1] = 0;
    du_build(t0 / 96u);
    for (i = 0; i < 2u; i++) { du.rx[i] = du.tx[i] = Q8(i ? 150 : 90); du.hop[i] = 0; du.jud_t[i] = 0; }
    mus_set(MUS_OFF, 0);                         /* a bar of clicks, then the tune from its own bar one */
    if (du.mission == DM_PIPE) midi_send(0xBFu, 114u, du.dial[du.seat]);
}
static void du_op(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    int ot = !du.seat;
    switch (op) {
    case DO_HELLO: du.peer_prog = (uint8_t)imin((int32_t)a, 4); du.peer_state = (uint8_t)b; break;
    case DO_EPOCH:                               /* the leader's clock in whole (ticks / 32): LINK only keeps the two
                                                    together modulo 2^19 ticks, and the games count bars from a start */
        if (du.seat == 1u) {
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
    case DO_READY: du.ready[ot] = (uint8_t)(a & 1u); if (a) snd_ui(88); break;
    case DO_GO:
        if (du.state == DUS_CARD || du.state == DUS_MENU) {
            uint32_t own = tr_tick / 96u, v = (own & ~16383u) | (b | c << 7);
            if (v + 8192u < own) v += 16384u; else if (v > own + 8192u) v -= 16384u;
            du.mission = (uint8_t)((a & 7u) % DM_N); du.story = (uint8_t)(a >> 3 & 1u);
            du_go(v * 96u);
        }
        break;
    case DO_HIT:
        if (du.state == DUS_PLAY || du.state == DUS_RESULT) {
            uint32_t i = a | c << 7;
            if (i < du.n && !du_n[i].st[ot]) {
                du_set((int)i, ot, b == 1u ? 1 : 2);
                if (du.mission == DM_PIPE) sfx(du_pipe_pitch(du_target((int)i, ot)), 0, 900, W_TRI, 1900, 20, 120);
                else du_sound(&du_n[i], 150, 0);
            }
        }
        break;
    case DO_MISS:
        if (du.state == DUS_PLAY || du.state == DUS_RESULT) du_set((int)(a | c << 7), ot, 3);
        break;
    case DO_STRAY: if (du.state == DUS_PLAY) du.stray[ot]++; break;
    case DO_QUIT:
        if (a) du.last_rx = fm1_ms - 5000u;      /* gone back up to the Arcave */
        else if (du.state == DUS_COUNT || du.state == DUS_PLAY) du_abort();
        break;
    }
}
/* a message of the link came in (ac_sync.h reads the MIDI and hands these over) */
static void duo_rx(uint32_t cc, uint32_t v)
{
    if (!du.on) return;
    du.last_rx = fm1_ms;
    if (cc == 110u) {
        if (v == du.nonce) du.nonce = (uint8_t)(1u + (ac_rnd() ^ fm1_ms ^ tr_now_q8()) % 126u);   /* the same number: draw again */
        du.pn = (uint8_t)v;
        du.seat = du.nonce < du.pn ? 0u : 1u;
    }
    else if (cc == 112u) { if (v < AC_NKEY) { du.pkeys |= 1u << v; du.pk_down |= 1u << v; } }
    else if (cc == 113u) { if (v == 127u) du.pkeys = 0; else if (v < AC_NKEY) du.pkeys &= ~(1u << v); }
    else if (cc == 114u) du.dial[!du.seat] = (uint8_t)(v % DU_PIPE_N);
    else if (cc == 115u) du.ra = (uint8_t)v;
    else if (cc == 116u) du.rb = (uint8_t)v;
    else if (cc == 118u) du.rc = (uint8_t)v;
    else if (cc == 117u) du_op(v, du.ra, du.rb, du.rc);
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
static int duo_follows(void) { return du.on && du.seat == 1u; }

static void duo_enter(void)
{
    uint32_t i, k;
    for (i = 0; i < sizeof du; i++) ((uint8_t *)&du)[i] = 0;
    du.on = 1; du.state = DUS_LOBBY;
    du.nonce = (uint8_t)(1u + (ac_rnd() ^ fm1_ms ^ (fm1_ms >> 7) ^ tr_now_q8()) % 126u);
    du.last_rx = fm1_ms - 5000u;
    for (i = 0; i < 4u; i++) {                   /* each lane's middle on the keyboard drawn at the foot */
        int32_t a = 999, b = -1;
        for (k = 0; k < AC_NKEY; k++) if (du_lane((int)k) == (int)i) { a = imin(a, gh_key_x((int)k)); b = imax(b, gh_key_x((int)k)); }
        du.lx[i] = (int16_t)((a + b) / 2);
    }
    du.rx[0] = du.tx[0] = Q8(90); du.rx[1] = du.tx[1] = Q8(150);
    mus_solo = 0; mus_demo = 0; mus_duo = 0;
    snd_whoosh();
}
static void duo_leave(void)
{
    if (du.on) { du_tx(DO_QUIT, 1, 0, 0); midi_send(0xBFu, 113u, 127u); }
    du.on = 0; mus_duo = 0;
}
static int du_items(uint8_t *it)                 /* what the two can choose: the story's next chapter, or (the story told)
                                                    its four games; and ROCK-OFF. 8: as the story */
{
    int n = 0, i, sp = imin(duo_prog, du.peer_prog);
    if (sp < 4) it[n++] = (uint8_t)(sp | 8);
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
    if (du.mission == DM_DUEL) du.cleared = du.sc[du.seat] >= du.sc[!du.seat];
    if (du.score > (int32_t)duo_best[du.mission]) { duo_best[du.mission] = (uint16_t)imin(du.score, 65535); save_dirty = 1; }
    if (du.mission < 4u && du.cleared && duo_prog <= du.mission) {
        duo_prog = (uint8_t)(du.mission + 1u); save_dirty = 1;
        if (duo_prog >= 4u) du.finale = 1;
    }
#ifdef DINGHY_HOST
    printf("%6u ms  UNDERSONG %s: %d of %d, %u stars, chapters %u\n", fm1_ms, DU_NAME[du.mission], (int)du.score, (int)du.max, du.stars, duo_prog);
#endif
    if (du.cleared) snd_chime(1);
    else { sfx(60, 0, 500, W_TRI, 3000, 5, 150); sfx(55, 180, 500, W_TRI, 3000, 5, 150); sfx(48, 360, 1200, W_TRI, 3000, 5, 150); }
}
static void du_play_tick(int32_t now)
{
    int me = du.seat, ot = !me, i, k;
    if (du.mission == DM_PIPE) {
        int r = iclamp(now / 768, 0, 7), d = iclamp(in.any_knob, -3, 3), v;
#ifdef AC_TEST_CATCH
        if (now >= r * 768 + 300) d = iclamp(du_target(r, me) - du.dial[me], -1, 1);
#endif
        v = iclamp(du.dial[me] + d, 0, DU_PIPE_N - 1);
        if (v != du.dial[me]) {
            du.dial[me] = (uint8_t)v; midi_send(0xBFu, 114u, (uint32_t)v);
            sfx(du_pipe_pitch(v), 0, 300, W_TRI, 2600, 4, 60);
        }
#ifdef AC_TEST_DUO
        if (now >= r * 768 + 500) du.dial[ot] = (uint8_t)(du.dial[ot] + iclamp(du_target(r, ot) - du.dial[ot], -1, 1));
#endif
        for (i = 0; i < du.n; i++) {
            if (now < du_n[i].step * 24) break;
            if (!du_n[i].st[me]) {
                int ok = du.dial[me] == du_target(i, me);
                du_set(i, me, ok ? 1 : 3); du_tx(ok ? DO_HIT : DO_MISS, (uint32_t)i, 1, 0);
                if (ok) { sfx(du_pipe_pitch(du.dial[me]), 0, 1100, W_TRI, 3000, 20, 140); sfx(du_pipe_pitch(du.dial[me]) + 12, 0, 800, W_SINE, 1500, 20, 140); }
                else du_sour(du_pipe_pitch(du.dial[me]));
                du.msg = ok ? T_PERFECT : T_MISS; du.msg_t = 600;
            }
#ifdef AC_TEST_DUO
            if (!du_n[i].st[ot]) du_set(i, ot, du.dial[ot] == du_target(i, ot) ? 1 : 3);
#endif
        }
        return;
    }
    for (k = 0; k < AC_NKEY; k++) {
        int best = -1, lane = du_lane(k);
        int32_t bd = DU_WIN + 1, early = 0;
        if (!(in.kdown >> k & 1u)) continue;
        for (i = 0; i < du.n; i++) {
            const du_note_t *n = &du_n[i];
            int32_t d = n->step * 24 - now;
            if (!(n->own >> me & 1u) || n->st[me] || (du.mission != DM_DRUM && n->lane != lane)) continue;
            if (iabs(d) < bd) { bd = iabs(d); best = i; early = d; }
        }
        du.tx[me] = Q8(du.lx[lane]); du.hop[me] = 240;
        if (best >= 0) {
            int q = bd <= DU_PERF ? 1 : 2;
            du_set(best, me, q); du_tx(DO_HIT, (uint32_t)best & 127u, (uint32_t)q, (uint32_t)best >> 7);
            du_sound(&du_n[best], 256, early);
            du.msg = q == 1 ? T_PERFECT : T_GOOD; du.msg_t = 500;
        } else if (du.mission == DM_DRUM) sfx_ex(41, 0, 160, W_SINE, 2600, 1, 0, 0, 0, 65080);
        else {
            sfx(mus_ladder(&SONGS[DU_TRACK[du.mission]], 5 + lane * 2) + 12, 0, 240, W_TRI, 1500, 4, 40);
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
#ifdef AC_TEST_DUO                               /* (tests) a friend who never misses */
        if ((n->own >> ot & 1u) && !n->st[ot] && d <= 1) { du_set(i, ot, 1); du_sound(n, 150, 0); }
#endif
        if ((n->own >> me & 1u) && !n->st[me] && d < -DU_WIN) {
            du_set(i, me, 3); du_tx(DO_MISS, (uint32_t)i & 127u, 0, (uint32_t)i >> 7);
            du_sour(n->pitch);
            du.msg = T_MISS; du.msg_t = 500;
        }
    }
}
static int duo_tick(int32_t dt)
{
    int me, ot, k, n, peer;
    uint8_t it[8];
#ifdef AC_TEST_DUO                               /* (tests) a friend is always there, one seat down, ready when you are */
    du.last_rx = fm1_ms; du.pn = 127; du.seat = 0; du.peer_prog = (uint8_t)imax(AC_TEST_DUO, duo_prog);
    if (du.state == DUS_CARD && du.ready[0]) du.ready[1] = 1;
#endif
    me = du.seat; ot = !me; peer = du_peer();
    du.t += dt;
    if (du.msg_t > 0) du.msg_t -= dt;
    for (k = 0; k < 2; k++) {
        if (du.hop[k] > 0) du.hop[k] -= dt;
        if (du.jud_t[k] > 0) du.jud_t[k] -= dt;
        du.rx[k] += (du.tx[k] - du.rx[k]) * imin(dt, 70) / 70;
    }
    for (k = 0; k < 4; k++) if (du.lflash[k] > 0) du.lflash[k] -= dt;
    /* what you do, to your friend's screen */
    for (k = 0; k < AC_NKEY; k++) {
        if (in.kdown >> k & 1u) midi_send(0xBFu, 112u, (uint32_t)k);
        if (in.kup >> k & 1u) midi_send(0xBFu, 113u, (uint32_t)k);
        if (du.pk_down >> k & 1u) {              /* and what your friend does, on yours */
            du.hop[ot] = 240;
            if (du.state == DUS_PLAY && du.mission != DM_DRUM && du.mission != DM_PIPE) du.tx[ot] = Q8(du.lx[du_lane(k)]);
            if (du.state != DUS_PLAY && du.state != DUS_COUNT) sfx(key_play(k) + 12, 0, 400, W_SINE, 1700, 6, 120);
        }
    }
    du.pk_down = 0;
    led_key_want |= in.keys;
    if (in.kdown) du.hop[me] = 240;
    if (du.state != DUS_PLAY && du.state != DUS_COUNT) snd_keys(in.kdown, 0);

    if (!peer && du.state != DUS_LOBBY) {         /* the friend is gone: back to the door */
        if (du.state == DUS_COUNT || du.state == DUS_PLAY || du.state == DUS_CARD) du_abort();
        du.state = DUS_LOBBY; du.t = 0; du.pkeys = 0;
        banner_show("", TR(T_DU_GONE), 0);
        sfx(55, 0, 300, W_TRI, 2400, 4, 100); sfx(48, 160, 500, W_TRI, 2400, 4, 100);
    }
    switch (du.state) {
    case DUS_LOBBY:
        if (peer) { du.state = DUS_MENU; du.t = 0; du.cur = 0; snd_chime(1); }
        else { led_breathe_want |= 1u << BTN_REC; if (BDOWN(BTN_REC)) return -1; }
        break;
    case DUS_MENU:
        n = du_items(it);
        if (du.cur >= n) du.cur = 0;
        if (in.any_knob) { du.cur = (uint8_t)((du.cur + n + (in.any_knob > 0 ? 1 : -1)) % n); du_tx(DO_PICK, du.cur, 0, 0); snd_ui(84); }
        led_play_breathe = 1; led_breathe_want |= 1u << BTN_REC;
        if (BDOWN(BTN_PLAY)) { du_tx(DO_OPEN, it[du.cur], 0, 0); du_open(it[du.cur]); }
        else if (BDOWN(BTN_REC)) return -1;
        break;
    case DUS_CARD:
        led_breathe_want |= 1u << BTN_REC;
        if (!du.ready[me]) led_play_breathe = 1;
        if (BDOWN(BTN_PLAY)) {
            if (du.page == 0u) { du.page = 1; snd_ui(84); }
            else if (!du.ready[me]) { du.ready[me] = 1; du_tx(DO_READY, 1, 0, 0); snd_ui(88); }
        } else if (BDOWN(BTN_REC)) { du_tx(DO_OPEN, 127, 0, 0); du_abort(); break; }
        if (me == 0 && du.ready[0] && du.ready[1]) {   /* the leader names the beat it starts on: five or six from now */
            uint32_t b = tr_tick / 96u + 6u;
            du_tx(DO_GO, du.mission | (uint32_t)du.story << 3, b & 127u, b >> 7 & 127u);
            du_go(b * 96u);
        }
        break;
    case DUS_COUNT: {
        int32_t now = du_now(), b = now < 0 ? -((-now + 95) / 96) : 0;
        if (BDOWN(BTN_REC)) { du_tx(DO_QUIT, 0, 0, 0); du_abort(); break; }
        if (b != du.beat && now < 0 && b >= -4) {
            du.beat = b;
            sfx_ex(47, 0, 230, W_SINE, 8000, 1, 0, 0, 0, 65010); sfx(b == -4 ? 84 : 79, 0, 60, W_SQUARE, 1300, 1, 0);
        }
        if (now >= -20) {
            mus_step0 = du.t0 / 24u; mus_duo = 1; mus_set(MUS_JUKE, DU_TRACK[du.mission]);
            du.state = DUS_PLAY; du.t = 0;
        }
        break; }
    case DUS_PLAY: {
        int32_t now = du_now();
        if (BDOWN(BTN_REC)) { du_tx(DO_QUIT, 0, 0, 0); du_abort(); break; }
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
static const char *du_who(int seat) { return TR(seat == du.seat ? T_DU_YOU : T_DU_FRIEND); }
static void du_tick_mark(int32_t x, int32_t y, col_t c) { gline2(x - 4, y, x - 1, y + 3, c); gline2(x - 1, y + 3, x + 5, y - 4, c); }
static void du_hud(void)
{
    char b[16];
    du_score();
    if (du.mission == DM_DUEL) {
        num_str(b, du.sc[du.seat]); gtext(6, 6, b, DU_COL[du.seat], 2);
        num_str(b, du.sc[!du.seat]); gtextr(234, 6, b, DU_COL[!du.seat], 2);
    } else { num_str(b, du.score); gtextr(234, 6, b, th->text, 2); }
    if (du.msg_t > 0) {
        galpha(imin(256, du.msg_t * 256 / 250));
        gfit(120, 50 - (500 - du.msg_t) / 50, 200, TR(du.msg), du.msg == T_MISS ? th->caps[0] : th->pad, ts(1, 2));
        galpha(256);
    }
}
static void du_kb(void)                          /* the keyboard at the foot: your keys, and over them your friend's */
{
    int k, ot = !du.seat;
    gh_kb(212, 0, 0x7FFFFFFu, in.keys, 4);
    for (k = 0; k < AC_NKEY; k++)
        if (du.pkeys >> k & 1u) grect(gh_key_x(k) - 4, 203, 9, 4, DU_COL[ot]);
}
/* the road of ECHO STEPS, TWIN ROAD and ROCK-OFF: four lanes in the keyboard's four colours, the notes coming down to
 * the line the two rocks stand on */
static void du_field(int32_t now)
{
    int i, me = du.seat, ot = !me;
    for (i = 0; i < 4; i++) {
        galpha(34); grect(du.lx[i], 34, 1, DU_YL - 34, GH_ZONE[i]);
        if (du.lflash[i] > 0) { galpha(du.lflash[i] * 150 / 240); grect(du.lx[i] - 20, 34, 41, DU_YL - 24, DU_COL[du.lcol[i]]); }
    }
    galpha(256);
    ghl(0, 240, DU_YL + 10, th->mute);
    for (i = 0; i < du.n; i++) {
        const du_note_t *n = &du_n[i];
        int32_t d = n->step * 24 - now, x = du.lx[n->lane], y = DU_YL - d * 146 / DU_LOOK;
        int mine = n->own >> me & 1, both = n->own == 3u && du.mission != DM_DUEL;
        col_t c = GH_ZONE[n->lane];
        if (d > DU_LOOK) break;
        if (d < -70) continue;
        if (du.mission == DM_ECHO && (!mine || n->hid)) continue;   /* (your friend's call is not shown: watch the rock) */
        if (mine ? (n->st[me] == 1u || n->st[me] == 2u) : (n->st[ot] == 1u || n->st[ot] == 2u)) continue;
        galpha((mine ? n->st[me] : n->st[ot]) == 3u ? 70 : 256);
        if (both) { gdisc(x, y, 10, c); gcirc(x, y, 12, th->text); gcirc(x, y, 13, th->text); }
        else if (mine) { gdisc(x, y, 7, c); gcirc(x, y, 9, DU_COL[me]); }
        else { gcirc(x, y, 7, c); gcirc(x, y, 6, c); }
    }
    galpha(256);
    du_rock(du.rx[ot] >> 8, DU_YL, ot, 210);
    du_rock(du.rx[me] >> 8, DU_YL, me, 256);
    if (du.mission == DM_ECHO && now >= 0 && now < 6 * 768) {   /* whose turn, in words, and the six rounds */
        int r = now / 768, call = (r & 1) == me, half = now % 768 >= 384;
        gfit(92, 24 + 2 * ts(1, 2), 176, TR(call ? (half ? T_DU_LISTEN : T_DU_CALL) : (half ? T_DU_ECHO : T_DU_WATCH)), DU_COL[call != half ? me : ot], ts(1, 2));
        for (i = 0; i < 6; i++) { if (i <= r) grect(6 + i * 9, 8, 6, 6, i == r ? th->text : th->mute); else gbox(6 + i * 9, 8, 6, 6, th->mute); }
    }
    du_kb();
}
static void du_drum(int32_t now)
{
    int i, me = du.seat;
    int32_t beat = now >= 0 ? 255 - (now % 96) * 255 / 96 : 0;
    galpha(40 + beat / 6); gdisc(120, 112, 44, th->caps[4]);
    galpha(256); gdisc(120, 112, 30, th->panel); gcirc(120, 112, 30, th->text); gcirc(120, 112, 31, th->text);
    for (i = 0; i < du.n; i++) {
        const du_note_t *n = &du_n[i];
        int32_t d = n->step * 24 - now, r = 31 + d * 84 / DU_LOOK;
        col_t c = n->own == 3u ? th->text : DU_COL[n->own == 2u];
        if (d > DU_LOOK) break;
        if (d < 0 || (n->own == 3u ? (n->st[0] && n->st[1]) : n->st[n->own == 2u])) continue;
        if (n->own == 3u && n->st[me]) galpha(90);
        garc(120, 112, r, r, 0, 65536, c, n->own == 3u || (n->own >> me & 1u));
        galpha(256);
    }
    for (i = 0; i < 2; i++) {
        int32_t x = i ? 196 : 44;
        du_rock(x, 150, i, 256);
        gfit(x, 170, 80, du_who(i), DU_COL[i], 1);
    }
    led_key_want |= 0x7FFFFFFu;
}
static void du_pipes(int32_t now)
{
    int s, k, me = du.seat, r = iclamp(now / 768, 0, 7);
    int32_t left = 768 - (now - r * 768);
    for (k = 0; k < 8; k++) {                    /* the eight chords: both right, one, none */
        const du_note_t *n = &du_n[k];
        int a = n->st[0] == 1u, b = n->st[1] == 1u;
        if (k < du.n && n->st[0] && n->st[1]) grect(6 + k * 10, 8, 7, 7, a && b ? th->pad : a || b ? th->mute : th->caps[0]);
        else gbox(6 + k * 10, 8, 7, 7, k == r ? th->text : th->mute);
    }
    if (now >= 0 && now < 8 * 768) {             /* the bar that runs out */
        gbox(40, 24, 160, 6, th->mute);
        grect(42, 26, 156 * iclamp(left, 0, 768) / 768, 2, left < 192 ? th->caps[0] : th->pad);
    }
    for (s = 0; s < 2; s++) {
        int32_t x = s ? 172 : 68, y = 190 - du.dial[s] * 12, t = du_target(r, s);
        col_t c = DU_COL[s];
        galpha(110); grect(x - 9, 46, 18, 156, th->panel); galpha(256);
        gbox(x - 10, 46, 20, 156, th->mute);
        for (k = 0; k < DU_PIPE_N; k++) ghl(x - 9, x - 5, 190 - k * 12, th->mute);
        if (s != me && now < 8 * 768) {          /* your friend's mark: only you can see it */
            int32_t ty = 190 - t * 12, bob = tsin(fm1_ms, 60, 0) >> 13;
            gtext(s ? x - 26 + bob : x + 16 - bob, ty - 3, s ? "\xE2\x86\x92" : "\xE2\x86\x90", th->text, 2);
            gbox(x - 12, ty - 7, 24, 14, th->text);
        } else if (now < 8 * 768) gtextc(x, 34, "?", c, 2);
        grect(x - 8, y - 4, 16, 9, c);
        du_rock(x, y - 13, s, 200);
        if (du.jud_t[s] > 0) {
            galpha(du.jud_t[s] * 256 / 900);
            if (du.jud_ok[s]) du_tick_mark(x + (s ? -26 : 26), 120, th->pad); else gtextc(x + (s ? -26 : 26), 114, "X", th->caps[0], 2);
            galpha(256);
        }
        gfit(x, 208, 100, du_who(s), c, ts(1, 2));
    }
    if (now >= 0 && now < 8 * 768) gfit(120, 226, 232, TR(T_DU_TELL), th->mute, 1);
}
static void du_play_draw(int32_t now)
{
    if (du.mission == DM_PIPE) du_pipes(now);
    else if (du.mission == DM_DRUM) du_drum(now);
    else du_field(now);
    du_hud();
}
static void du_pair(int32_t y, int ready_marks)  /* the two rocks side by side, named; a tick over the one that is ready */
{
    int s;
    for (s = 0; s < 2; s++) {
        int32_t x = s ? 164 : 76;
        if (s != du.seat && !du_peer()) {
            galpha(90 + ipulse(fm1_ms, 1400) * 100 / 256); gcirc(x, y, 10, th->mute); gtextc(x, y - 4, "?", th->mute, 2); galpha(256);
        } else du_rock(x, y, s, 256);
        gfit(x, y + 16, 110, du_who(s), DU_COL[s], ts(1, 2));
        if (ready_marks && du.ready[s]) du_tick_mark(x + 18, y - 4, th->pad);
    }
}
static void du_stars(int32_t cx, int32_t y, int n)
{
    int k;
    for (k = 0; k < 3; k++) gh_gem_draw(cx + (k - 1) * 22, y, 8, th->pad, k < n);
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
        gfit(120, 22, 232, "UNDERSONG", th->caps[(tm / 500u) % 5u], 4);
        du_pair(92, 0);
        galpha(150 + ipulse(tm, 1500) * 106 / 256);
        y = 126; y += 5 * gfit(120, y + 4, 232, TR(T_DU_WAIT), th->pad, ts(2, 3)) + 16;
        galpha(256);
        a = ts(1, 3);
        while (a > 1 && y + gblock_h(232, TR(T_DU_CONNECT), a) > 238 - gblock_h(232, TR(T_REC_BACK), 1) - 6) a--;
        gblock(4, y, 232, TR(T_DU_CONNECT), th->text, a, 1);
        gfoot(238, TR(T_REC_BACK), th->mute, th->bg, 1);
        break;
    case DUS_MENU:
        gfit(120, 12, 232, "UNDERSONG", th->caps[(tm / 500u) % 5u], 3);
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
                while (sc > 1 && gtextw(DU_NAME[m], sc) > (duo_best[m] ? 130 : 216)) sc--;
                gtext(14, y + 5 + (a - sc) * 2, DU_NAME[m], sel ? th->text : th->mute, sc);
                if (duo_best[m]) { num_str(b, duo_best[m]); gtextr(226, y + 5 + (a - 1) * 2, b, th->pad, imin(a, 2)); }
            }
            y += h + 3;
        }
        gfoot(238, TR(T_DU_CHOOSE), th->mute, th->bg, 1);
        break;
    case DUS_CARD: {
        const char *s = du.page == 0u ? TR(T_DU_S1 + du.mission) : TR(T_DU_H1 + du.mission);
        int32_t foot = du.page ? 150 : 214;      /* (how it is played: the two rocks under it, ticked as each is ready) */
        gfit(120, 14, 232, DU_NAME[du.mission], th->caps[du.mission % 5u], 3);
        a = ts(2, 3);
        while (a > 1 && 38 + gblock_h(228, s, a) > foot) a--;
        gblock(6, 38, 228, s, th->text, a, 1);
        if (du.page) du_pair(166, 1);
        galpha(150 + ipulse(tm, 1500) * 106 / 256);
        if (du.page == 0u) gfit(120, 224, 232, TR(T_PRESSPLAY), th->pad, ts(1, 2));
        else if (du.ready[du.seat]) gfit(120, 224, 232, TR(T_DU_READY), th->pad, ts(1, 2));
        else gfit(120, 224, 232, TR(T_DU_BOTH), th->pad, ts(1, 2));
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
            int s;
            for (s = 0; s < 2; s++) {
                int32_t x = s ? 172 : 68;
                du_rock(x, 66, s, 256);
                gfit(x, 82, 110, du_who(s), DU_COL[s], ts(1, 2));
                num_str(b, du.sc[s]); gfit(s ? 180 : 60, 110, 100, b, DU_COL[s], 3);
            }
            if (du.latched) gfit(120, 150, 232, TR(du.sc[0] == du.sc[1] ? T_DU_DRAW : du.sc[du.seat] > du.sc[!du.seat] ? T_DU_WIN : T_DU_LOSE), th->pad, ts(2, 3));
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
            b[n++] = ' '; num_str(b + n, duo_best[du.mission]);
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
    }
}
