#ifndef APP_HEARTBEAT_H
#define APP_HEARTBEAT_H

#include "app_protocol.h"

#define APP_HEARTBEAT_PERIOD_MS 100u

typedef struct {
    uint32_t last_ms;
    uint16_t counter;
} app_heartbeat_t;

void app_heartbeat_init(app_heartbeat_t *heartbeat, uint32_t now_ms);

/*
 * now_ms is the device's monotonic uptime (e.g. HAL_GetTick), modulo 2^32.
 * First frame is due 100 ms after init and has counter 1. No catch-up bursts:
 * a late poll emits one current frame and starts the next 100 ms interval.
 * A generated frame increments the counter, not a confirmed bus transmission.
 * Poll at least once per uint32_t tick cycle. All pointers must be non-NULL.
 * Returns false before the deadline and leaves frame untouched.
 */
bool app_heartbeat_poll(app_heartbeat_t *heartbeat, const app_state_t *state,
                        uint32_t now_ms, app_can_frame_t *frame);

#endif
