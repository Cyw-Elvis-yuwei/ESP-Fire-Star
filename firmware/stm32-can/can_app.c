/* Board adaptation of Embedfire's pinned #39 CAN example; see UPSTREAM.json.
 * HAL/CMSIS vendor sources are unmodified. Business protocol is shared with
 * the existing native tests; only the foreground changes logical LED state. */
#include "stm32f1xx_hal.h"
#include "can_app.h"
#include "../app_protocol.h"
#include "../app_heartbeat.h"
#include <string.h>

#define RX_SLOTS 16u
#define REPLY_SLOTS 8u
#define TX_TIMEOUT_MS 250u
#define RX_BUDGET 4u

CAN_HandleTypeDef Can_Handle;
volatile canbench_diagnostics_t canbench_diag;
static CanTxMsgTypeDef tx_message;
static CanRxMsgTypeDef rx_message;
static app_can_frame_t rx_queue[RX_SLOTS], replies[REPLY_SLOTS];
static volatile uint8_t rx_read, rx_write, tx_done, irq_fault;
static uint8_t reply_read, reply_write, reply_count;
static bool tx_busy, tx_is_reply, heartbeat_pending;
static uint32_t tx_started;
static app_can_frame_t heartbeat_frame;
static app_state_t app_state;
static app_heartbeat_t heartbeat;

static uint32_t irq_lock(void)
{
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    return saved;
}

static void stop_on_fault(uint32_t code)
{
    uint32_t saved = irq_lock();
    if (canbench_diag.fault == 0u) canbench_diag.fault = code;
    Can_Handle.Instance->IER = 0u;
    /* Cancel any pending mailbox; no blocking wait and no reinitialisation.
     * This baseline deliberately requires board RESET after a CAN fault. */
    Can_Handle.Instance->TSR = CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
    __set_PRIMASK(saved);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); /* red */
}

static bool rx_pop(app_can_frame_t *frame)
{
    uint32_t saved = irq_lock();
    bool available = rx_read != rx_write;
    if (available) {
        *frame = rx_queue[rx_read];
        rx_read = (uint8_t)((rx_read + 1u) % RX_SLOTS);
    }
    __set_PRIMASK(saved);
    return available;
}

bool canbench_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    CAN_FilterConfTypeDef filter = {0};
    HAL_StatusTypeDef status;
    uint32_t saved;
    memset(&Can_Handle, 0, sizeof(Can_Handle));
    memset((void *)&canbench_diag, 0, sizeof(canbench_diag));
    rx_read = rx_write = tx_done = irq_fault = 0u;
    reply_read = reply_write = reply_count = 0u;
    tx_busy = heartbeat_pending = false;
    app_protocol_init(&app_state);
    app_heartbeat_init(&heartbeat, HAL_GetTick());

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_CAN1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    __HAL_AFIO_REMAP_CAN1_2();

    Can_Handle.Instance = CAN1;
    Can_Handle.pTxMsg = &tx_message;
    Can_Handle.pRxMsg = &rx_message;
    Can_Handle.Init.TTCM = DISABLE;
    Can_Handle.Init.ABOM = ENABLE;
    Can_Handle.Init.AWUM = DISABLE;
    Can_Handle.Init.NART = DISABLE;
    Can_Handle.Init.RFLM = ENABLE;
    Can_Handle.Init.TXFP = DISABLE;
    Can_Handle.Init.Mode = CAN_MODE_NORMAL;
    Can_Handle.Init.SJW = CAN_SJW_1TQ;
    Can_Handle.Init.BS1 = CAN_BS1_5TQ;
    Can_Handle.Init.BS2 = CAN_BS2_3TQ;
    Can_Handle.Init.Prescaler = 8u; /* APB1 36 MHz / (8 * 9) = 500 kbit/s */
    if (HAL_CAN_Init(&Can_Handle) != HAL_OK) {
        stop_on_fault(1u);
        return false;
    }
    filter.FilterNumber = 0u;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = APP_CAN_REQUEST_ID << 5;
    filter.FilterMaskIdHigh = 0xFFE0u;
    filter.FilterMaskIdLow = CAN_ID_EXT | CAN_RTR_REMOTE;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.BankNumber = 14u;
    if (HAL_CAN_ConfigFilter(&Can_Handle, &filter) != HAL_OK) {
        stop_on_fault(1u);
        return false;
    }

    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5u, 0u);
    HAL_NVIC_SetPriority(USB_HP_CAN1_TX_IRQn, 5u, 0u);
    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_EnableIRQ(USB_HP_CAN1_TX_IRQn);
    HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
    saved = irq_lock();
    status = HAL_CAN_Receive_IT(&Can_Handle, CAN_FIFO0);
    __set_PRIMASK(saved);
    if (status != HAL_OK) {
        stop_on_fault(2u);
        return false;
    }
    return true;
}

void canbench_poll(void)
{
    app_can_frame_t request;
    const app_can_frame_t *next;
    HAL_StatusTypeDef status;
    uint32_t saved, now = HAL_GetTick();
    unsigned int i;
    if (canbench_diag.fault != 0u) return;

    /* Take completion/error flags together. RX/TX/SCE have equal preemption
     * priority; foreground HAL calls use short PRIMASK sections. */
    saved = irq_lock();
    if (tx_done != 0u && tx_busy) {
        if (tx_is_reply) {
            reply_read = (uint8_t)((reply_read + 1u) % REPLY_SLOTS);
            --reply_count;
        }
        tx_busy = false;
        ++canbench_diag.tx_completed;
    }
    tx_done = 0u;
    if (irq_fault != 0u) {
        __set_PRIMASK(saved);
        stop_on_fault(4u);
        return;
    }
    __set_PRIMASK(saved);
    if (tx_busy && (uint32_t)(now - tx_started) >= TX_TIMEOUT_MS) {
        stop_on_fault(5u);
        return;
    }

    for (i = 0u; i < RX_BUDGET && reply_count < REPLY_SLOTS; ++i) {
        if (!rx_pop(&request)) break;
        if (app_protocol_handle(&app_state, &request, &replies[reply_write])) {
            reply_write = (uint8_t)((reply_write + 1u) % REPLY_SLOTS);
            ++reply_count;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,
                app_state.led ? GPIO_PIN_RESET : GPIO_PIN_SET);
        } else {
            ++canbench_diag.rejected_frames;
        }
    }
    if (app_heartbeat_poll(&heartbeat, &app_state, now, &heartbeat_frame))
        heartbeat_pending = true;
    if (tx_busy || (reply_count == 0u && !heartbeat_pending)) return;

    next = reply_count != 0u ? &replies[reply_read] : &heartbeat_frame;
    saved = irq_lock();
    /* An error may have arrived while processing requests. Do not let a new
     * HAL submit clear its status or turn interrupts back on. */
    if (irq_fault != 0u) {
        __set_PRIMASK(saved);
        stop_on_fault(4u);
        return;
    }
    tx_message.StdId = next->id;
    tx_message.ExtId = 0u;
    tx_message.IDE = CAN_ID_STD;
    tx_message.RTR = CAN_RTR_DATA;
    tx_message.DLC = next->dlc;
    memcpy(tx_message.Data, next->data, APP_CAN_PAYLOAD_SIZE);
    tx_busy = true;
    tx_is_reply = reply_count != 0u;
    tx_started = now;
    status = HAL_CAN_Transmit_IT(&Can_Handle);
    if (status == HAL_OK) {
        if (!tx_is_reply) heartbeat_pending = false;
    } else {
        tx_busy = false; /* Keep queue head/candidate for a HAL_BUSY retry. */
    }
    __set_PRIMASK(saved);
    if (status == HAL_BUSY) ++canbench_diag.tx_busy_retries;
    else if (status != HAL_OK) stop_on_fault(3u);
}

void HAL_CAN_RxCpltCallback(CAN_HandleTypeDef *handle)
{
    uint8_t next_write;
    app_can_frame_t *frame;
    const CanRxMsgTypeDef *message = handle->pRxMsg;
    if (handle != &Can_Handle || canbench_diag.fault != 0u) return;
    ++canbench_diag.rx_frames;
    next_write = (uint8_t)((rx_write + 1u) % RX_SLOTS);
    if (next_write == rx_read) {
        ++canbench_diag.rx_dropped;
    } else {
        frame = &rx_queue[rx_write];
        frame->id = message->IDE == CAN_ID_STD ? message->StdId : message->ExtId;
        frame->flags = message->IDE == CAN_ID_STD ? 0u : APP_CAN_FLAG_EXTENDED;
        if (message->RTR != CAN_RTR_DATA) frame->flags |= APP_CAN_FLAG_REMOTE;
        frame->dlc = (uint8_t)message->DLC;
        memcpy(frame->data, message->Data, APP_CAN_PAYLOAD_SIZE);
        __DMB();
        rx_write = next_write;
    }
    if (HAL_CAN_Receive_IT(handle, CAN_FIFO0) != HAL_OK) irq_fault = 1u;
}

void HAL_CAN_TxCpltCallback(CAN_HandleTypeDef *handle)
{
    if (handle == &Can_Handle) tx_done = 1u;
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *handle)
{
    if (handle == &Can_Handle) {
        canbench_diag.last_hal_error = handle->ErrorCode;
        irq_fault = 1u;
    }
}

void USB_LP_CAN1_RX0_IRQHandler(void) { HAL_CAN_IRQHandler(&Can_Handle); }
void USB_HP_CAN1_TX_IRQHandler(void) { HAL_CAN_IRQHandler(&Can_Handle); }
void CAN1_SCE_IRQHandler(void) { HAL_CAN_IRQHandler(&Can_Handle); }
