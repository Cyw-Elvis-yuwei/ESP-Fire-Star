#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#define APP_PROTOCOL_VERSION 1u
#define APP_CAN_REQUEST_ID 0x321u
#define APP_CAN_RESPONSE_ID 0x322u
#define APP_CAN_HEARTBEAT_ID 0x123u
#define APP_CAN_PAYLOAD_SIZE 8u

/* Adapter flags: protocol frames must have flags == 0. */
#define APP_CAN_FLAG_EXTENDED 0x01u
#define APP_CAN_FLAG_REMOTE 0x02u
#define APP_CAN_FLAG_ERROR 0x04u

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[APP_CAN_PAYLOAD_SIZE];
} app_can_frame_t;

typedef struct {
    /* Logical state only: 1 = on; the board adapter handles active-low GPIO. */
    uint8_t led;
} app_state_t;

enum app_operation {
    APP_OP_SET_LED = 1,
    APP_OP_GET_STATUS = 2
};

enum app_result {
    APP_RESULT_OK = 0,
    APP_RESULT_UNSUPPORTED_OP = 1,
    APP_RESULT_BAD_ARGUMENT = 2,
    APP_RESULT_BAD_VERSION = 3
};

void app_protocol_init(app_state_t *state);

/*
 * All pointers must be non-NULL. Call from one owner (the foreground loop).
 * Returns false for a wrong ID, any flags, or DLC != 8; state/reply untouched.
 * Otherwise emits one response, echoing op and sequence even on an error.
 * Validation order: version, operation, arguments. Invalid input never changes
 * LED state. Does not transmit, wait, allocate, or access hardware.
 */
bool app_protocol_handle(app_state_t *state, const app_can_frame_t *request,
                         app_can_frame_t *reply);

#endif
