#include "G_DIGITAL_MODEL_FML.h"

#include "G_IIR_MODEL_BLL.h"
#include "adc.h"
#include "dac.h"
#include "tim.h"

#define G_DIGITAL_MODEL_ADC_MID_CODE  32768.0f
#define G_DIGITAL_MODEL_DAC_MID_CODE  2048.0f
#define G_DIGITAL_MODEL_ADC_TO_DAC    (1.0f / 16.0f)
#define G_DIGITAL_MODEL_DAC_MAX_CODE  4095.0f

extern DMA_HandleTypeDef hdma_adc1;

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t g_adc_buffer[G_DIGITAL_MODEL_BUFFER_LENGTH];

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t g_dac_buffer[G_DIGITAL_MODEL_BUFFER_LENGTH];

static float32_t g_iir_input[G_DIGITAL_MODEL_BUFFER_LENGTH / 2U];
static float32_t g_iir_output[G_DIGITAL_MODEL_BUFFER_LENGTH / 2U];
static volatile bool g_model_running = false;
static volatile bool g_dac_started = false;
static volatile uint32_t g_processed_blocks = 0U;
static volatile uint32_t g_error_count = 0U;

static float g_digital_model_clampf(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static uint32_t g_digital_model_get_timer_clock_hz(TIM_HandleTypeDef *htim)
{
    RCC_ClkInitTypeDef clock = {0};
    uint32_t flash_latency = 0U;
    uint32_t timer_clock_hz;

    HAL_RCC_GetClockConfig(&clock, &flash_latency);
    if ((htim->Instance == TIM1) || (htim->Instance == TIM8) ||
        (htim->Instance == TIM15) || (htim->Instance == TIM16) ||
        (htim->Instance == TIM17))
    {
        timer_clock_hz = HAL_RCC_GetPCLK2Freq();
        if (clock.APB2CLKDivider != RCC_HCLK_DIV1)
        {
            timer_clock_hz *= 2U;
        }
    }
    else
    {
        timer_clock_hz = HAL_RCC_GetPCLK1Freq();
        if (clock.APB1CLKDivider != RCC_HCLK_DIV1)
        {
            timer_clock_hz *= 2U;
        }
    }
    return timer_clock_hz;
}

static HAL_StatusTypeDef g_digital_model_set_sample_rate(TIM_HandleTypeDef *htim)
{
    uint32_t timer_clock_hz = g_digital_model_get_timer_clock_hz(htim);
    uint32_t divider = (timer_clock_hz +
                        (G_IIR_MODEL_SAMPLE_RATE_HZ / 2U)) /
                       G_IIR_MODEL_SAMPLE_RATE_HZ;

    if ((divider == 0U) || (divider > 65536U))
    {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_PRESCALER(htim, 0U);
    __HAL_TIM_SET_AUTORELOAD(htim, divider - 1U);
    __HAL_TIM_SET_COUNTER(htim, 0U);
    htim->Instance->EGR = TIM_EGR_UG;
    return HAL_OK;
}

static void g_digital_model_process_block(uint32_t offset)
{
    uint32_t index;
    const uint32_t block_length = G_DIGITAL_MODEL_BUFFER_LENGTH / 2U;
    uint32_t *adc_block = &g_adc_buffer[offset];
    uint32_t *dac_block = &g_dac_buffer[offset];

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_InvalidateDCache_by_Addr(adc_block,
                                    (int32_t)(block_length * sizeof(uint32_t)));
    }

    for (index = 0U; index < block_length; index++)
    {
        g_iir_input[index] = (float32_t)(adc_block[index] & 0xFFFFU) -
                             G_DIGITAL_MODEL_ADC_MID_CODE;
    }

    G_IIR_ModelBLL_Process(g_iir_input, g_iir_output, block_length);

    for (index = 0U; index < block_length; index++)
    {
        float dac_code = G_DIGITAL_MODEL_DAC_MID_CODE +
                         (g_iir_output[index] * G_DIGITAL_MODEL_ADC_TO_DAC);

        dac_code = g_digital_model_clampf(dac_code,
                                          0.0f,
                                          G_DIGITAL_MODEL_DAC_MAX_CODE);
        dac_block[index] = (uint32_t)(dac_code + 0.5f);
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanDCache_by_Addr(dac_block,
                                (int32_t)(block_length * sizeof(uint32_t)));
    }
    g_processed_blocks++;
}

HAL_StatusTypeDef G_DigitalModelFML_Start(void)
{
    ADC_MultiModeTypeDef multimode = {0};
    HAL_StatusTypeDef status;
    uint32_t index;

    if (g_model_running)
    {
        return HAL_OK;
    }

    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_TIM_Base_Stop(&htim4);
    (void)HAL_ADC_Stop_DMA(&hadc1);
    (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);

    status = g_digital_model_set_sample_rate(&htim1);
    if (status != HAL_OK)
    {
        return status;
    }
    status = g_digital_model_set_sample_rate(&htim4);
    if (status != HAL_OK)
    {
        return status;
    }

    multimode.Mode = ADC_MODE_INDEPENDENT;
    status = HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);
    if (status != HAL_OK)
    {
        return status;
    }

    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    status = HAL_ADC_Init(&hadc1);
    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_DMA_DeInit(&hdma_adc1);
    if (status != HAL_OK)
    {
        return status;
    }
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    status = HAL_DMA_Init(&hdma_adc1);
    if (status != HAL_OK)
    {
        return status;
    }
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    status = HAL_ADCEx_Calibration_Start(&hadc1,
                                         ADC_CALIB_OFFSET_LINEARITY,
                                         ADC_SINGLE_ENDED);
    if (status != HAL_OK)
    {
        return status;
    }

    G_IIR_ModelBLL_Init();
    for (index = 0U; index < G_DIGITAL_MODEL_BUFFER_LENGTH; index++)
    {
        g_adc_buffer[index] = (uint32_t)G_DIGITAL_MODEL_ADC_MID_CODE;
        g_dac_buffer[index] = (uint32_t)G_DIGITAL_MODEL_DAC_MID_CODE;
    }

    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
    {
        SCB_CleanInvalidateDCache_by_Addr(g_adc_buffer,
                                          (int32_t)sizeof(g_adc_buffer));
        SCB_CleanDCache_by_Addr(g_dac_buffer,
                                (int32_t)sizeof(g_dac_buffer));
    }

    g_processed_blocks = 0U;
    g_error_count = 0U;
    g_dac_started = false;
    g_model_running = true;

    status = HAL_ADC_Start_DMA(&hadc1,
                               g_adc_buffer,
                               G_DIGITAL_MODEL_BUFFER_LENGTH);
    if (status != HAL_OK)
    {
        g_model_running = false;
        return status;
    }

    status = HAL_TIM_Base_Start(&htim1);
    if (status != HAL_OK)
    {
        (void)HAL_ADC_Stop_DMA(&hadc1);
        g_model_running = false;
    }
    return status;
}

HAL_StatusTypeDef G_DigitalModelFML_Stop(void)
{
    HAL_StatusTypeDef adc_status;
    HAL_StatusTypeDef dac_status = HAL_OK;

    g_model_running = false;
    (void)HAL_TIM_Base_Stop(&htim1);
    (void)HAL_TIM_Base_Stop(&htim4);
    adc_status = HAL_ADC_Stop_DMA(&hadc1);
    if (g_dac_started)
    {
        dac_status = HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    }
    g_dac_started = false;
    return (adc_status == HAL_OK) ? dac_status : adc_status;
}

bool G_DigitalModelFML_IsRunning(void)
{
    return g_model_running;
}

void G_DigitalModelFML_ADCProcess(bool second_half)
{
    uint32_t offset;

    if (!g_model_running)
    {
        return;
    }

    offset = second_half ? (G_DIGITAL_MODEL_BUFFER_LENGTH / 2U) : 0U;
    g_digital_model_process_block(offset);

    /* Start the DAC only after its first half-buffer contains valid data. */
    if (!g_dac_started)
    {
        if ((HAL_DAC_Start_DMA(&hdac1,
                               DAC_CHANNEL_1,
                               g_dac_buffer,
                               G_DIGITAL_MODEL_BUFFER_LENGTH,
                               DAC_ALIGN_12B_R) == HAL_OK) &&
            (HAL_TIM_Base_Start(&htim4) == HAL_OK))
        {
            g_dac_started = true;
        }
        else
        {
            g_error_count++;
        }
    }
}

void G_DigitalModelFML_ADCError(void)
{
    g_error_count++;
}

uint32_t G_DigitalModelFML_GetProcessedBlockCount(void)
{
    return g_processed_blocks;
}

uint32_t G_DigitalModelFML_GetErrorCount(void)
{
    return g_error_count;
}
