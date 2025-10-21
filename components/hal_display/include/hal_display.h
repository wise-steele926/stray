#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include "lvgl.h"


#define TFT_HOST_ID SPI2_HOST
#define TFT_PIXEL_CLOCK_HZ (20 * 1000 * 1000)

#define TFT_WIDTH  240
#define TFT_HEIGHT 240 

#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_CS 5
#define TFT_DC 15
#define TFT_RST 2
#define TFT_BL 13


extern int32_t g_encoder_diff;
extern lv_indev_state_t g_encoder_btn_state;

void init_gui();//EventGroupHandle_t gui_init, const int GUI_SETUP_BIT


#endif // HAL_DISPLAY_H