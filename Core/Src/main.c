/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "AD9910_API.h"
#include "G_BASIC_API.h"
#include <stdio.h>

#include "G_CONSOLE_API.h"
#include "G_DIGITAL_MODEL_API.h"
#include "TJC_HMI_API.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define G_APP_SINGLE_TONE_TEST       0U
#define G_APP_TEST_FREQUENCY_HZ      1000U
#define G_APP_TEST_OUTPUT_MVPP       200U
#define G_APP_REQUIREMENT4_CALIBRATION_TEST  0U
#define G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE  0U
#define G_APP_REQUIREMENT4_CALIBRATION_MAILBOX  0U
#define G_APP_CALIBRATION_FREQUENCY_HZ       1000U
#define G_APP_CALIBRATION_TARGET_TENTHS_VPP    20U
#define G_APP_CALIBRATION_START_DELAY_MS      6000U
#define G_APP_CALIBRATION_DWELL_MS            5000U

#if (!G_APP_SINGLE_TONE_TEST && !G_APP_REQUIREMENT4_CALIBRATION_TEST && \
     !G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE && \
     !G_APP_REQUIREMENT4_CALIBRATION_MAILBOX)
#define G_APP_SELECTED_REQUIREMENT  G_CONSOLE_API_REQUIREMENT_ALL
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

#if (G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE || \
     G_APP_REQUIREMENT4_CALIBRATION_MAILBOX)
static const uint8_t g_app_calibration_targets_tenths_vpp[] =
{
  10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U
};

static uint8_t g_app_calibration_target_index;
static uint32_t g_app_calibration_frequency_hz = 100U;
static uint32_t g_app_calibration_last_tick;
static bool g_app_calibration_started;
static bool g_app_calibration_finished;

volatile uint32_t g_app_calibration_mailbox_frequency_hz = 100U;
volatile uint32_t g_app_calibration_mailbox_target_tenths_vpp = 10U;
volatile uint32_t g_app_calibration_mailbox_generation;
static uint32_t g_app_calibration_mailbox_applied_generation = UINT32_MAX;
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if (G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE || \
     G_APP_REQUIREMENT4_CALIBRATION_MAILBOX)
static void G_AppCalibrationSequence_StartCurrentPoint(void)
{
  if (G_BasicAPI_StartRequirement4(
          g_app_calibration_frequency_hz,
          g_app_calibration_targets_tenths_vpp[
              g_app_calibration_target_index]) != HAL_OK)
  {
    Error_Handler();
  }
}

static void G_AppCalibrationSequence_Process(void)
{
  uint32_t now = HAL_GetTick();

  if (g_app_calibration_finished)
  {
    return;
  }

  if (!g_app_calibration_started)
  {
    if (now < G_APP_CALIBRATION_START_DELAY_MS)
    {
      return;
    }

    g_app_calibration_started = true;
    g_app_calibration_last_tick = now;
    G_AppCalibrationSequence_StartCurrentPoint();
    return;
  }

  if ((now - g_app_calibration_last_tick) < G_APP_CALIBRATION_DWELL_MS)
  {
    return;
  }

  g_app_calibration_last_tick = now;
  g_app_calibration_target_index++;
  if (g_app_calibration_target_index >=
      (sizeof(g_app_calibration_targets_tenths_vpp) /
       sizeof(g_app_calibration_targets_tenths_vpp[0])))
  {
    g_app_calibration_target_index = 0U;
    g_app_calibration_frequency_hz += 100U;
    if (g_app_calibration_frequency_hz > 3000U)
    {
      g_app_calibration_finished = true;
      return;
    }
  }

  G_AppCalibrationSequence_StartCurrentPoint();
}

static void G_AppCalibrationMailbox_Process(void)
{
  uint32_t generation = g_app_calibration_mailbox_generation;

  if (generation == g_app_calibration_mailbox_applied_generation)
  {
    return;
  }

  if ((g_app_calibration_mailbox_target_tenths_vpp > UINT8_MAX) ||
      (G_BasicAPI_StartRequirement4(
          g_app_calibration_mailbox_frequency_hz,
          (uint8_t)g_app_calibration_mailbox_target_tenths_vpp) != HAL_OK))
  {
    Error_Handler();
  }

  g_app_calibration_mailbox_applied_generation = generation;
}
#endif
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  SCB_EnableICache();
  SCB_EnableDCache();

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
#if (G_APP_SINGLE_TONE_TEST || G_APP_REQUIREMENT4_CALIBRATION_TEST || \
     G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE || \
     G_APP_REQUIREMENT4_CALIBRATION_MAILBOX)
  MX_USART1_UART_Init();
#else
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
#endif
  /* USER CODE BEGIN 2 */

#if G_APP_SINGLE_TONE_TEST
  AD9910_output_sine(G_APP_TEST_FREQUENCY_HZ,
                     G_APP_TEST_OUTPUT_MVPP);
  printf("TEST sine=%luHz output=%lumVpp\r\n",
         (unsigned long)G_APP_TEST_FREQUENCY_HZ,
         (unsigned long)G_APP_TEST_OUTPUT_MVPP);
#elif G_APP_REQUIREMENT4_CALIBRATION_TEST
  if (G_BasicAPI_StartRequirement4(G_APP_CALIBRATION_FREQUENCY_HZ,
                                   G_APP_CALIBRATION_TARGET_TENTHS_VPP) != HAL_OK)
  {
    Error_Handler();
  }
  printf("CAL R4 f=%luHz target=%.1fVpp\r\n",
         (unsigned long)G_APP_CALIBRATION_FREQUENCY_HZ,
         (float)G_APP_CALIBRATION_TARGET_TENTHS_VPP / 10.0f);
#elif G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE
  printf("CAL R4 sequence 100Hz-3000Hz, 1.0Vpp-2.0Vpp\r\n");
#elif G_APP_REQUIREMENT4_CALIBRATION_MAILBOX
  printf("CAL R4 mailbox ready\r\n");
#else
  if (G_ConsoleAPI_Init(&huart1, G_APP_SELECTED_REQUIREMENT) != HAL_OK)
  {
    Error_Handler();
  }
  if (TJC_HMI_API_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  G_DigitalModelAPI_Init();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if G_APP_REQUIREMENT4_CALIBRATION_SEQUENCE
    G_AppCalibrationSequence_Process();
    __WFI();
#elif G_APP_REQUIREMENT4_CALIBRATION_MAILBOX
    G_AppCalibrationMailbox_Process();
    __WFI();
#elif (G_APP_SINGLE_TONE_TEST || G_APP_REQUIREMENT4_CALIBRATION_TEST)
    __WFI();
#else
    G_ConsoleAPI_Process();
    TJC_HMI_API_Process();
    G_DigitalModelAPI_Process();
#endif
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
