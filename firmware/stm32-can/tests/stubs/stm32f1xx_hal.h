/* Host-only seam: checks application scheduling, NOT the real HAL or CAN bus. */
#ifndef CANBENCH_TEST_HAL_H
#define CANBENCH_TEST_HAL_H
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t IER, TSR; } CAN_TypeDef;
typedef struct { uint32_t Pin, Mode, Pull, Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t StdId, ExtId, IDE, RTR, DLC; uint8_t Data[8]; } CanTxMsgTypeDef;
typedef CanTxMsgTypeDef CanRxMsgTypeDef;
typedef struct { uint32_t TTCM, ABOM, AWUM, NART, RFLM, TXFP, Mode, SJW, BS1, BS2, Prescaler; } CAN_InitTypeDef;
typedef struct {
    CAN_TypeDef *Instance;
    CanTxMsgTypeDef *pTxMsg;
    CanRxMsgTypeDef *pRxMsg;
    CAN_InitTypeDef Init;
    uint32_t ErrorCode;
} CAN_HandleTypeDef;
typedef struct {
    uint32_t FilterNumber, FilterMode, FilterScale, FilterIdHigh, FilterIdLow;
    uint32_t FilterMaskIdHigh, FilterMaskIdLow, FilterFIFOAssignment;
    uint32_t FilterActivation, BankNumber;
} CAN_FilterConfTypeDef;
extern CAN_TypeDef mock_can;
extern uint32_t mock_primask;
#define CAN1 (&mock_can)
#define GPIOB ((void *)0)
#define GPIO_PIN_0 1u
#define GPIO_PIN_5 32u
#define GPIO_PIN_8 256u
#define GPIO_PIN_9 512u
#define GPIO_MODE_INPUT 0u
#define GPIO_MODE_AF_PP 1u
#define GPIO_NOPULL 0u
#define GPIO_SPEED_FREQ_HIGH 3u
#define DISABLE 0u
#define ENABLE 1u
#define CAN_MODE_NORMAL 0u
#define CAN_SJW_1TQ 0u
#define CAN_BS1_5TQ 4u
#define CAN_BS2_3TQ 2u
#define CAN_FILTERMODE_IDMASK 0u
#define CAN_FILTERSCALE_32BIT 1u
#define CAN_ID_STD 0u
#define CAN_ID_EXT 4u
#define CAN_RTR_DATA 0u
#define CAN_RTR_REMOTE 2u
#define CAN_FIFO0 0u
#define CAN_FILTER_FIFO0 0u
#define CAN_TSR_ABRQ0 128u
#define CAN_TSR_ABRQ1 32768u
#define CAN_TSR_ABRQ2 8388608u
#define USB_LP_CAN1_RX0_IRQn 20
#define USB_HP_CAN1_TX_IRQn 19
#define CAN1_SCE_IRQn 22
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_AFIO_CLK_ENABLE() ((void)0)
#define __HAL_RCC_CAN1_CLK_ENABLE() ((void)0)
#define __HAL_AFIO_REMAP_CAN1_2() ((void)0)
#define __get_PRIMASK() mock_primask
#define __disable_irq() (mock_primask = 1u)
#define __set_PRIMASK(x) (mock_primask = (x))
#define __DMB() ((void)0)
uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(void *, uint32_t, GPIO_PinState);
void HAL_NVIC_SetPriority(int, uint32_t, uint32_t);
void HAL_NVIC_EnableIRQ(int);
HAL_StatusTypeDef HAL_CAN_Init(CAN_HandleTypeDef *);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *, CAN_FilterConfTypeDef *);
HAL_StatusTypeDef HAL_CAN_Receive_IT(CAN_HandleTypeDef *, uint8_t);
HAL_StatusTypeDef HAL_CAN_Transmit_IT(CAN_HandleTypeDef *);
void HAL_CAN_IRQHandler(CAN_HandleTypeDef *);
void HAL_CAN_RxCpltCallback(CAN_HandleTypeDef *);
void HAL_CAN_TxCpltCallback(CAN_HandleTypeDef *);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *);
#endif
