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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_hid.h"
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_PIN GPIO_PIN_13
#define LED_PORT GPIOC
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

#define BNO_ADDR (0x4A << 1)
#define REPORT_GYRO 0x02
#define CH_CONTROL 2
#define CH_INPUT 3
#define bno_max_packet 128

#define SENS_X          5.0f
#define SENS_Y          5.0f
#define DEADBAND        0.05f
#define ALPHA 0.3f

volatile float smooth_x = 0.0f;
volatile float smooth_y = 0.0f;

volatile float gx = 0.0f;
volatile float gy = 0.0f;
volatile uint8_t bno_found = 0;

#define rpt_product_id_rsp   0xF8
#define rpt_product_id_req   0xF9

volatile uint8_t last_channel = 0;
volatile uint16_t last_len = 0;
volatile uint8_t last_report_id = 0;


typedef struct __attribute__((packed)){
	uint8_t buttons;
	int8_t x;
	int8_t y;
	int8_t wheel;
} MouseReport;

static uint8_t tx_buf[128];
static uint8_t rx_buf[128];
static uint8_t seq_num[6] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//static uint16_t shtp_read(uint8_t *channel)
//{
//	uint8_t hdr[4];
//	if (HAL_I2C_Master_Receive(&hi2c1, BNO_ADDR, hdr, 4, 50) != HAL_OK)
//		return 0;
//	uint16_t total = ((uint16_t)(hdr[1] & 0x7F) << 8) | hdr[0];
//	if (total <= 4)
//		return 0;
//	uint16_t payload_len = total - 4;
//	if (payload_len > sizeof(rx_buf))
//		return 0;
//	*channel = hdr[2];
//	if (HAL_I2C_Master_Receive(&hi2c1, BNO_ADDR, rx_buf, payload_len, 100) != HAL_OK)
//		return 0;
//
//	return payload_len;
//}

static uint16_t bno_receive(void)
{
	memset(rx_buf, 0, bno_max_packet);
	    // Read header first to get length
	    if (HAL_I2C_Master_Receive(&hi2c1, BNO_ADDR, rx_buf, 4, 200) != HAL_OK)
	        return 0;

	    uint16_t pkt_len = (uint16_t)rx_buf[0] | ((uint16_t)(rx_buf[1] & 0x7F) << 8);
	    if (pkt_len <= 4 || pkt_len == 0x7FFF) return 0;
	    if (pkt_len > bno_max_packet) pkt_len = bno_max_packet;

	    memset(rx_buf, 0, bno_max_packet);
	    // Read full packet (header + payload)
	    if (HAL_I2C_Master_Receive(&hi2c1, BNO_ADDR, rx_buf, pkt_len, 200) != HAL_OK)
	        return 0;

	    return pkt_len;
}

static HAL_StatusTypeDef shtp_write(uint8_t channel, uint8_t *data, uint16_t len)
{
    uint16_t total = len + 4;
    if (total > sizeof(tx_buf)) return HAL_ERROR;
    tx_buf[0] = total & 0xFF;
    tx_buf[1] = (total >> 8) & 0x7F;
    tx_buf[2] = channel;
    tx_buf[3] = seq_num[channel]++;
    memcpy(&tx_buf[4], data, len);
    return HAL_I2C_Master_Transmit(&hi2c1, BNO_ADDR, tx_buf, total, 200);
}

static void bno_enable_report(uint8_t report_id, uint32_t interval_us)
{
    uint8_t cmd[17] = {0};
    cmd[0] = 0xFD;
    cmd[1] = report_id;
    cmd[5] =  interval_us        & 0xFF;
    cmd[6] = (interval_us >> 8)  & 0xFF;
    cmd[7] = (interval_us >> 16) & 0xFF;
    cmd[8] = (interval_us >> 24) & 0xFF;
    shtp_write(CH_CONTROL, cmd, 17);
}

//static void bno_init(void)
//{
//	HAL_Delay(300);
//	uint8_t ch;
//	for (uint8_t i = 0; i < 10; i++){
//		shtp_read(&ch);
//		HAL_Delay(20);
//	}
//	bno_enable_report(REPORT_GYRO, 10000);
//	HAL_Delay(50);
//}
static void bno_init(void)
{
    memset(seq_num, 0, sizeof(seq_num));
    HAL_Delay(300);
    // Drain boot packets
    for (int i = 0; i < 10; i++) {
        bno_receive();
        HAL_Delay(50);
    }
    // Enable gyro at 100 Hz
    bno_enable_report(REPORT_GYRO, 10000);
    HAL_Delay(100);
}

static int bno_read_gyro(volatile float *pgx, volatile float *pgy)
{
    uint16_t total = bno_receive();
    if (total < 10) return 0;

    uint8_t channel = rx_buf[2];
    last_channel = channel;
    last_len = total;

    if (channel != CH_INPUT) return 0;

    // Scan through all reports in this packet
    // Payload starts at rx_buf[4], walk through looking for REPORT_GYRO
    uint16_t i = 4;
    while (i < total)
    {
        uint8_t report_id = rx_buf[i];
        last_report_id = report_id;

        if (report_id == 0xFB) {
            // Base timestamp: 5 bytes, skip it
            i += 5;
            continue;
        }
        if (report_id == REPORT_GYRO) {
            // Found gyro report
            // layout: [i]=id [i+1]=seq [i+2]=status [i+3][i+4]=X [i+5][i+6]=Y
            if (i + 6 >= total) return 0;
            int16_t raw_x = (int16_t)(rx_buf[i+3] | (rx_buf[i+4] << 8));
            int16_t raw_y = (int16_t)(rx_buf[i+5] | (rx_buf[i+6] << 8));
            *pgx = (float)raw_x / 512.0f;
            *pgy = (float)raw_y / 512.0f;
            return 1;
        }
        // Unknown report, can't parse further
        break;
    }
    return 0;
}

static int8_t clamp8(float v)
{
	if (v >  127.0f) return  127;
	if (v < -127.0f) return -127;

	return (int8_t)v;
}

static void mousesend(int8_t dx, int8_t dy)
{
	MouseReport r = {0};
	r.x = dx;
	r.y = dy;
	USBD_HID_SendReport(&hUsbDeviceFS, (uint8_t *)&r, sizeof(r));
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
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(500);

  bno_init();

  if (HAL_I2C_IsDeviceReady(&hi2c1, BNO_ADDR, 3, 100) == HAL_OK)
      bno_found = 1;  // should be 1 if wiring is correct
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (bno_read_gyro(&gx, &gy))
	    {
	  	  if (fabsf(gx) < DEADBAND) gx = 0.0f;
	  	  if (fabsf(gy) < DEADBAND) gy = 0.0f;

	  	smooth_x = ALPHA * gx + (1.0f - ALPHA) * smooth_x;
	    smooth_y = ALPHA * gy + (1.0f - ALPHA) * smooth_y;

	  	  int8_t dx = clamp8(gx * SENS_X);
	  	  int8_t dy = clamp8(gy * SENS_Y);

	  	  if (dx != 0 || dy != 0)
	  		  mousesend(dx, dy);
	      HAL_GPIO_TogglePin(LED_PORT, LED_PIN); // blinks if gyro reading

	    }
	  HAL_Delay(10);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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
