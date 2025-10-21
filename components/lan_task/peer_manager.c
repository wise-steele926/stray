#include "peer_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h" // Для esp_timer_get_time()
#include <string.h>
#include <stdio.h>
#include "shared_resources.h"

static const char *TAG = "PEER_MANAGER";

// --- Внутрішні (приватні) змінні модуля ---

// Наша "Адресна книга" - масив для зберігання інформації про учасників.
static peer_info_t g_peer_list[MAX_PEERS];
// М'ютекс для захисту доступу до g_peer_list з різних задач.
static SemaphoreHandle_t g_peer_list_mutex;

extern QueueHandle_t main_queue_event;

// --- Реалізація публічних функцій ---

void peer_manager_init(void) {
    // esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGD(TAG, "peer_manager_init - started");
    // Крок 1: Створіть м'ютекс g_peer_list_mutex за допомогою xSemaphoreCreateMutex().
    // Обов'язково перевірте, що м'ютекс було створено успішно.
    g_peer_list_mutex = xSemaphoreCreateMutex();
    if (g_peer_list_mutex == NULL) {
        ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
        return; // Аварійне завершення
    }
    ESP_LOGD(TAG, "g_peer_list_mutex - created");
    // Крок 2: Очистіть весь масив g_peer_list. Найпростіше це зробити за допомогою memset().
    // Це гарантує, що при старті всі слоти будуть неактивними (is_active = false).
    memset(g_peer_list, 0, sizeof(g_peer_list));

    // Крок 1: Створюємо запис "All Users" на нульовій позиції
    g_peer_list[0].is_active = true;
    g_peer_list[0].sn = 0; // Використовуємо SN=0 як ознаку broadcast
    g_peer_list[0].ip_address = 0; // Немає конкретного IP
    strncpy(g_peer_list[0].name, "broadcast", PEER_NAME_LEN - 1);
    g_peer_list[0].name[PEER_NAME_LEN - 1] = '\0';

    ESP_LOGD(TAG, "масив g_peer_list очищено");
}  

void peer_manager_update_peer(uint64_t sn, const char* name, uint32_t ip_address) {
    ESP_LOGD(TAG, "peer_manager_update_peer");
    
    // Крок 1: Захопіть м'ютекс g_peer_list_mutex. Використовуйте portMAX_DELAY, щоб чекати вічно, якщо потрібно.
    if (xSemaphoreTake(g_peer_list_mutex, portMAX_DELAY) == pdTRUE) {   
        // Крок 2: Отримайте поточний час за допомогою esp_timer_get_time(). Це буде наш timestamp.
        int64_t timestamp = esp_timer_get_time();
        
        // uint8_t peer_num;
        // uint8_t free_slot = 0xff;
        bool peer_found = false;

        for (int i = 0; i < MAX_PEERS; i++) {
            if (g_peer_list[i].is_active && g_peer_list[i].sn == sn) {
                g_peer_list[i].ip_address = ip_address;
                g_peer_list[i].last_seen_timestamp_us = timestamp;
                ESP_LOGD(TAG, "peer: %s[%d] - is ALIVE", g_peer_list[i].name, i);
                peer_found = true;
                break; // Знайшли, виходимо з циклу
            }
        }

        // Якщо не знайшли, то додаємо нового
        if (!peer_found) {
            int free_slot = -1;
            // Шукаємо вільний слот
            for (int i = 0; i < MAX_PEERS; i++) {
                if (!g_peer_list[i].is_active) {
                    free_slot = i;
                    break;
                }
            }

            if (free_slot != -1) {
                g_peer_list[free_slot].is_active = true;
                g_peer_list[free_slot].sn = sn;
                g_peer_list[free_slot].ip_address = ip_address;
                g_peer_list[free_slot].last_seen_timestamp_us = timestamp;
                // Використовуйте strncpy для безпеки
                strncpy(g_peer_list[free_slot].name, name, PEER_NAME_LEN - 1);
                g_peer_list[free_slot].name[PEER_NAME_LEN - 1] = '\0';
                ESP_LOGD(TAG, "peer: %s[%d] - ADDED to list", g_peer_list[free_slot].name, free_slot);

                app_event_t event = { .source = EVENT_SOURCE_CHANNEL, .CmdCode = CHANNEL_EVENT_PEER_LIST_CHANGED };
                xQueueSend(main_queue_event, &event, 0);

            } else {                
                ESP_LOGW(TAG, "No FREE slots for peer");
            }
        }
        xSemaphoreGive(g_peer_list_mutex);
    }
    else 
        ESP_LOGE(TAG, "could not xSemaphoreTake ");
}

void peer_manager_cleanup_inactive(void) {
    // Крок 1: Захопіть м'ютекс g_peer_list_mutex.
    uint8_t peer_num;
    if (xSemaphoreTake(g_peer_list_mutex, portMAX_DELAY) == pdTRUE) {   
        int64_t timestamp = esp_timer_get_time();
        for (peer_num = 0; peer_num < MAX_PEERS; peer_num++ ){
            
            if (g_peer_list[peer_num].sn == 0) {
                continue;
            }

            if (g_peer_list[peer_num].is_active)

                if (timestamp - g_peer_list[peer_num].last_seen_timestamp_us > PEER_INACTIVITY_TIMEOUT_MS ){
                    g_peer_list[peer_num].is_active = false;
                    ESP_LOGD(TAG, "peer: %s[%d] - DELETED from list",g_peer_list[peer_num].name, peer_num);

                    app_event_t event = { .source = EVENT_SOURCE_CHANNEL, .CmdCode = CHANNEL_EVENT_PEER_LIST_CHANGED };
                    xQueueSend(main_queue_event, &event, 0);
                }
        }
    // Крок 4: Звільніть м'ютекс.
        xSemaphoreGive(g_peer_list_mutex);
    }
}



int8_t peer_manager_get_peers(peer_info_t *peer_list, int max_peers) {
    if (max_peers < 1) {
        return 0; // Наданий буфер занадто малий
    }

    int8_t total_copied = 0;
    if (xSemaphoreTake(g_peer_list_mutex, portMAX_DELAY) == pdTRUE) {
        // Крок 2: Копіюємо реальних учасників, починаючи з індексу 1
        for (int i = 0; i < MAX_PEERS; i++) {
            if (g_peer_list[i].is_active) {
                // Перевіряємо, чи є ще місце в буфері
                if (total_copied < max_peers) {
                    peer_list[total_copied] = g_peer_list[i];
                    // ESP_LOGW(TAG, "peer_list[%d] = g_peer_list[%d];",total_copied, i);
                    total_copied++;                    
                } else {
                    ESP_LOGW(TAG, "No FREE slots for peer");
                    break;
                }                
            }
            // else    
                // ESP_LOGW(TAG, "i %d <- passed",i);
        }
        
        xSemaphoreGive(g_peer_list_mutex);
    }
    // ESP_LOGW(TAG, "total_copied %d",total_copied);

    return total_copied;
}


