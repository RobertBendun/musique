/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

uint32_t adc_vals_old[2] = {};
uint32_t adc_vals[2];

uint8_t rx = 0;
uint8_t rx_buf[128] = {0};
int tail = 0, head = 0;

uint8_t notes[128] = {0};
uint8_t note_priority = 1;
uint32_t tones[] = {1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		/*c4*/ 1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065, 2146, 2225, 2304, 2383,
		1508, 1587, 1666, 1745, 1824, 1903, 1986, 2065};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint32_t endian_swap(uint32_t value)
{
	_Static_assert(sizeof(uint32_t) == 4, "I can't imagine world where this is false");
	char buf[sizeof(uint32_t)];
	memcpy(buf, &value, sizeof(value));
	// swap(buf[0], buf[3])
	uint32_t t = buf[0]; buf[0] = buf[3]; buf[3] = t;
	// swap(buf[1], buf[2])
	t = buf[1];	buf[1] = buf[2]; buf[2] = t;
	memcpy(&value, buf, sizeof(value));
	return value;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == IT1_Pin) {
		uint8_t val = 0b10000000;
		//HAL_UART_Transmit(&huart2, (uint8_t *) &val, sizeof(val), HAL_MAX_DELAY);

		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9)){
			// falling
			printf("D%04lu\n", 0);
		} else{
			// rising
			printf("D%04lu\n", 4095);
		}
	}

	if (GPIO_Pin == IT2_Pin) {
		uint8_t val = 0b10000001;
		//HAL_UART_Transmit(&huart2, (uint8_t *) &val, sizeof(val), HAL_MAX_DELAY);
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8)){
			// falling
			printf("C%04lu\n", 0);
		} else{
			// rising
			printf("C%04lu\n", 4095);
		}
	}

	if (GPIO_Pin == IT3_Pin) {
		uint8_t val = 0b10000010;
		//HAL_UART_Transmit(&huart2, (uint8_t *) &val, sizeof(val), HAL_MAX_DELAY);
		if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7)){
			// falling
			printf("E%04lu\n", 0);
		} else{
			// rising
			printf("E%04lu\n", 4095);
		}
	}
}

int __io_putchar(int ch)
{
	if (ch == '\n') {
		__io_putchar('\r');
	}
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return 1;
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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_DAC1_Init();
  /* USER CODE BEGIN 2 */


  // start both DAC channels
  HAL_DAC_Start(&hdac1, DAC1_CHANNEL_1);
  HAL_DAC_Start(&hdac1, DAC1_CHANNEL_2);

  // configure built-in ADC
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

  HAL_UART_Receive_IT(&huart2, &rx, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	/*int transmit_flag = 0;
	char adc_chars[sizeof(uint32_t)] = {};

	for(int i = 0; i < 2; i++){
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
		adc_vals[i] = HAL_ADC_GetValue(&hadc1);
		if((adc_vals[i] > adc_vals_old[i] && adc_vals[i] - adc_vals_old[i] > 100) ||
				(adc_vals[i] < adc_vals_old[i] && adc_vals_old[i] - adc_vals[i] > 100)){
			printf("NR %d do zmiany\n", i);
			uint32_t temp = adc_vals[i];
			adc_vals_old[i] = temp;

			adc_vals[i] = endian_swap(temp);
			memcpy(adc_chars + (sizeof(uint32_t)*i), &adc_vals[i], sizeof(uint32_t));
			transmit_flag = 1;
		}

	}

	if(transmit_flag == 1){
		//HAL_UART_Transmit(&huart2, (uint8_t *) &adc_chars[0], 2 * sizeof(uint32_t), HAL_MAX_DELAY);
	}*/


	int note_update_flag = 0;
	if (head != tail){
		if((rx_buf[tail] & 0b11110000) == 0b10010000){
			// note on
			int bytes_available = (head - tail + 128) % 128;
			if (bytes_available >= 3){
				// if all 3 bytes available
				// printf("G%04d\n", rx_buf[tail+1]);
				notes[rx_buf[tail+1]] = note_priority++;
				note_update_flag = 1;
				tail = (tail+3)%128;
			}


		}else if((rx_buf[tail] & 0b11110000) == 0b10000000){
			// note off
			int bytes_available = (head - tail + 128) % 128;
			if (bytes_available >= 3){
				// if all 3 bytes available
				// printf("G%04d\n", rx_buf[tail+1]);
				notes[rx_buf[tail+1]] = 0;
				note_update_flag = 1;
				tail = (tail+3)%128;
			}
		}else{
			// anything else
			// for now drop the data
			tail = (tail+1)%128;
		}

		if(note_update_flag){
			int newest_note = -1;
			uint8_t max_priority = 0;
			for(int i = 0; i < 128; i++){
				if(notes[i] > max_priority){
					max_priority = notes[i];
					newest_note = i;
				}
			}
			if(newest_note >= 0){
				DAC1->DHR12R1 = max(min(tones[newest_note], 4095), 0);
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
			}else{
				HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
			}
		}


		/////////////////////// A //////////////////////
		/*uint8_t data[3];
		data[0] = 0b10010000;
		data[1] = rx_buf[tail];
		data[2] = 0b01111111;
		HAL_UART_Transmit(&huart3, &data[0], 3, HAL_MAX_DELAY);

		HAL_Delay(500);
		data[0] = 0b10000000;
		HAL_UART_Transmit(&huart3, &data[0], 3, HAL_MAX_DELAY);

		tail = (tail+1)%128;*/
	}


	char adc_chars[sizeof(uint32_t)] = {};

	for(int i = 0; i < 2; i++){
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
		adc_vals[i] = 4095 - HAL_ADC_GetValue(&hadc1);
	}

	printf("A%04lu\n", adc_vals[0]);
	printf("B%04lu\n", adc_vals[1]);

/*
	for(int i = 0; i < 2; i++){
		uint32_t temp = adc_vals[i];
		adc_vals[i] = endian_swap(temp) << 3;

		memcpy(adc_chars + (sizeof(uint32_t)*i), &adc_vals[i], sizeof(uint32_t));
	}

	HAL_UART_Transmit(&huart2, (uint8_t *) &adc_chars[0], 2 * sizeof(uint32_t), HAL_MAX_DELAY);
*/
	HAL_Delay(100);

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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */
  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }
  /** DAC channel OUT1 config
  */
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /** DAC channel OUT2 config
  */
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : IT3_Pin IT2_Pin IT1_Pin */
  GPIO_InitStruct.Pin = IT3_Pin|IT2_Pin|IT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
