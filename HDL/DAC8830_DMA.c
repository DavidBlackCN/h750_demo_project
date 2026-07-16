#include "DAC8830_DMA.h"

#include "DAC8830_SPI.h"
#include "spi.h"
#include "tim.h"

#include <stdint.h>

#define DAC8830_DMA_CLEAR_SPI_FLAGS (SPI_IFCR_EOTC | SPI_IFCR_TXTFC | \
                                     SPI_IFCR_UDRC | SPI_IFCR_OVRC | \
                                     SPI_IFCR_TIFREC | SPI_IFCR_MODFC | \
                                     SPI_IFCR_TSERFC | SPI_IFCR_SUSPC)

#define DAC8830_DMA_MIN_TIMER_TICKS (12U)
#define DAC8830_DMA_TX_TICK         (6U)

static DMA_HandleTypeDef hdmaDac8830CsLow;
static DMA_HandleTypeDef hdmaDac8830SpiTx;
static DMA_HandleTypeDef hdmaDac8830CsHigh;

static uint32_t gDac8830CsLowWord
    __attribute__((section(".dma_buffer"), aligned(32)));
static uint32_t gDac8830CsHighWord
    __attribute__((section(".dma_buffer"), aligned(32)));
static HAL_StatusTypeDef gDac8830DmaLastStatus = HAL_OK;
static uint32_t gDac8830DmaErrorCount = 0U;

static void dac8830_dma_clean_dcache(const void *addr, uint32_t size)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    const uintptr_t start = (uintptr_t)addr & ~(uintptr_t)31U;
    const uintptr_t end = ((uintptr_t)addr + (uintptr_t)size + 31U) &
                          ~(uintptr_t)31U;

    SCB_CleanDCache_by_Addr((uint32_t *)start, (int32_t)(end - start));
#else
    (void)addr;
    (void)size;
#endif
}

static uint32_t dac8830_tim4_clock_hz(void)
{
    uint32_t timerClock = HAL_RCC_GetPCLK1Freq();
    const uint32_t d2cfgr = RCC->D2CFGR & RCC_D2CFGR_D2PPRE1;

    if (d2cfgr != RCC_HCLK_DIV1)
    {
        timerClock *= 2U;
    }

    return timerClock;
}

static HAL_StatusTypeDef dac8830_dma_init_stream(DMA_HandleTypeDef *hdma,
                                                 DMA_Stream_TypeDef *instance,
                                                 uint32_t request,
                                                 uint32_t memInc,
                                                 uint32_t periphAlign,
                                                 uint32_t memAlign)
{
    hdma->Instance = instance;
    hdma->Init.Request = request;
    hdma->Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = memInc;
    hdma->Init.PeriphDataAlignment = periphAlign;
    hdma->Init.MemDataAlignment = memAlign;
    hdma->Init.Mode = DMA_CIRCULAR;
    hdma->Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    (void)HAL_DMA_DeInit(hdma);
    return HAL_DMA_Init(hdma);
}

static HAL_StatusTypeDef dac8830_dma_configure_streams(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    if (dac8830_dma_init_stream(&hdmaDac8830CsLow, DMA1_Stream2,
                                DMA_REQUEST_TIM4_UP, DMA_MINC_DISABLE,
                                DMA_PDATAALIGN_WORD,
                                DMA_MDATAALIGN_WORD) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (dac8830_dma_init_stream(&hdmaDac8830SpiTx, DMA1_Stream3,
                                DMA_REQUEST_TIM4_CH1, DMA_MINC_ENABLE,
                                DMA_PDATAALIGN_HALFWORD,
                                DMA_MDATAALIGN_HALFWORD) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (dac8830_dma_init_stream(&hdmaDac8830CsHigh, DMA1_Stream4,
                                DMA_REQUEST_TIM4_CH2, DMA_MINC_DISABLE,
                                DMA_PDATAALIGN_WORD,
                                DMA_MDATAALIGN_WORD) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef dac8830_dma_configure_timer(uint32_t sampleRateHz)
{
    const uint32_t timerClock = dac8830_tim4_clock_hz();
    const uint32_t periodTicks = timerClock / sampleRateHz;
    uint32_t csHighTick;

    if ((sampleRateHz == 0U) ||
        (periodTicks < DAC8830_DMA_MIN_TIMER_TICKS) ||
        ((timerClock % sampleRateHz) != 0U))
    {
        return HAL_ERROR;
    }

    csHighTick = periodTicks - 1U;
    if (csHighTick <= DAC8830_DMA_TX_TICK)
    {
        return HAL_ERROR;
    }

    TIM4->CR1 &= ~TIM_CR1_CEN;
    TIM4->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE);
    TIM4->SR = 0U;

    TIM4->CR2 &= ~TIM_CR2_CCDS;
    TIM4->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S);
    TIM4->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC2E);
    TIM4->PSC = 0U;
    TIM4->ARR = periodTicks - 1U;
    TIM4->CCR1 = DAC8830_DMA_TX_TICK;
    TIM4->CCR2 = csHighTick;
    TIM4->CNT = 0U;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->SR = 0U;

    return HAL_OK;
}

static void dac8830_dma_prepare_spi(void)
{
    SPI_TypeDef *spi = hspi1.Instance;

    spi->CR1 &= ~SPI_CR1_SPE;
    spi->IFCR = DAC8830_DMA_CLEAR_SPI_FLAGS;
    spi->CFG1 &= ~SPI_CFG1_TXDMAEN;
    MODIFY_REG(spi->CR2, SPI_CR2_TSIZE, 0U);
    spi->CR1 |= SPI_CR1_SPE;
    spi->CR1 |= SPI_CR1_CSTART;
}

static void dac8830_dma_fail(void)
{
    gDac8830DmaLastStatus = HAL_ERROR;
    gDac8830DmaErrorCount++;
    DAC8830_DMA_Stop();
}

HAL_StatusTypeDef DAC8830_DMA_StartWaveform(const uint16_t *codes,
                                            uint16_t length,
                                            uint32_t sampleRateHz)
{
    if ((codes == NULL) || (length == 0U))
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    DAC8830_DMA_Stop();

    if (dac8830_dma_configure_streams() != HAL_OK)
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    if (dac8830_dma_configure_timer(sampleRateHz) != HAL_OK)
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    gDac8830CsLowWord =
        ((uint32_t)(DAC8830_CS1_Pin | DAC8830_CS2_Pin) << 16U);
    gDac8830CsHighWord = (uint32_t)(DAC8830_CS1_Pin | DAC8830_CS2_Pin);
    dac8830_dma_clean_dcache(&gDac8830CsLowWord, sizeof(gDac8830CsLowWord));
    dac8830_dma_clean_dcache(&gDac8830CsHighWord, sizeof(gDac8830CsHighWord));
    dac8830_dma_clean_dcache(codes, (uint32_t)length * sizeof(uint16_t));

    GPIOE->BSRR = gDac8830CsHighWord;

    if (HAL_DMA_Start(&hdmaDac8830CsLow, (uint32_t)&gDac8830CsLowWord,
                      (uint32_t)&GPIOE->BSRR, 1U) != HAL_OK)
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    if (HAL_DMA_Start(&hdmaDac8830SpiTx, (uint32_t)codes,
                      (uint32_t)&SPI1->TXDR, length) != HAL_OK)
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    if (HAL_DMA_Start(&hdmaDac8830CsHigh, (uint32_t)&gDac8830CsHighWord,
                      (uint32_t)&GPIOE->BSRR, 1U) != HAL_OK)
    {
        dac8830_dma_fail();
        return HAL_ERROR;
    }

    dac8830_dma_prepare_spi();

    TIM4->DIER |= TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE;
    TIM4->EGR = TIM_EGR_UG;
    TIM4->CR1 |= TIM_CR1_CEN;

    gDac8830DmaLastStatus = HAL_OK;
    return HAL_OK;
}

void DAC8830_DMA_Stop(void)
{
    TIM4->CR1 &= ~TIM_CR1_CEN;
    TIM4->DIER &= ~(TIM_DIER_UDE | TIM_DIER_CC1DE | TIM_DIER_CC2DE);

    (void)HAL_DMA_Abort(&hdmaDac8830CsLow);
    (void)HAL_DMA_Abort(&hdmaDac8830SpiTx);
    (void)HAL_DMA_Abort(&hdmaDac8830CsHigh);

    hspi1.Instance->CR1 &= ~SPI_CR1_SPE;
    hspi1.Instance->IFCR = DAC8830_DMA_CLEAR_SPI_FLAGS;

    DAC8830_CS1_GPIO_Port->BSRR = (uint32_t)DAC8830_CS1_Pin;
    DAC8830_CS2_GPIO_Port->BSRR = (uint32_t)DAC8830_CS2_Pin;
}

HAL_StatusTypeDef DAC8830_DMA_GetLastStatus(void)
{
    return gDac8830DmaLastStatus;
}

uint32_t DAC8830_DMA_GetErrorCount(void)
{
    return gDac8830DmaErrorCount;
}
