#include "nvs_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include "shared_resources.h"

static const char *TAG = "NVS_MANAGER";



static app_config_t g_app_config; // Єдине джерело правди про конфігурацію в RAM
static SemaphoreHandle_t g_config_mutex;

// --- Прототипи внутрішніх функцій ---
static void load_config_from_nvs(void);

// --- Реалізація ---

void nvs_manager_init(void) {
    
    ESP_LOGI(TAG, "nvs_manager_init");
    // Ініціалізація NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Створюємо м'ютекс для захисту g_app_config
    g_config_mutex = xSemaphoreCreateMutex();
    
    // Завантажуємо конфігурацію з NVS в RAM
    load_config_from_nvs();
}

static void load_config_from_nvs(void) {
    
    ESP_LOGI(TAG, "load_config_from_nvs");

    nvs_handle_t nvs_handle;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
            // Завантажуємо ім'я
            size_t required_size = sizeof(g_app_config.device_name);

            if (nvs_get_str(nvs_handle, "device_name", g_app_config.device_name, &required_size) != ESP_OK) {
                strcpy(g_app_config.device_name, "StrayRadio"); // Ім'я за замовчуванням
            }
            ESP_LOGI(TAG, "device_name: %s", g_app_config.device_name);
            // Завантажуємо кількість мереж
            
            if (nvs_get_u8(nvs_handle, "wifi_count", &g_app_config.wifi_count) != ESP_OK) {
                g_app_config.wifi_count = 0;
            }
            ESP_LOGI(TAG, "wifi_count: %u", g_app_config.wifi_count);

            if (nvs_get_u8(nvs_handle, "last_wifi_index", &g_app_config.last_wifi_index) != ESP_OK) {
                g_app_config.last_wifi_index = 0;
            }
            ESP_LOGI(TAG, "last_wifi_index: %u", g_app_config.last_wifi_index);

            // Завантажуємо самі мережі
            for (uint8_t i = 0; i < g_app_config.wifi_count; i++) {
                char key_ssid[16];
                char key_pass[16];

                // Генеруємо ключі для поточної ітерації, напр. "wifi_ssid_0", "wifi_pass_0"
                snprintf(key_ssid, sizeof(key_ssid), "wifi_ssid_%u", i);
                snprintf(key_pass, sizeof(key_pass), "wifi_pass_%u", i);

                // Визначаємо максимальний розмір буфера для читання
                size_t ssid_len = sizeof(g_app_config.wifi_credentials[i].ssid);
                size_t pass_len = sizeof(g_app_config.wifi_credentials[i].pass);

                // Читаємо SSID та пароль з NVS у нашу структуру в RAM
                nvs_get_str(nvs_handle, key_ssid, g_app_config.wifi_credentials[i].ssid, &ssid_len);
                nvs_get_str(nvs_handle, key_pass, g_app_config.wifi_credentials[i].pass, &pass_len);
                
                ESP_LOGI(TAG, "[%u]wifi_ssid: %s | wifi_pass: %s", i, g_app_config.wifi_credentials[i].ssid, g_app_config.wifi_credentials[i].pass);
            }
            nvs_close(nvs_handle);
        } else {
            // Якщо NVS не відкрито, встановлюємо значення за замовчуванням
            strcpy(g_app_config.device_name, "StrayRadio");
            g_app_config.wifi_count = 0;
        }
        xSemaphoreGive(g_config_mutex);
    }
}

void nvs_manager_save_config(void) {
    ESP_LOGI(TAG, "nvs_manager_save_config");

    nvs_handle_t nvs_handle;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_str(nvs_handle, "device_name", g_app_config.device_name);
            nvs_set_u8(nvs_handle, "wifi_count", g_app_config.wifi_count);
            nvs_set_u8(nvs_handle, "last_wifi_index", g_app_config.last_wifi_index);
            for (uint8_t i = 0; i < g_app_config.wifi_count; i++) {
                char key_ssid[16];
                char key_pass[16];

                // Генеруємо ключі для поточної ітерації
                snprintf(key_ssid, sizeof(key_ssid), "wifi_ssid_%u", i);
                snprintf(key_pass, sizeof(key_pass), "wifi_pass_%u", i);

                // Записуємо SSID та пароль з g_app_config в NVS
                nvs_set_str(nvs_handle, key_ssid, g_app_config.wifi_credentials[i].ssid);
                nvs_set_str(nvs_handle, key_pass, g_app_config.wifi_credentials[i].pass);
            }
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
            ESP_LOGI(TAG, "Конфігурацію збережено в NVS.");
        }
        xSemaphoreGive(g_config_mutex);
    }
}

// --- Реалізація гетерів ---

const char* nvs_manager_get_device_name(void) {
    ESP_LOGI(TAG, "nvs_manager_get_device_name");
    const char* name_ptr = NULL;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        // Увага: повертаємо вказівник на дані всередині структури.
        // Це безпечно, бо ми не збираємося їх змінювати ззовні.
        name_ptr = g_app_config.device_name;
        xSemaphoreGive(g_config_mutex);
    }
    // ПРИМІТКА: такий підхід не є 100% безпечним, якщо інша задача змінить ім'я після повернення вказівника.
    // Для максимальної безпеки гетер мав би копіювати рядок у буфер, наданий користувачем.
    // Але для нашої задачі цього достатньо.
    return name_ptr;
}

uint8_t nvs_manager_get_wifi_cred_count(void) {
    ESP_LOGI(TAG, "nvs_manager_get_wifi_cred_count");
    uint8_t count = 0;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        count = g_app_config.wifi_count;
        xSemaphoreGive(g_config_mutex);
    }
    return count;
}

uint8_t nvs_manager_get_last_wifi_index(void) {
    ESP_LOGI(TAG, "nvs_manager_get_last_wifi_index");
    uint8_t index = 0;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        index = g_app_config.last_wifi_index;
        xSemaphoreGive(g_config_mutex);
    }
    return index;
}


bool nvs_manager_get_wifi_cred_by_index(int index, wifi_cred_t *cred) {
    ESP_LOGI(TAG, "nvs_manager_get_wifi_cred_by_index");
    bool success = false;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        // Перевіряємо, чи індекс знаходиться в допустимих межах
        if (cred != NULL && index >= 0 && index < g_app_config.wifi_count) {
            *cred = g_app_config.wifi_credentials[index]; // Копіюємо структуру
            success = true;
        }
        xSemaphoreGive(g_config_mutex);
    }
    return success;
}

// --- РЕАЛІЗАЦІЯ СЕТЕРІВ ---

void nvs_manager_set_device_name(const char* name) {
    ESP_LOGI(TAG, "nvs_manager_set_device_name");
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        if (name != NULL) {
            strncpy(g_app_config.device_name, name, DEVICE_NAME_LEN - 1);
            g_app_config.device_name[DEVICE_NAME_LEN - 1] = '\0'; // Гарантуємо нуль-термінацію
        }
        xSemaphoreGive(g_config_mutex);
    }
}

void nvs_manager_set_last_wifi_index(int8_t index){
    ESP_LOGI(TAG, "nvs_manager_set_last_wifi_index");
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        g_app_config.last_wifi_index = index;        
        xSemaphoreGive(g_config_mutex);
    }
}

bool nvs_manager_add_wifi_credential(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "nvs_manager_add_wifi_credential");
    bool success = false;
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        // Перевіряємо, чи є вільне місце в масиві
        if (g_app_config.wifi_count < MAX_WIFI_CREDENTIALS) {
            int index = g_app_config.wifi_count;

            // Копіюємо нові дані
            strncpy(g_app_config.wifi_credentials[index].ssid, ssid, sizeof(g_app_config.wifi_credentials[index].ssid) - 1);
            strncpy(g_app_config.wifi_credentials[index].pass, pass, sizeof(g_app_config.wifi_credentials[index].pass) - 1);
            // Гарантуємо нуль-термінацію
            g_app_config.wifi_credentials[index].ssid[sizeof(g_app_config.wifi_credentials[index].ssid) - 1] = '\0';
            g_app_config.wifi_credentials[index].pass[sizeof(g_app_config.wifi_credentials[index].pass) - 1] = '\0';
            
            // Збільшуємо лічильник
            g_app_config.wifi_count++;
            success = true;
        } else {
            ESP_LOGW(TAG, "Не вдалося додати нову мережу: досягнуто ліміту.");
        }
        xSemaphoreGive(g_config_mutex);
    }
    return success;
}

/**
 * @brief Виводить у лог поточний стан конфігурації, що зберігається в RAM (g_app_config).
 */
void nvs_manager_print_ram_config(void) {
    if (xSemaphoreTake(g_config_mutex, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "--- Поточна конфігурація в RAM ---");
        ESP_LOGI(TAG, "Ім'я пристрою: %s", g_app_config.device_name);
        ESP_LOGI(TAG, "Індекс останньої Wi-Fi мережі: %d", g_app_config.last_wifi_index);
        ESP_LOGI(TAG, "Кількість збережених Wi-Fi мереж: %u", g_app_config.wifi_count);

        for (uint8_t i = 0; i < g_app_config.wifi_count; i++) {
            ESP_LOGI(TAG, "  Мережа #%u: SSID=[%s], Pass=[%s]", i, 
                     g_app_config.wifi_credentials[i].ssid, g_app_config.wifi_credentials[i].pass);
        }
        ESP_LOGI(TAG, "------------------------------------");

        xSemaphoreGive(g_config_mutex);
    }
}

/**
 * @brief Відкриває NVS і виводить у лог всі відомі параметри, що там зберігаються.
 */
void nvs_manager_print_nvs_content(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося відкрити NVS для читання.");
        return;
    }

    ESP_LOGI(TAG, "--- Вміст NVS ('storage') ---");

    // Читаємо ім'я пристрою
    char device_name[DEVICE_NAME_LEN];
    size_t required_size = sizeof(device_name);
    if (nvs_get_str(nvs_handle, "device_name", device_name, &required_size) == ESP_OK) {
        ESP_LOGI(TAG, "Ключ 'device_name': %s", device_name);
    } else {
        ESP_LOGW(TAG, "Ключ 'device_name' не знайдено.");
    }

    // Читаємо індекс останньої мережі
    int8_t last_wifi_index = -1;
    if (nvs_get_i8(nvs_handle, "last_wifi_index", &last_wifi_index) == ESP_OK) {
        ESP_LOGI(TAG, "Ключ 'wifi_last_active': %d", last_wifi_index);
    } else {
        ESP_LOGW(TAG, "Ключ 'wifi_last_active' не знайдено.");
    }
    
    // Читаємо кількість мереж
    uint8_t wifi_count = 0;
    if (nvs_get_u8(nvs_handle, "wifi_count", &wifi_count) == ESP_OK) {
        ESP_LOGI(TAG, "Ключ 'wifi_count': %u", wifi_count);

        // В циклі читаємо кожну мережу
        for (uint8_t i = 0; i < wifi_count; i++) {
            char key_ssid[16], key_pass[16];
            char ssid_buf[33], pass_buf[65];
            size_t ssid_len = sizeof(ssid_buf);
            size_t pass_len = sizeof(pass_buf);
            
            snprintf(key_ssid, sizeof(key_ssid), "wifi_ssid_%u", i);
            snprintf(key_pass, sizeof(key_pass), "wifi_pass_%u", i);

            if (nvs_get_str(nvs_handle, key_ssid, ssid_buf, &ssid_len) == ESP_OK) {
                if (nvs_get_str(nvs_handle, key_pass, pass_buf, &pass_len) == ESP_OK) {
                    ESP_LOGI(TAG, "  Мережа #%u: SSID=[%s], Pass=[%s]", i, ssid_buf, pass_buf);
                }
            } else {
                ESP_LOGW(TAG, "  Мережа #%u: не знайдено.", i);
            }
        }

    } else {
        ESP_LOGW(TAG, "Ключ 'wifi_count' не знайдено.");
    }

    ESP_LOGI(TAG, "------------------------------------");
    nvs_close(nvs_handle);
}