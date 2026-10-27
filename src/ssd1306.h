/*
 * SSD1306 128x64 OLED over I2C (address 0x3C), full-framebuffer driver.
 * Framebuffer layout matches the controller's GDDRAM (SSD1306 datasheet §8.7):
 * 8 pages of 128 columns; each byte is a vertical strip of 8 pixels, LSB on top.
 */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_ADDR   0x3Cu
#define SSD1306_WIDTH  128u
#define SSD1306_HEIGHT 64u
#define SSD1306_FB_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8u) /* 1024 bytes */

extern uint8_t ssd1306_fb[SSD1306_FB_SIZE];

int ssd1306_init(void);              /* 1 if every byte was ACKed */
void ssd1306_fill(uint8_t pattern);  /* set every framebuffer byte */
int ssd1306_flush(void);             /* send the whole framebuffer; 1 on success */

#endif
