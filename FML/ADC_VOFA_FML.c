#include "ADC_VOFA_FML.h"

#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>

#define ADC_VOFA_SAMPLE_RATE_HZ   1000000U
#define ADC_VOFA_DMA_SAMPLE_COUNT 1024U
#define ADC_VOFA_TX_BUFFER_SIZE   17408U
#define ADC_VOFA_CHANNEL_COUNT    1U

typedef enum
{
    ADC_VOFA_ERROR_NONE = 0,
    ADC_VOFA_ERROR_ADC,
    ADC_VOFA_ERROR_TRANSMIT,
    ADC_VOFA_ERROR_START
} adc_vofa_error_source_t;

/* ADC3 DMA writes this one-shot frame into cache-line-aligned RAM_D2. */
static uint16_t s_adc3_buffer[ADC_VOFA_DMA_SAMPLE_COUNT]
    __attribute__((section(".dma_buffer"), aligned(32)));
static char s_tx_buffer[ADC_VOFA_TX_BUFFER_SIZE];
static volatile bool s_active;
static volatile bool s_capture_ready;
static volatile bool s_capture_error;
static volatile uint32_t s_error_count;
static volatile uint32_t s_adc_error_code;
static volatile uint32_t s_dma_error_code;
static volatile adc_vofa_error_source_t s_error_source;
static uint8_t s_channel_index;
static bool s_transmitted;

static float adc_vofa_full_scale_code(const ADC_HandleTypeDef *hadc)
{
    switch (hadc->Init.Resolution)
    {
    case ADC_RESOLUTION_8B:
        return 255.0f;
    case ADC_RESOLUTION_10B:
        return 1023.0f;
    case ADC_RESOLUTION_12B:
        return 4095.0f;
    case ADC_RESOLUTION_14B:
        return 16383.0f;
    case ADC_RESOLUTION_16B:
        return 65535.0f;
    default:
        return 0.0f;
    }
}

static ADC_HandleTypeDef *adc_vofa_current_adc(void)
{
    static ADC_HandleTypeDef *const adc_list[] = {&hadc3};

    return adc_list[s_channel_index];
}

static uint16_t *adc_vofa_current_buffer(void)
{
    static uint16_t *const buffer_list[] = {s_adc3_buffer};

    return buffer_list[s_channel_index];
}

static const char *adc_vofa_current_name(void)
{
    static const char *const name_list[] = {"adc3_pc2"};

    return name_list[s_channel_index];
}

static void adc_vofa_send_status(float status)
{
    const int length = snprintf(s_tx_buffer, sizeof(s_tx_buffer),
                                "adc_port_status:%.0f\r\n", (double)status);

    if (length > 0)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)s_tx_buffer, (uint16_t)length, 250U);
    }
}

static void adc_vofa_send_error(void)
{
    const int length = snprintf(s_tx_buffer, sizeof(s_tx_buffer),
                                "adc_error:%lu\r\ndma_error:%lu\r\n",
                                (unsigned long)s_adc_error_code,
                                (unsigned long)s_dma_error_code);

    if (length > 0)
    {
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)s_tx_buffer, (uint16_t)length, 250U);
    }
}

static HAL_StatusTypeDef adc_vofa_start_current_channel(void)
{
    ADC_HandleTypeDef *const hadc = adc_vofa_current_adc();
    uint16_t *const buffer = adc_vofa_current_buffer();

    /* DMA 写入前丢弃 CPU 可能保留的旧缓存行，避免脏缓存覆盖 DMA 数据。 */
    s_capture_ready = false;
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)buffer, ADC_VOFA_DMA_SAMPLE_COUNT * sizeof(buffer[0]));

    /* 先挂接 normal DMA，再开启 TIM1；首个 TRGO 才会开始 ADC 转换。 */
    if (HAL_ADC_Start_DMA(hadc, (uint32_t *)buffer, ADC_VOFA_DMA_SAMPLE_COUNT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    s_active = true;
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK)
    {
        s_active = false;
        (void)HAL_ADC_Stop_DMA(hadc);
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef adc_vofa_send_channel(const char *name,
                                                const uint16_t *buffer,
                                                float full_scale_code)
{
    size_t length = 0U;

    if (full_scale_code <= 0.0f)
    {
        return HAL_ERROR;
    }

    /* 将整帧原始 ADC 码按实际分辨率换算为 VOFA+ FireWater 文本。 */
    for (uint32_t index = 0U; index < ADC_VOFA_DMA_SAMPLE_COUNT; ++index)
    {
        const float voltage = (3.3f * (float)buffer[index]) / full_scale_code;
        const int written = snprintf(&s_tx_buffer[length], ADC_VOFA_TX_BUFFER_SIZE - length,
                                     "%s:%.3f\r\n", name, (double)voltage);
        if ((written <= 0) || ((size_t)written >= (ADC_VOFA_TX_BUFFER_SIZE - length)))
        {
            return HAL_ERROR;
        }
        length += (size_t)written;
    }

    return HAL_UART_Transmit(&huart1, (uint8_t *)s_tx_buffer, (uint16_t)length, 250U);
}

/**
 * @brief  配置 TIM1 的更新事件为 ADC 外部触发源。
 * @details 根据当前 APB2 预分频和实际 PCLK2 计算 TIM1 计数时钟；当 APB2
 *          分频不为 1 时，STM32 定时器时钟为 PCLK2 的两倍。函数以 PSC=0、
 *          ARR=divider-1 配置更新频率，结果为最接近
 *          ADC_VOFA_SAMPLE_RATE_HZ 的采样触发频率，并将 TRGO 选择为 update。
 *          本函数只写定时器配置，不启动 TIM1；DMA 挂接完成后由采集启动函数
 *          再启动定时器，避免过早产生 ADC 触发。
 * @retval HAL_OK     TIM1 参数有效且配置完成。
 * @retval HAL_ERROR  所需分频不在 TIM1 16 位 ARR 的合法范围内。
 */
static HAL_StatusTypeDef adc_vofa_configure_trigger_timer(void)
{
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz = HAL_RCC_GetPCLK2Freq();
    uint32_t divider;

    HAL_RCC_GetClockConfig(&clocks, &flash_latency);
    if (clocks.APB2CLKDivider != RCC_APB2_DIV1)
    {
        timer_clock_hz *= 2U;
    }

    /* 用实际 APB2 定时器时钟计算分频，避免将采样率写死为特定系统时钟。 */
    divider = (timer_clock_hz + (ADC_VOFA_SAMPLE_RATE_HZ / 2U)) / ADC_VOFA_SAMPLE_RATE_HZ;
    if ((divider < 2U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    (void)HAL_TIM_Base_Stop(&htim1);
    __HAL_TIM_SET_PRESCALER(&htim1, 0U);
    __HAL_TIM_SET_AUTORELOAD(&htim1, divider - 1U);
    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    MODIFY_REG(htim1.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_UPDATE);
    htim1.Instance->EGR = TIM_EGR_UG;
    return HAL_OK;
}

HAL_StatusTypeDef ADC_VOFA_FML_Start(void)
{
    s_active = false;
    s_capture_ready = false;
    s_capture_error = false;
    s_error_count = 0U;
    s_adc_error_code = 0U;
    s_dma_error_code = 0U;
    s_error_source = ADC_VOFA_ERROR_NONE;
    s_channel_index = 0U;
    s_transmitted = false;

    /* 初始化阶段只启动第一帧；后续是否继续采集由 Process() 决定。 */
    if (adc_vofa_configure_trigger_timer() != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Leave a short observation window after reset before the one-shot frame. */
    HAL_Delay(500U);
    adc_vofa_send_status(1.0f);

    return adc_vofa_start_current_channel();
}

bool ADC_VOFA_FML_IsActive(void)
{
    return s_active;
}

void ADC_VOFA_FML_OnAdcHalfComplete(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    /* The first half is retained; transmission starts only after the full frame. */
}

void ADC_VOFA_FML_OnAdcComplete(ADC_HandleTypeDef *hadc)
{
    if (hadc == adc_vofa_current_adc())
    {
        /* 中断中只切断触发源并置标志；整帧处理和串口发送留给前台。 */
        __HAL_TIM_DISABLE(&htim1);
        s_active = false;
        s_capture_ready = true;
    }
}

void ADC_VOFA_FML_OnAdcError(ADC_HandleTypeDef *hadc)
{
    if (hadc == adc_vofa_current_adc())
    {
        /* 保存诊断信息，避免在 ADC/DMA 中断中执行阻塞串口发送。 */
        s_error_count++;
        s_adc_error_code = HAL_ADC_GetError(hadc);
        s_dma_error_code = (hadc->DMA_Handle != NULL) ? HAL_DMA_GetError(hadc->DMA_Handle) : 0xFFFFFFFFU;
        s_error_source = ADC_VOFA_ERROR_ADC;
        __HAL_TIM_DISABLE(&htim1);
        s_active = false;
        s_capture_error = true;
    }
}

void ADC_VOFA_FML_Process(void)
{
    if (s_capture_error)
    {
        /* 错误处置在前台完成：停止外设并把 HAL 原始错误码输出到串口。 */
        (void)HAL_TIM_Base_Stop(&htim1);
        (void)HAL_ADC_Stop_DMA(adc_vofa_current_adc());
        if (s_error_source == ADC_VOFA_ERROR_ADC)
        {
            adc_vofa_send_status(-1.0f - (float)s_channel_index);
            adc_vofa_send_error();
        }
        else if (s_error_source == ADC_VOFA_ERROR_TRANSMIT)
        {
            adc_vofa_send_status(-10.0f - (float)s_channel_index);
        }
        else
        {
            adc_vofa_send_status(-20.0f - (float)s_channel_index);
        }
        s_capture_error = false;
        s_transmitted = true;
        return;
    }

    if ((!s_capture_ready) || s_transmitted)
    {
        /* DMA 尚未完成，或这一帧已发送：保持主循环的低开销轮询。 */
        return;
    }

    s_capture_ready = false;
    /*
     * DMA 已写完 1024 点。停止 ADC/DMA 后再失效缓存，确保 CPU 从 RAM_D2
     * 读取 DMA 的最新数据；随后在前台一次性打包并发送整帧。
     */
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_ADC_Stop_DMA(adc_vofa_current_adc());
    SCB_InvalidateDCache_by_Addr((uint32_t *)adc_vofa_current_buffer(),
                                 ADC_VOFA_DMA_SAMPLE_COUNT * sizeof(s_adc3_buffer[0]));
    if (adc_vofa_send_channel(adc_vofa_current_name(), adc_vofa_current_buffer(),
                              adc_vofa_full_scale_code(adc_vofa_current_adc())) != HAL_OK)
    {
        s_error_source = ADC_VOFA_ERROR_TRANSMIT;
        s_capture_error = true;
        return;
    }

    adc_vofa_send_status(10.0f + (float)s_channel_index);

    s_channel_index++;
    if (s_channel_index >= ADC_VOFA_CHANNEL_COUNT)
    {
        /* 当前预设只有 ADC3 一路：一帧发送完成后永久停止。 */
        s_transmitted = true;
        return;
    }

    if (adc_vofa_start_current_channel() != HAL_OK)
    {
        s_error_source = ADC_VOFA_ERROR_START;
        s_capture_error = true;
    }
}
