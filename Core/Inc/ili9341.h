#ifndef INC_ILI9341_H_
#define INC_ILI9341_H_

#include "main.h"

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);

#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F

#endif
