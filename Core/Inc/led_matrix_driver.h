#ifndef __LED_MATRIX_DRIVER_H__
#define __LED_MATRIX_DRIVER_H__

#include <stdint.h>
#include "dma.h"

typedef struct Font {
  uint32_t codepoint;
  uint64_t data;
  uint8_t wh;
} Font;
extern const int chr_num;
extern const Font fonts[];

uint16_t to_rgb565(uint8_t r, uint8_t g, uint8_t b);
void init_driver(uint8_t rotate_x, uint8_t rotate_y, DMA_HandleTypeDef* hdma);
void draw_pixel_888(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void draw_pixel(int x, int y, uint16_t rgb);
void clear_frame();
void present_frame();
void clear_region(int x, int y, int w, int h);
void plot(uint16_t *colors, int src_x, int src_y, int src_w, int src_h, int x, int y, int w, int h);
void draw_line(int x1, int y1, int x2, int y2, uint16_t rgb);
void draw_line_888(int x1, int y1, int x2, int y2, int r, int g, int b);
void draw_rect(int x, int y, int w, int h, uint16_t rgb);
void draw_rect_888(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void draw_fill_rect(int x, int y, int w, int h, uint16_t border_rgb, uint16_t fill_rgb);
int draw_char(wchar_t wch, int x, int y, uint16_t rgb);
int draw_text(wchar_t *wstr, int x, int y, uint16_t rgb);


#endif