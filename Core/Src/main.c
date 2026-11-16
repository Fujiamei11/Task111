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
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "OLED.h"
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

/* USER CODE BEGIN PV */
uint8_t run_state = 1;
uint8_t dir_state = 0;
uint16_t step_ms  = 300;
uint8_t current_led = 1;
volatile uint8_t key1_event = 0;
volatile uint8_t key2_event = 0;


typedef struct __attribute__((__packed__))
{
  uint8_t header;
  uint8_t run_flag;
  uint8_t dir_flag;
  uint8_t step_l;
  uint8_t step_h;
  uint8_t checksum;
}UartPacket;
UartPacket rx_packet;
volatile uint8_t packet_ready = 0;

static  char tx_buffer[100];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,uint16_t Size)
{
  if (huart->Instance == USART2)
  {
    if (Size == 6)
    {
      packet_ready = 1;
    }
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2,(uint8_t*)&rx_packet,6);//使得有多次用串口输入控制流水灯的机会
    __HAL_UART_DISABLE_IT(&huart2,UART_IT_RXNE);//关闭过半传输完成中断
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == Key1_Pin)
  {
    key1_event = 1;



  }

  if (GPIO_Pin == Key2_Pin)
  {
    key2_event = 1;
  }
}

static void LEDs_ALLoff(void)
{
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
}

static void LED_On(uint8_t led)
{
  switch (led)
  {
  case 1 : HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET); break;
  case 2 : HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET); break;
  case 3 : HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET); break;
    default : break;
  }
}
static void UI_Refresh(void)
{
  char line_buf[24];

  sprintf(line_buf,"STATE: %s   ",run_state ? "RUN" : "PAUSE");
  OLED_ShowString(1,1,line_buf);
  sprintf(line_buf,"DIR: %s   ",dir_state ? "REV" : "FWD");
  OLED_ShowString(2,1,line_buf);
  sprintf(line_buf,"STEP: %dms   ",step_ms);
  OLED_ShowString(3,1,line_buf);
  sprintf(line_buf,"LED: %d   ",current_led);
  OLED_ShowString(4,1,line_buf);

  sprintf(tx_buffer,"STATE=%s,DIR=%s,STEP=%d,LED=%d\r\n",
  run_state ? "RUN" : "PAUSE",
  dir_state ? "REV" : "FWD",
  step_ms,current_led);
  HAL_UART_Transmit_IT(&huart2,(uint8_t*)tx_buffer,strlen(tx_buffer));

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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
OLED_Init();
OLED_Clear();

  HAL_UARTEx_ReceiveToIdle_DMA(&huart2,(uint8_t*)&rx_packet,6);
  __HAL_UART_DISABLE_IT(&huart2,UART_IT_RXNE);

  LEDs_ALLoff();
  UI_Refresh();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint8_t status_changed = 0;
    if (packet_ready)
    {
      packet_ready = 0;//当一个包传好之后，立马将标志清除
      uint8_t *bytes = (uint8_t*)&rx_packet;
      uint8_t check = (uint8_t)(bytes[0]+bytes[1]+bytes[2]+bytes[3]+bytes[4]);

      if (rx_packet.header == 0xAA && rx_packet.checksum == check)
      {
        run_state =rx_packet.run_flag;
        dir_state = rx_packet.dir_flag;//校验帧头和校验和之后就准备执行代码程序，所以这里需要先更新一下方向和始终
       uint16_t step_ms = (rx_packet.step_h << 8) | rx_packet.step_l;
        if (step_ms < 100)step_ms= 100;
        if (step_ms > 2000)step_ms = 2000;
        status_changed = 1;//设定一个标志位，使得OLED屏幕更新。前面已经有两个变量分别控制小灯的开关和方向了

      }
    }

    if (key1_event)
    {
      key1_event = 0;

      if (HAL_GPIO_ReadPin(Key1_GPIO_Port,Key1_Pin) == GPIO_PIN_SET)
      {
        HAL_Delay(10);
        if (HAL_GPIO_ReadPin(Key1_GPIO_Port,Key1_Pin) == GPIO_PIN_SET)
        {
          run_state = ! run_state;
          status_changed = 1;

        }
      }
    }
    if (key2_event)
    {
      key2_event = 0;
      if (HAL_GPIO_ReadPin(Key2_GPIO_Port,Key2_Pin) == GPIO_PIN_SET)
      {
        HAL_Delay(10);
        if (HAL_GPIO_ReadPin(Key2_GPIO_Port,Key2_Pin) == GPIO_PIN_SET)
        {
          dir_state = !dir_state;
          status_changed = 1;

        }
      }
    }
    if (status_changed)
    {
      UI_Refresh();
    }

    if (run_state == 1)
    {
        if (dir_state == 0)
        {
          current_led = (current_led >=3) ? 1 : current_led + 1;
        }else if (dir_state == 1)
        {
          current_led = (current_led <=1) ? 3 : current_led - 1;
        }
      LEDs_ALLoff();
      LED_On(current_led);
      HAL_Delay(step_ms);
        {
          char line_buf[24];
          sprintf(line_buf,"LED: %d    ",current_led);
          OLED_ShowString(4,1,line_buf);
        }

      }
    else if (run_state == 0)
    {

      HAL_Delay(10);
    }


    }





    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
