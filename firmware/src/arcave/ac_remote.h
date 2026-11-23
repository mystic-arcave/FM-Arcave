/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE: this FM-1 as the controller of the game in a browser (the PLAY page's PLAY WITH MY FM-1: web/emu/remote.js).
 * The browser runs the game and shows it. This FM-1 sends it the save it holds, then every button, key and knob, and
 * lights its keys and buttons as the browser's game lights them; whatever the browser saves comes back here and is
 * kept in this FM-1's flash. While it lasts the FM-1 shows a screen of its own and makes no sound.
 *
 * Like ARCAVE LINK (ac_sync.h), nothing but control changes on MIDI channel 16: no SysEx, so the browser build itself
 * can stand in for an FM-1 in the tests (tests/remote_test.mjs).
 *   to the FM-1   CC 78 n     hello, every half second (n 1-127: the page's own number); CC 78 0: goodbye
 *                 CC 80..92   the lights, 7 bits each: lit buttons (bit 14 of them PLAY lit, 15 PLAY breathing: 3),
 *                             lit keys (4), glowing keys (4), breathing buttons (2)
 *   from it       CC 70 1     here: the answer to every hello (1: this protocol's version)
 *                 CC 71 v     a button: its number (BTN_...), + 64 pressed
 *                 CC 72 v     a key (0 the lowest), + 64 pressed
 *                 CC 73 v     a knob: its number (ENC_...) * 16 + 8 + the detents turned (-8..7, never 0)
 *                 CC 79 r     what became of a save from the page: 1 kept, 2 not this FM-1's size (it needs the
 *                             page's version), 3 damaged on the way
 *   both ways     CC 74, 75   a save follows: its length in bytes, high 7 bits, then low
 *                 CC 76 v     its bytes, seven at a time: first their top bits (bit i: the group's byte i), then each
 *                             byte's low seven
 *                 CC 77 h, l  its end: the sum of its bytes (14 bits), high, then low
 * A hello with a new number (another page, or the page reloaded) has the save sent again. No hello for 3 s, or a
 * goodbye: the FM-1 is itself again, as after a power-on, with the save it now holds.
 * The save goes out four messages a millisecond at most: the USB ring holds 64 and is emptied at 2 kHz. The page
 * sends its own at two a millisecond, and while one comes in this FM-1 draws nothing, so the ring of 64 coming in is
 * read often enough (a picture can take a while). */

#define FMR_PROTO 1u
#define FMR_BUF 3072u                             /* the largest save taken (the game's is 2616 bytes with the music room) */
#define FMR_LOST_MS 3000u

static void fmr_game_enter(void);                 /* arcave.c: the game set aside, its save written */
static void fmr_game_leave(void);                 /* arcave.c: the game as after a power-on */
static int fmr_game_keep(const uint8_t *p, uint32_t n);   /* arcave.c: a save from the page: 1 kept, 2 size, 3 damaged */

static struct {
    uint8_t on, nonce, msg;                      /* msg: 0 none, 1 saved (for a while), 2 this FM-1 needs an update */
    uint32_t t0, last_hello, msg_ms;
    uint32_t prev_btn, prev_keys;
    uint8_t lg[13];                              /* the lights as the page sent them, 7 bits a group */
    const uint8_t *tx;                           /* the save going out: tx_st 1 its length, 2 its bytes, 0 done */
    uint16_t tx_len, tx_n, tx_sum;
    uint8_t tx_st, tx_k;
    uint32_t tx_ms;
    uint16_t rx_len, rx_n, rx_sum, rx_ck;        /* a save coming in */
    uint8_t rx_on, rx_k, rx_hdr, rx_hi, rx_ckn;
} fmr;
static uint8_t fmr_rxb[FMR_BUF];

/* (arcave.c, from fmr_game_enter) the controller from now on; the save at p goes out at once */
static void fmr_start(const void *p, uint32_t n)
{
    uint32_t i;
    fmr.on = 1; fmr.msg = 0; fmr.t0 = fm1_ms; fmr.rx_on = 0;
    fmr.prev_btn = fm1_in.buttons & 0x3FFFu; fmr.prev_keys = fm1_in.notes & ((1u << AC_NKEY) - 1u);
    for (i = 0; i < 13u; i++) fmr.lg[i] = 0;
    for (i = 0; i < 7u; i++) (void)fm1_enc_take(i);   /* (a turn from before is not sent) */
    fmr.tx = (const uint8_t *)p; fmr.tx_len = (uint16_t)n; fmr.tx_st = 1;
    fb_all = 1;
}

/* a control change on channel 16 that is the remote's (from link_poll): CC 74..92 */
static int fmr_rx(uint32_t d1, uint32_t d2)
{
    if (d1 == 78u) {
        if (!d2) { if (fmr.on) fmr_game_leave(); return 1; }
        midi_send(0xBFu, 70u, FMR_PROTO);
        fmr.last_hello = fm1_ms;
        if (!fmr.on) { fmr.nonce = (uint8_t)d2; fmr_game_enter(); }
        else if (d2 != fmr.nonce) { fmr.nonce = (uint8_t)d2; fmr.tx_st = 1; }   /* (a new page: the save again) */
        return 1;
    }
    if (d1 < 74u || d1 > 92u || d1 == 78u || d1 == 79u) return 0;
    if (!fmr.on) return 1;                        /* (not asked for: the page's messages to an FM-1 are only heard by one) */
    if (d1 >= 80u) { fmr.lg[d1 - 80u] = (uint8_t)d2; return 1; }
    if (d1 == 74u) { fmr.rx_hi = (uint8_t)d2; fmr.rx_on = 0; }
    else if (d1 == 75u) {
        fmr.rx_len = (uint16_t)((uint32_t)fmr.rx_hi << 7 | d2);
        fmr.rx_n = 0; fmr.rx_k = 0; fmr.rx_sum = 0; fmr.rx_ckn = 0; fmr.rx_on = 1;
    } else if (d1 == 76u && fmr.rx_on) {
        if (fmr.rx_k == 0u) fmr.rx_hdr = (uint8_t)d2;
        else {
            uint8_t b = (uint8_t)(d2 | (uint32_t)((fmr.rx_hdr >> (fmr.rx_k - 1u)) & 1u) << 7);
            if (fmr.rx_n < FMR_BUF) fmr_rxb[fmr.rx_n] = b;
            fmr.rx_n++; fmr.rx_sum = (uint16_t)(fmr.rx_sum + b);
        }
        if (++fmr.rx_k == 8u || (fmr.rx_k > 1u && fmr.rx_n >= fmr.rx_len)) fmr.rx_k = 0;
    } else if (d1 == 77u && fmr.rx_on) {
        if (!fmr.rx_ckn) { fmr.rx_ck = (uint16_t)(d2 << 7); fmr.rx_ckn = 1; }
        else {
            int r;
            fmr.rx_ck |= (uint16_t)d2; fmr.rx_on = 0;
            if (fmr.rx_n != fmr.rx_len || (fmr.rx_sum & 0x3FFFu) != fmr.rx_ck) r = 3;
            else if (fmr.rx_len > FMR_BUF) r = 2;
            else r = fmr_game_keep(fmr_rxb, fmr.rx_len);
            midi_send(0xBFu, 79u, (uint32_t)r);
            if (r == 1) { if (fmr.msg != 2u) fmr.msg = 1; fmr.msg_ms = fm1_ms; }
            else if (r == 2) fmr.msg = 2;
#ifdef DINGHY_HOST
            printf("%6u ms  remote: a save of %u bytes from the page: %s\n", fm1_ms, fmr.rx_len, r == 1 ? "kept" : r == 2 ? "not this size" : "damaged");
#endif
        }
    }
    return 1;
}

/* one press or release: the order is right if it was pressed and let go between two looks (or the other way) */
static void fmr_edges(uint32_t cc, uint32_t n, uint32_t cur, uint32_t prev, uint32_t down, uint32_t up)
{
    uint32_t i, press = (cur & ~prev) | down, lift = (prev & ~cur) | (down & ~cur) | up;
    for (i = 0; i < n; i++) {
        uint32_t p = press >> i & 1u, l = lift >> i & 1u;
        if (p && l && (cur >> i & 1u)) { midi_send(0xBFu, cc, i); midi_send(0xBFu, cc, i | 64u); }
        else { if (p) midi_send(0xBFu, cc, i | 64u); if (l) midi_send(0xBFu, cc, i); }
    }
}
/* every pass of the main loop while it lasts: the panel out at once, and the save a little at a time */
static void fmr_poll(void)
{
    uint32_t rel = 0, down, cur, e;
    int32_t budget = 4;
    if (!fmr.on) return;
    down = fm1_input_edges(&rel);
    cur = fm1_in.buttons & 0x3FFFu;
    fmr_edges(71u, 14u, cur, fmr.prev_btn, down & 0x3FFFu, rel & 0x3FFFu);
    fmr.prev_btn = cur;
    down = fm1_input_note_edges() & ((1u << AC_NKEY) - 1u);
    cur = fm1_in.notes & ((1u << AC_NKEY) - 1u);
    fmr_edges(72u, AC_NKEY, cur, fmr.prev_keys, down, 0);
    fmr.prev_keys = cur;
    for (e = 0; e < 7u; e++) {
        int32_t d = fm1_enc_take(e);
        while (d) { int32_t s = iclamp(d, -8, 7); midi_send(0xBFu, 73u, e * 16u + (uint32_t)(8 + s)); d -= s; }
    }
    if (!fmr.tx_st || fm1_ms == fmr.tx_ms) return;
    fmr.tx_ms = fm1_ms;
    while (budget > 0 && fmr.tx_st) {
        if (fmr.tx_st == 1u) {
            midi_send(0xBFu, 74u, (uint32_t)fmr.tx_len >> 7 & 127u); midi_send(0xBFu, 75u, fmr.tx_len & 127u);
            fmr.tx_st = 2; fmr.tx_n = 0; fmr.tx_k = 0; fmr.tx_sum = 0; budget -= 2;
        } else if (fmr.tx_n >= fmr.tx_len) {
            midi_send(0xBFu, 77u, (uint32_t)fmr.tx_sum >> 7 & 127u); midi_send(0xBFu, 77u, fmr.tx_sum & 127u);
            fmr.tx_st = 0; budget -= 2;
        } else if (fmr.tx_k == 0u) {              /* the next seven bytes' top bits */
            uint32_t h = 0, i;
            for (i = 0; i < 7u && fmr.tx_n + i < fmr.tx_len; i++) h |= (uint32_t)(fmr.tx[fmr.tx_n + i] >> 7) << i;
            midi_send(0xBFu, 76u, h); fmr.tx_k = 1; budget--;
        } else {
            uint8_t b = fmr.tx[fmr.tx_n];
            midi_send(0xBFu, 76u, b & 127u); fmr.tx_sum = (uint16_t)(fmr.tx_sum + b); fmr.tx_n++; budget--;
            if (++fmr.tx_k == 8u || fmr.tx_n >= fmr.tx_len) fmr.tx_k = 0;
        }
    }
}

/* the widest word of s at scale a (a word is never broken across lines: the text is made smaller instead) */
static int32_t fmr_word_w(const char *s, int32_t a)
{
    char w[40];
    uint32_t n = 0;
    int32_t best = 0;
    for (;; s++) {
        if (*s && *s != ' ' && n < sizeof w - 1u) { w[n++] = *s; continue; }
        w[n] = 0; best = imax(best, gtextw(w, a)); n = 0;
        if (!*s) return best;
    }
}
/* the FM-1's own screen meanwhile: the computer up above with the game's name on it, signals rising to it from Rock */
static void fmr_draw(void)
{
    uint32_t tm = fm1_ms - fmr.t0, i;
    int32_t ry = 152 + ((tsin(tm, 9, 0) * 3) >> 14), a = ts(3, 3), h;
    const char *title = TR(T_RM_TITLE);
    grect(0, 0, AC_W, AC_H, th->bg);
    gbox(40, 8, 160, 76, th->caps[0]); gbox(41, 9, 158, 74, th->caps[0]);   /* the computer's screen */
    grect(112, 84, 16, 8, th->caps[0]); grect(92, 92, 56, 3, th->caps[0]);   /* its stand */
    while (a > 1 && (gblock_h(146, title, a) > 62 || fmr_word_w(title, a) > 146)) a--;                    /* (the name, as large as fits, on its screen) */
    h = gblock_h(146, title, a);
    gblock(47, 46 - h / 2, 146, title, th->text, a, 1);
    for (i = 0; i < 3u; i++) {                   /* what the keys and knobs say, on its way up */
        int32_t p = (int32_t)((tm / 3u + i * 85u) % 256u), y = 122 - 24 * p / 256;
        galpha(256 - p); gdisc(120, y, 3, th->caps[1 + i]);
    }
    galpha(256);
    rock_draw(120, ry, 600, (int32_t)((tsin(tm, 7, 0) * 3) >> 4),
              (tm % 3900u) < 160u ? iabs((int32_t)(tm % 3900u) - 80) * 256 / 80 : 256,
              th->rock, th->rock_rim, 1, th->moss, th->eye, th->pupil);
    if (fmr.msg == 2u) gfoot(236, TR(T_RM_UPDATE), th->caps[0], th->bg, ts(1, 2));
    else if (fmr.msg == 1u && fm1_ms - fmr.msg_ms < 1800u) gfoot(236, TR(T_RM_SAVED), th->caps[2], th->bg, ts(1, 2));
    else gfoot(236, TR(T_RM_BODY), th->mute, th->bg, ts(1, 2));
}
/* in place of a frame of the game; the page gone quiet for 3 s: the game again */
static void fmr_frame(void)
{
    uint32_t lit = fmr.lg[0] | (uint32_t)fmr.lg[1] << 7 | (uint32_t)fmr.lg[2] << 14;
    if (fm1_ms - fmr.last_hello > FMR_LOST_MS) { fmr_game_leave(); return; }
    ac_leds_begin();
    led_btn_want = (lit & 0x3FFFu & ~(1u << BTN_PLAY)) | (lit >> 14 & 1u) << BTN_PLAY;
    led_play_breathe = (uint8_t)(lit >> 15 & 1u);
    led_key_want = (fmr.lg[3] | (uint32_t)fmr.lg[4] << 7 | (uint32_t)fmr.lg[5] << 14 | (uint32_t)fmr.lg[6] << 21) & ((1u << AC_NKEY) - 1u);
    led_key_dim_want = (fmr.lg[7] | (uint32_t)fmr.lg[8] << 7 | (uint32_t)fmr.lg[9] << 14 | (uint32_t)fmr.lg[10] << 21) & ((1u << AC_NKEY) - 1u);
    led_breathe_want = (fmr.lg[11] | (uint32_t)fmr.lg[12] << 7) & 0x3FFFu;
    if (fmr.rx_on) { ac_leds_end(); return; }   /* (a save coming in: the ring is read, not the picture drawn) */
    fmr_draw();
    ac_present();
    ac_leds_end();
}
