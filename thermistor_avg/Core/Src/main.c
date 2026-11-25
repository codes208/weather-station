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
#include <string.h>
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum{
	THERM_OK = 0,
	THERM_DISCONNECTED = 1,
	THERM_SHORTED = 2,
}ThermStatus;

typedef enum{
	THERM_REPORT_OK = 0,
	THERM_REPORT_READ_ERROR = 1,
	THERM_REPORT_SHORTED = 2,
	THERM_REPORT_DISCONNECTED = 3
}ThermTallyReport;

typedef struct{
	uint32_t ok_adc_sum;
	uint8_t read_ok;
	uint8_t disconnected;
	uint8_t shorted;
	uint8_t read_error;
}SampleTally;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ADC_TIMEOUT_MS 10
#define R_FIXED 9870.0f // measured fixed resistor (ohms)
#define R_0 10000.0f // thermistor resistance at 25 C
#define T_0 298.15f // 25 C in Kelvin
#define BETA 3950.0f
#define KELVIN_OFFSET 273.15f
#define UART_TIMEOUT_MS 100
#define ADC_MIN_VALID 50
#define ADC_MAX_VALID 4045
#define ADC_MAX_COUNT 4095.0f
#define SAMPLES_PER_AVE 10
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint8_t sample_flag = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN PFP */
HAL_StatusTypeDef readThermistorRaw(ADC_HandleTypeDef *hadc, uint32_t *raw_count);
float adcToTempF(float adc);
HAL_StatusTypeDef uartPrint(UART_HandleTypeDef *huart, const char *msg);
ThermStatus getThermStatus(uint32_t raw_count);
uint32_t getTallyTotal(const SampleTally *tally);
void recordSample(SampleTally *tally, HAL_StatusTypeDef status, uint32_t raw_count);
void resetTally(SampleTally *tally);
ThermTallyReport getDecision(const SampleTally *tally);
HAL_StatusTypeDef printReport(UART_HandleTypeDef *huart, ThermTallyReport decision, float temp_f);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  char buffer[32];
  SampleTally tally = {0};
  float temp_f = 0.0f;

  HAL_StatusTypeDef status = HAL_TIM_Base_Start_IT(&htim3);
  if(status != HAL_OK){
	  snprintf(buffer, sizeof(buffer), "Timer start error: %d\r\n", status);
	  (void)uartPrint(&huart2, buffer);
	  Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* USER CODE END WHILE */

	  /* USER CODE BEGIN 3 */
	  if(sample_flag == 1){
		  sample_flag = 0;
		  uint32_t adc = 0;
		  status = readThermistorRaw(&hadc1, &adc);
		  recordSample(&tally, status, adc);
		  if(getTallyTotal(&tally) == SAMPLES_PER_AVE){
			  ThermTallyReport decision = getDecision(&tally);
			  if(decision == THERM_REPORT_OK){
				  float ave_adc = (float)(tally.ok_adc_sum) / tally.read_ok;
				  temp_f = adcToTempF(ave_adc);
			  }
			  (void)printReport(&huart2, decision, temp_f);
			  resetTally(&tally);
		  }
	  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 8399;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	if(htim->Instance == TIM3){
		sample_flag = 1;
	}
}

HAL_StatusTypeDef readThermistorRaw(ADC_HandleTypeDef *hadc, uint32_t *raw_count){
	HAL_StatusTypeDef status = HAL_ADC_Start(hadc);
	if(status != HAL_OK){
		return status;
	}
	status = HAL_ADC_PollForConversion(hadc, ADC_TIMEOUT_MS);
	if(status != HAL_OK){
		return status;
	}
	*raw_count = HAL_ADC_GetValue(hadc);
	return status;
}

/*Expects a valid reading between ADC_MIN_VALID and ADC_MAX_VALID*/
float adcToTempF(float adc){
	float res_temp = R_FIXED * (ADC_MAX_COUNT / adc - 1.0f);
	float invert_t = 1.0f / T_0 + ((1.0f / BETA) * logf(res_temp / R_0));
	float temp_k = 1.0f / invert_t;
	float temp_c = temp_k - KELVIN_OFFSET;
	float temp_f = temp_c * (9.0f / 5.0f) + 32.0f;
	return temp_f;
}

HAL_StatusTypeDef uartPrint(UART_HandleTypeDef *huart, const char *msg){
	HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (const uint8_t *)msg, strlen(msg), UART_TIMEOUT_MS);
	return status;
}

ThermStatus getThermStatus(uint32_t raw_count){
	if(raw_count < ADC_MIN_VALID){
		return THERM_DISCONNECTED;
	}
	else if(raw_count > ADC_MAX_VALID){
		return THERM_SHORTED;
	}
	else{
		return THERM_OK;
	}
}

void resetTally(SampleTally *tally){
	*tally = (SampleTally){0};
}

uint32_t getTallyTotal(const SampleTally *tally){
	uint32_t total = tally->read_ok + tally->disconnected + tally->shorted + tally->read_error;
	return total;
}

void recordSample(SampleTally *tally, HAL_StatusTypeDef status, uint32_t raw_count){
	if(status != HAL_OK){
		tally->read_error++;
	}
	else{
		switch(getThermStatus(raw_count)){
			case THERM_OK:
				tally->read_ok++;
				tally->ok_adc_sum += raw_count;
				break;
			case THERM_DISCONNECTED:
				tally->disconnected++;
				break;
			case THERM_SHORTED:
				tally->shorted++;
				break;
		}
	}
}

ThermTallyReport getDecision(const SampleTally *tally){
	if(tally->read_ok > (getTallyTotal(tally) - tally->read_ok)){
		return THERM_REPORT_OK;
	}
	else if((tally->read_error >= tally->disconnected) && (tally->read_error >= tally->shorted)){
		return THERM_REPORT_READ_ERROR;
	}
	else if((tally->shorted >= tally->read_error) && (tally->shorted >= tally->disconnected)){
		return THERM_REPORT_SHORTED;
	}
	else{
		return THERM_REPORT_DISCONNECTED;
	}
}

HAL_StatusTypeDef printReport(UART_HandleTypeDef *huart, ThermTallyReport decision, float temp_f){
	switch(decision){
		case THERM_REPORT_OK: {
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "Temp: %.2f F\r\n", temp_f);
			return uartPrint(huart, buffer);
		}
		case THERM_REPORT_READ_ERROR:
			return uartPrint(huart, "Read error fault!\r\n");
		case THERM_REPORT_SHORTED:
			return uartPrint(huart, "Thermistor shorted!\r\n");
		case THERM_REPORT_DISCONNECTED:
			return uartPrint(huart, "Thermistor disconnected!\r\n");
	}
	return HAL_ERROR;
}
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
