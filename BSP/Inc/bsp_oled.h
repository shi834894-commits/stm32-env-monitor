#ifndef __BSP_OLED_H
#define __BSP_OLED_H

#include "main.h"

#define OLED_WIDTH    128
/* The connected module is used as a 128x32 panel.  Keeping all application
   content in the first 32 rows also remains visible on a 128x64 panel. */
#define OLED_HEIGHT   32
#define OLED_PAGES    4

#define OLED_I2C_ADDR 0x3C

uint8_t OLED_Init(void);
void OLED_Clear(void);
uint8_t OLED_Refresh(void);
void OLED_SetPixel(uint8_t x, uint8_t y, uint8_t val);
void OLED_DrawChar(uint8_t x, uint8_t y, char ch);
void OLED_DrawString(uint8_t x, uint8_t y, const char *str);
void OLED_DrawNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_DrawFloat(uint8_t x, uint8_t y, float num, uint8_t int_len, uint8_t fra_len);
void OLED_DrawChinese(uint8_t x, uint8_t y, uint8_t no);

#define CN_WEN    0
#define CN_DU     1
#define CN_SHI    2

#endif
