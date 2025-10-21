// components/hal_i2c_bus/hal_i2c_bus.h
#pragma once
#include <stdio.h>

#include "driver/i2c_master.h" // Використовуємо нові заголовки
#include "esp_err.h"
#include "bsp.h"

/**
 * @brief Ініціалізує спільну шину I2C.
 * @return ESP_OK у разі успіху.
 */
esp_err_t i2c_bus_init();

/**
 * @brief Додає новий пристрій на шину I2C.
 * @param device_address Адреса пристрою на шині.
 * @param[out] dev_handle_out Вказівник, куди буде збережено хендл пристрою.
 * @return ESP_OK у разі успіху.
 */
esp_err_t i2c_bus_add_device(uint8_t device_address, i2c_master_dev_handle_t* dev_handle_out);

bool i2c_init_status (void);