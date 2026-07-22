#include "AD9226.h"

#define AD9226_CAPTURE_TIMEOUT_MS (20UL)
#define AD9226_DCMI_WORDS         (AD9226_CAPTURE_SAMPLES / 2UL)
#define AD9226_DCMI_CLEAR_FLAGS   (DCMI_ICR_FRAME_ISC | DCMI_ICR_OVR_ISC | \
                                    DCMI_ICR_ERR_ISC | DCMI_ICR_VSYNC_ISC | \
                                    DCMI_ICR_LINE_ISC)

#if ((AD9226_CAPTURE_SAMPLES % 2UL) != 0UL)
#error AD9226_CAPTURE_SAMPLES must be even for DCMI packing
#endif

static uint16_t s_capture_buffer[AD9226_CAPTURE_SAMPLES]
    __attribute__((aligned(32)));
static uint32_t s_dcmi_buffer[AD9226_DCMI_WORDS]
    __attribute__((section(".dma_buffer"), aligned(32)));
static DMA_HandleTypeDef s_dcmi_dma;

static void AD9226_SetSync(GPIO_PinState state)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_2, state);
}

static void AD9226_WaitClockPeriods(uint32_t periods)
{
    TIM1->SR &= ~TIM_SR_UIF;

    while (periods != 0UL)
    {
        while ((TIM1->SR & TIM_SR_UIF) == 0UL)
        {
        }

        TIM1->SR &= ~TIM_SR_UIF;
        periods--;
    }
}

static void AD9226_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* TIM1_CH1 drives the AD9226 clock and is looped back to PA6/PIXCLK. */
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF13_DCMI;

    gpio.Pin = GPIO_PIN_6;             /* PIXCLK */
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 |
               GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_12; /* D0-D3,D8,D9 */
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = GPIO_PIN_5 | GPIO_PIN_6; /* D5,D10 */
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_2;             /* D11 */
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6; /* D4,D6,D7 */
    HAL_GPIO_Init(GPIOE, &gpio);

    /* Software frame gates: PB1->PA4/HSYNC and PB2->PB7/VSYNC. */
    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = 0U;
    HAL_GPIO_Init(GPIOB, &gpio);
    AD9226_SetSync(GPIO_PIN_SET);

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF13_DCMI;

    gpio.Pin = GPIO_PIN_4;             /* HSYNC */
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_7;             /* VSYNC */
    HAL_GPIO_Init(GPIOB, &gpio);
}

static HAL_StatusTypeDef AD9226_DCMI_Init(void)
{
    __HAL_RCC_DCMI_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();

    DCMI->CR = 0U;
    DCMI->IER = 0U;
    DCMI->ICR = AD9226_DCMI_CLEAR_FLAGS;
    /* Hardware sync, falling-edge PIXCLK, active-high sync, 12-bit data. */
    DCMI->CR = DCMI_CR_HSPOL | DCMI_CR_VSPOL | DCMI_CR_EDM_1;

    s_dcmi_dma.Instance = DMA2_Stream1;
    s_dcmi_dma.Init.Request = DMA_REQUEST_DCMI;
    s_dcmi_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    s_dcmi_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    s_dcmi_dma.Init.MemInc = DMA_MINC_ENABLE;
    s_dcmi_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    s_dcmi_dma.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    s_dcmi_dma.Init.Mode = DMA_NORMAL;
    s_dcmi_dma.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    s_dcmi_dma.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    s_dcmi_dma.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    s_dcmi_dma.Init.MemBurst = DMA_MBURST_INC4;
    s_dcmi_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;

    return HAL_DMA_Init(&s_dcmi_dma);
}

static HAL_StatusTypeDef AD9226_ClockInit(void)
{
    uint32_t timer_clock = HAL_RCC_GetPCLK2Freq();
    uint32_t period_counts;

    if ((RCC->D2CFGR & RCC_D2CFGR_D2PPRE2) != 0U)
    {
        timer_clock *= 2UL;
    }

    period_counts = (timer_clock + (AD9226_SAMPLE_RATE_HZ / 2UL)) /
                    AD9226_SAMPLE_RATE_HZ;
    if (period_counts < 4UL)
    {
        return HAL_ERROR;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    TIM1->CR1 = 0U;
    TIM1->CR2 = 0U;
    TIM1->SMCR = 0U;
    TIM1->DIER = 0U;
    TIM1->CCER = 0U;
    TIM1->PSC = 0U;
    TIM1->ARR = period_counts - 1UL;
    TIM1->CCR1 = period_counts / 2UL;
    TIM1->CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM1->CCER = TIM_CCER_CC1E;
    TIM1->BDTR = TIM_BDTR_MOE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;
    TIM1->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;

    return HAL_OK;
}

HAL_StatusTypeDef AD9226_Init(void)
{
    HAL_StatusTypeDef status;

    AD9226_GPIO_Init();

    status = AD9226_DCMI_Init();
    if (status != HAL_OK)
    {
        return status;
    }

    status = AD9226_ClockInit();
    if (status != HAL_OK)
    {
        return status;
    }

    HAL_Delay(20U);
    return HAL_OK;
}

HAL_StatusTypeDef AD9226_CaptureFrame(void)
{
    HAL_StatusTypeDef status;

    DCMI->CR &= ~(DCMI_CR_CAPTURE | DCMI_CR_ENABLE);
    DCMI->ICR = AD9226_DCMI_CLEAR_FLAGS;
    AD9226_SetSync(GPIO_PIN_SET);

    SCB_CleanInvalidateDCache_by_Addr(s_dcmi_buffer,
                                      (int32_t)sizeof(s_dcmi_buffer));
    status = HAL_DMA_Start(&s_dcmi_dma,
                           (uint32_t)&DCMI->DR,
                           (uint32_t)s_dcmi_buffer,
                           AD9226_DCMI_WORDS);
    if (status != HAL_OK)
    {
        return status;
    }

    DCMI->CR |= DCMI_CR_ENABLE | DCMI_CR_CAPTURE;
    __DSB();
    AD9226_WaitClockPeriods(4UL);
    AD9226_SetSync(GPIO_PIN_RESET);

    status = HAL_DMA_PollForTransfer(&s_dcmi_dma,
                                     HAL_DMA_FULL_TRANSFER,
                                     AD9226_CAPTURE_TIMEOUT_MS);

    DCMI->CR &= ~(DCMI_CR_CAPTURE | DCMI_CR_ENABLE);
    AD9226_SetSync(GPIO_PIN_SET);

    if (status != HAL_OK)
    {
        (void)HAL_DMA_Abort(&s_dcmi_dma);
        return status;
    }

    SCB_InvalidateDCache_by_Addr(s_dcmi_buffer,
                                 (int32_t)sizeof(s_dcmi_buffer));
    for (uint32_t i = 0UL; i < AD9226_DCMI_WORDS; i++)
    {
        uint32_t packed = s_dcmi_buffer[i];

        s_capture_buffer[2UL * i] = (uint16_t)(packed & 0x0FFFU);
        s_capture_buffer[(2UL * i) + 1UL] =
            (uint16_t)((packed >> 16U) & 0x0FFFU);
    }

    return HAL_OK;
}

const uint16_t *AD9226_GetCaptureBuffer(void)
{
    return s_capture_buffer;
}

uint32_t AD9226_GetCaptureLength(void)
{
    return AD9226_CAPTURE_SAMPLES;
}
