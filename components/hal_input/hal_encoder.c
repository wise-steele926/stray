#include "hal_encoder.h"
#include "hal_i2c_bus.h" // Наш менеджер шини
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
 #include "shared_resources.h"
 
static const char *TAG = "HAL_ENCODER";

// --- Константи з даташиту на AS5601 ---
#define AS5601_I2C_ADDRESS      0x36 // Стандартна адреса
#define AS5601_REG_RAW_ANGLE_H  0x0C // Старший байт сирого кута
#define AS5601_REG_RAW_ANGLE_L  0x0D // Молодший байт сирого кута

#define I2C_MASTER_TIMEOUT_MS       1000

extern EventGroupHandle_t system_event_group;

// Локальний хендл для нашого пристрою на шині I2C
static i2c_master_dev_handle_t i2c_encoder_dev_handle = NULL;

esp_err_t hal_encoder_init(void) {
    ESP_LOGD(TAG, "Запускаємо hal_encoder_init");
    esp_err_t ret;
    // bsp_i2c_config_t i2c_config;
    // g_bsp->get_i2c_config(&i2c_config);

    // if (!i2c_init_status ()){
    ESP_LOGD(TAG, "Запускаємо i2c_bus_init");
    bsp_i2c_config_t i2c_config;
    g_bsp->get_i2c_config(&i2c_config);
    ret = i2c_bus_init(&i2c_config);
    if (ret)
        xEventGroupSetBits(system_event_group, I2C_BUS_INIT_BIT);
    // }

    ret = i2c_bus_add_device(AS5601_I2C_ADDRESS, &i2c_encoder_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add encoder to I2C bus: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "AS5601 Encoder initialized successfully.\n");
    return ESP_OK;

}




esp_err_t hal_encoder_get_angle(uint16_t* angle) {
    if (!i2c_encoder_dev_handle) {
        return ESP_ERR_INVALID_STATE; // Драйвер не ініціалізовано
    }
    if (!angle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg_addr = AS5601_REG_RAW_ANGLE_H;
    uint8_t data_received[2] = {0}; 
    uint16_t raw = 0; 

    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_encoder_dev_handle, &reg_addr, 1, data_received, 2, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));

    raw = data_received[0] << 8 | data_received[1];
    *angle = raw;//((uint16_t)data_received[0] << 8) | data_received[1];
    // ESP_LOGD(TAG, "RAW_ANGLE = %d | %X_%X",raw, data_received[0], data_received[1]);

    return ESP_OK;
}