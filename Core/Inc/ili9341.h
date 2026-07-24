#ifndef INC_ILI9341_H_
#define INC_ILI9341_H_

#include "main.h"

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_DrawImageUpscaled2x(const uint16_t *data, uint16_t srcW, uint16_t srcH);

/* Draws one character at pixel position (x,y). scale=1 is native 8x8,
 * scale=2 draws it at 16x16, etc. bgColor fills the character's cell
 * behind the glyph (there is no "transparent" background without a
 * frame buffer, so this always draws a solid box). */
void ILI9341_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fgColor, uint16_t bgColor, uint8_t scale);

/* Draws a null-terminated string starting at (x,y), left to right. */
void ILI9341_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fgColor, uint16_t bgColor, uint8_t scale);

#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_GRAY    0x7BEF

#endif
