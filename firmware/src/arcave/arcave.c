/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE: the game. One file after another, as Dinghy builds: the platform is already above this (fm_arcave.c on
 * the FM-1 and in tests, web/emu/web_host.c in a browser).
 *
 *   the intro    title, "press PLAY to wake up Rock", the roll to the cave          ac_intro.h
 *   the cave     a platforming hub: the Hat, the dark jukebox, three machines       ac_cave.h
 *   the mending  three rings of glass, then the dish and your first song            ac_juke.h
 *   the machines CRYSTAL ROAD, JUICE WORKS, NIGHT BLOOM                         ac_mach.h
 *   the well     UNDERSONG: a story for two FM-1s, linked; and a game against each other    ac_duo.h
 *   the room     four instruments and a mixer, down a tunnel from the high right ledge   ac_room.h, ac_hall.h
 *   everywhere   GLO: settings (the game pauses); OCT- OCT+: the language; ALGORITHM: the jukebox;
 *                LINK: several FM-1s on one clock, over USB or Bluetooth            ac_menu.h, ac_sync.h
 *                HOME: how to play; FX: an effect; the scores on the board in the cave     ac_sheet.h
 *                EDIT: the upgrades, bought with the tickets the machines pay out; the player's profile (the
 *                settings' first line): a name for Rock           ac_prof.h, ac_deco.h, ac_shop.h
 *   the remote   this FM-1 as the controller of the game in a browser, its save shared   ac_remote.h */
#include "ac_base.h"
#include "ac_synth.h"
#include "ac_prof.h"
#include "ac_music.h"
#include "ac_text.h"
#include "ac_art.h"
#include "ac_deco.h"
#include "ac_intro.h"
#include "ac_cave.h"
#include "ac_juke.h"
#include "ac_mach.h"
#include "ac_duo.h"
#include "ac_room.h"
#include "ac_geode.h"
#include "ac_roots.h"
#include "ac_rack.h"
#include "ac_seam.h"
#include "ac_hall.h"
#include "ac_shop.h"
#include "ac_sheet.h"
#include "ac_remote.h"
#include "ac_sync.h"
#include "ac_menu.h"

enum { SC_INTRO, SC_CAVE, SC_LAMP, SC_DISH, SC_MACH, SC_DUO, SC_ROOM };
static uint8_t scene, replay, thanks_due;
static int dish_rec_screen(void) { return scene == SC_DISH && ds.stage != DS_STIR && !ds.intro; }   /* (OCT and EDIT are the dish's there) */
static uint32_t ac_last_ms, save_at;
static uint8_t ac_started, juke_track;
static int32_t thanks_t, algo_quiet, tip_t;
static uint8_t glo_used, was_paused, bt_seen;
static uint32_t pause_tick, play_saved_s;

typedef struct {                                 /* the save of 0.1 and 0.2: still read, so an update keeps the story */
    uint32_t magic;
    uint8_t lang, look, story, link, track, take_n, take_bpm, tsize1;   /* tsize1: the text size + 1 (0: never set) */
    uint8_t take[TAKE_MAX][2];
    uint32_t best[3];
} save1_t;
typedef struct {                                 /* the save of 0.3 to 0.14: still read */
    save1_t v1;
    uint32_t play_s;
    uint8_t ear, secrets, rsv[2];
    uint32_t race_cs;
    uint8_t fx1[4];
} save2_t;
typedef struct {                                 /* the save of 0.15 to 0.18: still read */
    save2_t v2;
    uint16_t gh_best[JUKE_N][3];
    uint8_t gh_gem[JUKE_N];
    uint8_t gh_flags, gh_mode;
} save3_t;
typedef struct {                                 /* (the same as save3_t as far as it goes, then UNDERSONG's) */
    save1_t v1;
    uint32_t play_s;                             /* the time played, seconds */
    uint8_t ear, secrets, rsv[2];
    uint32_t race_cs;                            /* the best race back, hundredths of a second (0: never run) */
    uint8_t fx1[4];                              /* the song's effects, each + 1 (0: a save from before them) */
    uint16_t gh_best[JUKE_N][3];                 /* CRYSTAL ROAD: the best of each song in each mode */
    uint8_t gh_gem[JUKE_N];                      /* its crystals, two bits a mode */
    uint8_t gh_flags, gh_mode;                   /* bit 0: ROCK and CRYSTAL open; the mode last chosen */
    uint16_t duo_best[DM_N];                     /* UNDERSONG: the best of each of its games */
    uint8_t duo_prog, duo_rsv;                   /* its chapters cleared, 0..4 */
    uint16_t duo_road[4];                        /* (0.20) TWIN ROAD's best on its four other songs */
    uint16_t trio_best[DM_N], trio_road[4];      /* (0.20) the same, played by three */
} save_t;
#define SAVE4_SIZE 192                           /* (the save of 0.19: up to duo_road) */
#define SAVE5_SIZE 200                           /* (a save of an early 0.20: up to trio_best) */
#define SAVE_MAGIC 0x41434156u                   /* "ACAV" */
static struct { save_t g; room_save_t room; prof_save_t prof; } svr;   /* the game's save, then the music room's, then the profile's (0.21) */
#define sv svr.g
_Static_assert(sizeof svr <= FMR_BUF, "the save no longer fits the remote's buffer (ac_remote.h)");

static void save_now(void)
{
    uint32_t i;
    save1_t *v = &sv.v1;
    room_pack();
    v->magic = SAVE_MAGIC; v->lang = lang; v->look = look_i; v->story = story; v->link = link_id; v->track = juke_track;
    v->take_n = take_n; v->take_bpm = take_bpm; v->tsize1 = (uint8_t)(tsize + 1u);
    for (i = 0; i < TAKE_MAX; i++) { v->take[i][0] = take_ev[i][0]; v->take[i][1] = take_ev[i][1]; }
    for (i = 0; i < 3u; i++) v->best[i] = best[i];
    sv.play_s = play_s; sv.ear = sy_ear; sv.secrets = cv.secrets; sv.race_cs = race_best_cs;
    for (i = 0; i < 4u; i++) sv.fx1[i] = (uint8_t)(take_fx[i] + 1u);
    for (i = 0; i < JUKE_N * 3u; i++) sv.gh_best[i / 3u][i % 3u] = gh_best[i / 3u][i % 3u];
    for (i = 0; i < JUKE_N; i++) sv.gh_gem[i] = gh_gem[i];
    sv.gh_flags = gh_flags; sv.gh_mode = gh_mode;
    for (i = 0; i < DM_N; i++) sv.duo_best[i] = duo_best[i];
    sv.duo_prog = duo_prog; sv.duo_rsv = 0;
    for (i = 0; i < 4u; i++) sv.duo_road[i] = duo_road[i];
    for (i = 0; i < DM_N; i++) sv.trio_best[i] = trio_best[i];
    for (i = 0; i < 4u; i++) sv.trio_road[i] = trio_road[i];
    play_saved_s = play_s;
    svr.prof = pf;
    settings_save(&svr, sizeof svr);
}
static void save_soon(void) { save_dirty = 1; save_at = fm1_ms + 1500u; }

static void to_cave(int fresh)
{
    if (scene == SC_ROOM) { room_leave(); save_soon(); }   /* (out of the music room: it is kept as it was left) */
    scene = SC_CAVE;
    pfx_clear();
    duo_leave();                                 /* (out of the well, if that is where Rock was: the friend is told) */
    cave_enter(fresh);
    mus_solo = 0; mus_demo = 0; gh.playing = 0;  /* (CRYSTAL ROAD's song or its demo, if one was under way, is over) */
    if (story >= 4) mus_set(MUS_JUKE, juke_track); else mus_set(MUS_OFF, 0);
}
static void story_reset(void)
{
    uint32_t i;
    uint8_t keep = cv.secrets;                   /* (starting the story over keeps what was found; wiping the save does not) */
    for (i = 0; i < sizeof cv; i++) ((uint8_t *)&cv)[i] = 0;
    cv.secrets = keep;
    take_fx[0] = 4; take_fx[1] = take_fx[2] = take_fx[3] = 0;
    story = 0; take_n = 0; juke_track = 0; replay = 0; thanks_due = 0; dlg.on = 0;
    scene = SC_INTRO; intro_reset(); mus_set(MUS_INTRO, 0);
    sy_drone_want = 0; sy_music_want = 256;
    save_soon();
}

/* the settings' last line, PLAY held: the whole save goes, scores, settings and the profile too (the language stays:
 * you are reading it) */
static void save_wipe(void)
{
    uint32_t i;
    pf_reset(); bt_set(1); pu.on = PU_OFF; pfx_clear(); tix.on = 0;   /* (as new: Bluetooth on) */
#ifdef DINGHY_HOST
    printf("%6u ms  the whole save wiped\n", fm1_ms);
#endif
    best[0] = best[1] = best[2] = 0; cv.secrets = 0; race_best_cs = 0;
    for (i = 0; i < JUKE_N * 3u; i++) gh_best[i / 3u][i % 3u] = 0;
    for (i = 0; i < JUKE_N; i++) gh_gem[i] = 0;
    gh_flags = 0; gh_mode = 0;
    for (i = 0; i < DM_N; i++) duo_best[i] = 0;
    for (i = 0; i < 4u; i++) duo_road[i] = trio_road[i] = 0;
    for (i = 0; i < DM_N; i++) trio_best[i] = 0;
    duo_prog = 0;
    link_id = 0; set_look(0); mn.on = 0; tsize = 1; sy_ear = 0; play_s = 0; play_ms = 0; sh.on = SH_NONE;
    story_reset();
    banner_show("", TR(T_WIPED), 0);
    sfx(60, 0, 400, W_TRI, 3000, 4, 150); sfx(48, 200, 900, W_TRI, 3000, 4, 150);
}

/* into a machine; CRYSTAL ROAD always starts on its slowest song, OPAL TIDE */
static void mach_go(int which)
{
    if (which == 0) { juke_track = 4; mus_set(MUS_JUKE, juke_track); }
    mach_enter(which);
}
static void arcave_init(void)
{
    uint32_t i;
    hat_init();
    RD[RI_ROOTS] = &ROOTS_DESC; RD[RI_GEODE] = &GE_DESC; RD[RI_RACK] = &RACK_DESC; RD[RI_SEAM] = &SEAM_DESC;
    room_sv = &svr.room;
    int have = settings_load(&svr, sizeof svr) == 0;
    if (!have) {                                 /* a save from before the profile (shorter): all of it, no name, no tickets */
        for (i = 0; i < sizeof svr; i++) ((uint8_t *)&svr)[i] = 0;
        have = settings_load(&svr, sizeof svr - sizeof svr.prof) == 0;
    }
    if (!have) {                                 /* a save from before the music room (shorter): all of it, the room as new */
        for (i = 0; i < sizeof svr; i++) ((uint8_t *)&svr)[i] = 0;
        have = settings_load(&sv, sizeof sv) == 0;
    }
    if (!have) {                                 /* a save of an early 0.20 (shorter): all of it, nothing played by three */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv, SAVE5_SIZE) == 0;
    }
    if (!have) {                                 /* a save of 0.19 (shorter): all of it, TWIN ROAD's other songs unplayed */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv, SAVE4_SIZE) == 0;
    }
    if (!have) {                                 /* a save from before UNDERSONG (shorter): all of it, the well untouched */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv, sizeof(save3_t)) == 0;
    }
    if (!have) {                                 /* a save from before CRYSTAL ROAD (shorter): all of it, its scores at nothing */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv, sizeof(save2_t)) == 0;
    }
    if (!have) {                                 /* a save from 0.1 or 0.2 (shorter still): its story, with the new things at their start */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv.v1, sizeof sv.v1) == 0;
    }
    if (have && sv.v1.magic == SAVE_MAGIC) {
        const save1_t *v = &sv.v1;
        set_lang(v->lang); set_look(v->look); story = v->story > 4u ? 0u : v->story; link_id = v->link % 5u; juke_track = v->track % JUKE_N;
        take_bpm = v->take_bpm ? v->take_bpm : 90u;
        tsize = v->tsize1 >= 1u && v->tsize1 <= 3u ? (uint8_t)(v->tsize1 - 1u) : 1u;
        for (i = 0; i < TAKE_MAX; i++) { take_ev[i][0] = tk_sane(v->take[i][0]); take_ev[i][1] = v->take[i][1] % AC_NKEY; }
        take_n = v->take_n > TAKE_MAX ? 0u : v->take_n;
        for (i = 0; i < 3u; i++) best[i] = v->best[i];
        play_s = play_saved_s = sv.play_s; sy_ear = sv.ear ? 1u : 0u; cv.secrets = sv.secrets & 14u; race_best_cs = sv.race_cs;
        for (i = 0; i < 4u; i++) if (sv.fx1[i]) take_fx[i] = (uint8_t)imin(sv.fx1[i] - 1, TAKE_FX_MAX);
        for (i = 0; i < JUKE_N * 3u; i++) gh_best[i / 3u][i % 3u] = sv.gh_best[i / 3u][i % 3u];
        for (i = 0; i < JUKE_N; i++) gh_gem[i] = sv.gh_gem[i] & 63u;
        gh_flags = sv.gh_flags & 1u; gh_mode = (uint8_t)(sv.gh_mode % 3u);
        if (!(gh_flags & 1u)) gh_mode = 0;
        for (i = 0; i < DM_N; i++) duo_best[i] = sv.duo_best[i];
        for (i = 0; i < 4u; i++) duo_road[i] = sv.duo_road[i];
        for (i = 0; i < DM_N; i++) trio_best[i] = sv.trio_best[i];
        for (i = 0; i < 4u; i++) trio_road[i] = sv.trio_road[i];
        duo_prog = sv.duo_prog > 4u ? 0u : sv.duo_prog;
        if (story == 3u) story = 2u;             /* the light was back but no song yet: the rings again (they are quick) */
    }
    pf = svr.prof; pf_check();                   /* (no profile in the save: a new one) */
#ifdef AC_TEST_TICKETS                           /* (tests) tickets in the pocket */
    pf.tickets = AC_TEST_TICKETS;
#endif
#ifdef AC_TEST_DONE                              /* (tests) rounds already finished: a bit a machine */
    pf.mach_done = AC_TEST_DONE;
#endif
#ifdef AC_TEST_WEAR                              /* (tests) everything owned; a stone, a hat, a face and a trail on; each key colour in its own things */
    {
        static const uint8_t Z[4][4] = {{1, 1, 1, 1}, {7, 4, 2, 3}, {3, 5, 5, 5}, {6, 7, 6, 6}};
        for (i = 0; i < sizeof pf.own; i++) pf.own[i] = 255;
        pf.eq[0] = 2; pf.eq[1] = 10; pf.eq[2] = 7; pf.eq[3] = AC_TEST_WEAR;
        for (i = 0; i < 16u; i++) pf.zone[i / 4u][i % 4u] = Z[i / 4u][i % 4u];
    }
#endif
    bt_set((pf.flags & PF_BT) != 0);
#ifdef AC_TEST_TSIZE                             /* (tests) the text size: 0 small, 1 medium, 2 large */
    tsize = AC_TEST_TSIZE;
#endif
#ifdef AC_TEST_BIG                               /* (tests) as on the browser emulator's BIG SCREEN: the text a size smaller */
    tsize_down = 1;
#endif
#ifdef AC_TEST_STORY                             /* (tests) start further on in the story */
    story = AC_TEST_STORY;
    if (story >= 4u) { take_n = 3; take_ev[0][0] = 0; take_ev[0][1] = 5; take_ev[1][0] = 8; take_ev[1][1] = 12; take_ev[2][0] = 20; take_ev[2][1] = 9; }
#endif
    scene = SC_INTRO;
    intro_reset();
    mus_set(MUS_INTRO, 0);
    fb_all = 1;
#if defined AC_TEST_SCENE && !defined AC_TEST_X  /* (tests) where scene 9 puts Rock, unless AT=X,Y says */
#define AC_TEST_X 466
#define AC_TEST_Y 274
#endif
#ifdef AC_TEST_SCENE                             /* (tests) straight to a scene: 1 the rings, 2 the dish, 3-5 a machine */
    if (AC_TEST_SCENE == 1) { scene = SC_LAMP; lamp_reset(); }
    else if (AC_TEST_SCENE == 2) { scene = SC_DISH; dish_reset(story >= 4u); }
    else if (AC_TEST_SCENE == 6) intro_race_start();                 /* (6: out of the upper door, the race back) */
    else if (AC_TEST_SCENE == 8) {               /* (8: on the course, up on the ledge behind the start) */
        G.phase = IP_PLAY; G.t = 0; G.eyes = 256; G.turned = 1;
        G.r.x = Q8(CO_WALLX - 4); G.r.y = Q8(CO_LEDGE) - rock_rad(0); G.cam_y = G.r.y - Q8(130);
    }
    else if (AC_TEST_SCENE == 9) {               /* (9: in the cave at AC_TEST_X, AC_TEST_Y: run_arcave.sh AT=x,y) */
        scene = SC_CAVE; cave_enter(1); cv.met = 1; cv.x = Q8(AC_TEST_X); cv.y = Q8(AC_TEST_Y);
        cv.camx = cv.x - Q8(120); cv.camy = cv.y - Q8(176);
    }
    else if (AC_TEST_SCENE == 7) { scene = SC_CAVE; cave_enter(1); cv.x = Q8(CV_DOOR_X + 62); cv.y = Q8(181); }   /* (7: in the room by the door) */
    else if (AC_TEST_SCENE == 10) { scene = SC_DUO; mus_set(MUS_JUKE, 1); duo_enter(); }
    else if (AC_TEST_SCENE == 11) { scene = SC_ROOM; cave_enter(1); room_enter(); }   /* (11: in the music room) */   /* (10: down the well, UNDERSONG) */
    else { scene = SC_MACH; mus_set(MUS_JUKE, 2); mach_go(AC_TEST_SCENE - 3); }
#endif
}

/* the remote (ac_remote.h): a page in a browser plays the game with this FM-1 as its controller. Its own game is set
 * aside, with everything kept; afterwards it starts again as after a power-on, from the save the page sent back */
static void fmr_game_enter(void)
{
    if (scene == SC_ROOM) room_leave();
    if (du.on) duo_leave();
    save_dirty = 0; save_now();
    mus_set(MUS_OFF, 0); mus_solo = 0; mus_demo = 0; gh.playing = 0;
    sy_drone_want = 0; sy_rumble_want = 0;
#ifdef DINGHY_HOST
    printf("%6u ms  remote: a page plays with this FM-1 (%u bytes of save sent)\n", fm1_ms, (unsigned)sizeof svr);
#endif
    fmr_start(&svr, sizeof svr);
}
static void fmr_game_leave(void)
{
    uint32_t i;
    fmr.on = 0;
    for (i = 0; i < sizeof cv; i++) ((uint8_t *)&cv)[i] = 0;
    dlg.on = 0; mn.on = 0; sh.on = SH_NONE; pu.on = PU_OFF; tix.on = 0; banner.on = 0; pfx_clear();
    replay = 0; thanks_due = 0; glo_used = 0; was_paused = 0;
#ifdef DINGHY_HOST
    printf("%6u ms  remote: the page is gone; the game again, from the save\n", fm1_ms);
#endif
    arcave_init();
}
static int fmr_game_keep(const uint8_t *p, uint32_t n)
{
    if (n != sizeof svr) return 2;               /* (another version's save: this FM-1 needs the page's version) */
    if (((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24) != SAVE_MAGIC) return 3;
    return settings_save(p, n) == 0 ? 1 : 3;
}

static int lang_tip_up(void) { return scene == SC_INTRO && G.phase == IP_WIDE && G.t < 1800; }
static void juke_change(int32_t d)
{
    if (story < 4) {
        if (algo_quiet <= 0) { banner_show("", TR(T_JUKE_BROKEN), 0); sfx_ex(41, 0, 240, W_SAW, 2200, 2, 160, 0, 0, 65250); algo_quiet = 900; }
        return;
    }
    juke_track = (uint8_t)((juke_track + JUKE_N + (d > 0 ? 1 : -1)) % JUKE_N);
    mus_set(MUS_JUKE, juke_track);
    if (!(scene == SC_MACH && mc.which == 0))   /* (CRYSTAL ROAD's card shows the song itself) */
        banner_show(TR(T_NOWPLAYING), juke_track ? SONGS[juke_track].name : TR(T_YOURSONG), 1);
    snd_ui(84);
    save_soon();
}

/* every pass of the main loop (and while the screen is being sent): the link wants its messages read promptly */
static void arcave_poll(void) { if (ac_started) { link_poll(); duo_poll(); fmr_poll(); } }

static void arcave_frame(void)
{
    uint32_t now = fm1_ms;
    int32_t dt, r;
    if (!ac_started) { ac_started = 1; ac_last_ms = now; arcave_init(); }
    if (now - ac_last_ms < 40u) return;
    dt = imin((int32_t)(now - ac_last_ms), 60);
    ac_last_ms = now;
    if (fmr.on) { fmr_frame(); return; }           /* a page plays with this FM-1: its own screen, the page's lights */
    ac_input();
    ac_leds_begin();
    mus_shared = link_id != 0;                   /* LINK on: every tune in the one shared key (ac_music.h) */
    if (algo_quiet > 0) algo_quiet -= dt;

    /* OCT- and OCT+ step through the languages, anywhere */
    if (scene != SC_ROOM)                        /* the player's song's echo and reverb, while it is what plays (ac_music.h) */
        take_bus((scene == SC_DISH && ds.stage != DS_STIR) || (mus_mode == MUS_JUKE && mus_track % JUKE_N == 0u));
    if ((BDOWN(BTN_OCT_DOWN) || BDOWN(BTN_OCT_UP)) && !mn.on && !pu.on && scene != SC_ROOM && !dish_rec_screen()) {   /* (nor at the dish, recording: the keys' octave) */   /* (in the music room they are the octave) */   /* (in the settings OCT+ confirms and OCT- steps back) */
        set_lang(lang + (BDOWN(BTN_OCT_UP) ? 1 : L_N - 1));
        lang_touched = 1;
        if (lang_tip_up()) ;                     /* the tip is up: it shows the new language itself */
        else if (!(scene == SC_INTRO && G.phase == IP_TITLE) && !mn.on && !dlg.on) banner_show(TR(T_LANGUAGE), TR(T_LANG), 0);   /* (the Hat's box says it in the new language itself) */
        snd_ui(72 + lang * 2); save_soon();
    }
    /* GLO: a tap opens the settings (as it is let go). Wiping the save is in there, on its last line, and nowhere else */
    if (BDOWN(BTN_GLO)) glo_used = mn.on || pu.on;
    if ((in.bup >> BTN_GLO & 1u) && !glo_used && !pu.on) menu_open();
    /* the radio, if it was on and did not start: off, and said so (the next power-on does not try again) */
    if ((pf.flags & PF_BT) && bt_state() == BT_FAILED) { pf.flags &= (uint8_t)~PF_BT; bt_set(0); banner_show("", TR(T_BT_FAIL), 0); save_soon(); }
    if (bt_state() == BT_CONN && !bt_seen) { bt_seen = 1; banner_show("BLUETOOTH", TR(T_BT_CONN), 0); snd_chime(0); }
    else if (bt_state() != BT_CONN) bt_seen = 0;
    /* the time played; kept with every save, and by itself when a quarter of an hour has gone by in the Arcave */
    play_ms += (uint32_t)dt;
    if (play_ms >= 1000u) { play_ms -= 1000u; play_s++; }
    if (scene == SC_CAVE && play_s - play_saved_s >= 900u && !save_dirty) save_soon();
    /* a machine on the clock (the run, the bloom) holds still while a sheet or the settings are up: its start moves on */
    {
    int held = mn.on || sh.on || pu.on || (scene == SC_MACH && mc.state == MS_PLAY && mc.ask);   /* (and REC's box over a game) */
    if (held && !was_paused) { was_paused = 1; pause_tick = tr_now_q8() >> 8; gh_pause(); }
    if (!held && was_paused) { uint32_t d = (tr_now_q8() >> 8) - pause_tick; was_paused = 0; nb.t0 += d; gh_resume(); }
    }
    if (mn.on) {
        r = menu_tick(dt);
        if (r == MA_REPLAY) { replay = 1; scene = SC_INTRO; intro_reset(); mus_set(MUS_INTRO, 0); sy_drone_want = 0; sy_music_want = 256; }
        else if (r == MA_RESET) story_reset();
        else if (r == MA_SKIP) {                 /* into the cave as if PLAY had just been pressed at its mouth: Rock at the
                                                    entrance, the fade in; the story stays where it stands */
            if (story < 1u) story = 1;
            replay = 0; sy_rumble_want = 0; dlg.on = 0; to_cave(1); save_soon();
        }
        else if (r == MA_CHANGED) save_soon();
        else if (r == MA_WIPE) save_wipe();
        else if (r == MA_PROFILE) pu_open(pf_named() ? PU_CARD : PU_NAME, 1);   /* (the first time: NAME YOUR ROCK) */
        else if (r == MA_BT) {                   /* kept at once: the first time it is turned on the FM-1 restarts by itself */
            pf.flags ^= PF_BT; save_dirty = 0; save_now();
            bt_set((pf.flags & PF_BT) != 0);
        }
        sy_rumble_want = 0;
    } else if (pu.on) {                          /* the profile's screens and the upgrades: the game holds still under them */
        r = pu_tick(dt);
        if (r == PA_MENU) menu_open();
        else if (r == PA_SCORES) sheet_open(SH_SCORES);
        sy_rumble_want = 0;
    } else if (sh.on) {
        r = sheet_tick(dt);
        if (r == SA_LEAVE) {
            if (scene == SC_DISH) { dish_abort(); if (story < 4u) story = 2; }   /* (from the effects' page: no song kept) */
            to_cave(0);
        }
    } else {
        /* (in NIGHT BLOOM the ALGORITHM knob turns a ring, and CRYSTAL ROAD's song plays on: the jukebox waits) */
        if (scene == SC_MACH && mc.which == 0) { /* CRYSTAL ROAD's cards: SELECT, PRESETS and ALGORITHM all turn the song */
            int32_t d = in.enc[ENC_ALGORITHM] + in.enc[ENC_SELECT] + in.enc[ENC_PRESETS];
            if (d && mc.state != MS_PLAY) {      /* (in their own order: slowest first) */
                juke_track = (uint8_t)gh_song_step(juke_track, d);
                mus_set(MUS_JUKE, juke_track); snd_ui(84); save_soon();
            }
        } else if (in.enc[ENC_ALGORITHM] && scene != SC_INTRO && scene != SC_LAMP && scene != SC_DISH && scene != SC_DUO && scene != SC_ROOM
            && !(scene == SC_MACH && mc.which == 2 && mc.state == MS_PLAY)) juke_change(in.enc[ENC_ALGORITHM]);
        if (BDOWN(BTN_FX) && scene != SC_ROOM) fx_trigger();   /* (the music room's buttons are its own) */
        if (BDOWN(BTN_EDIT) && !(scene == SC_INTRO && G.phase == IP_TITLE) && scene != SC_DUO && scene != SC_ROOM && !dish_rec_screen()) {
            pu_open(PU_SHOP, 0); in.bdown &= ~(1u << BTN_EDIT);   /* (the scores: the board in the cave, or the profile) */
        }
        else if (scene == SC_INTRO && G.phase == IP_PLAY && BDOWN(BTN_HOME)) sheet_open(SH_INTRO);
        if (!sh.on && !pu.on) switch (scene) {
        case SC_INTRO:
            if (G.phase == IP_WIDE && story >= 1u && !replay) {   /* the story is further on: straight in (a named Rock is greeted) */
                to_cave(0);
                if (pf_named()) banner_show(TR(T_NAME_HELLO), pf.name, 0);
                break;
            }
            if (intro_tick(dt)) { if (story < 1u) story = 1; replay = 0; to_cave(1); save_soon(); }
            if (race_save_due) { race_save_due = 0; save_soon(); }
            if (G.hollow) { G.hollow = 0; if (!(cv.secrets & 8u)) { cave_secret(3); save_soon(); } }   /* the third secret: the hollow behind the start */
            break;
        case SC_CAVE:
            r = cave_tick(dt);
            if (thanks_due && !dlg.on) { thanks_t -= dt; if (thanks_t <= 0) { thanks_due = 0; dlg_open(T_HAT, T_HAT_THANKS); } }
            if (r == CVA_LAMP) { scene = SC_LAMP; lamp_reset(); sy_rumble_want = 0; }
            else if (r == CVA_SCORES) sheet_open(SH_SCORES);
            else if (r == CVA_DISH) { scene = SC_DISH; dish_reset(1); sy_rumble_want = 0; snd_ui(79); }
            else if (r == CVA_EXIT) {            /* out of the upper door: the course again, to be raced back */
                scene = SC_INTRO; replay = 1; intro_race_start(); mus_set(MUS_INTRO, 0); sy_drone_want = 0; sy_music_want = 256; sy_rumble_want = 0;
                snd_ui(79);
            }
            else if (r == CVA_DUO) { scene = SC_DUO; duo_enter(); sy_rumble_want = 0; }
            else if (r == CVA_ROOM) { scene = SC_ROOM; room_enter(); }
            else if (r == CVA_SHOP) pu_open(PU_SHOP, 0);   /* the gift shop's counter: the upgrades (as EDIT, anywhere) */
            else if (r >= CVA_M1) { scene = SC_MACH; mach_go(r - CVA_M1); sy_rumble_want = 0; }
            break;
        case SC_LAMP:
            r = lamp_tick(dt);
            if (r < 0) to_cave(0);
            else if (r > 0) { story = 3; scene = SC_DISH; dish_reset(0); }
            break;
        case SC_DISH:
            r = dish_tick(dt);
            if (r < 0) { if (story < 4u) story = 2; to_cave(0); }
            else if (r == 2) sheet_open(SH_FX);
            else if (r > 0) {                    /* the song is kept: the first time, the power comes back */
                int first = story < 4u;
                story = 4; juke_track = 0;
                gh_best[0][0] = gh_best[0][1] = gh_best[0][2] = 0; gh_gem[0] = 0;   /* (a new song: YOUR SONG's scores start again) */
                to_cave(0);
                cv.juke_ready = 0;               /* (the offer of another song waits until Rock comes back to it) */
                if (first) { cv.power_t = 1600; cv.fade = 0; thanks_due = 1; thanks_t = 2600; sfx(36, 0, 2400, W_SINE, 5200, 20, 0); }
                banner_show(TR(T_NOWPLAYING), TR(T_YOURSONG), 1);
                snd_chime(1);
                save_soon();
            }
            break;
        case SC_MACH:
            r = mach_tick(dt);
            if (r < 0) to_cave(0); else if (r > 0) sheet_open(SH_MACH);
            break;
        case SC_DUO:
            if (duo_tick(dt) < 0) to_cave(0);
            break;
        case SC_ROOM:
            if (room_tick(dt) < 0) {             /* back out of the door, into the tunnel's end */
                to_cave(0);
                cv.camx = Q8(CV_TUN_X1 + 30 - 240); cv.camy = iclamp(cv.y - Q8(176), Q8(CV_GROUND - 372), Q8(CV_GROUND - 214)); cv.door = 256;
            }
            break;
        }
    }
    if (scene != SC_DUO && du.on) duo_leave();   /* (out of the well some other way: the settings, a wipe) */
    switch (scene) {
    case SC_INTRO: intro_draw(); break;
    case SC_CAVE: cave_draw(); break;
    case SC_LAMP: lamp_draw(); break;
    case SC_DISH: dish_draw(); break;
    case SC_MACH: mach_draw(); break;
    case SC_DUO: duo_draw(); break;
    case SC_ROOM: room_draw(); break;
    }
    /* the language tip: in with the title's last second, and out exactly as "press PLAY to wake up Rock" comes in */
    if (scene == SC_INTRO && G.phase == IP_TITLE) lang_tip_draw(smooth8(LANG_TIP_AT, LANG_TIP_AT + 700, G.t));
    else if (lang_tip_up() && !mn.on) lang_tip_draw(256 - iclamp((G.t - 1000) * 256 / 800, 0, 256));
    fx_apply(dt);
    if (sh.on && !mn.on) sheet_draw();
    if (pu.on && !mn.on) pu_draw();
    if (mn.on) menu_draw();
    banner_draw(dt);
    if (!mn.on && batt_pct() >= 0 && batt_low && (now / 500u) & 1u) {   /* the battery running low: it flashes in the corner, on any screen */
        grect(216, 1, 23, 11, th->bg);
        batt_draw(237, 2, batt_pct(), 0, th->caps[0], th->caps[0]);
    }
    ac_present();
    ac_leds_end();
    if (pf_dirty) { pf_dirty = 0; if (!save_dirty) save_soon(); }   /* (tickets, a name, something bought or put on) */
    if (save_dirty && (int32_t)(fm1_ms - save_at) >= 0) { save_dirty = 0; save_now(); }
}
