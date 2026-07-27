#include "ili9341.h"
#include "font8x8.h"

extern SPI_HandleTypeDef hspi1;

extern volatile uint32_t tft_debug_step;
extern volatile HAL_StatusTypeDef tft_spi_status;


static void ILI9341_Select(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
}

static void ILI9341_Unselect(void)
{
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

static void ILI9341_Reset(void)
{
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);

    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(150);
}

static void ILI9341_WriteCommand(uint8_t command)
{
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);

    tft_spi_status = HAL_SPI_Transmit(&hspi1, &command, 1, HAL_MAX_DELAY);

    if (tft_spi_status != HAL_OK)
    {
        tft_debug_step = 100;
        Error_Handler();
    }
}

static void ILI9341_WriteData(uint8_t *data, uint16_t size)
{
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    tft_spi_status = HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);

    if (tft_spi_status != HAL_OK)
    {
        tft_debug_step = 101;
        Error_Handler();
    }
}

static void ILI9341_WriteByte(uint8_t data)
{
    ILI9341_WriteData(&data, 1);
}

static void ILI9341_SetAddressWindow(
    uint16_t xStart,
    uint16_t yStart,
    uint16_t xEnd,
    uint16_t yEnd)
{
    uint8_t data[4];

    /* Column address */
    ILI9341_WriteCommand(0x2A);

    data[0] = xStart >> 8;
    data[1] = xStart & 0xFF;
    data[2] = xEnd >> 8;
    data[3] = xEnd & 0xFF;

    ILI9341_WriteData(data, 4);

    /* Row address */
    ILI9341_WriteCommand(0x2B);

    data[0] = yStart >> 8;
    data[1] = yStart & 0xFF;
    data[2] = yEnd >> 8;
    data[3] = yEnd & 0xFF;

    ILI9341_WriteData(data, 4);

    /* Begin memory write */
    ILI9341_WriteCommand(0x2C);
}

void ILI9341_Init(void)
{
    ILI9341_Reset();
    ILI9341_Select();

    /* Software reset */
    ILI9341_WriteCommand(0x01);
    HAL_Delay(150);

    /* Power control B */
    ILI9341_WriteCommand(0xCF);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0xC1);
    ILI9341_WriteByte(0x30);

    /* Power on sequence control */
    ILI9341_WriteCommand(0xED);
    ILI9341_WriteByte(0x64);
    ILI9341_WriteByte(0x03);
    ILI9341_WriteByte(0x12);
    ILI9341_WriteByte(0x81);

    /* Driver timing control A */
    ILI9341_WriteCommand(0xE8);
    ILI9341_WriteByte(0x85);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0x78);

    /* Power control A */
    ILI9341_WriteCommand(0xCB);
    ILI9341_WriteByte(0x39);
    ILI9341_WriteByte(0x2C);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0x34);
    ILI9341_WriteByte(0x02);

    /* Pump ratio control */
    ILI9341_WriteCommand(0xF7);
    ILI9341_WriteByte(0x20);

    /* Driver timing control B */
    ILI9341_WriteCommand(0xEA);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0x00);

    /* Power control 1 */
    ILI9341_WriteCommand(0xC0);
    ILI9341_WriteByte(0x23);

    /* Power control 2 */
    ILI9341_WriteCommand(0xC1);
    ILI9341_WriteByte(0x10);

    /* VCOM control 1 */
    ILI9341_WriteCommand(0xC5);
    ILI9341_WriteByte(0x3E);
    ILI9341_WriteByte(0x28);

    /* VCOM control 2 */
    ILI9341_WriteCommand(0xC7);
    ILI9341_WriteByte(0x86);

    /* Memory access control */
    ILI9341_WriteCommand(0x36);
    ILI9341_WriteByte(0x48);

    /* 16-bit RGB565 pixel format */
    ILI9341_WriteCommand(0x3A);
    ILI9341_WriteByte(0x55);

    /* Frame rate control */
    ILI9341_WriteCommand(0xB1);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0x18);

    /* Display function control */
    ILI9341_WriteCommand(0xB6);
    ILI9341_WriteByte(0x08);
    ILI9341_WriteByte(0x82);
    ILI9341_WriteByte(0x27);

    /* Disable 3-gamma function */
    ILI9341_WriteCommand(0xF2);
    ILI9341_WriteByte(0x00);

    /* Gamma curve */
    ILI9341_WriteCommand(0x26);
    ILI9341_WriteByte(0x01);

    /* Positive gamma correction */
    ILI9341_WriteCommand(0xE0);
    ILI9341_WriteByte(0x0F);
    ILI9341_WriteByte(0x31);
    ILI9341_WriteByte(0x2B);
    ILI9341_WriteByte(0x0C);
    ILI9341_WriteByte(0x0E);
    ILI9341_WriteByte(0x08);
    ILI9341_WriteByte(0x4E);
    ILI9341_WriteByte(0xF1);
    ILI9341_WriteByte(0x37);
    ILI9341_WriteByte(0x07);
    ILI9341_WriteByte(0x10);
    ILI9341_WriteByte(0x03);
    ILI9341_WriteByte(0x0E);
    ILI9341_WriteByte(0x09);
    ILI9341_WriteByte(0x00);

    /* Negative gamma correction */
    ILI9341_WriteCommand(0xE1);
    ILI9341_WriteByte(0x00);
    ILI9341_WriteByte(0x0E);
    ILI9341_WriteByte(0x14);
    ILI9341_WriteByte(0x03);
    ILI9341_WriteByte(0x11);
    ILI9341_WriteByte(0x07);
    ILI9341_WriteByte(0x31);
    ILI9341_WriteByte(0xC1);
    ILI9341_WriteByte(0x48);
    ILI9341_WriteByte(0x08);
    ILI9341_WriteByte(0x0F);
    ILI9341_WriteByte(0x0C);
    ILI9341_WriteByte(0x31);
    ILI9341_WriteByte(0x36);
    ILI9341_WriteByte(0x0F);

    /* Exit sleep */
    ILI9341_WriteCommand(0x11);
    HAL_Delay(120);

    /* Display on */
    ILI9341_WriteCommand(0x29);
    HAL_Delay(100);

    ILI9341_Unselect();
}

void ILI9341_FillScreen(uint16_t color)
{
    uint8_t colorData[2];
    colorData[0] = (uint8_t)(color >> 8);
    colorData[1] = (uint8_t)(color & 0xFF);

    ILI9341_Select();
    ILI9341_SetAddressWindow(0, 0, 239, 319);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    for (uint32_t pixel = 0; pixel < 240UL * 320UL; pixel++)
    {
        HAL_SPI_Transmit(&hspi1, colorData, 2, HAL_MAX_DELAY);
    }

    ILI9341_Unselect();
}

void ILI9341_DrawImageUpscaled2x(const uint16_t *data, uint16_t srcW, uint16_t srcH)
{
    uint16_t lineBuf[240];

    ILI9341_Select();
    ILI9341_SetAddressWindow(0, 0, (srcW * 2) - 1, (srcH * 2) - 1);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    for (uint16_t y = 0; y < srcH; y++)
    {
        const uint16_t *srcRow = &data[y * srcW];

        for (uint16_t x = 0; x < srcW; x++)
        {
            /* The STM32 is little-endian, so a uint16_t written to
             * memory normally lands as [low byte][high byte]. But the
             * ILI9341 expects [high byte][low byte] over SPI (see
             * FillScreen above, which sends color >> 8 first). Byte-
             * swapping here before storing into lineBuf makes the
             * little-endian memory layout come out in the right order
             * when we transmit the buffer as raw bytes below. */
            uint16_t pixel = srcRow[x];
            uint16_t swapped = (uint16_t)((pixel >> 8) | (pixel << 8)); //swap for TFT to get expected order

            lineBuf[x * 2]     = swapped; //sent it over twice, for 2x purposes
            lineBuf[x * 2 + 1] = swapped;
        }

        HAL_SPI_Transmit(&hspi1, (uint8_t *)lineBuf, sizeof(lineBuf), HAL_MAX_DELAY);
        HAL_SPI_Transmit(&hspi1, (uint8_t *)lineBuf, sizeof(lineBuf), HAL_MAX_DELAY);
    }

    ILI9341_Unselect();
}

void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fgColor, uint16_t bgColor, uint8_t scale)
{
    if (c < FONT8X8_FIRST_CHAR || c > FONT8X8_LAST_CHAR)
    {
        c = ' ';
    }

    const uint8_t *glyph = font8x8_basic[(uint8_t)c - FONT8X8_FIRST_CHAR];

    uint16_t w = FONT_CHAR_WIDTH * scale;
    uint16_t h = FONT_CHAR_HEIGHT * scale;

    ILI9341_Select();
    ILI9341_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

    uint8_t fgBytes[2] = { (uint8_t)(fgColor >> 8), (uint8_t)(fgColor & 0xFF) };
    uint8_t bgBytes[2] = { (uint8_t)(bgColor >> 8), (uint8_t)(bgColor & 0xFF) };

    for (uint8_t row = 0; row < FONT_CHAR_HEIGHT; row++)
    {
        uint8_t bits = glyph[row];

        for (uint8_t ry = 0; ry < scale; ry++)
        {
            for (uint8_t col = 0; col < FONT_CHAR_WIDTH; col++)
            {
                uint8_t *px = (bits & (1 << (7 - col))) ? fgBytes : bgBytes;

                for (uint8_t rx = 0; rx < scale; rx++)
                {
                    HAL_SPI_Transmit(&hspi1, px, 2, HAL_MAX_DELAY);
                }
            }
        }
    }

    ILI9341_Unselect();
}

void ILI9341_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fgColor, uint16_t bgColor, uint8_t scale)
{
    uint16_t cursorX = x;

    while (*str)
    {
        ILI9341_DrawChar(cursorX, y, *str, fgColor, bgColor, scale);
        cursorX += FONT_CHAR_WIDTH * scale;
        str++;
    }
}
