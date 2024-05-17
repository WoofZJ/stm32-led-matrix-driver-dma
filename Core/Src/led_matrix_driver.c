#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <math.h>
#include "dma.h"
#include "led_matrix_driver.h"

#define R_MASK(i) (0x0001U << (15-(i)))
#define G_MASK(i) (0x0001U << (10-(i)))
#define B_MASK(i) (0x0001U << (4-(i)))

/**
 *              PB 15 14 13  12 11 10 9 8 7 6 5  4  3  2  1  0
 * uint16_t data = 0  0   0   0  0  0 0 0 0 0 0  0  0  0  0  0
 *                 OE LAT CLK ?  ?  e d c b a r1 g1 b1 r2 g2 b2
 */
uint16_t dma_buf[6][32][64][2];
uint16_t m_colors[64][64][2];
uint8_t cur;
uint8_t ROTATE_X, ROTATE_Y;
uint16_t lumConvTab[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
  3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6,
  6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11,
  11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24,
  25, 25, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 33, 33, 34,
  35, 35, 36, 37, 38, 38, 39, 40, 41, 41, 42, 43, 44, 45, 45, 46,
  47, 48, 49, 50, 51, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78,
  80, 81, 82, 83, 84, 86, 87, 88, 90, 91, 92, 93, 95, 96, 98, 99,
  100, 102, 103, 105, 106, 107, 109, 110, 112, 113, 115, 116, 118, 120, 121, 123,
  124, 126, 128, 129, 131, 133, 134, 136, 138, 139, 141, 143, 145, 146, 148, 150,
  152, 154, 156, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181,
  183, 185, 187, 189, 192, 194, 196, 198, 200, 203, 205, 207, 209, 212, 214, 216,
  218, 221, 223, 226, 228, 230, 233, 235, 238, 240, 243, 245, 248, 250, 253, 255
};

void init_dma_buf() {
  for (int k = 0; k < 6; ++k) {
    for (int i = 0; i < 32; ++i) {
      for (int j = 0; j < 64; ++j) {
        dma_buf[k][i][j][0] = (1U << 15) | (((i+31)%32) << 6);
        dma_buf[k][i][j][1] = (1U << 15) | (1U<<13) | (((i+31)%32) << 6);
      }
      dma_buf[k][i][63][0] |= 1 << 14;
      dma_buf[k][i][63][1] |= 1 << 14;
      if (i == 0) {
        for (int bright = 0; bright < ((1 << (5-(k+5)%6))); ++bright) {
          dma_buf[k][i][8+bright][0] &= ~(1U<<15);
          dma_buf[k][i][8+bright][1] &= ~(1U<<15);
        }
      } else {
        for (int bright = 0; bright < ((1 << (5-k))); ++bright) {
          dma_buf[k][i][8+bright][0] &= ~(1U<<15);
          dma_buf[k][i][8+bright][1] &= ~(1U<<15);
        }
      }
    }
  }
}

void init_driver(uint8_t rotate_x, uint8_t rotate_y, DMA_HandleTypeDef* hdma) {
  ROTATE_X = rotate_x;
  ROTATE_Y = rotate_y;
  // reinit DMA for circulation
  hdma->Init.Mode = DMA_CIRCULAR;
  if (HAL_DMA_Init(&hdma_memtomem_dma2_stream0) != HAL_OK)
  {
    Error_Handler();
  }
  init_dma_buf();
  HAL_DMA_Start(hdma, (uint32_t)dma_buf, (uint32_t)&GPIOB->ODR, 32*64*2*6);
}

void transform(int *x, int *y) {
  if (ROTATE_X) {
    *x = 63 - *x;
  }
  if (ROTATE_Y) {
    *y = 63 - *y;
  }
}

uint16_t to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (((uint16_t)r & 0xf8) << 8 ) | (((uint16_t)g&0xfc) << 3) | ((b&0xf8)>>3);
}

void draw_pixel_888(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  draw_pixel(x, y, to_rgb565(r, g, b));
}

void draw_pixel(int x, int y, uint16_t rgb) {
  if (x >= 0 && x < 64 && y >= 0 && y < 64) {
    transform(&x, &y);
    m_colors[x][y][cur] = rgb;
  }
}

void update_pixel(int x, int y) {
  static uint16_t mask, offset, rgb;
  static uint8_t r, g, b;
  transform(&x, &y);
  if (m_colors[x][y][0] == m_colors[x][y][1]) {
    return ;
  }
  rgb = m_colors[x][y][cur];
  r = (rgb >> 8) & 0b11111000;
  g = (rgb >> 3) & 0b11111100;
  b = (rgb << 3) & 0b11111000;
  r = lumConvTab[r];
  g = lumConvTab[g];
  b = lumConvTab[b];
  rgb = to_rgb565(r, g, b);
  if (x >= 32) {
    mask = ~0x7;
    offset = 0;
    x -= 32;
  } else {
    mask = ~0x38;
    offset = 3;
  }
  for (int k = 0; k < 5; ++k) {
    uint8_t c = (rgb & R_MASK(k)) ? 1 : 0;
    c <<= 1;
    c |= (rgb & G_MASK(k)) ? 1 : 0;
    c <<= 1;
    c |= (rgb & B_MASK(k)) ? 1 : 0;
    dma_buf[k][x][y][0] &= mask;
    dma_buf[k][x][y][0] |= c << offset;
    dma_buf[k][x][y][1] &= mask;
    dma_buf[k][x][y][1] |= c << offset;
  }
  dma_buf[5][x][y][0] &= mask;
  dma_buf[5][x][y][0] |= ((rgb & G_MASK(5)) ? 0x2 : 0) << offset;
  dma_buf[5][x][y][1] &= mask;
  dma_buf[5][x][y][1] |= ((rgb & G_MASK(5)) ? 0x2 : 0) << offset;
}

void plot(uint16_t *colors, int src_x, int src_y, int src_w, int src_h, int x, int y, int w, int h) {
  colors += src_y*src_w+src_x;
  for (int i = y; i < y+h; ++i) {
    for (int j = x; j < x+w; ++j) {
      draw_pixel(j, i, *colors++);
    }
    colors += (src_w-w);
  }
}

void draw_line_888(int x1, int y1, int x2, int y2, int r, int g, int b) {
  draw_line(x1, y1, x2, y2, to_rgb565(r, g, b));
}

void draw_line(int x1, int y1, int x2, int y2, uint16_t rgb) {
  if (x1 == x2) {
    if (y1 > y2) {
      int t = y1; y1 = y2; y2 = t;
    }
    for (int y = y1; y < y2; ++y) {
      draw_pixel(x1, y, rgb);
    }
  } else if (y1 == y2) {
    if (x1 > x2) {
      int t = x1; x1 = x2; x2 = t;
    }
    for (int x = x1; x < x2; ++x) {
      draw_pixel(x, y1, rgb);
    }
  } else {
    int dx = abs(x2 - x1), dy = (y2 - y1);
    int step = dx > dy ? dx : dy;
    float fdx = (x2-x1)*1.0f/step, fdy = (y2-y1)*1.0f/step;
    for (int i = 0; i < step; ++i) {
      draw_pixel(round(x1+fdx*i), round(y1+fdy*i), rgb);
    }
  }
}

void draw_rect(int x, int y, int w, int h, uint16_t rgb) {
  draw_line(x, y, x, y+h, rgb);
  draw_line(x, y, x+w, y, rgb);
  draw_line(x+w-1, y, x+w-1, y+h, rgb);
  draw_line(x, y+h-1, x+w, y+h-1, rgb);
}

void draw_rect_888(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  draw_rect(x, y, w, h, to_rgb565(r, g, b));
}

void draw_fill_rect(int x, int y, int w, int h, uint16_t border_rgb, uint16_t fill_rgb) {
  draw_rect(x, y, w, h, border_rgb);
  for (int i = y+1; i < y+h-1; ++i) {
    draw_line(x+1, i, x+w-1, i, fill_rgb);
  }
}

int draw_char(wchar_t wch, int x, int y, uint16_t rgb) {
  int id = 0;
  while (fonts[id].codepoint != wch && id < chr_num) {
    ++id;
  }
  if (id >= chr_num) {
    return 0;
  }
  uint64_t d = fonts[id].data;
  int w = fonts[id].wh>>4;
  for (int j = y; j < y+8; ++j) {
    for (int i = x; i < w+x; ++i) {
      if (d & 0x1) {
        draw_pixel(i, j, rgb);
      }
      d >>= 1;
    }
  }
  return w;
}

int draw_text(wchar_t *wstr, int x, int y, uint16_t rgb) {
  int w = 0, len = x;
  wchar_t *str = wstr;
  for (int i = 0; i < wcslen(wstr); ++i) {
    len += draw_char(*str++, len, y, rgb) + 1;
  }
  return len;
}

void clear_region(int x, int y, int w, int h) {
  uint8_t mask, offset, xoffset;
  for (int i = x; i <= x+w; ++i) {
    for (int j = y; j <= y+h; ++j) {
      draw_pixel(i, j, 0);
    }
  }
}

void clear_frame() {
  for (int x = 0; x < 64; ++x) {
    for (int y = 0; y < 64; ++y) {
      draw_pixel(x, y, 0);
    }
  }
}

void present_frame() {
  for (int x = 0; x < 64; ++x) {
    for (int y = 0; y < 64; ++y) {
      update_pixel(x, y);
    }
  }
  cur = !cur;
}
