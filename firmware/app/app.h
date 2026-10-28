/* SPDX-License-Identifier: MPL-2.0 */
/* FIZZ on fm1-base: the app's half of the contract with the platform (fm1-base device/unity.c, host/plat_host.c).
 * The app itself is app.c (fm1-base's side: the panel, the LEDs, the screen, the sound's ring, MIDI and SysEx) and
 * fz_port.c with FIZZ's core (core/), joined by fizz_port.h. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "FIZZ"
#define APP_SPLASH_RGB RGB(255, 168, 40)     /* the cola's amber */
#define APP_TAGLINE ""
#define APP_BLOCK 256u                       /* frames per audio half: 5.8 ms (FIZZ's sound made in the interrupt: a
                                              * main-loop lock may hold it up to ~5 ms before a half is late) */
/* fm1-base's canvas is for its own screens only (boot, crash, update, the splash): FIZZ draws in its own strips and
 * sends them from where they are. 16 rows: the tallest face (APP_BIG_FONT; P2.5: the pool is full) */
#define APP_CANVAS_PX (240u * 16u)
#if QD_PKG_BLE && !defined(PLAT_HOST)
/* (the Bluetooth package) that canvas in FIZZ's frame, not 7,680 B of the pool of its own: the frame's rows in the pool
 * (app.c fz_fb_hi), redrawn whole after any of fm1-base's screens (firmware/build.py BLE_OVERLAY) */
#include <stdint.h>
extern uint8_t fz_fb_hi[][240];
#define APP_CANVAS_AT ((uint16_t *)(void *)fz_fb_hi)
#endif
#define APP_BIG_FONT FONT_B                  /* (FONT_L's 20 KB of flash saved; the splash's name in B, 16 rows) */
#define APP_SYSEX                            /* FIZZ's SysEx (7D 46 5A: LIST, DUMP, STORE, LABELS ..) */
#define APP_USB_CMD                          /* the data blob written over USB (firmware/tools/fizz_xdata.py) */
#define FM1B_UAC_RING_POOL 1                 /* USB audio in's 2 KB ring in the pool (FIZZ's RAM is full: BUDGET.md) */
#if QD_PKG_BLE
/* (the Bluetooth package) the radio's MIDI queues, in packets (firmware/patches/fm1-base-ble-sysex.patch: radio.c, its
 * rings in the pool: firmware/build.py BLE_OVERLAY): out, a whole FIZZ reply at once (LABELS ~560 B: 187 packets of 3
 * bytes); in, read every audio half */
#define BLE_OUT_Q 256u
#define BLE_IN_Q 64u
/* Bluetooth at power-on: SETTINGS page 2's BLUETOOTH (app.c fz_ble_at_boot: FIZZ's store started first) */
int fz_ble_at_boot(void);
#define APP_BLE_AT_BOOT fz_ble_at_boot()
#endif

/* the store's objects: the panel's calibration (the platform's), in Felucca's settings sector pair; FIZZ's own data are
 * raw sectors of the data region (fz_port.c, the blob at PLAT_XDATA1). The Bluetooth package (firmware/build.py --ble:
 * FM1B_BLE) adds the radio's (its address: OBJ_BLE, fm1-base radio.c) in XDATA1's first two sectors, 0xE5000..0xE6FFF
 * (Felucca's autosave pair: fm1-base's storage writes only inside FL_STORE_OK, which 0xFA000 is not); its first blob
 * starts at 0xE7000. An object's number is its record's: OBJ_PANEL stays 0 */
#if FM1B_BLE
enum { OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xE5000u}
#else
enum { OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u}
#endif

/* the contract: the platform calls these */
void app_init(void);                         /* boot: FIZZ's core (or, its data missing, the screen that says so) */
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);   /* audio interrupt: FIZZ's sound, MASTER's gain */
void app_panic(void);                        /* silence now (before an update rewrites the flash) */
void app_ui_init(void);                      /* the screen drawn whole again (after the splash, an update, a restore) */
void app_frame(void);                        /* the main loop, ~60 a second: input, the clock, a frame when due */
void app_input_only(void);                   /* the main loop between those: the same */
void app_say(const char *a, const char *b);  /* (the platform's messages: none reach FIZZ's screen) */
int app_sysex(const uint8_t *p, uint32_t n);
void app_usb_cmd(const uint8_t *d, uint32_t n);

/* the host simulator: expect fz_* (fz_port.c, app.c) and the command "sysex HEX .." */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
