/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE: the game. One file after another, as Dinghy builds: the platform is already above this (fm_arcave.c on
 * the FM-1 and in tests, web/emu/web_host.c in a browser).
 *
 *   the intro    title, "press PLAY to wake up Rock", the roll to the cave          ac_intro.h
 *   the cave     a platforming hub: the Hat, the dark jukebox, three machines       ac_cave.h
 *   the mending  three rings of glass, then the dish and your first song            ac_juke.h
 *   the machines MYSTIC RUN, JUICE WORKS, NIGHT BLOOM                           ac_mach.h
 *   everywhere   GLO: settings (the game pauses); OCT- OCT+: the language; ALGORITHM: the jukebox;
 *                LINK: several FM-1s on one clock                                   ac_menu.h, ac_sync.h
 *                HOME: how to play; EDIT: the scores and the time played; FX: an effect     ac_sheet.h */
#include "ac_base.h"
#include "ac_synth.h"
#include "ac_music.h"
#include "ac_text.h"
#include "ac_art.h"
#include "ac_intro.h"
#include "ac_cave.h"
#include "ac_juke.h"
#include "ac_mach.h"
#include "ac_sheet.h"
#include "ac_sync.h"
#include "ac_menu.h"

enum { SC_INTRO, SC_CAVE, SC_LAMP, SC_DISH, SC_MACH };
static uint8_t scene, replay, thanks_due;
static uint32_t ac_last_ms, save_at;
static uint8_t ac_started, juke_track;
static int32_t thanks_t, algo_quiet, tip_t, glo_t, wipe_show, wipe_hold;
static uint8_t glo_used, was_paused;
static uint32_t pause_tick, play_saved_s;

typedef struct {                                 /* the save of 0.1 and 0.2: still read, so an update keeps the story */
    uint32_t magic;
    uint8_t lang, look, story, link, track, take_n, take_bpm, tsize1;   /* tsize1: the text size + 1 (0: never set) */
    uint8_t take[TAKE_MAX][2];
    uint32_t best[3];
} save1_t;
typedef struct {
    save1_t v1;
    uint32_t play_s;                             /* the time played, seconds */
    uint8_t ear, secrets, rsv[2];
    uint32_t race_cs;                            /* the best race back, hundredths of a second (0: never run) */
    uint8_t rsv2[4];
} save_t;
#define SAVE_MAGIC 0x41434156u                   /* "ACAV" */
static save_t sv;

static void save_now(void)
{
    uint32_t i;
    save1_t *v = &sv.v1;
    v->magic = SAVE_MAGIC; v->lang = lang; v->look = look_i; v->story = story; v->link = link_id; v->track = juke_track;
    v->take_n = take_n; v->take_bpm = take_bpm; v->tsize1 = (uint8_t)(tsize + 1u);
    for (i = 0; i < TAKE_MAX; i++) { v->take[i][0] = take_ev[i][0]; v->take[i][1] = take_ev[i][1]; }
    for (i = 0; i < 3u; i++) v->best[i] = best[i];
    sv.play_s = play_s; sv.ear = sy_ear; sv.secrets = cv.secrets; sv.race_cs = race_best_cs;
    play_saved_s = play_s;
    settings_save(&sv, sizeof sv);
}
static void save_soon(void) { save_dirty = 1; save_at = fm1_ms + 1500u; }

static void to_cave(int fresh)
{
    scene = SC_CAVE;
    cave_enter(fresh);
    if (story >= 4) mus_set(MUS_JUKE, juke_track); else mus_set(MUS_OFF, 0);
}
static void story_reset(void)
{
    uint32_t i;
    uint8_t keep = cv.secrets;                   /* (starting the story over keeps what was found; wiping the save does not) */
    for (i = 0; i < sizeof cv; i++) ((uint8_t *)&cv)[i] = 0;
    cv.secrets = keep;
    story = 0; take_n = 0; juke_track = 0; replay = 0; thanks_due = 0; dlg.on = 0;
    scene = SC_INTRO; intro_reset(); mus_set(MUS_INTRO, 0);
    sy_drone_want = 0; sy_music_want = 256;
    save_soon();
}

/* GLO held: the whole save goes, scores and settings too (the language stays: you are reading it) */
static void save_wipe(void)
{
    best[0] = best[1] = best[2] = 0; cv.secrets = 0; race_best_cs = 0;
    link_id = 0; set_look(0); mn.on = 0; tsize = 1; sy_ear = 0; play_s = 0; play_ms = 0; sh.on = SH_NONE;
    story_reset();
    banner_show("", TR(T_WIPED), 0);
    sfx(60, 0, 400, W_TRI, 3000, 4, 150); sfx(48, 200, 900, W_TRI, 3000, 4, 150);
}

static void arcave_init(void)
{
    uint32_t i;
    hat_init();
    int have = settings_load(&sv, sizeof sv) == 0;
    if (!have) {                                 /* a save from 0.1 or 0.2 (shorter): its story, with the new things at their start */
        for (i = 0; i < sizeof sv; i++) ((uint8_t *)&sv)[i] = 0;
        have = settings_load(&sv.v1, sizeof sv.v1) == 0;
    }
    if (have && sv.v1.magic == SAVE_MAGIC) {
        const save1_t *v = &sv.v1;
        set_lang(v->lang); set_look(v->look); story = v->story > 4u ? 0u : v->story; link_id = v->link % 5u; juke_track = v->track % JUKE_N;
        take_bpm = v->take_bpm ? v->take_bpm : 90u;
        tsize = v->tsize1 >= 1u && v->tsize1 <= 3u ? (uint8_t)(v->tsize1 - 1u) : 1u;
        for (i = 0; i < TAKE_MAX; i++) { take_ev[i][0] = v->take[i][0] % 32u; take_ev[i][1] = v->take[i][1] % AC_NKEY; }
        take_n = v->take_n > TAKE_MAX ? 0u : v->take_n;
        for (i = 0; i < 3u; i++) best[i] = v->best[i];
        play_s = play_saved_s = sv.play_s; sy_ear = sv.ear ? 1u : 0u; cv.secrets = sv.secrets & 7u; race_best_cs = sv.race_cs;
        if (story == 3u) story = 2u;             /* the light was back but no song yet: the rings again (they are quick) */
    }
#ifdef AC_TEST_TSIZE                             /* (tests) the text size: 0 small, 1 medium, 2 large */
    tsize = AC_TEST_TSIZE;
#endif
#ifdef AC_TEST_STORY                             /* (tests) start further on in the story */
    story = AC_TEST_STORY;
    if (story >= 4u) { take_n = 3; take_ev[0][0] = 0; take_ev[0][1] = 5; take_ev[1][0] = 8; take_ev[1][1] = 12; take_ev[2][0] = 20; take_ev[2][1] = 9; }
#endif
    scene = SC_INTRO;
    intro_reset();
    mus_set(MUS_INTRO, 0);
    fb_all = 1;
#ifdef AC_TEST_SCENE                             /* (tests) straight to a scene: 1 the rings, 2 the dish, 3-5 a machine */
    if (AC_TEST_SCENE == 1) { scene = SC_LAMP; lamp_reset(); }
    else if (AC_TEST_SCENE == 2) { scene = SC_DISH; dish_reset(); }
    else if (AC_TEST_SCENE == 6) intro_race_start();                 /* (6: out of the upper door, the race back) */
    else if (AC_TEST_SCENE == 7) { scene = SC_CAVE; cave_enter(1); cv.x = Q8(-20); cv.y = Q8(181); }   /* (7: in the room by the door) */
    else { scene = SC_MACH; mach_enter(AC_TEST_SCENE - 3); mus_set(MUS_JUKE, 2); }
#endif
}

static void juke_change(int32_t d)
{
    if (story < 4) {
        if (algo_quiet <= 0) { banner_show("", TR(T_JUKE_BROKEN), 0); sfx_ex(41, 0, 240, W_SAW, 2200, 2, 160, 0, 0, 65250); algo_quiet = 900; }
        return;
    }
    juke_track = (uint8_t)((juke_track + JUKE_N + (d > 0 ? 1 : -1)) % JUKE_N);
    mus_set(MUS_JUKE, juke_track);
    banner_show(TR(T_NOWPLAYING), juke_track ? SONGS[juke_track].name : TR(T_YOURSONG), 1);
    snd_ui(84);
    save_soon();
}

/* every pass of the main loop (and while the screen is being sent): the link wants its messages read promptly */
static void arcave_poll(void) { if (ac_started) link_poll(); }

static void arcave_frame(void)
{
    uint32_t now = fm1_ms;
    int32_t dt, r;
    if (!ac_started) { ac_started = 1; ac_last_ms = now; arcave_init(); }
    if (now - ac_last_ms < 40u) return;
    dt = imin((int32_t)(now - ac_last_ms), 60);
    ac_last_ms = now;
    ac_input();
    ac_leds_begin();
    if (algo_quiet > 0) algo_quiet -= dt;

    /* OCT- and OCT+ step through the languages, anywhere */
    if ((BDOWN(BTN_OCT_DOWN) || BDOWN(BTN_OCT_UP)) && !mn.on) {   /* (in the settings OCT+ confirms and OCT- steps back) */
        set_lang(lang + (BDOWN(BTN_OCT_UP) ? 1 : L_N - 1));
        lang_touched = 1;
        if (tip_t > 0) tip_t = imax(tip_t, 2600);                    /* the tip is up: it shows the new language a while longer */
        else if (!(scene == SC_INTRO && G.phase == IP_TITLE) && !mn.on && !dlg.on) banner_show(TR(T_LANGUAGE), TR(T_LANG), 0);   /* (the Hat's box says it in the new language itself) */
        snd_ui(72 + lang * 2); save_soon();
    }
    /* GLO: a tap opens the settings (as it is let go). Held a second, a notice comes up; held three seconds while the
     * notice is up (still holding, or pressed again), the whole save is wiped */
    if (BDOWN(BTN_GLO)) { glo_t = 0; glo_used = mn.on; }
    if (BHELD(BTN_GLO)) {
        glo_t += dt;
        if (glo_t >= 1000 && wipe_show <= 0) { wipe_show = 1; wipe_hold = 0; mn.on = 0; snd_ui(60); }
        if (wipe_show > 0) {
            glo_used = 1; wipe_show = 4000; wipe_hold += dt;
            if (wipe_hold >= 3000) { wipe_show = 0; wipe_hold = 0; glo_t = -100000; save_wipe(); }
        }
    } else {
        wipe_hold = 0;
        if (wipe_show > 0) wipe_show -= dt;
        if ((in.bup >> BTN_GLO & 1u) && !glo_used && glo_t < 700) menu_open();
    }
    /* the time played; kept with every save, and by itself when a quarter of an hour has gone by in the Arcave */
    play_ms += (uint32_t)dt;
    if (play_ms >= 1000u) { play_ms -= 1000u; play_s++; }
    if (scene == SC_CAVE && play_s - play_saved_s >= 900u && !save_dirty) save_soon();
    /* a machine on the clock (the run, the bloom) holds still while a sheet or the settings are up: its start moves on */
    if ((mn.on || sh.on) && !was_paused) { was_paused = 1; pause_tick = tr_now_q8() >> 8; }
    if (!(mn.on || sh.on) && was_paused) { uint32_t d = (tr_now_q8() >> 8) - pause_tick; was_paused = 0; mr.t0 += d; nb.t0 += d; }
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
        sy_rumble_want = 0;
    } else if (sh.on) {
        r = sheet_tick(dt);
        if (r == SA_LEAVE) to_cave(0);
    } else {
        /* (in NIGHT BLOOM the ALGORITHM knob turns a ring: the jukebox waits) */
        if (in.enc[ENC_ALGORITHM] && scene != SC_INTRO && scene != SC_LAMP && scene != SC_DISH
            && !(scene == SC_MACH && mc.which == 2 && mc.state == MS_PLAY)) juke_change(in.enc[ENC_ALGORITHM]);
        if (BDOWN(BTN_FX)) fx_trigger();
        if (BDOWN(BTN_EDIT) && !(scene == SC_INTRO && G.phase == IP_TITLE)) { sheet_open(SH_SCORES); in.bdown &= ~(1u << BTN_EDIT); }
        else if (scene == SC_INTRO && G.phase == IP_PLAY && BDOWN(BTN_HOME)) sheet_open(SH_INTRO);
        if (!sh.on) switch (scene) {
        case SC_INTRO:
            if (G.phase == IP_WIDE && story >= 1u && !replay) { to_cave(0); break; }   /* the story is further on: straight in */
            if (intro_tick(dt)) { if (story < 1u) story = 1; replay = 0; to_cave(1); save_soon(); }
            if (race_save_due) { race_save_due = 0; save_soon(); }
            break;
        case SC_CAVE:
            r = cave_tick(dt);
            if (thanks_due && !dlg.on) { thanks_t -= dt; if (thanks_t <= 0) { thanks_due = 0; dlg_open(T_HAT, T_HAT_THANKS); } }
            if (r == CVA_LAMP) { scene = SC_LAMP; lamp_reset(); sy_rumble_want = 0; }
            else if (r == CVA_SCORES) sheet_open(SH_SCORES);
            else if (r == CVA_EXIT) {            /* out of the upper door: the course again, to be raced back */
                scene = SC_INTRO; replay = 1; intro_race_start(); mus_set(MUS_INTRO, 0); sy_drone_want = 0; sy_music_want = 256; sy_rumble_want = 0;
                snd_ui(79);
            }
            else if (r >= CVA_M1) { scene = SC_MACH; mach_enter(r - CVA_M1); sy_rumble_want = 0; }
            break;
        case SC_LAMP:
            r = lamp_tick(dt);
            if (r < 0) to_cave(0);
            else if (r > 0) { story = 3; scene = SC_DISH; dish_reset(); }
            break;
        case SC_DISH:
            r = dish_tick(dt);
            if (r < 0) { story = 2; to_cave(0); }
            else if (r > 0) {                    /* the song is kept: the power comes back */
                story = 4; juke_track = 0;
                to_cave(0);
                cv.power_t = 1600; cv.fade = 0; thanks_due = 1; thanks_t = 2600;
                banner_show(TR(T_NOWPLAYING), TR(T_YOURSONG), 1);
                snd_chime(1); sfx(36, 0, 2400, W_SINE, 5200, 20, 0);
                save_soon();
            }
            break;
        case SC_MACH:
            r = mach_tick(dt);
            if (r < 0) to_cave(0); else if (r > 0) sheet_open(SH_MACH);
            break;
        }
    }
    switch (scene) {
    case SC_INTRO: intro_draw(); break;
    case SC_CAVE: cave_draw(); break;
    case SC_LAMP: lamp_draw(); break;
    case SC_DISH: dish_draw(); break;
    case SC_MACH: mach_draw(); break;
    }
    if (scene == SC_INTRO && G.phase == IP_TITLE) { tip_t = 3600; lang_tip_draw(smooth8(3200, 3900, G.t)); }
    else if (tip_t > 0) { tip_t -= dt; if (!mn.on) lang_tip_draw(imin(256, tip_t * 256 / 600)); }   /* three seconds more, then it fades */
    fx_apply(dt);
    if (sh.on && !mn.on) sheet_draw();
    if (mn.on) menu_draw();
    if (wipe_show > 0) {                         /* the notice, and a bar that fills while GLO is held */
        int32_t a = ts(1, 3), h = gblock_h(204, TR(T_WIPE), a), y = 120 - (h + 30) / 2;
        grect(8, y, 224, h + 30, th->panel); gbox(8, y, 224, h + 30, th->caps[0]);
        gblock(18, y + 6, 204, TR(T_WIPE), th->text, a, 1);
        gbox(30, y + h + 14, 180, 9, th->mute);
        grect(32, y + h + 16, 176 * imin(wipe_hold, 3000) / 3000, 5, th->caps[0]);
    }
    banner_draw(dt);
    ac_present();
    ac_leds_end();
    if (save_dirty && (int32_t)(fm1_ms - save_at) >= 0) { save_dirty = 0; save_now(); }
}
