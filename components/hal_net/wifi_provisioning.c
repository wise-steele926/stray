#include "wifi_provisioning.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include "shared_resources.h"
#include "nvs_manager.h"

// ... та інші необхідні хедери

static const char *TAG = "WIFI_PROV";

// --- Прототипи обробників HTTP-запитів ---

// Ця функція буде викликана, коли користувач відкриє http://192.168.4.1
static esp_err_t get_handler(httpd_req_t *req);

// Ця функція буде викликана, коли користувач натисне "Зберегти" на веб-сторінці
static esp_err_t post_handler(httpd_req_t *req);

static esp_err_t favicon_get_handler(httpd_req_t *req);

static httpd_handle_t server = NULL;


// --- Основна логіка ---
void wifi_provisioning_start(void) {
    if (server) {
        ESP_LOGW(TAG, "Веб-сервер уже запущен.");
        return;
    }

    ESP_LOGI(TAG, "Запуск веб-сервера для настройки...");

    // Шаг 1: Создаем конфигурацию сервера
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Шаг 2: Регистрируем обработчики URI
    httpd_uri_t get_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = get_handler,
    };
    httpd_uri_t post_uri = {
        .uri      = "/save",
        .method   = HTTP_POST,
        .handler  = post_handler,
    };
    httpd_uri_t favicon_uri = {
        .uri      = "/favicon.ico",
        .method   = HTTP_GET,
        .handler  = favicon_get_handler,
    };

    
    // Шаг 3: Запускаем сервер и регистрируем обработчики
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &get_uri);
        httpd_register_uri_handler(server, &post_uri);
        httpd_register_uri_handler(server, &favicon_uri);
        ESP_LOGI(TAG, "Веб-сервер запущен на 192.168.4.1");
    } else {
        ESP_LOGE(TAG, "Не удалось запустить веб-сервер.");
    }
}

void wifi_provisioning_stop(void) {
    if (server) {
        ESP_LOGI(TAG, "Остановка веб-сервера...");
        httpd_stop(server);
        server = NULL;
    }
}

// --- Реалізація обробників ---

// Визначаємо статичні частини як константи
static const char HTML_HEADER[] =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>StrayRadio Setup</title><style>"
    "body{font-family:-apple-system,system-ui,BlinkMacSystemFont,'Segoe UI',Roboto,'Helvetica Neue',Arial,sans-serif; background:#2c3e50; color:#ecf0f1; text-align:center; padding:20px;}"
    "h1{color:#3498db;} form{background:#34495e; padding:20px; border-radius:8px; display:inline-block;}"
    "h3{margin-top:0;} select,input{width:100%; box-sizing:border-box; padding:12px; margin-bottom:15px; border-radius:5px; border:none; background:#ecf0f1; color:#2c3e50;}"
    "input[type=submit]{background:#3498db; color:#fff; font-weight:bold; font-size:1em; cursor:pointer;}"
    "</style></head><body><h1>StrayRadio Setup</h1>"
    "<form action='/save' method='post'>"
    "<h3>Choose Wi-Fi Network:</h3>"
    "<select name='ssid'>";

// "Підвал" сторінки з полем для пароля та кнопкою
static const char HTML_PASS[] =
    "</select><br>"
    "<h3>Enter Password:</h3>"
    "<input type='password' name='pass' placeholder='Password'><br><br>";

static const char HTML_FOOTER[] =
    "<input type='submit' value='Save & Reboot'>"
    "</form></body></html>";


static esp_err_t get_handler(httpd_req_t *req) {
     ESP_LOGI(TAG, "Обробка GET-запиту на /");

    // === Крок 1: Сканування мереж ===
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false
    };
    // Запускаємо блокуюче сканування. Це нормально, бо ми відповідаємо на HTTP-запит.
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося запустити сканування Wi-Fi");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint16_t num_aps = 0;
    esp_wifi_scan_get_ap_num(&num_aps);
    if (num_aps == 0) {
        ESP_LOGW(TAG, "Жодної точки доступу не знайдено");
        httpd_resp_sendstr(req, "<html><body><h1>No Wi-Fi networks found</h1><p>Please try again later.</p></body></html>");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "знайдено %d точок доступу",num_aps);

    // Виділяємо пам'ять під результати сканування
    wifi_ap_record_t *ap_info = (wifi_ap_record_t *)malloc(num_aps * sizeof(wifi_ap_record_t));
    if (ap_info == NULL) {
        ESP_LOGE(TAG, "Не вдалося виділити пам'ять для результатів сканування");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num_aps, ap_info));
    // === Крок 2: Динамічна генерація HTML ===
    // Виділяємо буфер для динамічної частини (списку <option>)
    // Кожен <option> займає ~30 + довжина SSID. Беремо з запасом 64 байти на запис.
    char *options_buffer = (char *)malloc(num_aps * 64);
    if (options_buffer == NULL) {
        ESP_LOGE(TAG, "Не вдалося виділити пам'ять для options_buffer");
        free(ap_info);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    options_buffer[0] = '\0'; // Починаємо з порожнього рядка

    // Заповнюємо буфер <option> тегами
    for (int i = 0; i < num_aps; i++) {
        char option_tag[100];
        snprintf(option_tag, sizeof(option_tag), "<option value='%s'>%s</option>", 
                 ap_info[i].ssid, ap_info[i].ssid);
        strcat(options_buffer, option_tag);
        ESP_LOGI(TAG, "Додано [%d] - %s",i, ap_info[i].ssid);
    }
    
    // Звільняємо пам'ять, використану для результатів сканування
    free(ap_info);

     // --- ДОДАНО: Формуємо поле для введення імені пристрою ---
    char name_field_buffer[200];
    snprintf(name_field_buffer, sizeof(name_field_buffer),
             "<h3>Set Device Name:</h3>"
             "<input type='text' name='devicename' value='%s'><br><br>",
             nvs_manager_get_device_name()); // Отримуємо поточне ім'я


    // === Крок 3: Відправка сторінки частинами ===
    // Це ефективніше по пам'яті, ніж створювати один гігантський буфер
    httpd_resp_sendstr_chunk(req, HTML_HEADER);
    httpd_resp_sendstr_chunk(req, options_buffer);
    httpd_resp_sendstr_chunk(req, HTML_PASS);
    httpd_resp_sendstr_chunk(req, name_field_buffer); // Відправляємо поле з ім'ям
    httpd_resp_sendstr_chunk(req, HTML_FOOTER);

    // Завершуємо відповідь (відправляємо порожній "чанк")
    httpd_resp_sendstr_chunk(req, NULL);

    // === Крок 4: Звільнення пам'яті ===
    free(options_buffer);

    return ESP_OK;
}

//todo add to readme a menuconfig HTTP server 1024 bytes header
static esp_err_t post_handler(httpd_req_t *req) {
    // --- Крок 1: Підготовка до зчитування тіла запиту ---
    char *buf;
    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;

    // Перевіряємо, чи є тіло запиту
    if (total_len >= 256) { // Обмеження, щоб уникнути виділення занадто багато пам'яті
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    buf = malloc(total_len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Не вдалося виділити пам'ять для тіла POST-запиту");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // --- Крок 2: Зчитування тіла POST-запиту ---
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            // Помилка або кінець з'єднання
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                // Тайм-аут
                continue;
            }
            ESP_LOGE(TAG, "Помилка при зчитуванні тіла POST-запиту");
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read request body");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0'; // Гарантуємо нуль-термінацію

    // --- Крок 3: Парсинг отриманих даних ---
    char ssid[33] = {0};
    char pass[65] = {0};
    char devicename[17] = {0}; // Розмір вашого sender_name + 1

    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "pass", pass, sizeof(pass));
    httpd_query_key_value(buf, "devicename", devicename, sizeof(devicename));

    free(buf); // Буфер більше не потрібен

    ESP_LOGI(TAG, "Отримано дані з форми: SSID=[%s],PASS =[%s] Name=[%s]", ssid, pass, devicename);

   
    nvs_manager_set_device_name(devicename);

    if ( nvs_manager_add_wifi_credential(ssid, pass)){
        const char *resp_str = "<h1>Settings Saved!</h1><p>Device is rebooting...</p>";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
        nvs_manager_save_config(); // todo send to queue msg and try to connect instantly to choosed network 
        ESP_LOGI(TAG, "Налаштування збережено. Перезавантаження через 5 секунди...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    else {
        const char *resp_str = "<h1>SOMETHING WENT WRONG!</h1><p>TRY again...</p>";
        httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}


// Додайте цю функцію у wifi_provisioning.c
static esp_err_t favicon_get_handler(httpd_req_t *req) {
    // Просто відповідаємо статусом "204 No Content"
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}