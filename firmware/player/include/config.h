/**
 * Player (contestant) hardware and pairing config.
 * Override PLAYER_ID at build time: -DPLAYER_ID=n  (1..8)
 */
#pragma once

#ifndef PLAYER_ID
#define PLAYER_ID 1
#endif

#if PLAYER_ID < 1 || PLAYER_ID > 8
#error "PLAYER_ID must be 1..8"
#endif

/* XIAO ESP32-S3 silk → GPIO (see docs/hardware.md) */
#define PIN_BUTTON   2   /* D1 / GPIO2 — buzz button, active LOW (to GND) */
#define PIN_LED      1   /* D0 / GPIO1 — external status LED, active HIGH */
#define PIN_LED_ONBOARD 21 /* USER_LED, active LOW — mirrored optionally */

#define USE_ONBOARD_LED 1
#define BUTTON_ACTIVE_LOW 1
#define DEBOUNCE_MS 40

/**
 * Pairing mode:
 *   PAIR_LEARN_FROM_ARM (default): remember base MAC from first MSG_ARM
 *   PAIR_FIXED_BASE_MAC: only accept control from BASE_MAC below
 */
#define PAIR_LEARN_FROM_ARM 1

/* Used when PAIR_LEARN_FROM_ARM is 0 — replace with your base STA MAC */
#ifndef BASE_MAC_0
#define BASE_MAC_0 0xFF
#define BASE_MAC_1 0xFF
#define BASE_MAC_2 0xFF
#define BASE_MAC_3 0xFF
#define BASE_MAC_4 0xFF
#define BASE_MAC_5 0xFF
#endif

#define LED_BLINK_ARMED_MS 250
#define LED_BLINK_LATE_ON_MS 80
#define LED_BLINK_LATE_OFF_MS 320
