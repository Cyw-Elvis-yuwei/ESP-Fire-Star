#include "app_heartbeat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static const char *case_name;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s at line %d: %s\n", \
                case_name, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static app_can_frame_t request_for(uint8_t op, uint8_t value)
{
    app_can_frame_t request = {
        APP_CAN_REQUEST_ID, APP_CAN_PAYLOAD_SIZE, 0u,
        {1u, 0u, 0x34u, 0x12u, 0u, 0u, 0u, 0u}
    };
    request.data[1] = op;
    request.data[4] = value;
    return request;
}

static void expect_frame(const app_can_frame_t *actual, uint32_t id,
                         const uint8_t expected[8])
{
    CHECK(actual->id == id);
    CHECK(actual->flags == 0u);
    CHECK(actual->dlc == 8u);
    CHECK(memcmp(actual->data, expected, 8u) == 0);
}

static void test_command_flow(void)
{
    app_state_t state;
    app_can_frame_t reply;
    app_can_frame_t request = request_for(APP_OP_SET_LED, 1u);
    const uint8_t on[] = {1u, 1u, 0x34u, 0x12u, 0u, 1u, 0u, 0u};
    const uint8_t status[] = {1u, 2u, 0x34u, 0x12u, 0u, 1u, 0u, 0u};
    const uint8_t off[] = {1u, 1u, 0xffu, 0xffu, 0u, 0u, 0u, 0u};
    unsigned int repeat;

    case_name = "SET/GET with idempotent replay and sequence echo";
    app_protocol_init(&state);
    CHECK(state.led == 0u);
    for (repeat = 0u; repeat < 2u; ++repeat) {
        memset(&reply, 0xa5, sizeof(reply));
        CHECK(app_protocol_handle(&state, &request, &reply));
        CHECK(state.led == 1u);
        expect_frame(&reply, APP_CAN_RESPONSE_ID, on);
    }

    request = request_for(APP_OP_GET_STATUS, 0u);
    CHECK(app_protocol_handle(&state, &request, &reply));
    CHECK(state.led == 1u);
    expect_frame(&reply, APP_CAN_RESPONSE_ID, status);

    request = request_for(APP_OP_SET_LED, 0u);
    request.data[2] = 0xffu;
    request.data[3] = 0xffu;
    CHECK(app_protocol_handle(&state, &request, &reply));
    CHECK(state.led == 0u);
    expect_frame(&reply, APP_CAN_RESPONSE_ID, off);
}

static void expect_rejection(app_state_t *state, app_can_frame_t *request,
                             uint8_t result)
{
    app_can_frame_t reply;
    uint8_t expected[] = {1u, 0u, 0x34u, 0x12u, 0u, 1u, 0u, 0u};
    expected[1] = request->data[1];
    expected[4] = result;
    CHECK(app_protocol_handle(state, request, &reply));
    CHECK(state->led == 1u);
    expect_frame(&reply, APP_CAN_RESPONSE_ID, expected);
}

static void test_invalid_commands(void)
{
    app_state_t state;
    app_can_frame_t request = request_for(APP_OP_SET_LED, 1u);
    app_can_frame_t reply;
    unsigned int byte;

    case_name = "invalid arguments leave LED unchanged";
    app_protocol_init(&state);
    CHECK(app_protocol_handle(&state, &request, &reply));
    request = request_for(APP_OP_SET_LED, 2u);
    expect_rejection(&state, &request, APP_RESULT_BAD_ARGUMENT);
    for (byte = 5u; byte < 8u; ++byte) {
        request = request_for(APP_OP_SET_LED, 0u);
        request.data[byte] = 1u;
        expect_rejection(&state, &request, APP_RESULT_BAD_ARGUMENT);
    }
    for (byte = 4u; byte < 8u; ++byte) {
        request = request_for(APP_OP_GET_STATUS, 0u);
        request.data[byte] = 1u;
        expect_rejection(&state, &request, APP_RESULT_BAD_ARGUMENT);
    }

    case_name = "version before operation before arguments";
    request = request_for(0x55u, 0xffu);
    expect_rejection(&state, &request, APP_RESULT_UNSUPPORTED_OP);
    request.data[0] = 2u;
    expect_rejection(&state, &request, APP_RESULT_BAD_VERSION);
    request = request_for(APP_OP_SET_LED, 0u);
    request.data[0] = 0u;
    expect_rejection(&state, &request, APP_RESULT_BAD_VERSION);
}

static void test_malformed_frames(void)
{
    app_state_t state;
    app_can_frame_t request;
    app_can_frame_t reply;
    app_can_frame_t sentinel;
    unsigned int variant;
    const uint8_t flags[] = {
        APP_CAN_FLAG_EXTENDED, APP_CAN_FLAG_REMOTE, APP_CAN_FLAG_ERROR, 0x80u
    };

    case_name = "wrong identifier/flags/DLC silently dropped";
    app_protocol_init(&state);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    for (variant = 0u; variant < 7u; ++variant) {
        request = request_for(APP_OP_SET_LED, 1u);
        memcpy(&reply, &sentinel, sizeof(reply));
        if (variant == 0u) {
            request.id = APP_CAN_HEARTBEAT_ID;
        } else if (variant <= 4u) {
            request.flags = flags[variant - 1u];
        } else {
            request.dlc = (variant == 5u) ? 7u : 9u;
        }
        CHECK(!app_protocol_handle(&state, &request, &reply));
        CHECK(state.led == 0u);
        CHECK(memcmp(&reply, &sentinel, sizeof(reply)) == 0);
    }
}

static void test_heartbeat(void)
{
    app_state_t state;
    app_heartbeat_t heartbeat;
    app_can_frame_t frame;
    app_can_frame_t sentinel;
    const uint8_t first[] = {1u, 0u, 1u, 0u, 100u, 0u, 0u, 0u};
    const uint8_t late[] = {1u, 1u, 2u, 0u, 0x38u, 0x13u, 0u, 0u};
    const uint8_t wrapped[] = {1u, 0u, 1u, 0u, 49u, 0u, 0u, 0u};
    app_can_frame_t request = request_for(APP_OP_SET_LED, 1u);
    app_can_frame_t reply;

    case_name = "100ms heartbeat, no catch-up burst, little endian uptime";
    app_protocol_init(&state);
    app_heartbeat_init(&heartbeat, 0u);
    memset(&sentinel, 0x5a, sizeof(sentinel));
    memcpy(&frame, &sentinel, sizeof(frame));
    CHECK(!app_heartbeat_poll(&heartbeat, &state, 0u, &frame));
    CHECK(!app_heartbeat_poll(&heartbeat, &state, 99u, &frame));
    CHECK(memcmp(&frame, &sentinel, sizeof(frame)) == 0);
    CHECK(app_heartbeat_poll(&heartbeat, &state, 100u, &frame));
    expect_frame(&frame, APP_CAN_HEARTBEAT_ID, first);
    CHECK(app_protocol_handle(&state, &request, &reply));
    CHECK(app_heartbeat_poll(&heartbeat, &state, 4920u, &frame));
    expect_frame(&frame, APP_CAN_HEARTBEAT_ID, late);
    CHECK(!app_heartbeat_poll(&heartbeat, &state, 4920u, &frame));
    CHECK(!app_heartbeat_poll(&heartbeat, &state, 5019u, &frame));
    CHECK(app_heartbeat_poll(&heartbeat, &state, 5020u, &frame));
    CHECK(frame.data[2] == 3u);

    case_name = "32bit uptime rollover and 16bit heartbeat counter rollover";
    app_protocol_init(&state);
    app_heartbeat_init(&heartbeat, UINT32_MAX - 50u);
    CHECK(!app_heartbeat_poll(&heartbeat, &state, 48u, &frame));
    CHECK(app_heartbeat_poll(&heartbeat, &state, 49u, &frame));
    expect_frame(&frame, APP_CAN_HEARTBEAT_ID, wrapped);
    heartbeat.counter = UINT16_MAX;
    CHECK(app_heartbeat_poll(&heartbeat, &state, 149u, &frame));
    CHECK(frame.data[2] == 0u && frame.data[3] == 0u);
}

int main(void)
{
    test_command_flow();
    test_invalid_commands();
    test_malformed_frames();
    test_heartbeat();
    printf("PASS: %u native firmware checks (protocol + heartbeat)\n", checks);
    return EXIT_SUCCESS;
}
