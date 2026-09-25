#include "app_protocol.h"

void app_protocol_init(app_state_t *state)
{
    state->led = 0u;
}

bool app_protocol_handle(app_state_t *state, const app_can_frame_t *request,
                         app_can_frame_t *reply)
{
    uint8_t result = APP_RESULT_OK;
    uint8_t op;
    uint8_t seq_low;
    uint8_t seq_high;

    if (request->id != APP_CAN_REQUEST_ID || request->flags != 0u ||
        request->dlc != APP_CAN_PAYLOAD_SIZE) {
        return false;
    }

    op = request->data[1];
    seq_low = request->data[2];
    seq_high = request->data[3];

    if (request->data[0] != APP_PROTOCOL_VERSION) {
        result = APP_RESULT_BAD_VERSION;
    } else if (op != APP_OP_SET_LED && op != APP_OP_GET_STATUS) {
        result = APP_RESULT_UNSUPPORTED_OP;
    } else if (request->data[5] != 0u || request->data[6] != 0u ||
               request->data[7] != 0u ||
               (op == APP_OP_SET_LED && request->data[4] > 1u) ||
               (op == APP_OP_GET_STATUS && request->data[4] != 0u)) {
        result = APP_RESULT_BAD_ARGUMENT;
    } else if (op == APP_OP_SET_LED) {
        state->led = request->data[4];
    }

    reply->id = APP_CAN_RESPONSE_ID;
    reply->dlc = APP_CAN_PAYLOAD_SIZE;
    reply->flags = 0u;
    reply->data[0] = APP_PROTOCOL_VERSION;
    reply->data[1] = op;
    reply->data[2] = seq_low;
    reply->data[3] = seq_high;
    reply->data[4] = result;
    reply->data[5] = state->led;
    reply->data[6] = 0u;
    reply->data[7] = 0u;
    return true;
}
