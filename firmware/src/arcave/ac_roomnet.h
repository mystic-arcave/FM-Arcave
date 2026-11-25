/* SPDX-License-Identifier: GPL-3.0-only */
/* The music room with friends: up to four FM-1s (or emulators) in one room, through the same relay as UNDERSONG (the
 * Link page, LOCAL or ONLINE). Whoever has TOGETHER on (the room's settings; it is on unless turned off) and is in
 * their music room is in: there is no door to it, because the Link page is the door.
 *
 *   one room      the one of whoever was there first. Someone coming in gets all of it sent over (TUNING IN, a few
 *                 seconds), and their own room is left as it was in their save: it is theirs again the next time they
 *                 come in alone. SAVE in a friend's room keeps that room as one's own.
 *   one each      an instrument, or the pool, is one player's at a time. Their Rock sits on it in the room; PLAY and
 *                 its button do nothing for the others until it is free. Two at the same moment: the lower seat stays.
 *   watching      GLO held and its button (ac_hall.h room_jump) goes onto it anyway, to watch: nothing there answers
 *                 but HOME. The one watching is told whose it is, the one playing who watches (a strip on both
 *                 screens). OCT- and OCT+ pressed together by both, within RN_BOTH: they swap. The one playing decides
 *                 and says so (RO_SWAP), so the two FM-1s never disagree. When the player leaves it, it is the watcher's.
 *   what is sent  not sound. Every FM-1 plays the whole room itself, on the shared clock (seat 0's, as in UNDERSONG),
 *                 and what the players change is sent: each FM-1 compares the room with what it last told or heard
 *                 (a copy, rn.sh) and sends the bytes that differ, a few a frame. So the laws, layouts, mixer, scale
 *                 and tempo are the same everywhere, the steady devices fall on the same beats, and the loose parts of
 *                 each world (stones, pods, shards) fall as each FM-1 has them. Keys played by hand are sent as keys.
 *                 Whatever changes by chance (a layout tending itself, the pool's dives) is drawn by one FM-1 only,
 *                 the one whose player is on that instrument, else the host; the loop is recorded by that one too and
 *                 each hit recorded is told to the rest.
 *   I HEAR        MY OWN (the settings): each FM-1 sounds only the instrument its player is on, for players in one
 *                 room with four loudspeakers.
 *   the same four  every FM-1 plays the whole room itself, so all of them need the same instrument in each place
 *                 (the site's MACHINES page can load others: ac_slots.h). Each says which it has (rn_kit, in its
 *                 hello); one that hears a lower seat with other instruments steps out (it says no more that it is
 *                 here, and has its own room) for as long as that one is in its music room.
 *
 * The messages: control changes on channel 16, as all of the link's are.
 *   CC 20 n       "I am in my music room", every 300 ms; n: a number drawn on coming in, above every number heard, so
 *                 seats (the numbers in order) do not move when someone joins
 *   CC 21 a, CC 22 b, CC 23 c, CC 24+s op    a message of three numbers from seat s (RO_...); the relay passes the
 *                 four on in one piece
 * Seat 0 also says where its clock is (ac_sync.h, as UNDERSONG's leader does), and its whole clock (RO_EPOCH). */

#define RN_CC 20u
enum { RO_HELLO = 1, RO_EPOCH, RO_SIT, RO_POS, RO_LOOK, RO_NAME, RO_POKE, RO_SCRUB, RO_KEY, RO_LA, RO_LB, RO_WIPE,
       RO_NEED, RO_SNAP0, RO_SPOKE, RO_SLA, RO_SLB, RO_DONE, RO_BYE, RO_SWAP };
#define RN_BOTH 1500u                            /* ms: OCT- and OCT+ by both at once, near enough */
/* what is the same on every FM-1, as one row of bytes: each instrument up to its lights, the four strips, the pool's
   seven after its cursor, nine of the room's own */
#define RI_REP ((int)__builtin_offsetof(rinst_t, fl))
#define RO_RUN ((int)__builtin_offsetof(rinst_t, run))
#define RO_ALGO ((int)__builtin_offsetof(rinst_t, algo))
#define RO_K ((int)__builtin_offsetof(rinst_t, k))
#define RN_MIX (RI_N * RI_REP)
#define RN_MX (RN_MIX + RI_N * 8)
#define RN_RM (RN_MX + 7)
#define RN_N (RN_RM + 9)
#define RN_REC (RN_RM + 8)
static const col_t RN_COL[4] = {AC_RGB(240, 96, 200), AC_RGB(110, 232, 210), AC_RGB(255, 150, 50), AC_RGB(190, 230, 90)};
typedef struct {
    uint8_t nonce, tag, st, est, look[3], look_ok, pos_ok, alien, vw;   /* alien: it has other instruments in its places; vw: the station it watches */
    uint32_t last;
    int32_t x, y, tx, ty, th;                    /* where their Rock is (px), and where it was last said to be */
    char nm[PF_NAME_MAX + 2];
} rn_peer_t;
static struct {
    uint8_t on, nonce, tag, seat, np, st, tuning, guest, src, ra, rb, rc, hello_i, reinit, was_lesson, dump_on, kicked, out;   /* out: stepped out (other instruments) */
    uint8_t vw, svw, ask;                        /* the station this player watches (and last said); 1 + the seat asking for ours */
    uint32_t swap_t, chord_t, ask_t, last_sit;
    uint8_t ros[4];                              /* the numbers in the room, by seat */
    int8_t slot[4];                              /* each seat's place in p[] (-1: this FM-1, or nobody) */
    rn_peer_t p[4];
    uint32_t listen, last_tx, last_pos, last_need, last_scrub, last_local, snap_rx;
    uint16_t la[4][3], dump, scan, scrub_i, got, total;
    uint8_t la_ok[4];
    int16_t px, py;
    uint8_t sh[RN_N], pend[(RN_N + 7) / 8];      /* the room as last told or heard; bytes heard while a lesson had the room */
} rn;

static int rn_live(int i) { return rn.p[i].nonce && fm1_ms - rn.p[i].last < 2500u; }
/* which instruments stand in this FM-1's four places, as ten bits: 0 the firmware's own four */
static uint32_t rn_kit(void)
{
    uint32_t h = 0, j;
    for (j = 0; j < RI_N; j++) if (mi_on((int)j)) h = h * 31u + (j + 1u) * 7919u + mb.id[MM_NSLOT + j] * 131u + mb.ver[MM_NSLOT + j];
    return h ? 1u + h % 1023u : 0u;
}
static int rn_act(void) { return rn.on && rn.nonce && rn.np > 1u && rn.seat < 4u; }   /* with at least one friend, and a seat */
static int rn_lesson(void) { return rt.on && rt.list != 1; }   /* a lesson has the room: it is not the shared one just now */
static rn_peer_t *rn_at(int seat) { return seat >= 0 && seat < 4 && rn.slot[seat] >= 0 ? &rn.p[rn.slot[seat]] : 0; }
static int rn_guest(void) { return rn.guest; }
static int rn_tuning(void) { return rn.on && rn.tuning; }
static int rn_leads(void) { return rn_act() && rn.seat == 0u; }   /* (ac_sync.h: this FM-1 says where its clock is) */
static int rn_follows(void) { return rn_act() && rn.seat > 0u; }
static int rn_host(void)                         /* the lowest seat that has the room: it draws what nobody is on, and mends */
{
    int s;
    if (!rn_act()) return 1;
    if (!rn.tag) return 0;
    for (s = 0; s < (int)rn.seat; s++) { const rn_peer_t *q = rn_at(s); if (q && q->est) return 0; }
    return 1;
}
static int rn_st_of(int who) { return who == RI_ROOTS ? 1 : who == RI_GEODE ? 2 : who == RI_MIX ? 3 : who == RI_RACK ? 4 : who == RI_SEAM ? 5 : 0; }
static int rn_seat_on(int st)                    /* the friend's seat that is on station st (1..5); -1: none */
{
    int s;
    if (!rn_act() || !st) return -1;
    for (s = 0; s < (int)rn.np; s++) { const rn_peer_t *q = rn_at(s); if (q && q->st == st) return s; }
    return -1;
}
static int rn_free(int st) { return rn_seat_on(st) < 0; }
static int rn_auth(int inst)
{
    int st = rn_st_of(inst);
    if (!rn_act() || !rn.tag) return 1;
    if (rn.st == st) return 1;
    if (rn_seat_on(st) >= 0) return 0;
    return rn_host();
}
static int rn_loop_room(int inst)                /* (so no two FM-1s fill the loop's last places with different hits) */
{
    int k, n = 0;
    if (!rn_act()) return 1;
    for (k = 0; k < rm.loop_n; k++) n += (rm.loop[k].ik >> 4) == inst;
    return n < RL_N / RI_N;
}
static int rn_deaf(int inst) { return rset.mine && rn_act() && rn.st && rn.st != 3 && RH_WHO[rn.st - 1] != inst; }
static const char *rn_name(int seat)
{
    static char b[4][12];
    const rn_peer_t *q = rn_at(seat);
    const char *s = "PLAYER";
    int n = 0;
    if (q && q->nm[0]) return q->nm;
    while (*s) b[seat & 3][n++] = *s++;
    b[seat & 3][n++] = ' '; b[seat & 3][n++] = (char)('1' + (seat & 3)); b[seat & 3][n] = 0;
    return b[seat & 3];
}
static const char *rn_who(int st) { int s = rn_seat_on(st); return s < 0 ? "" : rn_name(s); }
static int rn_sit(int st)                        /* this player opens station st: 0 if a friend got there first */
{
    if (rn_free(st)) return 1;
    banner_show(rn_who(st), "IS PLAYING IT", 0); snd_ui(55);
    return 0;
}
static void rn_jumped(int st)                    /* GLO and its button (ac_hall.h room_jump): a friend's is watched */
{
    rn.vw = 0;
    if (!rn_act() || rn_free(st)) return;
    rn.vw = (uint8_t)st;
    banner_show("WATCHING", rn_who(st), 0); snd_ui(60);
}

static uint32_t rn_budget(void) { uint32_t r = bt_tx_room(); return r > 40u ? 6u : r > 24u ? 2u : 0u; }   /* messages this frame */
static void rn_tx(uint32_t op, uint32_t a, uint32_t b, uint32_t c)
{
    if (rn.seat > 3u) return;
    midi_send(0xBFu, 21u, a & 127u); midi_send(0xBFu, 22u, b & 127u); midi_send(0xBFu, 23u, c & 127u);
    midi_send(0xBFu, 24u + rn.seat, op & 127u);
}
static uint8_t *rn_byte(int i)
{
    static uint8_t *const G[9] = {&rm.scale, &rm.root, &rm.bars, &rm.quant, &rm.etime, &rm.efb, &rm.bpm, &rm.loop_off, &rm.rec};
    if (i < RN_MIX) return (uint8_t *)&ri[i / RI_REP] + i % RI_REP;
    if (i < RN_MX) return (uint8_t *)&rmx[(i - RN_MIX) / 8] + (i - RN_MIX) % 8;
    if (i < RN_RM) return (uint8_t *)&mx + 1 + (i - RN_MX);
    return G[i - RN_RM];
}
/* a byte as it may be: whatever comes in over the link is brought into range before it is believed */
static uint8_t rn_sane(int i, uint32_t v)
{
    static const uint8_t AM[4] = {3, 3, 3, 1}, SM[8] = {16, 16, 8, 8, 8, 8, 1, 1}, XM[7] = {1, 8, 3, 1, 3, 2, 15};
    if (i < RN_MIX) {
        const rdesc_t *d = RD[i / RI_REP];
        int o = i % RI_REP, j;
        if (o < 2 * RP_N * 4) return (uint8_t)((o & 3) == 0 ? v % d->nkind : (o & 3) == 2 ? imin((int32_t)v, 20) : v & 7u);
        j = (o - RO_K) & 3;
        switch ((o - RO_K) >> 2) {
        case 0: return (uint8_t)imin((int32_t)v, d->kmax[j]);
        case 1: case 2: return (uint8_t)imin((int32_t)v, 8);
        case 3: return (uint8_t)imin((int32_t)v, d->sqmax[j]);
        case 4: return (uint8_t)imin((int32_t)v, AM[j]);
        default: break;
        }
        switch (o - RO_K - 20) {                 /* lfo_rate, lfo_on, run, arp, seq, algo, preset, sel, oct */
        case 0: return (uint8_t)(v % 6u);
        case 5: return (uint8_t)(v % d->nalgo);
        case 6: return (uint8_t)(v % d->npreset);
        case 7: return (uint8_t)imin((int32_t)v, d->selmax);
        case 8: return (uint8_t)(int8_t)iclamp((int8_t)v, -2, 2);
        default: return (uint8_t)(v & 1u);
        }
    }
    if (i < RN_MX) return (uint8_t)imin((int32_t)v, SM[(i - RN_MIX) & 7]);
    if (i < RN_RM) return (uint8_t)imin((int32_t)v, XM[i - RN_MX]);
    switch (i - RN_RM) {
    case 0: return (uint8_t)(v % RSC_N);
    case 1: return (uint8_t)(v % 12u);
    case 2: return (uint8_t)(v == 1u || v == 2u || v == 8u ? v : 4u);
    case 3: return (uint8_t)(v % 3u);
    case 4: return (uint8_t)(v & 3u);
    case 5: return (uint8_t)imin((int32_t)v, 8);
    case 6: return (uint8_t)iclamp((int32_t)v, 50, 200);
    default: return (uint8_t)(v & 1u);
    }
}
static void rn_applied(int i)                    /* a byte was changed from outside: what has to follow */
{
    if (i < RN_MIX) {
        int w = i / RI_REP, o = i % RI_REP;
        if (o < RP_N * 4 || o == RO_ALGO || o == RO_ALGO + 1) rn.reinit |= (uint8_t)(1u << w);
        else if (o == RO_RUN) RD[w]->play(ri[w].run);
    } else if (i >= RN_RM + 4 && i <= RN_RM + 6) room_echo_set();
}
static void rn_put(int i, uint32_t v)
{
    uint8_t x;
    if (i < 0 || i >= RN_N) return;
    x = rn_sane(i, v);
    rn.sh[i] = x;
    if (rn_lesson()) { rn.pend[i >> 3] |= (uint8_t)(1u << (i & 7)); return; }   /* (the lesson has the room: it is put there after) */
    if (*rn_byte(i) != x) { *rn_byte(i) = x; rn_applied(i); }
}
static void rn_loop_add(uint32_t a, uint32_t b, uint32_t c, uint32_t a2, uint32_t b2, uint32_t c2)
{
    rev_t e;
    int k;
    e.t = (uint16_t)(a << 5 | (b & 31u)); e.ik = (uint8_t)((b >> 5 & 3u) << 4 | (c & 15u));
    e.deg = (uint8_t)imin((int32_t)a2, 40); e.vel = (uint8_t)imax(1, (int32_t)(b2 << 1 | (c2 >> 3 & 1u)));
    e.sq = (uint8_t)((c >> 4 & 7u) << 4 | (c2 & 7u));
    if ((int)(e.ik & 15u) >= ri_nsnd(e.ik >> 4) || e.t >= 8u * TICKS_BAR || rm.loop_n >= RL_N) return;
    for (k = 0; k < rm.loop_n; k++) if (rm.loop[k].t == e.t && rm.loop[k].ik == e.ik && rm.loop[k].deg == e.deg) return;   /* (heard twice) */
    rm.loop[rm.loop_n++] = e;
}
static void rn_loop_send(const rev_t *e, int snap)
{
    rn_tx(snap ? RO_SLA : RO_LA, e->t >> 5, (e->t & 31u) | (uint32_t)(e->ik >> 4) << 5, (e->ik & 15u) | (uint32_t)(e->sq >> 4) << 4);
    rn_tx(snap ? RO_SLB : RO_LB, e->deg, e->vel >> 1, (e->sq & 7u) | (e->vel & 1u) << 3);
}
static void rn_loop_tx(const rev_t *e) { if (rn_act() && rn.tag && !rn.tuning) rn_loop_send(e, 0); }
static void rn_key_tx(int inst, int top, int i) { if (rn_act() && rn.tag && !rn.tuning) rn_tx(RO_KEY, (uint32_t)inst | (uint32_t)top << 2, (uint32_t)i, 100); }
static void rn_wipe_tx(int who) { if (rn_act() && rn.tag && !rn.tuning) rn_tx(RO_WIPE, (uint32_t)who, 0, 0); }
static void rn_shadow(void) { int i; for (i = 0; i < RN_N; i++) rn.sh[i] = *rn_byte(i); }
static void rn_give(int s)                       /* this player's station is seat s's now, who watched it: this one watches */
{
    rn_peer_t *q = rn_at(s);
    if (q) { q->st = rn.st; q->vw = 0; }
    rn_tx(RO_SWAP, rn.st, 1, (uint32_t)s);
    rn.vw = rn.st; rn.ask = 0; rn.swap_t = fm1_ms;
    banner_show(rn_name(s), "PLAYS IT NOW", 0); snd_ui(67);
#ifdef DINGHY_HOST
    printf("%6u ms  swapped: seat %d plays station %u, seat %u watches\n", fm1_ms, s, rn.vw, rn.seat);
#endif
}
/* on a station, OCT- and OCT+ together: the one watching asks, the one playing gives if both did it near enough */
static void rn_chord(void)
{
    if (!rn_act() || !(BHELD(BTN_OCT_DOWN) && BHELD(BTN_OCT_UP) && (BDOWN(BTN_OCT_DOWN) || BDOWN(BTN_OCT_UP)))) return;
    rn.chord_t = fm1_ms;
    if (rn.vw) rn_tx(RO_SWAP, rn.vw, 0, 0);
    else if (rn.st && rn.ask && fm1_ms - rn.ask_t < RN_BOTH) rn_give(rn.ask - 1);
}

/* who is in: this FM-1 and the ones heard lately, the lowest number first; the first four have seats */
static void rn_roster(void)
{
    uint8_t v[5], t;
    int n = 0, i, j, s;
    if (!rn.out) v[n++] = rn.nonce;
    for (i = 0; i < 4; i++) if (rn_live(i)) v[n++] = rn.p[i].nonce;
    for (i = 1; i < n; i++) for (j = i; j > 0 && v[j] < v[j - 1]; j--) { t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    rn.np = (uint8_t)imin(n, 4); rn.seat = 4;
    for (s = 0; s < 4; s++) {
        rn.ros[s] = s < n ? v[s] : 0; rn.slot[s] = -1;
        if (s >= n) continue;
        if (v[s] == rn.nonce) rn.seat = (uint8_t)s;
        else for (i = 0; i < 4; i++) if (rn_live(i) && rn.p[i].nonce == v[s]) rn.slot[s] = (int8_t)i;
    }
}
static void rn_unpack_own(void)                  /* this FM-1's own room out of its save again; its settings stay as they are now */
{
    uint8_t keep[sizeof rset];
    int i;
    for (i = 0; i < (int)sizeof rset; i++) keep[i] = ((const uint8_t *)&rset)[i];
    room_unpack(); room_echo_set();
    for (i = 0; i < (int)sizeof rset; i++) ((uint8_t *)&rset)[i] = keep[i];
}
static void rn_own_room(void)                    /* the friend's room is over, or never came: this FM-1's own again */
{
    if (rn.guest) { rn_unpack_own(); rn.guest = 0; }
    rn.tuning = 0; rn.tag = rn.nonce; rn_shadow();
}
static void rn_op(int s, uint32_t op, uint32_t a, uint32_t b, uint32_t c)   /* s: the seat that sent it */
{
    rn_peer_t *q = rn_at(s);
    int i;
    if (!q) return;
    if (op != RO_HELLO && op != RO_BYE && (q->alien || rn.out)) return;   /* (another room's: not ours to hear) */
    switch (op) {
    case RO_HELLO:
        q->tag = (uint8_t)a; q->st = (uint8_t)((b & 7u) % 6u); q->est = b >> 3 & 1u;
        q->alien = (c | (b >> 4 & 7u) << 7) != rn_kit();
        if (q->alien && !rn.out && s < (int)rn.seat) {   /* a lower seat with other instruments: this FM-1 steps out */
            rn_tx(RO_BYE, 0, 0, 0);
            if (rn.guest) rn_unpack_own();
            rn.guest = 0; rn.tuning = 0; rn.tag = 0; rn.st = 0; rn.dump_on = 0; rn.out = 1;
            banner_show("OTHER INSTRUMENTS", "YOUR OWN ROOM", 0); snd_ui(55);
#ifdef DINGHY_HOST
            printf("%6u ms  seat %d has other instruments: stepped out\n", fm1_ms, s);
#endif
        }
        break;
    case RO_EPOCH:                               /* the leader's whole clock: the loop is counted from its zero */
        if (s == 0 && rn.seat != 0u) {
            int32_t d = (int32_t)((a << 7 | b | c << 14) << 5) - (int32_t)(tr_tick & 0x3FFFFFFu), k;
            k = (d + (d >= 0 ? 1 << 18 : -(1 << 18))) / (1 << 19);
            if (k) tr_tick += (uint32_t)(k * (1 << 19));
        }
        break;
    case RO_SIT:                                 /* a: where it plays; b: what it watches */
        if (b % 6u && b % 6u == rn.st && b % 6u != q->vw) { banner_show(rn_name(s), "IS WATCHING YOU", 0); snd_ui(60); }
        q->st = (uint8_t)(a % 6u); q->vw = (uint8_t)(b % 6u);
        break;
    case RO_SWAP:
        if (b) {                                 /* given: seat c plays station a now, s watches it */
            rn_peer_t *g = rn_at((int)c);
            q->st = 0; q->vw = (uint8_t)(a % 6u);
            if (g) { g->st = q->vw; g->vw = 0; }
            if (c == rn.seat && a == rn.vw) {
                rn.vw = 0; rn.swap_t = fm1_ms; banner_show("SWAPPED", "YOU PLAY IT", 0); snd_chime(0);
#ifdef DINGHY_HOST
                printf("%6u ms  swapped: this seat plays station %u\n", fm1_ms, a);
#endif
            }
        } else if (a && a == rn.st) {            /* asked for by one watching it */
            rn.ask = (uint8_t)(s + 1); rn.ask_t = fm1_ms;
            if (fm1_ms - rn.chord_t < RN_BOTH) rn_give(s);
        }
        break;
    case RO_POS:
        q->tx = (int32_t)(a << 3 | (c >> 4 & 7u)); q->ty = (int32_t)(b << 1);
        if (!q->pos_ok) { q->x = q->tx; q->y = q->ty; q->pos_ok = 1; }
        break;
    case RO_LOOK: q->look[0] = (uint8_t)(a % 10u); q->look[1] = (uint8_t)b; q->look[2] = (uint8_t)c; q->look_ok = 1; break;
    case RO_NAME:                                /* two letters of a name, from a: (0, 0) where it ends */
        if (a + 1u < sizeof q->nm) {
            q->nm[a] = (char)(b >= 32u && b < 127u ? b : 0); q->nm[a + 1u] = (char)(b && c >= 32u && c < 127u ? c : 0);
            if (!b || !c) for (i = (int)a + 2; i < (int)sizeof q->nm; i++) q->nm[i] = 0;
            q->nm[sizeof q->nm - 1] = 0;
        }
        break;
    case RO_SCRUB: if (rn.tuning || fm1_ms - rn.last_local < 1000u) break;   /* (a mend from the host; not over what was just changed here) */
        /* fall through */
    case RO_POKE: rn_put((int)(a << 3 | (b & 7u)), (b >> 3 & 1u) << 7 | c); break;
    case RO_KEY:
        if (rn_lesson() || rn.tuning) break;
        if (a & 4u) { if (b < 11u) RD[a & 3u]->top((int)b); } else if (b < 16u) RD[a & 3u]->key((int)b, 200);
        break;
    case RO_LA: case RO_SLA: rn.la[s][0] = (uint16_t)a; rn.la[s][1] = (uint16_t)b; rn.la[s][2] = (uint16_t)c; rn.la_ok[s] = (uint8_t)op; break;
    case RO_LB: case RO_SLB:
        if (rn.la_ok[s] != (op == RO_LB ? RO_LA : RO_SLA)) break;
        rn.la_ok[s] = 0;
        if (op == RO_SLB) { if (rn.tuning != 2u || q->nonce != rn.src) break; rn.got++; rn.snap_rx = fm1_ms; }
        rn_loop_add(rn.la[s][0], rn.la[s][1], rn.la[s][2], a, b, c);
        break;
    case RO_WIPE:
        if (a >= RI_N) rm.loop_n = 0;
        else { uint16_t n = 0; for (i = 0; i < rm.loop_n; i++) if ((rm.loop[i].ik >> 4) != (int)a) rm.loop[n++] = rm.loop[i]; rm.loop_n = n; }
        break;
    case RO_NEED: if (a == rn.nonce && rn.tag && !rn.tuning) { rn.dump_on = 1; rn.dump = 0; } break;   /* (asked for the room: all of it, from the start) */
    case RO_SNAP0:
        if (rn.tuning && q->nonce == rn.src) {
            if (!rn.guest) { room_save_now(); rn.guest = 1; }   /* (this FM-1's own room is kept as it is, first) */
            if (rt.on) rtutor_close();
            rn.tuning = 2; rn.got = 0; rn.total = (uint16_t)imax(1, (int32_t)(a << 7 | b)); rn.snap_rx = fm1_ms; rm.loop_n = 0;
        }
        break;
    case RO_SPOKE:
        if (rn.tuning == 2u && q->nonce == rn.src) { rn_put((int)(a << 3 | (b & 7u)), (b >> 3 & 1u) << 7 | c); rn.got++; rn.snap_rx = fm1_ms; }
        break;
    case RO_DONE:
        if (rn.tuning == 2u && q->nonce == rn.src) {
            rn.tuning = 0; rn.tag = (uint8_t)a; rn.reinit = 15; rn_shadow();
            for (i = 0; i < (int)sizeof rn.pend; i++) rn.pend[i] = 0;
            banner_show("MUSIC ROOM", rn_name(s), 0); snd_chime(0);
#ifdef DINGHY_HOST
            printf("%6u ms  tuned in: seat %d's room, %u of %u heard, loop %u\n", fm1_ms, s, rn.got, rn.total, rm.loop_n);
#endif
        }
        break;
    case RO_BYE: q->last = fm1_ms - 5000u; break;
    default: break;
    }
}
/* a message of the link came in (ac_sync.h) */
static void rn_rx(uint32_t cc, uint32_t v)
{
    int i, fr = -1;
    if (!rn.on || rset.alone) return;
    if (cc == RN_CC) {                           /* an FM-1 in its music room: a number not heard yet takes a free place */
        if (v == rn.nonce) rn.nonce = (uint8_t)imin(127, rn.nonce + 1 + (int32_t)((ac_rnd() ^ fm1_ms) % 8u));   /* the same number: another */
        for (i = 0; i < 4; i++) { if (rn.p[i].nonce == v) break; if (fr < 0 && !rn_live(i)) fr = i; }
        if (i == 4) {
            if (fr < 0 || !v) return;
            i = fr;
            { uint8_t *z = (uint8_t *)&rn.p[i]; int k; for (k = 0; k < (int)sizeof rn.p[i]; k++) z[k] = 0; }
            rn.p[i].nonce = (uint8_t)v;
        }
        rn.p[i].last = fm1_ms;
        return;
    }
    if (cc == 21u) rn.ra = (uint8_t)v;
    else if (cc == 22u) rn.rb = (uint8_t)v;
    else if (cc == 23u) rn.rc = (uint8_t)v;
    else if (cc >= 24u && cc <= 27u && rn.nonce && (int)(cc - 24u) != (int)rn.seat) { rn_roster(); rn_op((int)(cc - 24u), v, rn.ra, rn.rb, rn.rc); }
}
/* every pass of the main loop: "I am here", who sits where, and in turn how Rock looks and his name */
static void rn_poll(void)
{
    uint32_t now = fm1_ms;
    if (!rn.on || rset.alone || !rn.nonce || rn.out || now - rn.last_tx < 300u) return;
    rn.last_tx = now;
    midi_send(0xBFu, RN_CC, rn.nonce);
    rn_roster();
    if (rn.np < 2u) return;
    { uint32_t kit = rn_kit(); rn_tx(RO_HELLO, rn.tag, (uint32_t)rn.st | (rn.tag && !rn.tuning ? 8u : 0u) | (kit >> 7) << 4, kit & 127u); }
    {
        int k = rn.hello_i++ % 8u, p = (k - 1) * 2;
        if (k == 0) rn_tx(RO_LOOK, pf.eq[0], pf.eq[1], pf.eq[2]);
        else if (pf_named() ? p <= PF_NAME_MAX && (p == 0 || pf.name[p - 1]) : p == 0)
            rn_tx(RO_NAME, (uint32_t)p, pf_named() && p < PF_NAME_MAX ? (uint32_t)(uint8_t)pf.name[p] : 0u,
                  pf_named() && p + 1 < PF_NAME_MAX && pf.name[p] ? (uint32_t)(uint8_t)pf.name[p + 1] : 0u);
    }
    if (rn.seat == 0u) { uint32_t v = (tr_tick & 0x3FFFFFFu) >> 5; rn_tx(RO_EPOCH, v >> 7, v, v >> 14); }
}
static void rn_enter(void)
{
    uint32_t i;
    for (i = 0; i < sizeof rn; i++) ((uint8_t *)&rn)[i] = 0;
    for (i = 0; i < 4u; i++) rn.slot[i] = -1;
    rn.on = 1; rn.np = 1; rn.listen = fm1_ms;
}
static void rn_leave(void)
{
    if (rn.on && rn.nonce && rn.np > 1u) rn_tx(RO_BYE, 0, 0, 0);
    if (rn.on && rn.guest) rm.on = 0;            /* (it was a friend's room: this FM-1's own is read from its save next time) */
    rn.on = 0; rn.guest = 0;
}
static void room_keep(void)                      /* SAVE: the room as it is now is this FM-1's (a friend's room too) */
{
    rn.guest = 0;
    room_save_now();
}
/* once a frame in the room */
static void rn_frame(int32_t dt)
{
    uint32_t now = fm1_ms, room;
    int i, s, st, src = -1, lesson = rn_lesson();
    if (!rn.on) return;
    if (rset.alone) {                            /* TOGETHER turned off: gone from the others' rooms, one's own room again */
        if (rn.nonce) { if (rn.np > 1u) rn_tx(RO_BYE, 0, 0, 0); if (rn.guest) rn_unpack_own(); rn_enter(); }
        return;
    }
    if (!rn.nonce) {                             /* just come in: listening first, then a number above all those heard */
        int hi = 0;
        if (now - rn.listen < 600u) return;
        for (i = 0; i < 4; i++) if (rn_live(i)) hi = imax(hi, rn.p[i].nonce);
        rn.nonce = (uint8_t)(hi >= 120 ? 1u + (ac_rnd() ^ now ^ tr_now_q8()) % 119u : (uint32_t)hi + 1u + (ac_rnd() ^ now ^ tr_now_q8()) % (hi ? 6u : 30u));
        rn.last_tx = now - 300u;
    }
    if (rn.out) {                                /* stepped out: back in, as someone new, once nobody with other instruments is there */
        int any = 0;
        for (i = 0; i < 4; i++) any |= rn_live(i) && rn.p[i].alien;
        if (!any) { rn_enter(); return; }
    }
    rn_roster();
    for (i = 0; i < 4; i++) {                    /* the friends' rocks, toward where they were last said to be */
        rn_peer_t *q = &rn.p[i];
        int32_t dx;
        if (!rn_live(i)) { if (q->nonce && now - q->last > 6000u) q->nonce = 0; continue; }
        dx = q->tx - q->x;
        q->x += dx * imin(dt, 120) / 120; q->y += (q->ty - q->y) * imin(dt, 120) / 120; q->th += dx * imin(dt, 120) / 120 * 1159;
    }
    /* whose room: with nobody else, one's own. Else the lowest seat that has it settled is the room; anyone whose
       room is another one (or none yet) asks that seat for all of it */
    if (rn.np < 2u || rn.seat > 3u) {
        if (rn.tuning || !rn.tag) rn_own_room();
        rn.dump_on = 0; rn.st = 0;
        rn_shadow();
    } else {
        for (s = 0; s < (int)rn.np && src < 0; s++) {
            if (s == (int)rn.seat) { if (rn.tag && !rn.tuning) src = s; }
            else { const rn_peer_t *q = rn_at(s); if (q && q->est && q->tag) src = s; }
        }
        if (src < 0) {                           /* nobody has a room settled yet: the lowest seat's it is */
            if (rn.seat == 0u && !rn.tag && now - rn.listen > 1500u) { rn.tag = rn.nonce; rn_shadow(); }
        } else if (src != (int)rn.seat && rn_at(src)->tag != rn.tag && !rn.tuning) {
            rn.tuning = 1; rn.src = rn_at(src)->nonce; rn.last_need = now - 2000u; rn.dump_on = 0;
            if (rm.focus != RI_WALK || rt.on) { if (rt.on) rtutor_close(); room_back(); rh.zoom = 0; rh.closing = 0; }
#ifdef DINGHY_HOST
            printf("%6u ms  tuning in to seat %d (number %u)\n", fm1_ms, src, rn.src);
#endif
        }
        if (rn.tuning) {
            int there = 0;
            for (i = 0; i < 4; i++) if (rn_live(i) && rn.p[i].nonce == rn.src) there = 1;
            if (!there) { rn.tuning = 0; if (!rn.tag) rn_own_room(); }   /* (whoever was asked has gone) */
            else if (rn.tuning == 2u && now - rn.snap_rx > 3000u) rn.tuning = 1;
            if (rn.tuning == 1u && now - rn.last_need >= 1500u) { rn.last_need = now; rn_tx(RO_NEED, rn.src, 0, 0); }
        }
    }
    if (!rn_act()) { rn.was_lesson = (uint8_t)lesson; rn.vw = 0; return; }
    /* a lesson is over: what was heard while it had the room is put in now */
    if (rn.was_lesson && !lesson) {
        for (i = 0; i < RN_N; i++) if (rn.pend[i >> 3] >> (i & 7) & 1u) { if (*rn_byte(i) != rn.sh[i]) { *rn_byte(i) = rn.sh[i]; rn_applied(i); } }
        for (i = 0; i < (int)sizeof rn.pend; i++) rn.pend[i] = 0;
        rm.rec = rn.sh[RN_REC];
    }
    rn.was_lesson = (uint8_t)lesson;
    /* where this player sits, or what it watches: said when it changes (watching, every second); a lower seat on the
       same one, and this player is back in the room. Just after a swap the others' word of it may be on its way */
    if (rn.vw && rn_st_of(rm.focus) != rn.vw) rn.vw = 0;
    if (rn.vw && rn_free(rn.vw) && now - rn.swap_t > RN_BOTH) { rn.vw = 0; banner_show("NOBODY ON IT", "YOU PLAY IT", 0); snd_chime(0); }
    st = rt.on ? rn.st : rn.vw ? 0 : rn_st_of(rm.focus);
    if (st != rn.st || rn.vw != rn.svw || (rn.vw && now - rn.last_sit >= 1000u)) {
        rn.st = (uint8_t)st; rn.svw = rn.vw; rn.last_sit = now; rn_tx(RO_SIT, (uint32_t)st, rn.vw, 0);
    }
    if (rn.st) {
        s = rn_seat_on(rn.st);
        if (s >= 0 && s < (int)rn.seat && now - rn.swap_t > RN_BOTH) {
            if (rt.on) rtutor_close();
            room_back(); rn.st = 0; rn_tx(RO_SIT, 0, 0, 0);
            banner_show(rn_name(s), "IS PLAYING IT", 0); snd_ui(55);
        }
    } else if (rm.focus == RI_WALK && now - rn.last_pos >= 150u) {   /* loose in the room: where Rock is */
        int x = (int)(rh.x >> 8), y = (int)(rh.y >> 8);
        if (iabs(x - rn.px) > 1 || iabs(y - rn.py) > 1 || now - rn.last_pos >= 1200u) {
            rn.px = (int16_t)x; rn.py = (int16_t)y; rn.last_pos = now;
            rn_tx(RO_POS, (uint32_t)iclamp(x, 0, 1016) >> 3, (uint32_t)iclamp(y, 0, 254) >> 1, ((uint32_t)iclamp(x, 0, 1016) & 7u) << 4);
        }
    }
    room = rn_budget();
    if (rn.dump_on) {                            /* the whole room, for whoever asked: its bytes, then its loop */
        if (rn.dump == 0u && room) { uint32_t tot = (uint32_t)RN_N + rm.loop_n; rn_tx(RO_SNAP0, tot >> 7, tot & 127u, 0); room--; }
        while (room && rn.dump < RN_N) { uint32_t v = *rn_byte(rn.dump); rn_tx(RO_SPOKE, (uint32_t)rn.dump >> 3, (rn.dump & 7u) | (v >> 7) << 3, v & 127u); rn.dump++; room--; }
        while (room >= 2u && rn.dump >= RN_N && rn.dump - RN_N < rm.loop_n) { rn_loop_send(&rm.loop[rn.dump - RN_N], 1); rn.dump++; room -= 2u; }
        if (room && rn.dump >= RN_N + rm.loop_n) { rn_tx(RO_DONE, rn.tag, 0, 0); rn.dump_on = 0; }
    } else if (rn.tag && !rn.tuning && !lesson) {
        /* what was changed here since it was last told: a few bytes a frame, from where the last frame stopped */
        for (i = 0; i < RN_N && room; i++) {
            int k = (rn.scan + i) % RN_N;
            uint32_t v = *rn_byte(k);
            if (v == rn.sh[k]) continue;
            rn.sh[k] = (uint8_t)v; rn.last_local = now;
            rn_tx(RO_POKE, (uint32_t)k >> 3, ((uint32_t)k & 7u) | (v >> 7) << 3, v & 127u); room--;
            if (!room) rn.scan = (uint16_t)((k + 1) % RN_N);
        }
        if (room && rn_host() && now - rn.last_scrub >= 250u) {   /* the host mends: one byte of the room, round and round */
            uint32_t v;
            rn.last_scrub = now; rn.scrub_i = (uint16_t)((rn.scrub_i + 1u) % RN_N); v = rn.sh[rn.scrub_i];
            rn_tx(RO_SCRUB, (uint32_t)rn.scrub_i >> 3, (rn.scrub_i & 7u) | (v >> 7) << 3, v & 127u);
        }
    }
    if (rn.reinit && !lesson) { for (i = 0; i < RI_N; i++) if (rn.reinit >> i & 1u) RD[i]->init(); rn.reinit = 0; }
}

/* ---------------------------------------------------------------- what is seen of the others */
static void rn_body(int32_t x, int32_t y, int seat, int32_t z, int32_t a)
{
    uint8_t e[4] = {0, 0, 0, 0};
    const rn_peer_t *q = rn_at(seat);
    if (q && q->look_ok) { e[0] = q->look[0]; e[1] = q->look[1]; e[2] = q->look[2]; }
    deco_rock_ex(x, y, z, a, (fm1_ms + (uint32_t)seat * 1300u) % 3100u < 150u ? 30 : 256, e, fm1_ms, RN_COL[seat & 3]);
}
static void rn_station_draw(int st0, int32_t x, int32_t fy)   /* a friend on station st0 (0..4): their Rock over it, and whose it is */
{
    int s = rn_seat_on(st0 + 1);
    int32_t y;
    if (s < 0) return;
    y = fy - 84 - (tsin(rm.t, 40, s * 9000) >> 12);
    galpha(60); gdisc(x, y, 15, RN_COL[s]); galpha(256);
    rn_body(x, y, s, 200, (int32_t)(rm.t * 20u));
    gfit(x, y - 22, 76, rn_name(s), RN_COL[s], 1);
}
static void rn_rocks_draw(int32_t cx)
{
    int s;
    if (!rn_act()) return;
    for (s = 0; s < (int)rn.np; s++) {
        const rn_peer_t *q = rn_at(s);
        if (!q || q->st || q->vw || !q->pos_ok) continue;
        if (q->x - cx < -20 || q->x - cx > 260) continue;
        rn_body(q->x - cx, q->y, s, 256, q->th);
        gfit(q->x - cx, q->y - 22, 76, rn_name(s), RN_COL[s], 1);
    }
}
static void rn_draw(void)                        /* over the room: who is here; a friend's room on its way */
{
    int s;
    if (!rn.on || rset.alone) return;
    if (rn_act() && rm.focus == RI_WALK && !rn.tuning) {   /* a light for each seat, top left: this player's is ringed */
        for (s = 0; s < (int)rn.np; s++) {
            gdisc(8 + s * 12, 8, 3, RN_COL[s]);
            if (s == (int)rn.seat) gcirc(8 + s * 12, 8, 5, th->text);
        }
    }
    if (rn_act() && rm.focus != RI_WALK && !rn.tuning) {   /* on a station: whose it is, watching; who watches, playing */
        int w = -1;
        if (rn.vw) w = rn_seat_on(rn.vw);
        else if (rn.st) for (s = 0; s < (int)rn.np && w < 0; s++) { const rn_peer_t *q = rn_at(s); if (q && q->vw == rn.st) w = s; }
        if (w >= 0) {
            const char *a = rn.vw ? "WATCHING " : "", *n = rn_name(w);
            int32_t x = 3 + gtextw(a, 1);
            grect(0, 0, 240, 11, th->panel);
            gtext(3, 2, a, th->text, 1); gtext(x, 2, n, RN_COL[w & 3], 1);
            if (!rn.vw) gtext(x + gtextw(n, 1), 2, " IS WATCHING", th->text, 1);
            gtextr(237, 2, "OCT- + OCT+: SWAP", th->mute, 1);
        }
    }
    if (rn.tuning) {
        int32_t w = 200, x = 20, y = 86, f = rn.tuning == 2u ? imin(256, rn.got * 256 / imax(1, rn.total)) : 0;
        gveil(th->bg, 26);
        grect(x, y, w, 68, th->panel); gbox(x, y, w, 68, th->pad);
        gfit(120, y + 10, w - 12, "TUNING IN", th->text, 2);
        gfit(120, y + 30, w - 12, "A FRIEND'S MUSIC ROOM IS ON ITS WAY", th->mute, 1);
        gbox(x + 14, y + 44, w - 28, 9, th->mute); grect(x + 16, y + 46, (w - 32) * f / 256, 5, th->pad);
        gfit(120, y + 58, w - 12, "YOUR OWN ROOM IS KEPT AS IT WAS", th->mute, 1);
    }
}
