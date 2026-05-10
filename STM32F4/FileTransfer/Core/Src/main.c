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
#include "file_transfer.h"
#include "W25Qxx.h"
#include "lfs.h"


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
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
/*	LittleFS Variables	*/
lfs_t lfs;

lfs_file_t file1;

lfs_dir_t dir;

struct lfs_info info;

uint8_t readBuffer[256];
uint32_t sizeFile1;
uint8_t sendingFlag = 0;

/*	UART DMA Idle Mode Packet	*/
uint8_t uart3_rx_dma[1024];

/*	File Transfer Variables		*/

/*-----------------------------*/
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/*--------------------------
|							|
|	LittleFS Functions		|
| 							|
 --------------------------*/
int lfs_read(const struct lfs_config *c,
              lfs_block_t block,
              lfs_off_t off,
              void *buffer,
              lfs_size_t size)
{
    uint32_t addr = block * c->block_size + off;
    uint32_t page = addr / 256;
	uint32_t offset = addr % 256;
    W25Q_Read(page, offset, size, buffer);
    return 0;
}

int lfs_prog(const struct lfs_config *c,
              lfs_block_t block,
              lfs_off_t off,
              const void *buffer,
              lfs_size_t size)
{
	uint32_t addr = block * c->block_size + off;
	    const uint8_t *data = buffer;

	    while(size)
	    {
	        uint32_t page = addr / 256;
	        uint32_t offset = addr % 256;

	        uint32_t chunk = 256 - offset;
	        if(chunk > size) chunk = size;

	        W25Q_Write(page, offset, chunk, (uint8_t*)data);

	        addr += chunk;
	        data += chunk;
	        size -= chunk;
	    }
    return 0;
}

int lfs_erase(const struct lfs_config *c,
               lfs_block_t block)
{
    uint32_t addr = block * c->block_size;
    uint16_t numsector = addr / 4096;
    W25Q_Erase_Sector(numsector);
    return 0;
}

int lfs_sync(const struct lfs_config *c)
{
    return 0;
}


/*--------------------------
|							|
|	USART IDEAL Mode		|
| 							|
 --------------------------*/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,
                                uint16_t Size)
{
    if(huart->Instance == USART3)
    {
        for(uint16_t i = 0; i < Size; i++)
        {
            FT_processByte(uart3_rx_dma[i]);
        }

        HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
                                     uart3_rx_dma,
                                     sizeof(uart3_rx_dma));

        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx,
                             DMA_IT_HT);
    }
}

fileHandeler flhdl;
/*--------------------------
|							|
|	File From PC to MCU		|
| 							|
 --------------------------*/
void writeFileNOR(void){
	if(flhdl.writeFlag)
		  {
		      flhdl.writeFlag = 0;

		      flhdl.fsBusy = 1;

		      if(flhdl.sequenceWrite == 1)
		      {
		          lfs_file_open(&lfs,
		                        &file1,
		                        (char*)flhdl.filenameWrite,
		                        LFS_O_WRONLY |
		                        LFS_O_CREAT |
		                        LFS_O_TRUNC);
			      lfs_file_write(&lfs,
			                     &file1,
			                     (void*)flhdl.writeBuffer,
								 flhdl.payloadWrite);
		      }
		      else
		      {
			      lfs_file_write(&lfs,
			                     &file1,
			                     (void*)flhdl.writeBuffer,
								 flhdl.payloadWrite);
		      }

		      flhdl.fsBusy = 0;
		  }
	      if(flhdl.fileClose){
	    	  lfs_file_close(&lfs, &file1);
	    	  flhdl.fileClose = 0;
	      }
}

/*--------------------------
|							|
|	File From MCU to PC		|
| 							|
 --------------------------*/
void sendFileToPC(void){
	  if(flhdl.requestFile)
	  {
	      flhdl.requestFile = 0;

	      FT_SendFile(
	          (char*)flhdl.requestName
	      );
	  }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*------------------------------
|								|
|	LittleFS Configuration		|
| 								|
 ------------------------------*/
struct lfs_config cfg =
{
.read  = lfs_read,
.prog  = lfs_prog,
.erase = lfs_erase,
.sync  = lfs_sync,

.read_size = 256,
.prog_size = 256,
.block_size = 4096,
.block_count = 1024,
.cache_size = 1024,
.lookahead_size = 64,
.block_cycles = 500
};

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
  MX_USART3_UART_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart3,
                               uart3_rx_dma,
                               sizeof(uart3_rx_dma));
  W25Q_Reset();
  uint32_t ID = 0;
  ID = W25Q_ReadID();
  if(ID == 0){
	  printf("failed to initialize Flash Memory\n");
  }

  int err = lfs_mount(&lfs, &cfg);
  int err1 = 0 ;
  if (err < 0)
  {
      printf("file open failed %d\n", err);
  }
  if (err)
  {
      lfs_format(&lfs, &cfg);
      err1 = lfs_mount(&lfs, &cfg);
  }
  if (err1 < 0)
    {
        printf("file open failed %d\n", err1);
    }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  lfs_dir_open(&lfs, &dir, "/");

  while (lfs_dir_read(&lfs, &dir, &info) > 0)
  {
	  char buf_info[300];
      sprintf(buf_info,"File: %s, size:%ld\n", info.name, info.size);
      HAL_UART_Transmit(&huart1, (uint8_t *)buf_info, strlen(buf_info), 100);
  }

  lfs_dir_close(&lfs, &dir);
  flhdl.requestFile = 1;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  writeFileNOR();
	  sendFileToPC();


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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
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
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
