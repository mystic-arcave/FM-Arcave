/* SPDX-License-Identifier: GPL-3.0-only */
/* FM ARCAVE on the FM-1: Dinghy's platform under the game (firmware/src/arcave/). */
#define APP_NAME "FM ARCAVE"
#define STORE_MAGIC 0x56414341u                 /* "ACAV": our own, so another Dinghy firmware's data is never taken for ours */
#define CV_MAX (240u * 16u)                     /* the platform's own text boxes only: the game has its own picture */
#define LCD_BAUD 2u                             /* the screen at 20 MHz: a whole picture in under 50 ms */
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while a band of the picture goes out: the link's messages */
#include "platform.c"
#include "arcave/arcave.c"

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
