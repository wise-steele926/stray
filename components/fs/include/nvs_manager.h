// #include nvs_manager.h

#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "shared_resources.h"

// --- Приватні дані модуля ---
#define MAX_WIFI_CREDENTIALS 5
#define DEVICE_NAME_LEN 16

// Структура для однієї мережі
typedef struct {
    char ssid[33];
    char pass[65];
} wifi_cred_t;

// Структура app_config_t тепер приватна і живе тільки тут
typedef struct {
    char device_name[DEVICE_NAME_LEN];
    uint8_t wifi_count;
    uint8_t last_wifi_index;
    wifi_cred_t wifi_credentials[MAX_WIFI_CREDENTIALS];
} app_config_t;

// Ініціалізує сам NVS (nvs_flash_init)
void nvs_manager_init(void);
void nvs_manager_save_config(void);

// --- Функції-гетери (для читання даних) ---
const char* nvs_manager_get_device_name(void);
uint8_t nvs_manager_get_wifi_cred_count(void);
uint8_t nvs_manager_get_last_wifi_index(void);
bool nvs_manager_get_wifi_cred_by_index(int index, wifi_cred_t *cred);

// --- Функції-сетери (для зміни даних) ---
void nvs_manager_set_device_name(const char* name);
// Додає нову мережу. Повертає true, якщо успішно.
void nvs_manager_set_last_wifi_index(int8_t index);
bool nvs_manager_add_wifi_credential(const char* ssid, const char* pass);


void nvs_manager_print_ram_config(void); //todo clear after debug
void nvs_manager_print_nvs_content(void); // For Debugging

#endif