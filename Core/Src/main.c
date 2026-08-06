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
#include "ili9341.h"
#include "sunset_img.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Every screen the app can be on. Add more here as the project grows
 * (e.g. SCREEN_BOOT, SCREEN_ANIMATION, SCREEN_NOTIFICATIONS...). */
typedef enum
{
    SCREEN_MENU,
    SCREEN_APPLICATIONS,
    SCREEN_ENVIRONMENT,
    SCREEN_COMPUTER,
	SCREEN_ABOUT
} AppScreen_t;

/* One row in the menu: the title(unchangeable) + what bg screen it goes to */
typedef struct
{
    const char *label;
    AppScreen_t targetScreen;
} MenuItem_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MENU_START_Y   60 //Names constants for layout
#define MENU_ROW_H     28
#define MENU_ARROW_X   16
#define MENU_LABEL_X   40
#define MENU_TEXT_SCALE 2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

volatile uint32_t tft_debug_step = 0;
volatile HAL_StatusTypeDef tft_spi_status = HAL_OK;

uint8_t lastCLKState;

static const MenuItem_t menuItems[] =
{
	{ "DEV TOOLS", SCREEN_APPLICATIONS },
	{ "ENVIRONMENT",  SCREEN_ENVIRONMENT  },
	{ "COMPUTER",     SCREEN_COMPUTER     },
    { "ABOUT",     SCREEN_ABOUT     },
};
#define MENU_ITEM_COUNT (sizeof(menuItems) / sizeof(menuItems[0]))

static volatile AppScreen_t currentScreen = SCREEN_MENU;
static volatile uint8_t needsFullRedraw = 1;   /* screen changed entirely   */
static volatile uint8_t menuCursorMoved = 0;   /* only the cursor moved    */
static uint8_t menuCursor = 0;
static uint8_t previousMenuCursor = 0;


#define UART_LINE_MAX 32 //UART Computer globals

uint8_t uartRxByte;
char uartLineBuf[UART_LINE_MAX];
uint8_t uartLineIdx = 0;

volatile char uartLineComplete[UART_LINE_MAX];
volatile uint8_t uartLineReady = 0;

typedef struct
{
    int cpu;
    int ram;
} ComputerStats_t;

volatile ComputerStats_t computerStats = { 0, 0 };
volatile uint8_t computerNeedsUpdate = 0;

typedef enum { APP_BUILD, APP_FLASH, APP_OPEN_CODE, APP_GIT_STATUS, APP_COUNT } AppChoice_t;

static const char *appLabels[APP_COUNT] = { "BUILD", "FLASH", "OPEN CODE", "GIT STATUS" };
static const char *appCommands[APP_COUNT] = { "ACTION:BUILD\n", "ACTION:FLASH\n", "ACTION:OPEN_CODE\n", "ACTION:GIT_STATUS\n" };

#define APPS_START_Y 60
#define APPS_ROW_H   28
#define APPS_ARROW_X 16
#define APPS_LABEL_X 40

uint8_t appsCursor = 0;
uint8_t previousAppsCursor = 0;
volatile uint8_t appsCursorMoved = 0;



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

static void DrawMenuArrow(uint8_t index, uint8_t show);
static void RenderMenuScreen(void);
static void RenderPlaceholderScreen(const char *title);
static void RenderCurrentScreen(void);
static void RenderComputerScreen(void);
static void UpdateComputerValues(void);
static void ParseUartLine(const char *line);

static void DrawAppsArrow(uint8_t index, uint8_t show);
static void RenderApplicationsScreen(void);
static void SendUartCommand(const char *cmd);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Draws or erases the ">" cursor next to menu item `index`.
 * show=1 draws the arrow, show=0 draws a blank space over it instead
 * (this is how we "erase" the old arrow position without touching
 * anything else on screen). */
static void DrawMenuArrow(uint8_t index, uint8_t show)
{
    uint16_t y = MENU_START_Y + (index * MENU_ROW_H);
    char glyph = show ? '>' : ' ';

    ILI9341_DrawChar(MENU_ARROW_X, y, glyph, ILI9341_YELLOW, ILI9341_BLACK, MENU_TEXT_SCALE);
}

/* Full redraw of the menu screen: background image, every label, and
 * the arrow at whatever menuCursor currently is. Only called when we
 * just switched INTO the menu screen -- not on every cursor move,
 * since that would repaint the whole background every time you turn
 * the knob and cause visible flicker. */
static void RenderMenuScreen(void)
{
    ILI9341_DrawImageUpscaled2x(sunset_img, sunset_img_WIDTH, sunset_img_HEIGHT);

    for (uint8_t i = 0; i < MENU_ITEM_COUNT; i++)
    {
        uint16_t y = MENU_START_Y + (i * MENU_ROW_H);
        ILI9341_DrawString(MENU_LABEL_X, y, menuItems[i].label, ILI9341_WHITE, ILI9341_BLACK, MENU_TEXT_SCALE);
    }

    DrawMenuArrow(menuCursor, 1);
    previousMenuCursor = menuCursor;
}

/* Placeholder for the screens the menu leads to. Replace this with
 * real dashboard/settings/about rendering later -- for now it just
 * proves the state machine + navigation actually works end to end. */
static void RenderPlaceholderScreen(const char *title)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_DrawString(20, 20, title, ILI9341_WHITE, ILI9341_BLACK, MENU_TEXT_SCALE);
    ILI9341_DrawString(20, 60, "PRESS USER BTN", ILI9341_GRAY, ILI9341_BLACK, 1);
    ILI9341_DrawString(20, 75, "TO GO BACK", ILI9341_GRAY, ILI9341_BLACK, 1);
}


/* Draws `text`, automatically breaking it into multiple lines so it
 * never runs off the right edge of the screen. Breaks at the last
 * space before the line limit, so it doesn't cut words in half. */
static void DrawWrappedText(uint16_t x, uint16_t y, uint16_t lineHeight,
                             const char *text, uint16_t fgColor, uint16_t bgColor,
                             uint8_t scale, uint8_t maxCharsPerLine)
{
    char lineBuf[40];
    uint16_t curY = y;
    const char *p = text;

    while (*p)
    {
        const char *scan = p;
        const char *lastSpace = NULL;
        uint8_t lineLen = 0;

        while (*scan && lineLen < maxCharsPerLine)
        {
            if (*scan == ' ') lastSpace = scan;
            scan++;
            lineLen++;
        }

        const char *lineEnd;
        if (*scan == '\0')       lineEnd = scan;
        else if (lastSpace)      lineEnd = lastSpace;
        else                     lineEnd = scan;

        uint8_t copyLen = (uint8_t)(lineEnd - p);
        if (copyLen > sizeof(lineBuf) - 1) copyLen = sizeof(lineBuf) - 1;
        memcpy(lineBuf, p, copyLen);
        lineBuf[copyLen] = '\0';

        ILI9341_DrawString(x, curY, lineBuf, fgColor, bgColor, scale);
        curY += lineHeight;

        p = (*lineEnd == ' ') ? lineEnd + 1 : lineEnd;
    }
}

static void RenderAboutScreen(void)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_DrawString(20, 20, "ABOUT", ILI9341_YELLOW, ILI9341_BLACK, 2);

    DrawWrappedText(10, 55, 12,
        "This project is an Embedded USB console, it allows you to "
        "control your applications, see your computers data, and "
        "get environment data. Hope you like it!",
        ILI9341_WHITE, ILI9341_BLACK, 1, 27);

    ILI9341_DrawString(10, 290, "PRESS USER BTN TO GO BACK", ILI9341_GRAY, ILI9341_BLACK, 1);
}

static void SendUartCommand(const char *cmd)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, strlen(cmd), HAL_MAX_DELAY);
}

static void DrawAppsArrow(uint8_t index, uint8_t show)
{
    uint16_t y = APPS_START_Y + (index * APPS_ROW_H);
    char glyph = show ? '>' : ' ';
    ILI9341_DrawChar(APPS_ARROW_X, y, glyph, ILI9341_YELLOW, ILI9341_BLACK, MENU_TEXT_SCALE);
}

static void RenderApplicationsScreen(void)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_DrawString(20, 20, "DEV TOOLS", ILI9341_YELLOW, ILI9341_BLACK, 2);

    for (uint8_t i = 0; i < APP_COUNT; i++)
    {
        uint16_t y = APPS_START_Y + (i * APPS_ROW_H);
        ILI9341_DrawString(APPS_LABEL_X, y, appLabels[i], ILI9341_WHITE, ILI9341_BLACK, MENU_TEXT_SCALE);
    }

    DrawAppsArrow(appsCursor, 1);
    previousAppsCursor = appsCursor;

    ILI9341_DrawString(10, 290, "PRESS USER BTN TO GO BACK", ILI9341_GRAY, ILI9341_BLACK, 1);
}

/* Called once per loop iteration. Decides whether anything actually
 * needs to be drawn this pass, and if so, draws the minimum necessary
 * -- a full redraw on screen change, or just the arrow on cursor move. */
static void RenderCurrentScreen(void)
{
    if (needsFullRedraw)
    {
        switch (currentScreen)
        {

        case SCREEN_MENU: //Updated Menu
            RenderMenuScreen();
            break;
        case SCREEN_APPLICATIONS:
            RenderApplicationsScreen();
            break;
        case SCREEN_ENVIRONMENT:
        	RenderPlaceholderScreen("ENVIRONMENT");
        	break;
        case SCREEN_COMPUTER:
        	RenderComputerScreen();
        	break;
        case SCREEN_ABOUT:
            RenderAboutScreen();
            break;
        }
        needsFullRedraw = 0;
        menuCursorMoved = 0;
    }
    else if (menuCursorMoved && currentScreen == SCREEN_MENU)
    {
        DrawMenuArrow(previousMenuCursor, 0);
        DrawMenuArrow(menuCursor, 1);
        previousMenuCursor = menuCursor;
        menuCursorMoved = 0;
    }

    else if (computerNeedsUpdate && currentScreen == SCREEN_COMPUTER)
    {
        UpdateComputerValues();
        computerNeedsUpdate = 0;
    }

//add one of these for each menu screen
    else if (appsCursorMoved && currentScreen == SCREEN_APPLICATIONS)
    {
        DrawAppsArrow(previousAppsCursor, 0);
        DrawAppsArrow(appsCursor, 1);
        previousAppsCursor = appsCursor;
        appsCursorMoved = 0;
    }
}


static void ParseUartLine(const char *line)
{
    if (strncmp(line, "CPU:", 4) == 0)
    {
        computerStats.cpu = atoi(line + 4);
        computerNeedsUpdate = 1;
    }
    else if (strncmp(line, "RAM:", 4) == 0)
    {
        computerStats.ram = atoi(line + 4);
        computerNeedsUpdate = 1;
    }
}

//Displays real time CPU and RAM data
static void UpdateComputerValues(void)
{
    char buf[20];

    snprintf(buf, sizeof(buf), "CPU: %3d%%", computerStats.cpu);
    ILI9341_DrawString(20, 60, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    snprintf(buf, sizeof(buf), "RAM: %3d%%", computerStats.ram);
    ILI9341_DrawString(20, 90, buf, ILI9341_WHITE, ILI9341_BLACK, 2);
}

static void RenderComputerScreen(void)
{
    ILI9341_FillScreen(ILI9341_BLACK);
    ILI9341_DrawString(20, 20, "COMPUTER", ILI9341_YELLOW, ILI9341_BLACK, 2);
    UpdateComputerValues();
    ILI9341_DrawString(10, 290, "PRESS USER BTN TO GO BACK", ILI9341_GRAY, ILI9341_BLACK, 1);
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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  lastCLKState = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);

  HAL_UART_Receive_IT(&huart2, &uartRxByte, 1);

  tft_debug_step = 1;
  ILI9341_Init();
  tft_debug_step = 2;

  /* USER CODE END 2 */

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    uint8_t currentCLKState =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10);

    /* Detect encoder rotation */
    if (currentCLKState != lastCLKState)
    {
      if (currentCLKState == GPIO_PIN_RESET)
      {
        if (currentScreen == SCREEN_MENU)
        {
          if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) != currentCLKState)
          {
            /* Clockwise -> move cursor down */
            if (menuCursor < MENU_ITEM_COUNT - 1)
            {
              menuCursor++;
              menuCursorMoved = 1;
            }
          }
          else
          {
            /* Counter-clockwise -> move cursor up */
            if (menuCursor > 0)
            {
              menuCursor--;
              menuCursorMoved = 1;
            }
          }
        }



        else if (currentScreen == SCREEN_APPLICATIONS)
        {
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3) != currentCLKState)
            {
                if (appsCursor < APP_COUNT - 1)
                {
                    appsCursor++;
                    appsCursorMoved = 1;
                }
            }
            else
            {
                if (appsCursor > 0)
                {
                    appsCursor--;
                    appsCursorMoved = 1;
                }
            }
        }



      }

      lastCLKState = currentCLKState;
    }

    /* Detect encoder button press -> select current menu item */
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET)
    {
      HAL_Delay(20);

      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET)
      {
        if (currentScreen == SCREEN_MENU)
        {
          currentScreen = menuItems[menuCursor].targetScreen;
          needsFullRedraw = 1;
        }

        else if (currentScreen == SCREEN_APPLICATIONS)
        {
            SendUartCommand(appCommands[appsCursor]);
        }

        while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET)
        {
          /* Wait for release */
        }

        HAL_Delay(20);
      }
    }

    if (uartLineReady)
    {
        ParseUartLine((const char *)uartLineComplete);
        uartLineReady = 0;
    }
    /* Draw whatever actually changed this pass, if anything did */
    RenderCurrentScreen();

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
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : TFT_DC_Pin */
  GPIO_InitStruct.Pin = TFT_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TFT_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_RST_Pin */
  GPIO_InitStruct.Pin = TFT_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TFT_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_CS_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TFT_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* This overrides a "weak" (default, do-nothing) function that the HAL
 * calls automatically whenever ANY EXTI-configured pin interrupts --
 * including the Nucleo's onboard USER button, which we set up above
 * with BSP_PB_Init(..., BUTTON_MODE_EXTI). We use it here as a
 * universal "back to menu" button from any screen. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == USER_BUTTON_PIN)
  {
    if (currentScreen != SCREEN_MENU)
    {
      currentScreen = SCREEN_MENU;
      needsFullRedraw = 1;
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (uartRxByte == '\n')
        {
            uartLineBuf[uartLineIdx] = '\0';
            strncpy((char *)uartLineComplete, uartLineBuf, UART_LINE_MAX);
            uartLineReady = 1;
            uartLineIdx = 0;
        }
        else if (uartRxByte != '\r' && uartLineIdx < UART_LINE_MAX - 1)
        {
            uartLineBuf[uartLineIdx++] = uartRxByte;
        }

        HAL_UART_Receive_IT(&huart2, &uartRxByte, 1);
    }
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
