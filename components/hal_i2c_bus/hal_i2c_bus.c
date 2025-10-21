// components/hal_i2c_bus/hal_i2c_bus.c
#include "hal_i2c_bus.h"
#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"

#include "esp_log.h"
static const char *TAG = "hal_i2c_bus.c: ";

static bool i2c_initialized = false;
static i2c_master_bus_handle_t g_bus_handle = NULL; // Хендл нашої єдиної шини

esp_err_t i2c_bus_init() {

    if (i2c_initialized) {
        ESP_LOGD(TAG, "i2c_bus_init: i2c_initialized ALREADY\n");
        return ESP_OK;
    }
    
    bsp_i2c_config_t i2c_config;
    g_bsp->get_i2c_config(&i2c_config);

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_config.i2c_port,
        .scl_io_num = i2c_config.pin_scl,
        .sda_io_num = i2c_config.pin_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    }; 

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &g_bus_handle));
    
    
    esp_err_t ret = i2c_master_probe(g_bus_handle, 0x36, -1);
    ESP_LOGD(TAG, "i2c_master_probe: AS5601_I2C_ADDRESS -> %s\n", esp_err_to_name(ret));
    

    ret = i2c_master_probe(g_bus_handle, 0x18, -1);
    ESP_LOGD(TAG, "i2c_master_probe: AIC3120_I2C_ADDR -> %s\n", esp_err_to_name(ret));

    i2c_initialized = true;
    
    ESP_LOGD(TAG, "i2c_bus_init: IS NOW i2c_initialized \n");
    return ESP_OK;
}




esp_err_t i2c_bus_add_device(uint8_t device_address, i2c_master_dev_handle_t* dev_handle_out) {
   
    if (!i2c_initialized) 
        return ESP_ERR_INVALID_STATE; // Шину не ініціалізовано
    

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address,
        .scl_speed_hz = 400000, // Можна передавати як параметр
    };

    // Додаємо пристрій до шини і повертаємо його унікальний хендл
    return i2c_master_bus_add_device(g_bus_handle, &dev_cfg, dev_handle_out);
}


bool i2c_init_status (void){
    return i2c_initialized;
}