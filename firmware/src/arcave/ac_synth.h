/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE's sound: a small bank of voices (sine, triangle, square, saw, noise) with an echo, the cave's hum, the
 * rock's rumble, and the clock every tune runs on. It all runs in the audio interrupt; the game asks for sounds
 * through a queue (one writer: the main loop), so nothing is shared half-written.
 *
 * The clock counts ticks, 96 to a quarter note, from power-on and never stops: bars are 384 ticks. Tunes read their
 * place from it, so two FM-1s whose clocks agree (ac_sync.h) play in step whatever tune or scene each is in. */

#define SY_NV 22
enum { W_SINE, W_TRI, W_SQUARE, W_SAW, W_NOISE, W_HAT };
typedef struct {
    uint32_t ph, inc;
    int32_t amp, vol, att;                       /* Q23: the level now, its peak, the attack's step a block */
    uint32_t dec;                                /* the decay: amp * dec >> 16 a block */
    uint32_t sweep;                              /* pitch: inc * sweep >> 16 a block (65536 = steady) */
    int32_t gl, gr, send;                        /* Q8 */
    int32_t rs, dr;                              /* Q8: how much goes to the reverb; the drive into a clip (0: none) */
    /* the music room's (ac_room.h): a resonant filter whose cutoff starts fenv above fcut and falls to it, and a glide */
    int32_t fcut, fenv, fq, lp, bp;              /* Q8 semitones (a note number), Q8 semitones, the damping Q15, its state */
    uint32_t fdec, inc_to;                       /* the cutoff's fall a block (as dec); the pitch a glide ends on */
    uint16_t swn;                                /* blocks of sweep left before the pitch settles on inc_to (0: no end) */
    uint8_t on, wave, stage, fmode;              /* fmode 0: no filter; 1: low-pass; 2: band-pass */
} sy_voice_t;
static sy_voice_t sy_v[SY_NV];

typedef struct {
    uint32_t at;                                 /* in samples of sy_t */
    uint16_t vol, att_ms, dur_ms, sweep;
    int8_t note, det, pan;
    uint8_t wave, send;
    uint8_t rsend, drv;                          /* the reverb's share 0..255; distortion 0..8 */
    uint8_t fmode, cut, res, fenv, fms8;         /* the filter: 0 none; its cutoff as a note; resonance 0..255; how many
                                                    semitones above the cutoff it opens to; that fall's time, 8 ms units */
    int8_t gl; uint8_t glms8;                    /* a glide: it starts gl semitones away and takes glms8 * 8 ms to arrive */
} sy_ev_t;
/* the filter and glide of the next note asked for with sfx_fx (the music room sets it, sfx_fx takes it and clears it) */
static struct { uint8_t fmode, cut, res, fenv, fms8, glms8; int8_t gl; } sy_x;
static void sfx_filter(int mode, int32_t cut, int32_t res, int32_t fenv, int32_t ms)
{
    sy_x.fmode = (uint8_t)mode; sy_x.cut = (uint8_t)(cut < 1 ? 1 : cut > 127 ? 127 : cut); sy_x.res = (uint8_t)(res < 0 ? 0 : res > 255 ? 255 : res);
    sy_x.fenv = (uint8_t)(fenv < 0 ? 0 : fenv > 96 ? 96 : fenv); sy_x.fms8 = (uint8_t)(ms < 16 ? 2 : ms > 2000 ? 250 : ms / 8);
}
static void sfx_glide(int32_t semis, int32_t ms) { sy_x.gl = (int8_t)(semis < -36 ? -36 : semis > 36 ? 36 : semis); sy_x.glms8 = (uint8_t)(ms < 8 ? 1 : ms > 2000 ? 250 : ms / 8); }
#define SY_QN 64u
static sy_ev_t sy_q[SY_QN];
static volatile uint32_t sy_qw;                  /* main loop writes */
static uint32_t sy_qr;                           /* the interrupt reads */
#define SY_PN 64u
static sy_ev_t sy_pend[SY_PN];
static uint32_t sy_npend;
static volatile uint32_t sy_t;                   /* samples since power-on */

#define SY_DLN 16384u                            /* the echo: 0.74 s at half the sample rate */
static int16_t sy_dl[SY_DLN] AC_POOL;
static uint32_t sy_dlw;
static int32_t sy_dllp;
static volatile int32_t sy_echo_fb = 115;        /* Q8 */
static volatile uint32_t sy_echo_len = SY_DLN;   /* the echo's time, in half-rate samples (the music room sets it to the tempo) */
/* the reverb (the player's song may ask for it): three damped combs and an allpass, at half the sample rate */
#define RV_L0 1117u
#define RV_L1 1277u
#define RV_L2 1423u
#define RV_LA 347u
static int16_t sy_rv[RV_L0 + RV_L1 + RV_L2 + RV_LA] AC_POOL;
static uint32_t sy_rvi[4];
static int32_t sy_rvlp[3], sy_rv_live;           /* (blocks it still has something to say: it costs nothing when silent) */
static volatile uint8_t sy_ear;                  /* child ear protection: everything 12 dB down and capped well under full scale */

static volatile int32_t sy_drone_want, sy_rumble_want, sy_rumble_pitch = 256, sy_music_want = 256;   /* 0..256 */
static int32_t sy_drone, sy_rumble, sy_music;

/* ---- the clock */
#define TICKS_BEAT 96u
#define TICKS_BAR 384u
#define TICKS_STEP 24u                           /* a sixteenth */
static volatile uint32_t tr_bpm_q8 = 120u << 8;  /* the tempo now (the sync nudges it) */
static volatile uint32_t tr_tick;                /* ticks since the clock's zero */
static volatile uint32_t tr_frac;                /* the part of a tick gone by, 2^32 a tick */
static volatile int32_t tr_slew;                 /* ticks (Q16) still to make up or lose, a little each block */
static volatile uint32_t tr_tick_at;             /* sy_t when tr_tick last changed (for a reading between ticks) */

static uint32_t sy_inc(int32_t note, int32_t det)
{
    uint32_t inc;
    note = iclamp(note, 0, 131);
    inc = AC_INC[note % 12] >> (10 - note / 12);
    if (det) inc = (uint32_t)((int32_t)inc + (int32_t)(((int64_t)inc * det * 38789) >> 26));
    return inc;
}

static void sy_start(const sy_ev_t *e)           /* in the interrupt */
{
    uint32_t i, pick = 0;
    int32_t low = 0x7FFFFFFF, blocks;
    sy_voice_t *v;
    for (i = 0; i < SY_NV; i++) {
        if (!sy_v[i].on) { pick = i; break; }
        if (sy_v[i].amp < low) { low = sy_v[i].amp; pick = i; }
    }
    v = &sy_v[pick];
    v->inc = sy_inc(e->note, e->det);
    v->ph = e->wave == W_SINE ? 0u : 0x40000000u;
    v->vol = (int32_t)e->vol << 8;
    blocks = ((int32_t)e->att_ms * 441) / 320;   /* 32-sample blocks in the attack */
    v->att = blocks > 0 ? imax(v->vol / blocks, 1) : v->vol;
    v->amp = blocks > 0 ? 0 : v->vol;
    v->stage = blocks > 0 ? 0u : 1u;
    v->dec = 65536u - 328499u / (uint32_t)imax((int32_t)e->dur_ms - (int32_t)e->att_ms, 20);
    v->sweep = e->sweep ? e->sweep : 65536u;
    v->gl = e->pan > 0 ? 256 - e->pan * 3 : 256;
    v->gr = e->pan < 0 ? 256 + e->pan * 3 : 256;
    v->send = e->send;
    v->rs = e->rsend;
    v->dr = e->drv ? 256 + e->drv * 200 : 0;
    v->wave = e->wave;
    v->fmode = e->fmode; v->lp = v->bp = 0; v->swn = 0;
    if (e->fmode) {
        v->fcut = (int32_t)e->cut << 8; v->fenv = (int32_t)e->fenv << 8;
        v->fq = 46000 - (int32_t)e->res * 170;   /* damping: 1.4 (none) down to 0.08 (it rings) */
        v->fdec = 65536u - 328499u / (uint32_t)imax((int32_t)e->fms8 * 8, 20);
    }
    if (e->gl && e->glms8) {                     /* a glide: from gl semitones away, a block at a time, then exactly there */
        int32_t nb = imax(1, (int32_t)e->glms8 * 8 * 441 / 320);
        v->inc_to = v->inc; v->inc = sy_inc(e->note + e->gl, e->det);
        v->sweep = (uint32_t)(65536 - e->gl * 3785 / nb); v->swn = (uint16_t)nb;
    }
    v->on = 1;
}

/* from the main loop: a note, `delay_ms` from now. vol 0..32767; send 0..255 (how much goes to the echo) */
static void sfx_fx(int32_t note, uint32_t delay_ms, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms,
                   uint32_t send, int32_t det, int32_t pan, uint32_t sweep, uint32_t rsend, uint32_t drv)
{
    uint32_t w = sy_qw;
    sy_ev_t *e;
    if (w - sy_qr >= SY_QN) return;              /* full: the sound is dropped, nothing breaks */
    e = &sy_q[w % SY_QN];
    e->at = sy_t + delay_ms * 441u / 10u;
    e->note = (int8_t)iclamp(note, 0, 127); e->det = (int8_t)det; e->pan = (int8_t)pan;
    e->vol = (uint16_t)vol; e->att_ms = (uint16_t)att_ms; e->dur_ms = (uint16_t)imin((int32_t)dur_ms, 60000);
    e->wave = (uint8_t)wave; e->send = (uint8_t)send; e->sweep = (uint16_t)(sweep ? sweep : 0u);
    e->rsend = (uint8_t)rsend; e->drv = (uint8_t)drv;
    e->fmode = sy_x.fmode; e->cut = sy_x.cut; e->res = sy_x.res; e->fenv = sy_x.fenv; e->fms8 = sy_x.fms8; e->gl = sy_x.gl; e->glms8 = sy_x.glms8;
    sy_x.fmode = 0; sy_x.gl = 0;
    sy_qw = w + 1u;
}
static void sfx_ex(int32_t note, uint32_t delay_ms, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms,
                   uint32_t send, int32_t det, int32_t pan, uint32_t sweep)
{
    sfx_fx(note, delay_ms, dur_ms, wave, vol, att_ms, send, det, pan, sweep, 0, 0);
}
static void sfx(int32_t note, uint32_t delay_ms, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms, uint32_t send)
{
    sfx_ex(note, delay_ms, dur_ms, wave, vol, att_ms, send, 0, 0, 0);
}
/* in the interrupt: a note now */
static void sy_now_fx(int32_t note, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms, uint32_t send, int32_t det, int32_t pan,
                      uint32_t sweep, uint32_t rsend, uint32_t drv)
{
    sy_ev_t e;
    e.at = 0; e.note = (int8_t)iclamp(note, 0, 127); e.det = (int8_t)det; e.pan = (int8_t)pan;
    e.vol = (uint16_t)((vol * (uint32_t)sy_music) >> 8); e.att_ms = (uint16_t)att_ms; e.dur_ms = (uint16_t)dur_ms;
    e.wave = (uint8_t)wave; e.send = (uint8_t)send; e.sweep = (uint16_t)sweep; e.rsend = (uint8_t)rsend; e.drv = (uint8_t)drv;
    e.fmode = 0; e.gl = 0; e.glms8 = 0;
    if (e.vol) sy_start(&e);
}
static void sy_now(int32_t note, uint32_t dur_ms, uint32_t wave, uint32_t vol, uint32_t att_ms, uint32_t send, int32_t det, int32_t pan, uint32_t sweep)
{
    sy_now_fx(note, dur_ms, wave, vol, att_ms, send, det, pan, sweep, 0, 0);
}

static inline int32_t sy_sine(uint32_t ph)
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y;
}

static void mus_tick(uint32_t tick);             /* ac_music.h: the tunes */

static uint32_t sy_noise = 0xACE1ACE1u;
static uint32_t sy_dph[3], sy_lfo;
static int32_t sy_rlp, sy_rlp2, sy_hat_z;

static void arcave_audio(int32_t *out, uint32_t n)
{
    uint32_t b, i, v;
    while (sy_qr != sy_qw && sy_npend < SY_PN) sy_pend[sy_npend++] = sy_q[sy_qr++ % SY_QN];
    for (b = 0; b < n; b += 32u) {
        int32_t mix[64], snd[32], rvb[32];
        uint32_t t = sy_t, k, f0, f1;
        for (i = 0; i < 64u; i++) mix[i] = 0;
        for (i = 0; i < 32u; i++) snd[i] = rvb[i] = 0;
        /* the clock: at most one tick a block (tempos to 300) */
        {
            int32_t sl = tr_slew, step = 0;
            int64_t inc = (int64_t)tr_bpm_q8 * 19479;
            if (sl) {                            /* a twenty-fourth of what is owed each block, half a tick at most */
                step = sl / 24;
                if (!step) step = sl;
                step = iclamp(step, -32768, 32768);
                tr_slew = sl - step;
                inc += (int64_t)step << 16;
            }
            if (inc < 0) inc = 0;
            if (inc > 0xF0000000ll) inc = 0xF0000000ll;
            f0 = tr_frac;
            f1 = f0 + (uint32_t)inc;
            tr_frac = f1;
            if (f1 < f0) { tr_tick++; tr_tick_at = t; mus_tick(tr_tick); }
        }
        /* notes that are due */
        for (k = 0; k < sy_npend; ) {
            if ((int32_t)(sy_pend[k].at - t) <= 0) { sy_start(&sy_pend[k]); sy_pend[k] = sy_pend[--sy_npend]; }
            else k++;
        }
        sy_music += (sy_music_want - sy_music) > 0 ? imin(4, sy_music_want - sy_music) : imax(-4, sy_music_want - sy_music);
        for (v = 0; v < SY_NV; v++) {
            sy_voice_t *o = &sy_v[v];
            uint32_t ph, inc;
            int32_t a, gl, gr, sd, rs, dr, ff = 0, fq = 0, flp = 0, fbp = 0;
            if (!o->on) continue;
            if (o->stage == 0u) { o->amp += o->att; if (o->amp >= o->vol) { o->amp = o->vol; o->stage = 1; } }
            else { o->amp = (int32_t)(((int64_t)o->amp * o->dec) >> 16); if (o->amp < 2048) { o->on = 0; continue; } }
            if (o->sweep != 65536u) {
                o->inc = (uint32_t)(((uint64_t)o->inc * o->sweep) >> 16);
                if (o->swn && --o->swn == 0u) { o->inc = o->inc_to; o->sweep = 65536u; }
            }
            if (o->fmode) {                      /* the filter's cutoff, once a block */
                int32_t c = o->fcut + o->fenv;
                o->fenv = (int32_t)(((int64_t)o->fenv * o->fdec) >> 16);
                ff = (int32_t)(((uint64_t)sy_inc(c >> 8, ((c & 255) * 100) >> 8) * 205887u) >> 32);
                if (ff > 24000) ff = 24000;
                fq = o->fq; flp = o->lp; fbp = o->bp;
            }
            ph = o->ph; inc = o->inc; a = o->amp >> 8; gl = o->gl; gr = o->gr; sd = o->send; rs = o->rs; dr = o->dr;
            if (rs) sy_rv_live = 4200;           /* (three seconds after its last note) */
            for (i = 0; i < 32u; i++) {
                int32_t s;
                ph += inc;
                switch (o->wave) {
                case W_SINE: s = sy_sine(ph); break;
                case W_TRI: s = (int32_t)(ph >> 15) & 0xFFFF; s = (ph & 0x80000000u) ? 49151 - s : s - 16384; if (s > 32767) s = 32767; break;
                case W_SQUARE: s = (ph & 0x80000000u) ? -14000 : 14000; break;
                case W_SAW: s = ((int32_t)ph >> 17); break;
                case W_NOISE:
                    if (ph < inc) { sy_noise ^= sy_noise << 13; sy_noise ^= sy_noise >> 17; sy_noise ^= sy_noise << 5; }
                    s = (int32_t)(sy_noise & 0xFFFFu) - 32768; break;
                default:
                    sy_noise ^= sy_noise << 13; sy_noise ^= sy_noise >> 17; sy_noise ^= sy_noise << 5;
                    s = (int32_t)(sy_noise & 0xFFFFu) - 32768; { int32_t z = s; s = (s - sy_hat_z) >> 1; sy_hat_z = z; } break;
                }
                if (o->fmode) {                  /* a state-variable filter: low-pass or band-pass */
                    flp += (ff * fbp) >> 15;
                    if (flp > 60000) flp = 60000; else if (flp < -60000) flp = -60000;
                    fbp += (int32_t)(((int64_t)ff * (s - flp - (int32_t)(((int64_t)fq * fbp) >> 15))) >> 15);
                    if (fbp > 60000) fbp = 60000; else if (fbp < -60000) fbp = -60000;
                    s = o->fmode == 1u ? flp : fbp;
                    if (s > 60000) s = 60000; else if (s < -60000) s = -60000;
                }
                if (dr) { s = (s * dr) >> 8; if (s > 30000) s = 30000; else if (s < -30000) s = -30000; }   /* driven into a clip */
                s = (s * a) >> 15;
                mix[2u * i] += (s * gl) >> 8;
                mix[2u * i + 1u] += (s * gr) >> 8;
                snd[i] += (s * sd) >> 8;
                rvb[i] += (s * rs) >> 8;
            }
            o->ph = ph;
            if (o->fmode) { o->lp = flp; o->bp = fbp; }
        }
        /* the cave's hum: two low sines a hair apart and a fifth that comes and goes */
        sy_drone += iclamp(sy_drone_want - sy_drone, -1, 1);
        sy_lfo += 2600u * 32u;                   /* about 0.06 Hz */
        if (sy_drone > 0) {
            int32_t l1 = 128 + (sy_sine(sy_lfo) >> 8), l2 = 128 + (sy_sine(sy_lfo * 3u + 0x30000000u) >> 8);
            int32_t wob = sy_sine(sy_lfo * 5u) >> 5;   /* the pitch wavers a little */
            for (i = 0; i < 32u; i++) {
                int32_t s;
                sy_dph[0] += 5356000u + (uint32_t)(wob * 40);   /* 55 Hz */
                sy_dph[1] += 5395000u - (uint32_t)(wob * 30);   /* 55.4 Hz */
                sy_dph[2] += 8040000u + (uint32_t)(wob * 90);   /* a fifth above, flat of true */
                s = (sy_sine(sy_dph[0]) * 9 + sy_sine(sy_dph[1]) * 9 * l1 / 256 + sy_sine(sy_dph[2]) * 5 * l2 / 256) >> 7;
                s = (s * sy_drone) >> 8;
                mix[2u * i] += s + ((s * l1) >> 10);
                mix[2u * i + 1u] += s + ((s * l2) >> 10);
                if (!(i & 7u)) snd[i] += s >> 2;
            }
        }
        /* the rock rolling: low noise, through two low-pass stages so no hiss is left on top (one stage sounded
           like static), and softer than it was */
        sy_rumble += iclamp(sy_rumble_want - sy_rumble, -6, 6);
        if (sy_rumble > 0) {
            int32_t kq = iclamp(sy_rumble_pitch, 40, 420);
            for (i = 0; i < 32u; i++) {
                int32_t s;
                sy_noise ^= sy_noise << 13; sy_noise ^= sy_noise >> 17; sy_noise ^= sy_noise << 5;
                s = (int32_t)(sy_noise & 0xFFFFu) - 32768;
                sy_rlp += ((s - sy_rlp) * kq) >> 14;
                sy_rlp2 += ((sy_rlp - sy_rlp2) * kq) >> 14;
                s = (sy_rlp2 * sy_rumble * 3) >> 8;
                mix[2u * i] += s; mix[2u * i + 1u] += s;
            }
        }
        /* the echo, at half rate, and out */
        for (i = 0; i < 32u; i += 2u) {
            int32_t in_ = (snd[i] + snd[i + 1u]) >> 1, o = sy_dl[sy_dlw];
            sy_dllp += ((o - sy_dllp) * 13000) >> 15;
            sy_dl[sy_dlw] = (int16_t)iclamp(in_ + ((sy_dllp * sy_echo_fb) >> 8), -32000, 32000);
            if (++sy_dlw >= sy_echo_len) sy_dlw = 0;
            mix[2u * i] += sy_dllp; mix[2u * i + 1u] += sy_dllp;
            mix[2u * i + 2u] += sy_dllp; mix[2u * i + 3u] += sy_dllp;
        }
        if (sy_rv_live > 0) {                    /* the reverb */
            static const uint16_t LEN[4] = {RV_L0, RV_L1, RV_L2, RV_LA}, OFF[4] = {0, RV_L0, RV_L0 + RV_L1, RV_L0 + RV_L1 + RV_L2};
            sy_rv_live--;
            for (i = 0; i < 32u; i += 2u) {
                int32_t x = (rvb[i] + rvb[i + 1u]) >> 1, c[3], s, o, w, wl, wr;
                for (k = 0; k < 3u; k++) {
                    int16_t *p = &sy_rv[OFF[k] + sy_rvi[k]];
                    c[k] = *p;
                    sy_rvlp[k] += ((c[k] - sy_rvlp[k]) * 17000) >> 15;   /* the highs die first */
                    *p = (int16_t)iclamp(x + ((sy_rvlp[k] * 214) >> 8), -32000, 32000);
                    if (++sy_rvi[k] >= LEN[k]) sy_rvi[k] = 0;
                }
                s = (c[0] + c[1] + c[2]) >> 1;
                o = sy_rv[OFF[3] + sy_rvi[3]];
                w = iclamp(s + (o >> 1), -32000, 32000);
                sy_rv[OFF[3] + sy_rvi[3]] = (int16_t)w;
                if (++sy_rvi[3] >= LEN[3]) sy_rvi[3] = 0;
                o -= w >> 1;
                wl = o + ((c[0] - c[1]) >> 2); wr = o + ((c[1] - c[2]) >> 2);   /* a little apart, left and right */
                mix[2u * i] += wl; mix[2u * i + 1u] += wr;
                mix[2u * i + 2u] += wl; mix[2u * i + 3u] += wr;
            }
        }
        for (i = 0; i < 64u; i++) {
            int32_t s = mix[i];
            if (s > 20000) s = 20000 + ((s - 20000) >> 2);   /* a soft knee before the platform's clip */
            else if (s < -20000) s = -20000 + ((s + 20000) >> 2);
            if (sy_ear) { s >>= 2; s = s > 5000 ? 5000 : s < -5000 ? -5000 : s; }
            out[2u * b + i] += s;
        }
        sy_t = t + 32u;
    }
}

/* where the clock is, in ticks Q8, read from the main loop */
static uint32_t tr_now_q8(void)
{
    uint32_t t = tr_tick, f = tr_frac;
    if (t != tr_tick) { t = tr_tick; f = tr_frac; }
    return (t << 8) | (f >> 24);
}
/* the clock moved by d ticks (Q8), smoothly: the sync's phase correction. A big jump is made at once. */
static void tr_nudge_q8(int32_t d)
{
    if (d > (int32_t)(TICKS_STEP << 8) * 2 || d < -(int32_t)(TICKS_STEP << 8) * 2) {
        uint32_t now = tr_now_q8() + (uint32_t)d;
        tr_tick = now >> 8; tr_frac = now << 24; tr_slew = 0;
    } else tr_slew = d * 256;
}
