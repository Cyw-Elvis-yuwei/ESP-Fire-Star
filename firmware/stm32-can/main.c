/* Clock/GPIO configuration adapted from Embedfire #39, pinned in UPSTREAM.json.
 * Cortex-M3 startup, CMSIS and HAL retain their original notices in vendor/. */
#include "stm32f1xx_hal.h"
#include "can_app.h"

volatile uint32_t canbench_boot_stage;

static void fatal(uint32_t stage)
{
    canbench_boot_stage = stage;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
    while (1) __WFI();
}

static void led_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_5, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static bool clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) return false;
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    return HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) == HAL_OK &&
           HAL_RCC_GetPCLK1Freq() == 36000000u;
}

int main(void)
{
    /* Vendor SystemInit returns on HSI, while its variable defaults to 72 MHz.
     * Refresh it before HAL sets SysTick, so initial oscillator timeouts use ms. */
    SystemCoreClockUpdate();
    HAL_Init();
    led_init();
    canbench_boot_stage = 1u;
    if (!clock_init()) fatal(0xE1u);
    canbench_boot_stage = 2u;
    if (!canbench_init()) fatal(0xE2u);
    canbench_boot_stage = 3u;
    while (1) {
        canbench_poll();
        __WFI(); /* SysTick wakes at 1 ms; CAN IRQs also wake the foreground. */
    }
}

void SysTick_Handler(void) { HAL_IncTick(); }
void HardFault_Handler(void)
{
    canbench_boot_stage = 0xEFu;
    while (1) { /* Preserve registers for the debugger. */ }
}
