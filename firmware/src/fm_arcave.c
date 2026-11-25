/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-ARCAVE on the FM-1: Dinghy's platform under the game (firmware/src/arcave/). */
#define APP_NAME "FM-ARCAVE"
#define STORE_MAGIC 0x56414341u                 /* "ACAV": our own, so another Dinghy firmware's data is never taken for ours */
#define CV_MAX (240u * 16u)                     /* the platform's own text boxes only: the game has its own picture */
#define LCD_BAUD 2u                             /* the screen at 20 MHz: a whole picture in under 50 ms */
#define BANK_LEN (7u * 0xA000u)                  /* the bank (platform.c bank_at): a slot for each ledge's machine and each place of the music room */
#ifndef DINGHY_HOST                              /* on the FM-1 what is loaded runs from the bank (arcave/ac_slots.h): seven slots of */
#define MM_XIP 1                                 /* 40 KiB from flash 0x97000; instructions may be fetched up to the bank's end, flash */
#if !BOARD_SMK37                                 /* 0xDD000 (hal/fm1_guard.h) */
#define FM1_CODE_BANK_END 0xDD000u
#endif
#endif
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while a band of the picture goes out: the link's messages */
#include "platform.c"
#include "arcave/arcave.c"
#if defined MM_XIP && !BOARD_SMK37
_Static_assert(MM_BASE == FL_XIP(SLOTS_AT) && MB_NSLOT * MM_SLOT == BANK_LEN && FM1_CODE_BANK_END == SLOTS_AT + BANK_LEN,
               "the machine bank: mm.h, platform.c and the PC limit disagree about where it is");
#endif

static void app_audio(int32_t *out, uint32_t n) { arcave_audio(out, n); }
static uint8_t idle_ok;
static void app_idle(void) { if (idle_ok) arcave_poll(); }
static void app_main(void)
{
    for (;;) {
        platform_poll();
        arcave_poll();
        idle_ok = 1;
        arcave_frame();
        idle_ok = 0;
    }
}
