#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/spi_master.h"

#include "hal_display.h"
#include "hal_display_helpers.h"
#include "hal_display_logic.h"
// Замість одного хедера тепер потрібні ці два
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
#include "esp_lcd_panel_vendor.h"

#include "ui.h"

static const char *TAG = "GUI_APP";



int32_t g_encoder_diff = 0;
lv_indev_state_t g_encoder_btn_state = LV_INDEV_STATE_RELEASED;

static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    data->enc_diff = g_encoder_diff;
    g_encoder_diff = 0; // Скидаємо лічильник, бо ми вже передали значення

    data->state = g_encoder_btn_state;
    // ESP_LOGD(TAG, "g_encoder_diff: %d | g_encoder_btn_state: %d", (uint16_t)g_encoder_diff, g_encoder_btn_state);
}

// Глобальна змінна для групи
static lv_group_t * g = NULL;

static void add_screen1_objects(void)
{
    if (g == NULL) return;
    
    // lv_group_add_obj(g, ui_Button1); 
    // lv_group_add_obj(g, ui_Slider1);
    // lv_group_add_obj(g, ui_Slider2);
}

static void add_screen2_objects(void)
{
    if (g == NULL) return;
    
    // lv_group_add_obj(g, ui_Roller1);
    // lv_group_add_obj(g, ui_Slider3);
    // lv_group_add_obj(g, ui_TabView1);
}

static void lvgl_port_indev_init(void)
{
    lv_indev_t * encoder_indev = lv_indev_create();
    lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(encoder_indev, encoder_read_cb);

    g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(encoder_indev, g);

    if (uic_roller_peer_list) {
        lv_group_add_obj(g, ui_rollerpeerlist);
        ESP_LOGD(TAG, "Roller peer list added to the input group.");
    }
    
    lv_group_set_editing(g, true);
    // // Так само можна додати слайдер гучності
    // if (uic_slider_volume) {
    //      lv_group_add_obj(g, ui_slidervolume);
    // }
    
    // // Додаємо всі елементи одразу
    // add_screen1_objects();
    // add_screen2_objects();
}


void init_gui()//EventGroupHandle_t gui_init, const int GUI_SETUP_BIT
{
    initialize_backlight();

    esp_lcd_panel_handle_t panel_handle = NULL;
    
    ESP_LOGD(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = {
        .sclk_io_num = TFT_SCLK,
        .mosi_io_num = TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_HOST_ID, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGD(TAG, "Init panel io");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT_DC,
        .cs_gpio_num = TFT_CS,
        .spi_mode = 0, //
        .pclk_hz = TFT_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TFT_HOST_ID, &io_config, &io_handle));

    ESP_LOGD(TAG, "Install ST7789 panel driver");
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RST,
        .rgb_ele_order = COLOR_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    lcd_set_brightness (100);

    ESP_LOGD(TAG, "Initialize LVGL port");
    const lvgl_port_cfg_t lvgl_cfg = { .task_priority = 4, .task_stack = 8192, .timer_period_ms = 5 };
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
    
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .control_handle = NULL, // Ми керуємо підсвіткою напряму, тому тут NULL
        .hres = TFT_HEIGHT,
        .vres = TFT_WIDTH,
        .buffer_size = TFT_HEIGHT * 20,
        .double_buffer = true,
        .flags = { 
            .buff_dma = true,
            .swap_bytes = true
        }
    };
    lv_disp_t * disp = lvgl_port_add_disp(&disp_cfg);
    
    if (lvgl_port_lock(0)) {
        ESP_LOGD(TAG, "Setting screen orientation");
        if (disp) 
            lv_disp_set_rotation(disp, LV_DISPLAY_ROTATION_270);    

        
        ui_init();

        lvgl_port_indev_init();

        lvgl_port_unlock();
    }
    // gui_show_main_button(false);
    // xEventGroupSetBits(gui_init, GUI_SETUP_BIT);
    // ESP_LOGD(TAG, "GUI Initialized with test objects!");
    
    xEventGroupSetBits(system_event_group, GUI_INITIALIZED_BIT);    
    // ESP_LOGW(TAG, "GUI_INITIALIZED_BIT started"); //clean
}
 