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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sensor.h"
#include "control.h"
#include "log.h"
#include "ControlCore.h"
#include "test.h"
#include "crc8.h"
#include "CanRX.h"
#include "Watchdog.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
TaskHandle_t SensorTaskHandle;
TaskHandle_t ControlTaskHandle;
TaskHandle_t LogTaskHandle;
TaskHandle_t CanTXTaskHandle;
TaskHandle_t CanRXTaskHandle;
TaskHandle_t WatchdogTaskHandle;

QueueHandle_t Queue_Sensor_TX_t;      //传感器数据->CAN总线
QueueHandle_t Queue_Sensor_CAN_t;	  //CAN总线处理后的传感器数据
QueueHandle_t Queue_Sensor_Control_t; //传感器数据->控制器
QueueHandle_t Queue_Control_TX_t;     //控制器->CAN总线
QueueHandle_t Queue_UART_cmd_t;       //串口接收数据
QueueHandle_t Queue_CAN_RX_t;       // CAN中断 → CanRX任务
QueueHandle_t Queue_Control_Cmd_t;  // UART/CAN → Control任务

uint8_t receive_byte = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_IWDG_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <stdio.h>
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_IWDG_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  __HAL_DBGMCU_FREEZE_IWDG();

  //test
  printf("\r\n=================================\r\n");
  printf("       System Booting...         \r\n");
  printf("=================================\r\n");

  // 1. 检查是否为独立看门狗 (IWDG) 复位
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
      printf("[RST_CAUSE] FATAL: IWDG Reset! (Watchdog Bite)\r\n");
  }
  // 2. 检查是否为上电/掉电复位 (POR/PDR)
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
  {
      printf("[RST_CAUSE] Normal: Power-On Reset\r\n");
  }
  // 3. 检查是否为引脚复位 (按下了板子上的 RST 键，或 ST-LINK 烧录复位)
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
  {
      printf("[RST_CAUSE] Normal: Pin Reset (NRST)\r\n");
  }
  // 4. 检查是否为软件复位 (代码中调用了 NVIC_SystemReset)
  else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
  {
      printf("[RST_CAUSE] Software Reset\r\n");
  }
  else
  {
      printf("[RST_CAUSE] Other/Unknown Reset\r\n");
  }
  __HAL_RCC_CLEAR_RESET_FLAGS();

  Test_CRC8_J1850();

  Queue_Sensor_TX_t = xQueueCreate(1, sizeof(Sensor_Sample_t));
  Queue_Sensor_Control_t = xQueueCreate(1, sizeof(Sensor_Sample_t));
  Queue_Sensor_CAN_t = xQueueCreate(1, sizeof(Can_Frame_t));
  Queue_Control_TX_t = xQueueCreate(1, sizeof(ControlContext_t));
  Queue_UART_cmd_t = xQueueCreate(1, sizeof(ControlCommand_t));
  Queue_CAN_RX_t = xQueueCreate(16, sizeof(Can_Frame_t));
  Queue_Control_Cmd_t = xQueueCreate(4, sizeof(ControlCommand_t));

  if (Queue_Sensor_TX_t == NULL || Queue_Sensor_Control_t == NULL ||
        Queue_Sensor_CAN_t == NULL || Queue_Control_TX_t == NULL || Queue_UART_cmd_t == NULL)
  {
      printf("[FATAL] Queue create failed!\r\n");
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET); // 强制关断
      while(1);
  }

  if (xTaskCreate(Sensor_Task, "Sensor", 128, NULL, 2, &SensorTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }
  if (xTaskCreate(Log_Task, "Log", 256, NULL, 3, &LogTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }
  if (xTaskCreate(Control_Task, "Control", 128, NULL, 3, &ControlTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }
  if (xTaskCreate(CanTX_Task, "CanTX", 128, NULL, 2, &CanTXTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }
  if (xTaskCreate(Watchdog_Task, "Watchdog", 256, NULL, 4, &WatchdogTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }
  if (xTaskCreate(CanRX_Task, "CanRX", 256, NULL, 3, &CanRXTaskHandle) != pdPASS)
  {
      HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
      while(1); // 初始化失败，直接卡死
  }

  HAL_UART_Receive_IT(&huart1, &receive_byte, sizeof(uint8_t));

  vTaskStartScheduler();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 6;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_11TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  CAN_FilterTypeDef sFilterConfig;
  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

  sFilterConfig.FilterIdHigh = 0x200 << 5;
  sFilterConfig.FilterIdLow = 0x0000;

  sFilterConfig.FilterMaskIdHigh = 0x7FF << 5;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  if(HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK) {
      Error_Handler();
  }
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(
      &hcan1,
      CAN_IT_TX_MAILBOX_EMPTY |
      CAN_IT_RX_FIFO0_MSG_PENDING |
      CAN_IT_ERROR_WARNING |
      CAN_IT_ERROR_PASSIVE |
      CAN_IT_BUSOFF |
      CAN_IT_LAST_ERROR_CODE |
      CAN_IT_ERROR);
  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = 4000;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Control_Pin */
  GPIO_InitStruct.Pin = LED_Control_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_Control_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // 1. 设置故障 LED 状态 (假设高电平点亮红灯报警)
    HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);

    // 2. 彻底关闭中断，停止系统调度
    __disable_irq();

    // 3. 死循环卡住，停止喂狗，等待硬件看门狗复位
    while(1)
    {
    }
}

// Malloc 失败钩子
void vApplicationMallocFailedHook(void)
{
    HAL_GPIO_WritePin(LED_Control_GPIO_Port, LED_Control_Pin, GPIO_PIN_RESET);
    __disable_irq();
    while(1)
    {
    }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
