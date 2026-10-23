/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.0 (tools/budget): the device link's entry. The core linked for a 32-bit target (arm-none-eabi, Cortex-M4, the
 * pi32v2's stand-in for the linker) from the calls the FM-1's platform will make, as FM1VST's glue makes them: so
 * --gc-sections keeps what the device keeps, and .data + .bss is the core's static RAM. The app is one static
 * qd_app_t, as on the device. The platform's own buffers (the LCD strips and their DMA, USB, the NOR driver) are not
 * the core's: budget_lcd stands for what the core writes the screen into (counted apart, as the platform's; P2.1: one
 * strip, qd_app_frame_strips, the LCD's DMA sending each as it comes; P2.1b: in place in the core's strip surface, sent
 * before the next strip is drawn: none). Never run. */
#include <stdint.h>
#include <string.h>

#include "app.h"
#include "persist.h"

static qd_app_t A;
uint16_t budget_lcd[1];                                /* (P2.1b: none: each strip in place, sent before the next) */
static uint8_t sx_frame[64];
static int16_t audio[256];
volatile uint32_t budget_sink;

static void lcd_out(void *ctx, int y0, int rows, const uint16_t *px)
{
    (void)ctx;
    budget_sink += (uint32_t)y0 + (uint32_t)rows + px[0];
}
static int fl_read(void *c, uint32_t off, void *dst, uint32_t n) { (void)c, (void)off, memset(dst, 0xFF, n); return 0; }
static int fl_erase(void *c, uint32_t off) { (void)c, (void)off; return 0; }
static int fl_prog(void *c, uint32_t off, const void *src, uint32_t n) { (void)c, (void)off, (void)src, (void)n; return 0; }

int main(void)
{
    static const qd_flash_t fl = {fl_read, fl_erase, fl_prog, 0};
    qd_leds_t leds;
    uint32_t ms = 0;
    qd_app_init(&A);
    qd_app_sysex_version(&A, "budget");
    qd_persist_attach(&A, &fl);
    for (;;) {
        qd_app_tick(&A, ++ms);
        qd_app_key(&A, (int)(ms % QD_NKEY), (int)(ms & 1u), 100);
        qd_app_button(&A, (int)(ms % QD_NBTN), (int)(ms & 2u));
        qd_app_knob(&A, (int)(ms % QD_NKNOB), 1);
        qd_app_master(&A, (int)(ms & 1023u));
        qd_app_midi(&A, 0x90, 60, 100);
        qd_app_sysex(&A, sx_frame, sizeof sx_frame);
        budget_sink += (uint32_t)qd_app_sysex_take(&A, sx_frame, sizeof sx_frame) + (uint32_t)qd_app_sysex_peek(&A);
        qd_app_audio(&A, audio, 128);
        qd_app_frame_strips(&A, 0, lcd_out, 0);
        qd_app_leds(&A, &leds);
        budget_sink += leds.key[0];
        if (ms % 1000u == 0u)
            budget_sink += (uint32_t)qd_persist_flush(&A);
    }
}
