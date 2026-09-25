#include "stm32f1xx_hal.h"
#include "../can_app.h"
#include "../../app_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern CAN_HandleTypeDef Can_Handle;
CAN_TypeDef mock_can;
uint32_t mock_primask;
static uint32_t tick, tx_count, green_on, red_on;
static HAL_StatusTypeDef init_status, rx_status, tx_status;
static CanTxMsgTypeDef sent[64];
static bool complete_inside_submit;
static CAN_FilterConfTypeDef actual_filter;

uint32_t HAL_GetTick(void) { return tick; }
void HAL_GPIO_Init(void *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
void HAL_GPIO_WritePin(void *p, uint32_t pin, GPIO_PinState state)
{
    (void)p;
    if (pin == GPIO_PIN_0) green_on = state == GPIO_PIN_RESET;
    if (pin == GPIO_PIN_5) red_on = state == GPIO_PIN_RESET;
}
void HAL_NVIC_SetPriority(int irq, uint32_t a, uint32_t b)
{ (void)irq; assert(a == 5u && b == 0u); }
void HAL_NVIC_EnableIRQ(int irq) { (void)irq; }
HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *h)
{ assert(h->Init.Prescaler == 8u); return init_status; }
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *h, CAN_FilterConfTypeDef *f)
{ (void)h; actual_filter = *f; return HAL_OK; }
HAL_StatusTypeDef HAL_CAN_Receive_IT(CAN_HandleTypeDef *h, uint8_t fifo)
{ (void)h; assert(fifo == CAN_FIFO0); return rx_status; }
HAL_StatusTypeDef HAL_CAN_Transmit_IT(CAN_HandleTypeDef *h)
{
    assert(mock_primask == 1u);
    if (tx_status != HAL_OK) return tx_status;
    assert(tx_count < 64u);
    sent[tx_count++] = *h->pTxMsg;
    if (complete_inside_submit) HAL_CAN_TxCpltCallback(h);
    return HAL_OK;
}
void HAL_CAN_IRQHandler(CAN_HandleTypeDef *h) { (void)h; }

static void setup(void)
{
    tick = tx_count = green_on = red_on = mock_primask = 0u;
    init_status = rx_status = tx_status = HAL_OK;
    complete_inside_submit = false;
    memset(&mock_can, 0, sizeof(mock_can));
    assert(canbench_init());
}
static void request(uint16_t seq, uint8_t op, uint8_t arg)
{
    CanRxMsgTypeDef *rx = Can_Handle.pRxMsg;
    memset(rx, 0, sizeof(*rx));
    rx->StdId = APP_CAN_REQUEST_ID;
    rx->DLC = 8u;
    rx->Data[0] = 1u;
    rx->Data[1] = op;
    rx->Data[2] = (uint8_t)seq;
    rx->Data[3] = (uint8_t)(seq >> 8);
    rx->Data[4] = arg;
    HAL_CAN_RxCpltCallback(&Can_Handle);
}
static void completed(void)
{ HAL_CAN_TxCpltCallback(&Can_Handle); canbench_poll(); }

int main(void)
{
    unsigned int i;
    setup();
    assert(actual_filter.FilterIdHigh == (0x321u << 5));
    assert(actual_filter.FilterMaskIdHigh == 0xffe0u);
    assert(actual_filter.FilterMaskIdLow == 6u && actual_filter.FilterIdLow == 0u);
    tick = 99u; canbench_poll(); assert(tx_count == 0u);
    tick = 100u; canbench_poll();
    assert(tx_count == 1u && sent[0].StdId == 0x123u);
    assert(sent[0].Data[2] == 1u && sent[0].Data[4] == 100u);
    puts("PASS filter, first heartbeat boundary");

    setup(); request(0x1234u, 1u, 1u); canbench_poll();
    assert(green_on && tx_count == 1u && sent[0].StdId == 0x322u);
    assert(sent[0].Data[2] == 0x34u && sent[0].Data[3] == 0x12u);
    assert(sent[0].Data[4] == 0u && sent[0].Data[5] == 1u);
    request(0x1235u, 2u, 0u); canbench_poll(); assert(tx_count == 1u);
    completed(); assert(tx_count == 2u && sent[1].Data[2] == 0x35u);
    assert(sent[1].Data[5] == 1u && canbench_diag.tx_completed == 1u);
    puts("PASS copied RX frames, LED, reply ordering, one in flight");

    setup(); request(7u, 1u, 1u); tx_status = HAL_BUSY;
    canbench_poll(); assert(tx_count == 0u && canbench_diag.tx_busy_retries == 1u);
    tx_status = HAL_OK; canbench_poll();
    assert(tx_count == 1u && sent[0].Data[2] == 7u);
    puts("PASS HAL_BUSY retains reply");

    setup(); complete_inside_submit = true;
    request(8u, 2u, 0u); canbench_poll(); canbench_poll();
    request(9u, 2u, 0u); canbench_poll();
    assert(tx_count == 2u && canbench_diag.tx_completed == 1u);
    puts("PASS early completion does not stick in flight");

    setup();
    for (i = 0u; i < 16u; ++i) request((uint16_t)i, 2u, 0u);
    assert(canbench_diag.rx_dropped == 1u);
    canbench_poll(); canbench_poll(); canbench_poll();
    for (i = 0u; i < 15u; ++i) {
        assert(tx_count == i + 1u && sent[i].Data[2] == i);
        completed();
    }
    assert(tx_count == 15u && canbench_diag.tx_completed == 15u);
    puts("PASS RX overflow count and bounded reply backpressure");

    setup(); tick = 100u; canbench_poll(); tick = 200u; canbench_poll();
    tick = 300u; request(10u, 2u, 0u); completed();
    assert(tx_count == 2u && sent[1].StdId == 0x322u);
    completed(); assert(tx_count == 3u && sent[2].StdId == 0x123u);
    assert(sent[2].Data[2] == 3u && sent[2].Data[4] == 44u && sent[2].Data[5] == 1u);
    puts("PASS replies before latest heartbeat, no heartbeat backlog");

    setup(); request(1u, 1u, 2u); canbench_poll();
    assert(!green_on && sent[0].Data[4] == APP_RESULT_BAD_ARGUMENT);
    completed(); request(2u, 2u, 0u); /* valid copied into queue */
    Can_Handle.pRxMsg->DLC = 7u; HAL_CAN_RxCpltCallback(&Can_Handle);
    canbench_poll(); assert(canbench_diag.rejected_frames == 1u);
    puts("PASS invalid requests and bad DLC");

    setup(); request(1u, 2u, 0u); canbench_poll();
    Can_Handle.ErrorCode = 0x123u; HAL_CAN_ErrorCallback(&Can_Handle);
    canbench_poll(); tick = 1000u; canbench_poll();
    assert(canbench_diag.fault == 4u && canbench_diag.last_hal_error == 0x123u);
    assert(red_on && mock_can.IER == 0u && tx_count == 1u);
    puts("PASS error latched, abort and no new TX");

    setup(); request(1u, 2u, 0u); canbench_poll();
    tick = 249u; canbench_poll(); assert(!canbench_diag.fault);
    tick = 250u; canbench_poll(); assert(canbench_diag.fault == 5u && red_on);
    puts("PASS bounded TX timeout");

    setup(); rx_status = HAL_BUSY; request(1u, 2u, 0u); canbench_poll();
    assert(canbench_diag.fault == 4u && tx_count == 0u);
    setup(); tx_status = HAL_ERROR; request(1u, 2u, 0u); canbench_poll();
    assert(canbench_diag.fault == 3u);
    setup(); init_status = HAL_ERROR; assert(!canbench_init());
    assert(canbench_diag.fault == 1u);
    puts("PASS RX rearm, TX submit and init failures");

    setup(); mock_primask = 1u; canbench_poll(); assert(mock_primask == 1u);
    puts("PASS incoming PRIMASK preserved");
    puts("11 host-only adapter scenarios passed; no hardware verified.");
    return 0;
}
