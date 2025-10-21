#include "esp_sntp.h"          // Основний API для роботи з SNTP
#include "esp_log.h"           // Для виводу логів (ESP_LOGI, ESP_LOGW)
#include "time.h"              // Для роботи зі структурами часу (time_t, struct tm) та часовою зоною (setenv, tzset)
#include "hal_sntp.h"

// Для взаємодії з вашою системою подій
#include "shared_resources.h"        // Для визначення app_event_t та типів подій
#include "freertos/FreeRTOS.h" // Основний хедер FreeRTOS
#include "freertos/queue.h"    // Для доступу до функцій черги (xQueueSend)


extern QueueHandle_t main_queue_event;

static const char *TAG = "SNTP: ";

static void time_sync_notification_cb(struct timeval *tv) {
    app_event_t event = {
        .source = EVENT_SOURCE_SNTP, // Потрібно буде додати в app_events.h
        .CmdCode = SNTP_EVENT_TIME_SYNCED, // Потрібно буде додати
    };
    // Надсилаємо подію в головну чергу
    xQueueSend(main_queue_event, &event, 0);
    ESP_LOGI(TAG, "Час було успішно синхронізовано");
}

// 2. Функція ініціалізації
void hal_sntp_init(void) {
    // Налаштовуємо режим роботи
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    
    // Встановлюємо ім'я сервера часу
    sntp_setservername(0, "pool.ntp.org");
    
    // Встановлюємо нашу функцію-сповіщувач
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    
    // Запускаємо сервіс SNTP
    sntp_init();
    
    // Встановлюємо часову зону (ВАЖЛИВО!)
    // Для України це "EET-2EEST,M3.5.0/3,M10.5.0/4"
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
}