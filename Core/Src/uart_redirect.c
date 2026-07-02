/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart_redirect.c
  * @brief   Redirect standard I/O to USART1.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "usart.h"

/*
 * printf() 最终会经过 syscalls.c 里的 _write()，而 _write() 会调用
 * __io_putchar()。这里提供一个强定义，把单字符输出转发到 USART1。
 *
 * 注意：
 *   1. 必须在 MX_USART1_UART_Init() 之后再调用 printf()。
 *   2. 这里不自动把 '\n' 转成 "\r\n"，避免用户已经写 "\r\n" 时出现重复回车。
 *   3. 这是阻塞式发送，适合调试日志；高频大数据输出仍建议使用 VOFA_Debug_SendFrame()
 *      或后续改成 UART DMA。
 */
int __io_putchar(int ch)
{
  uint8_t data = (uint8_t)ch;

  if (HAL_UART_Transmit(&huart1, &data, 1U, HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return ch;
}

/*
 * scanf()/getchar() 会经过 syscalls.c 里的 _read()，再调用 __io_getchar()。
 * 这里也重定向到 USART1 RX。这个函数是阻塞式接收，如果没有数据会一直等待。
 */
int __io_getchar(void)
{
  uint8_t data = 0U;

  if (HAL_UART_Receive(&huart1, &data, 1U, HAL_MAX_DELAY) != HAL_OK)
  {
    return -1;
  }

  return (int)data;
}
