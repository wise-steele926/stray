#include "hal_display_helpers.h"
#include "hal_display.h"
#include "driver/ledc.h"
#include "ui.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "HAL_Disp_helpers:";


void initialize_backlight(void)
{
    ESP_LOGD(TAG, "Initialize LEDC for backlight");

    // 1. Налаштування таймера LEDC
    const ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_10_BIT, // Роздільна здатність 10 біт (0-1023)
        .freq_hz          = 5000,              // Частота PWM 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Налаштування каналу LEDC
    const ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = TFT_BL,
        .duty           = 0, // Початкова яскравість 0
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void lcd_set_brightness (int brightness)
{
    if (brightness > 100)
        brightness = 100;
    if (brightness < 0)
        brightness = 0;
        
    int duty = 1023 * (brightness/100.0);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

    ESP_LOGD(TAG, "set lcd brightness to: %d - duty: %d", brightness, duty);
}

// void gui_show_main_button(bool show)
// {
//     lv_obj_t* btn = ui_Button1;

//     lvgl_port_lock(0);

//     if (show) {
//         // lv_obj_clear_flag(ui_Button1, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_clear_state(btn, LV_STATE_DISABLED);
//     } else {
//         // lv_obj_add_flag(ui_Button1, LV_OBJ_FLAG_HIDDEN);
//         lv_obj_add_state(btn, LV_STATE_DISABLED);
//     }

//     lvgl_port_unlock();
// }