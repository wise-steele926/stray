#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

// #include "app_events.h"
#include "hal_net.h"
#include "shared_resources.h"
#include "nvs_manager.h"
#include "wifi_provisioning.h"
// #include "channel_manager.h"

#include "lwip/err.h"
#include "lwip/sys.h"


#define PROV_SSID "StrayRadio-Setup"
#define PROV_PASS "12345678"

static const char *TAG = "hal_net";

extern QueueHandle_t main_queue_event;

static bool wifi_scan_and_connect_to_best(void);


// Це наш обробник подій
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    app_event_t ev;
    ev.source = EVENT_SOURCE_WIFI;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // esp_wifi_connect();
        ev.CmdCode = WIFI_EVENT_STA_READY;
        xQueueSend(main_queue_event, &ev, 0);
        ESP_LOGW(TAG, "WIFI_EVENT_STA_START...");

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGW(TAG, "Успішно підключено! Отримано IP: " IPSTR, IP2STR(&event->ip_info.ip));

        esp_ip4addr_ntoa(&event->ip_info.ip, my_ip_str, sizeof(my_ip_str));
        ESP_LOGW(TAG, "Отримано IP: %s", my_ip_str);

        ev.CmdCode = WIFI_EVENT_CONNECTED;
        ev.payload.wifi.is_connected = true;
        xQueueSend(main_queue_event, &ev, 0);

        xEventGroupSetBits(system_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Не вдалося підключитися. Спроба ще раз...");
        
        ev.CmdCode = WIFI_EVENT_DISCONNECTED;
        ev.payload.wifi.is_connected = false;
        xQueueSend(main_queue_event, &ev, 0);
        // esp_wifi_connect();
    }
    
}

void wifi_init(void)
{
    // 1. Ініціалізація мережевого інтерфейсу та циклу подій
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // esp_netif_create_default_wifi_sta(); // Створюємо Wi-Fi станцію
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    // 2. Ініціалізація Wi-Fi з конфігурацією за замовчуванням
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Реєстрація обробників подій
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
   

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );

    wifi_config_t ap_config = {
        .ap = {
            .ssid = PROV_SSID,
            .ssid_len = strlen(PROV_SSID),
            .password = PROV_PASS,
            .max_connection = 4, // Ограничиваем количество клиентов
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
    // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));


    // 5. Запуск Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start() );
    // esp_wifi_connect();
    ESP_LOGD(TAG, "Wi-Fi ініціалізовано в режимі APSTA.");

    
}


void hal_wifi_connect (void){
    int8_t last_idx = nvs_manager_get_last_wifi_index();
    wifi_cred_t cred_to_try;
    EventBits_t bits = 0;

    if (last_idx >= 0 && nvs_manager_get_wifi_cred_by_index(last_idx, &cred_to_try)) {
        wifi_config_t sta_config = {0};
        strcpy((char*)sta_config.sta.ssid, cred_to_try.ssid);
        strcpy((char*)sta_config.sta.password, cred_to_try.pass);
        
        // Даємо команду підключитися
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
        ESP_LOGI(TAG, "З NVS : Швидка спроба: підключення до '%s'... pass: '%s'", cred_to_try.ssid, cred_to_try.pass);  
        bits = xEventGroupWaitBits(system_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));      
    }else {
        ESP_LOGE(TAG, "НЕма до кого підключаться");
    }

    // --- Очікування ---
     // Чекаємо 15 секунд

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi підключено!");
    } else {
        ESP_LOGW(TAG, "Швидка спроба не вдалася. Запускаємо План Б...");
        // --- План Б ---
        if (!wifi_scan_and_connect_to_best()) {
            wifi_provisioning_start();
            ESP_LOGW(TAG, "Запускаємо режим налаштування...");            
        }        
    }
}


static bool wifi_scan_and_connect_to_best(void) {
    ESP_LOGI(TAG, "План Б: Починаємо сканування мереж...");

    // 1. Запускаємо сканування
    wifi_scan_config_t scan_config = {0}; // Сканувати всі канали, всі SSID
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) {
        ESP_LOGE(TAG, "Не вдалося запустити сканування.");
        return false;
    }

    // 2. Отримуємо результати сканування
    uint16_t found_aps_count = 0;
    esp_wifi_scan_get_ap_num(&found_aps_count);
    if (found_aps_count == 0) {
        ESP_LOGW(TAG, "Сканування завершено, мереж не знайдено.");
        return false;
    }

    wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(found_aps_count * sizeof(wifi_ap_record_t));
    if (ap_list == NULL) {
        ESP_LOGE(TAG, "Не вдалося виділити пам'ять для результатів сканування.");
        return false;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&found_aps_count, ap_list));

    // 3. Отримуємо список збережених мереж з nvs_manager
    uint8_t saved_aps_count = nvs_manager_get_wifi_cred_count();
    if (saved_aps_count == 0) {
        ESP_LOGW(TAG, "В NVS немає збережених мереж.");
        free(ap_list);
        return false;
    }

    // 4. Шукаємо найкращий збіг
    int8_t best_rssi = -100;
    int best_match_index = -1;

    for (int i = 0; i < saved_aps_count; i++) {
        wifi_cred_t saved_cred;
        if (nvs_manager_get_wifi_cred_by_index(i, &saved_cred)) {
            for (int j = 0; j < found_aps_count; j++) {
                // Порівнюємо SSID
                if (strcmp(saved_cred.ssid, (char *)ap_list[j].ssid) == 0) {
                    ESP_LOGI(TAG, "Знайдено збіг: '%s', RSSI: %d", saved_cred.ssid, ap_list[j].rssi);
                    // Якщо сигнал цієї мережі кращий, запам'ятовуємо її
                    if (ap_list[j].rssi > best_rssi) {
                        best_rssi = ap_list[j].rssi;
                        best_match_index = i;
                    }
                }
            }
        }
    }
    
    free(ap_list); // Звільняємо пам'ять, використану для результатів сканування

    // 5. Підключаємося до найкращої знайденої мережі
    if (best_match_index != -1) {
        wifi_cred_t best_cred;
        nvs_manager_get_wifi_cred_by_index(best_match_index, &best_cred);
        nvs_manager_set_last_wifi_index(best_match_index);
        
        ESP_LOGI(TAG, "План Б: Спроба підключення до найкращої мережі '%s' (RSSI: %d)", best_cred.ssid, best_rssi);
        
        wifi_config_t sta_config = {0};
        strcpy((char*)sta_config.sta.ssid, best_cred.ssid);
        strcpy((char*)sta_config.sta.password, best_cred.pass);

        ESP_ERROR_CHECK(esp_wifi_disconnect()); // Розриваємо попередню невдалу спробу
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        ESP_ERROR_CHECK(esp_wifi_connect());

        return true; // Повідомляємо main, що ми почали спробу підключення
    }
    // 6. Якщо збігів немає
    ESP_LOGW(TAG, "Не знайдено жодної відомої мережі в ефірі.");
    return false;
}