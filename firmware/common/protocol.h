/**
 * Shared ESP-NOW protocol for Jeopardy wireless buzzers.
 * Included by both player and base firmware (Arduino / PlatformIO).
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Protocol identity */
#define JEOPARDY_MAGIC     0x4Au  /* 'J' */
#define JEOPARDY_VERSION   1u
#define JEOPARDY_MAX_PLAYERS 8u

/** Message types (wire values) */
enum JeopardyMsgType : uint8_t {
  MSG_ARM       = 1,  /* Base -> all: accept buzzes for this round */
  MSG_DISARM    = 2,  /* Base -> all: ignore presses */
  MSG_BUZZ      = 3,  /* Player -> base: first press after ARM */
  MSG_LOCKOUT   = 4,  /* Base -> all: round locked (player_id = winner) */
  MSG_RESET     = 5,  /* Base -> all: clear round, return to idle */
  MSG_ACK       = 6,  /* Base -> player: ack buzz; seq = place (1=first) */
  MSG_HEARTBEAT = 7,  /* Optional keep-alive either direction */
};

/**
 * Packed wire frame (10 bytes). Keep fields ordered for ESP-NOW payload.
 *
 * player_id:
 *   - MSG_BUZZ / MSG_ACK: 1..8
 *   - MSG_LOCKOUT: winner id 1..8 (0 if unknown)
 *   - broadcast control: 0
 *
 * seq:
 *   - MSG_ARM / MSG_RESET / MSG_DISARM / MSG_LOCKOUT: round id
 *   - MSG_BUZZ: player local press counter (debug)
 *   - MSG_ACK: place in buzz order (1 = first)
 */
typedef struct __attribute__((packed)) {
  uint8_t  magic;         /* JEOPARDY_MAGIC */
  uint8_t  version;       /* JEOPARDY_VERSION */
  uint8_t  type;          /* JeopardyMsgType */
  uint8_t  player_id;     /* 0 = base/broadcast, 1..8 = contestant */
  uint16_t seq;           /* round id or buzz place */
  uint32_t timestamp_ms;  /* sender millis() at send time */
} JeopardyMsg;

static inline int jeopardy_msg_valid(const JeopardyMsg *m) {
  return m && m->magic == JEOPARDY_MAGIC && m->version == JEOPARDY_VERSION;
}

#ifdef __cplusplus
}
#endif
