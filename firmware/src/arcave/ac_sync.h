/* SPDX-License-Identifier: GPL-3.0-only */
/* ARCAVE LINK: several FM-1s on one clock. Every FM-1 with LINK on says where its clock is twice a second; a hub
 * (the Link page of the site, or anything that speaks this) hears them all, picks one as the leader, and tells the
 * others where the leader's clock is, each message moved forward by that FM-1's own delay. A follower takes the
 * leader's tempo and slides its clock into place; its tune and scene stay its own, only bar and beat are shared.
 *
 * The messages are plain control changes on MIDI channel 16, so they pass through anything that carries MIDI:
 * USB, the TRS jack, a Bluetooth MIDI link that drops SysEx.
 *   CC 102, 103   the tempo: BPM * 32, high 7 bits then low 7
 *   CC 104..106   the clock: quarter ticks (384 to a beat) modulo 2^21, high to low
 *   CC 107        who and what: bits 0-2 the sender's number (1-4; 0 = the hub), bit 3 "follow this", bit 4 "you
 *                 lead". The message counts from the moment this last byte arrives.
 *   CC 108 n      a ping from the hub; the FM-1 answers CC 109 n at once (the hub measures the delay from it)
 * A plain MIDI clock (F8, FA) is followed too, when no hub speaks.
 * CC 110..118 are UNDERSONG's, the game for two (ac_duo.h); its leader speaks as sender 5 with "follow".
 * CC 20..27 are the music room's, played together (ac_roomnet.h); its seat 0 speaks as sender 5 too.
 * CC 70..92 are the remote's: this FM-1 as the controller of the game in a browser (ac_remote.h). */

enum { LK_ALONE, LK_FOLLOW, LK_LEAD, LK_CLOCK };
static uint8_t link_id;                          /* 0: off; 1-4 */
static struct {
    uint32_t last_tx, last_rx, last_lead, last_clk, clk_ms0, last_duo;
    uint32_t tempo, pos;
    int32_t best, nbest, clk_pulses, clk_pos;
    uint8_t state;
} lk;

static void link_apply(int32_t diff_q8)          /* the leader's clock minus ours */
{
    /* a message read late only ever looks early: of four in a row, trust the one that asks for the most */
    if (!lk.nbest || diff_q8 > lk.best) lk.best = diff_q8;
    if (++lk.nbest >= 4 || iabs(diff_q8) > (int32_t)(TICKS_BEAT << 8)) {
        if (iabs(lk.best) > 96) tr_nudge_q8(lk.best);   /* under a third of a tick: leave it */
        lk.nbest = 0;
    }
}
static void link_poll(void)
{
    uint8_t st, d1, d2;
    uint32_t now = fm1_ms;
    while (midi_get(&st, &d1, &d2)) {
        if (st == 0xBFu && ((d1 >= 74u && d1 <= 92u) || d1 == 60u || d1 == 61u) && fmr_rx(d1, d2)) ;   /* the page that plays with this FM-1 (ac_remote.h) */
        else if (st == 0xBFu && d1 >= 110u && d1 <= 118u) duo_rx(d1, d2);   /* UNDERSONG's own messages (ac_duo.h) */
        else if (st == 0xBFu && d1 >= 20u && d1 <= 27u) rn_rx(d1, d2);   /* the music room with friends (ac_roomnet.h) */
        else if (st == 0xBFu && (link_id || duo_follows() || rn_follows())) {
            if (d1 == 102u) lk.tempo = (uint32_t)d2 << 7;
            else if (d1 == 103u) lk.tempo |= d2;
            else if (d1 == 104u) lk.pos = (uint32_t)d2 << 14;
            else if (d1 == 105u) lk.pos |= (uint32_t)d2 << 7;
            else if (d1 == 106u) lk.pos |= d2;
            else if (d1 == 107u) {
                /* (sender 5: the leader of a game of UNDERSONG, followed by its second seat only) */
                if ((d2 & 8u) && ((d2 & 7u) == 5u ? duo_follows() || rn_follows() : link_id && (d2 & 7u) != link_id)) {
                    int32_t mine = (int32_t)((tr_now_q8() >> 6) & 0x1FFFFFu), d = (int32_t)lk.pos - mine;
                    if (d > 0x100000) d -= 0x200000;
                    if (d < -0x100000) d += 0x200000;
                    if (lk.tempo >= 40u * 32u && lk.tempo <= 300u * 32u) tr_bpm_q8 = lk.tempo << 3;
                    link_apply(d * 64);
                    lk.last_rx = now; tr_follow = 1;
                }
                if (d2 & 16u) lk.last_lead = now;
            } else if (d1 == 108u && link_id) midi_send(0xBFu, 109u, d2);
        } else if (st == 0xFAu) {                /* a MIDI clock starts: bar one, beat one */
            lk.clk_pulses = 0; lk.clk_pos = 0; lk.clk_ms0 = now;
            if (!tr_follow || lk.state == LK_CLOCK) { tr_tick = 0; tr_frac = 0; tr_slew = 0; }
        } else if (st == 0xF8u && now - lk.last_rx > 4000u) {
            if (lk.clk_pulses == 0) lk.clk_ms0 = now;
            lk.clk_pulses++; lk.clk_pos += 4 << 8;
            if (lk.clk_pulses % 24 == 0) {
                uint32_t ms = now - lk.clk_ms0;
                lk.clk_ms0 = now;
                if (ms > 180u && ms < 1600u) {
                    uint32_t bpm = 60000u * 256u / ms;                 /* Q8 */
                    tr_bpm_q8 = (tr_bpm_q8 * 3u + bpm) / 4u;
                    if (lk.last_clk == 0u || now - lk.last_clk > 3000u) lk.clk_pos = (int32_t)tr_now_q8();   /* joining: take the clock as it is */
                    else link_apply(lk.clk_pos - (int32_t)tr_now_q8());
                    tr_follow = 1;
                }
                lk.last_clk = now;
            }
        }
    }
    if (tr_follow && now - lk.last_rx > 4000u && now - lk.last_clk > 3000u) { tr_follow = 0; lk.nbest = 0; }
    lk.state = (now - lk.last_rx <= 4000u && lk.last_rx) ? LK_FOLLOW : (lk.last_clk && now - lk.last_clk <= 3000u) ? LK_CLOCK
             : (lk.last_lead && now - lk.last_lead <= 4000u) ? LK_LEAD : LK_ALONE;
    if (link_id && now - lk.last_tx >= 500u) {
        uint32_t t = tr_bpm_q8 >> 3, p;
        lk.last_tx = now;
        midi_send(0xBFu, 102u, (t >> 7) & 127u); midi_send(0xBFu, 103u, t & 127u);
        p = (tr_now_q8() >> 6) & 0x1FFFFFu;
        midi_send(0xBFu, 104u, (p >> 14) & 127u); midi_send(0xBFu, 105u, (p >> 7) & 127u); midi_send(0xBFu, 106u, p & 127u);
        midi_send(0xBFu, 107u, link_id);
    }
    if ((duo_leads() || rn_leads()) && now - lk.last_duo >= 250u) {   /* (and the music room's seat 0) */   /* UNDERSONG: seat 0's clock, for seat 1 to follow */
        uint32_t t = tr_bpm_q8 >> 3, p;
        lk.last_duo = now;
        midi_send(0xBFu, 102u, (t >> 7) & 127u); midi_send(0xBFu, 103u, t & 127u);
        p = (tr_now_q8() >> 6) & 0x1FFFFFu;
        midi_send(0xBFu, 104u, (p >> 14) & 127u); midi_send(0xBFu, 105u, (p >> 7) & 127u); midi_send(0xBFu, 106u, p & 127u);
        midi_send(0xBFu, 107u, 8u | 5u);
    }
}
