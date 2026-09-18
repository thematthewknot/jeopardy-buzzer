/**
 * Base station hardware and host-keystroke config.
 */
#pragma once

/* XIAO ESP32-S3 pins — see docs/hardware.md */
#define PIN_BTN_ARM    2   /* D1 / GPIO2 — ARM / RESET (short vs long) */
#define PIN_BTN_HOST   3   /* D2 / GPIO3 — optional dedicated host key */
#define PIN_LED        1   /* D0 / GPIO1 — status LED */
#define PIN_LED_ONBOARD 21 /* USER_LED active LOW */

#define USE_ONBOARD_LED 1
#define USE_DEDICATED_HOST_BTN 1 /* set 0 for single-button (long=host key) */
#define BUTTON_ACTIVE_LOW 1
#define DEBOUNCE_MS 40
#define LONG_PRESS_MS 700

/**
 * Key injected to host PC when host button fires.
 * Default: space (common Jeopardy software "buzzer" / next).
 * Change to KEY_F1, 'b', etc. as needed (USBHIDKeyboard codes).
 */
#ifndef HOST_KEY
#define HOST_KEY ' '
#endif

#define MAX_ORDER 8
#define WIFI_CHANNEL 1
