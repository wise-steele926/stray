#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Ініціалізує драйвер енкодера AS5601.
 * @return ESP_OK у разі успіху.
 */
esp_err_t hal_encoder_init(void);

/**
 * @brief Отримує "сире" 12-бітне значення кута з енкодера.
 * @param[out] angle Вказівник для збереження значення кута (0-4095).
 * @return ESP_OK у разі успіху.
 */
esp_err_t hal_encoder_get_angle(uint16_t* angle);