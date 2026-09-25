#include "app_heartbeat.h"

void app_heartbeat_init(app_heartbeat_t *heartbeat, uint32_t now_ms)
{
    heartbeat->last_ms = now_ms;
    heartbeat->counter = 0u;
}

bool app_heartbeat_poll(app_heartbeat_t *heartbeat, const app_state_t *state,
                        uint32_t now_ms, app_can_frame_t *frame)
{
    if ((uint32_t)(now_ms - heartbeat->last_ms) < APP_HEARTBEAT_PERIOD_MS) {
        return false;
    }

    heartbeat->last_ms = now_ms;
    heartbeat->counter = (uint16_t)(heartbeat->counter + 1u);
    frame->id = APP_CAN_HEARTBEAT_ID;
    frame->dlc = APP_CAN_PAYLOAD_SIZE;
    frame->flags = 0u;
    frame->data[0] = APP_PROTOCOL_VERSION;
    frame->data[1] = state->led;
    frame->data[2] = (uint8_t)heartbeat->counter;
    frame->data[3] = (uint8_t)(heartbeat->counter >> 8);
    frame->data[4] = (uint8_t)now_ms;
    frame->data[5] = (uint8_t)(now_ms >> 8);
    frame->data[6] = (uint8_t)(now_ms >> 16);
    frame->data[7] = (uint8_t)(now_ms >> 24);
    return true;
}
