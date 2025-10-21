// EventGroupHandle_t system_event_group;
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h" // Порядок include важливий

// Ваші власні заголовкові файли
#include "shared_resources.h"
#include "bsp.h"

#include "fs.h"
#include "nvs_manager.h"

#include "hal_input.h"
#include "hal_i2c_bus.h"
#include "hal_audio.h"
#include "hal_net.h"
#include "wifi_provisioning.h"
#include "hal_sntp.h"

#include "hal_display.h"
#include "hal_display_helpers.h"
#include "esp_lvgl_port.h"

#include "lan_task.h" 
#include "peer_manager.h"
#include "lvgl.h"
#include "hal_display_logic.h"


#include "esp_flash.h"

// --- Оголошення глобальних змінних та функцій ---

static const char *TAG = "MAIN";



extern const bsp_t* bsp_get_rev1_interface(void);

// Глобальні змінні
EventGroupHandle_t system_event_group;
const bsp_t* g_bsp = NULL;
QueueHandle_t main_queue_event;
// QueueHandle_t audio_queue;

QueueHandle_t mic_to_net_queue;
QueueHandle_t net_to_speaker_queue;
QueueHandle_t shared_buffer_pool;

app_state_t stray_cur_state;
SemaphoreHandle_t cur_state_mutex;

bool WiFiconnected = false;
char my_ip_str[16];

// Оголошення статичних функцій 
static void initialize_bsp(void);
static void handle_input_events(const app_event_t* event);



static void initialize_bsp(void) {
    #if CONFIG_BOARD_REVISION_1
        g_bsp = bsp_get_rev1_interface();
        printf("BSP: Initializing for Board Revision 1.\n");
    #elif CONFIG_BOARD_REVISION_2
        g_bsp = bsp_get_rev2_interface();
        printf("BSP: Initializing for Board Revision 2.\n");
    #else
        #error "Board revision is not selected in menuconfig!"
    #endif

    // Перевіряємо, чи ініціалізація пройшла успішно
    if (!g_bsp) {
        // Зависнути або перезавантажитись, бо без BSP система не може працювати
        printf("FATAL ERROR: BSP pointer is NULL!\n");
        while(1);
    }    
    g_bsp->init();
}



static const char *types[] = {"0", "KB_KEY_4", "KB_KEY_2", "KB_KEY_1", "KB_KEY_3", "KB_KEY_5", "KB_KEY_6", "KB_KEY_7",
                                        "INPUT_ENCODER",       // Rotary encoder steps
                                        "INPUT_PTT",           // Dedicated PTT input
                                        "INPUT_POWER"    }; 

static const char *press[] = {"PRESS", "RELEASE", "LONG"};


void handle_input_events(const app_event_t* event) {
    
    uint8_t key_code = event->payload.input.key_code;
    int16_t press_type = event->payload.input.press_type;

    switch (event->CmdCode)     {
        
        case INPUT_ENCODER_TURN:
            event->payload.input.press_type > 0 ? --g_encoder_diff : ++g_encoder_diff ;

            printf("[INPUT] %-8s | key: %d | %d | time: %lu ms\n",
                types[event->payload.input.key_code],
                event->payload.input.key_code,
                event->payload.input.press_type,
                (unsigned long)event->payload.input.timestamp_ms);

            break;

        case INPUT_KEY_PRESS:

            printf("[INPUT] %-8s | key: %d | %s | time: %lu ms\n",
                types[event->payload.input.key_code],
                event->payload.input.key_code,
                press[event->payload.input.press_type],
                (unsigned long)event->payload.input.timestamp_ms);

            switch (key_code){

                case INPUT_POWER:
                    if (press_type == 2){
                        printf("\n Power OFF!!! \n");
                        nvs_manager_save_config();
                        PlayFile("/spiffs/off.wav");
                        vTaskDelay(pdMS_TO_TICKS(50));
                        g_bsp->power_latch_off();
                    }
                    break;

                case INPUT_PTT:
                    if (press_type == 1){
                        // g_encoder_btn_state = LV_INDEV_STATE_RELEASED;
                        printf("PTT is RELEASED!");
                        hal_display_set_tx_mode(false);
                        xEventGroupSetBits(system_event_group, PTT_RELEASED_BIT);
                    }
                    else   {
                        // g_encoder_btn_state = LV_INDEV_STATE_PRESSED;
                        printf("PTT is pressed!");
                        hal_display_set_tx_mode(true);
                        xEventGroupSetBits(system_event_group, PTT_PRESSED_BIT);
                    }
                    break;
                case KB_KEY_1: 
                    if (press_type == 1)
                        change_tab(0);
                    break;
                case KB_KEY_2: 
                    if (press_type == 1)
                        change_tab(1);
                    break;
                case KB_KEY_3: 
                    if (press_type == 1)
                        change_tab(2);
                    break;

                case KB_KEY_4: 
                    if (press_type == 1){
                        nvs_manager_print_ram_config();
                        nvs_manager_print_nvs_content();

                        // // nvs_manager_add_wifi_credential("YOUR_SSID", "YOUR_PASSWORD");
                        // // nvs_manager_set_device_name("ALEX");
                        // nvs_manager_set_last_wifi_index(3);
                        // nvs_manager_save_config();
                        // nvs_manager_print_nvs_content();
                    }
                    break;

                case KB_KEY_5: 
                    if (press_type == 1)
                        wifi_provisioning_start();
                    break;  
            }
                
    }
}
// void handle_wifi_events(app_event_t* event) {
    
// }

void monitoring_task(void* pvParameters) {
    // Буфер для збереження текстового звіту. Має бути достатньо великим.
    char stats_buffer[2048];

    while (1) {
        printf("\n\n--- СТАТИСТИКА ЗАЙНЯТОСТІ ПРОЦЕСОРА ---\n");
        // Генеруємо звіт
        vTaskGetRunTimeStats(stats_buffer);
        // Виводимо його в консоль
        printf("%s\n", stats_buffer);
        printf("----------------------------------------\n\n");
        
        // Чекаємо 5 секунд перед наступним виводом
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}



void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_ERROR);
    // esp_log_level_set("LAN_TASK", ESP_LOG_DEBUG);
    // esp_log_level_set("PEER_MANAGER", ESP_LOG_DEBUG);
    // esp_log_level_set("DISPLAY_LOGIC", ESP_LOG_DEBUG);
    // esp_log_level_set("lan_Tx_task", ESP_LOG_DEBUG);
    // esp_log_level_set("lan_Rx_task", ESP_LOG_DEBUG);
    // esp_log_level_set("lan_ping_task", ESP_LOG_DEBUG);
    // esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("hal_net", ESP_LOG_DEBUG);
    esp_log_level_set("WIFI_PROV", ESP_LOG_DEBUG);


    uint32_t size_flash_chip;
    esp_flash_get_size(NULL, &size_flash_chip);
    ESP_LOGW(TAG,"size_flash_chip= %lu || %lu МБ", size_flash_chip, (size_flash_chip/ (1024 * 1024)));

    system_event_group = xEventGroupCreate();

    cur_state_mutex = xSemaphoreCreateMutex();
    if (cur_state_mutex == NULL) {
        ESP_LOGE(TAG, "Не вдалося створити м'ютекс!");
        return; // Аварійне завершення
    }
    
    main_queue_event = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(app_event_t));
    
    mic_to_net_queue = xQueueCreate(NUM_AUDIO_BUFFERS, sizeof(audio_chunk_t));
    net_to_speaker_queue = xQueueCreate(NUM_AUDIO_BUFFERS, sizeof(audio_chunk_t));
    shared_buffer_pool = xQueueCreate(NUM_AUDIO_BUFFERS, sizeof(uint8_t*)); // Ця черга зберігає лише вказівники


    if (!mic_to_net_queue || !net_to_speaker_queue || !shared_buffer_pool) {
        ESP_LOGE(TAG, "Не вдалося створити одну з черг!");
        return; // Аварійне завершення
    }

    // Заповнюємо пул пам'яті (shared_buffer_pool)
    for (int i = 0; i < NUM_AUDIO_BUFFERS; i++) {
        uint8_t* buffer = (uint8_t*) malloc(AUDIO_BUFFER_SIZE);
        if (buffer) {
            xQueueSend(shared_buffer_pool, &buffer, 0);
        } else {
            ESP_LOGE(TAG, "Не вдалося виділити пам'ять для пулу буферів!");
            return;
        }
    }
    ESP_LOGD(TAG, "Пул з %d буферів створено.", NUM_AUDIO_BUFFERS);

    nvs_manager_init();
    nvs_manager_print_ram_config();

    initialize_bsp();
    init_spiffs ();
    init_gui();
    ESP_LOGD(TAG, "Запускаємо ініціалізацію Wi-Fi в режимі станції (APSTA)");
    wifi_init();
    
    xTaskCreate(input_task, "input_task", INPUT_TASK_STACK_SIZE, NULL, INPUT_TASK_PRIORITY, NULL);
    vTaskDelay(pdMS_TO_TICKS(150));
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);
    
    // xTaskCreate(monitoring_task, "monitoring_task", INPUT_TASK_STACK_SIZE, NULL, INPUT_TASK_PRIORITY, NULL);

   
    app_event_t event;    
    static peer_info_t g_selected_peer_info; // Замість g_selected_peer_index

    lan_manager_init (LOCAL_PORT);
    hal_display_roller_timer_init();

    
    while (1)
    {
        while (xQueueReceive(main_queue_event, &event, 0)) {
            
            switch (event.source) 
            {
                case EVENT_SOURCE_INPUT:  handle_input_events(&event);  break;

                case EVENT_SOURCE_WIFI:  
                    ESP_LOGE(TAG, "EVENT_SOURCE_WIFI -> wifi_event_type_t - %d", event.CmdCode);   
                    // gui_show_main_button(true);
                    if (event.CmdCode == WIFI_EVENT_STA_READY){
                       hal_wifi_connect();                       
                    }

                    if (event.CmdCode == WIFI_EVENT_CONNECTED){
                        WiFiconnected = true;
                        hal_display_set_wifi_img(true);
                        hal_sntp_init(); // init SNTP service for real time sync     

                    }

                    if (event.CmdCode == WIFI_EVENT_DISCONNECTED){
                        hal_display_set_wifi_img(false);
                        
                    }

                    break; //handle_wifi_events(&event);

                case EVENT_SOURCE_GUI:
                    if (event.CmdCode == GUI_EVENT_PEER_SELECTED){

                        g_selected_peer_info = event.payload.gui.peer_info;

                        lan_set_tx_target (&g_selected_peer_info);

                        ESP_LOGD(TAG, "New peer selected %s, index: %d" , event.payload.gui.peer_info.name , event.payload.gui.selected_peer);
                    }
                    break;

                case EVENT_SOURCE_CHANNEL:
                    if (event.CmdCode == CHANNEL_EVENT_PEER_SPEAKING) {
                        uint64_t speaker_sn = event.payload.channel.peer_speaking;
                        // Даємо команду модулю UI встановити фокус
                        hal_display_set_rcv_img(true);
                        hal_display_focus_on_peer(speaker_sn);
                    }
                    if (event.CmdCode == CHANNEL_EVENT_PEER_SPEAKING_END){
                        hal_display_set_rcv_img(false);
                    }
                    if (event.CmdCode == CHANNEL_EVENT_PEER_LIST_CHANGED) {
                        ESP_LOGD(TAG, "Отримано подію про зміну списку учасників, оновлюємо UI.");
                        // Даємо пряму команду UI оновити список
                        hal_display_update_peer_list_widget(); // Цю функцію треба буде зробити публічною
                    }
                    break;

                case EVENT_SOURCE_SNTP:
                    if (event.CmdCode == SNTP_EVENT_TIME_SYNCED) {
                        // Час синхронізовано! Тепер ми можемо його отримати і використовувати.
                        time_t now;
                        struct tm timeinfo;
                        char strftime_buf[64];

                        time(&now); // Отримуємо поточний час
                        localtime_r(&now, &timeinfo); // Перетворюємо його в локальний час

                        // Форматуємо час у рядок для виводу в лог
                        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
                        ESP_LOGI(TAG, "Поточний час: %s", strftime_buf);

                        // Тепер ви можете передати timeinfo в UI для оновлення годинника
                    }
                    break;
                        
                default: 
                    ESP_LOGW (TAG,  "UNKNOWN event [%d] with CMDcode [%d]", event.source, event.CmdCode);
                    break;
            }            
        }
        hal_display_periodic_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

