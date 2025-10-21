#include <stdio.h>
#include <string.h>
#include <time.h>

#include "hal_display_logic.h"
#include "ui.h"
#include "esp_lvgl_port.h"
#include "peer_manager.h"
// #include "shared_resources.h"
#include "esp_log.h"
#include "bsp.h"

static const char *TAG = "DISPLAY_LOGIC";

void change_tab(uint32_t tab){
    if (lvgl_port_lock(0)) {
        lv_tabview_set_act(ui_tabviewmain, tab, LV_ANIM_ON);
        lvgl_port_unlock(); 
    }
}

static peer_info_t ui_peer_list_cache[MAX_PEERS]; 
static uint16_t ui_total_list_count = 0;

extern QueueHandle_t main_queue_event;

// --- Прототипи внутрішніх функцій-виконавців ---
static int map_voltage_to_percentage(int voltage_mv);
static void update_header_bar(void);




// --- Публічні функції ---

/**
 * @brief Головна функція періодичного оновлення UI. Викликається з main.c.
 */
void hal_display_periodic_update(void) {
    static uint32_t last_update_time = 0;

    // Виконуємо оновлення не частіше, ніж раз на 2 секунди
    if (last_update_time == 0 || (esp_log_timestamp() - last_update_time > 60 * 1000)) {
        last_update_time = esp_log_timestamp();

        ESP_LOGD(TAG, "Running periodic UI update...");

        // Блокуємо LVGL для безпечного доступу з іншої задачі
        if (lvgl_port_lock(0)) {
   
            update_header_bar();

            lvgl_port_unlock();
        }
    }
}


/**
 * @brief Оновлює віджет роллера зі списком учасників.
 */
void hal_display_update_peer_list_widget(void) {
    
    ui_total_list_count = peer_manager_get_peers(ui_peer_list_cache, MAX_PEERS); //todo заборонить виконнаня без запущеного 
    
    // Якщо список порожній (повернувся 0, хоча має бути хоча б 1), показуємо повідомлення
    if (ui_total_list_count <= 0) {
        lv_roller_set_options(uic_roller_peer_list, "Searching...", LV_ROLLER_MODE_NORMAL);
        return;
    }

    char roller_options[(MAX_PEERS) * (PEER_NAME_LEN + 1)];

    strcpy(roller_options, ui_peer_list_cache[0].name); 

    // Додаємо решту учасників, розділяючи їх символом нового рядка '\n'
    for (uint16_t i = 1; i < ui_total_list_count; i++) {
        strcat(roller_options, "\n");
        strcat(roller_options, ui_peer_list_cache[i].name);
    }
    
    // ESP_LOGE(TAG, "roller_options -> %s",roller_options);
    // Крок 4: Встановлюємо згенерований рядок як нові опції для віджета роллера.
   
    uint16_t current_selection_index = lv_roller_get_selected(uic_roller_peer_list);
    if (lvgl_port_lock(0)) {
        lv_roller_set_options(uic_roller_peer_list, roller_options, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(uic_roller_peer_list, current_selection_index, LV_ANIM_OFF);
        lvgl_port_unlock();
    }    
}




// --- Реалізація внутрішніх функцій ---



/**
 * @brief Оновлює шапку (годинник, статуси).
 */
static void update_header_bar(void) { //todo update on very start of life
    int battery_mv = g_bsp->battery_status();
    int battery_percentage = map_voltage_to_percentage(battery_mv * 4829 / 1000);
    ESP_LOGI(TAG, "Напруга батареї %%: %d | %d", battery_percentage, battery_mv * 4829 / 1000);

    if (battery_percentage > 60) {
        lv_obj_add_state(uic_bar_battery_status, LV_STATE_USER_1);
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_2);
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_3);
    }
    else if (battery_percentage > 35) {
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_1);
        lv_obj_add_state(uic_bar_battery_status, LV_STATE_USER_2);
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_3);
    }
    else {
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_1);
        lv_obj_remove_state(uic_bar_battery_status, LV_STATE_USER_2);
        lv_obj_add_state(uic_bar_battery_status, LV_STATE_USER_3);
    }

    lv_bar_set_value(uic_bar_battery_status, battery_percentage, LV_ANIM_OFF);

    char percentage_str[5];
    snprintf(percentage_str, sizeof(percentage_str), "%d%%", battery_percentage);
    lv_label_set_text(uic_label_battery_percent, percentage_str);
    
   
    time_t now;
    struct tm timeinfo;
    char strf_all_time_buf[64];
    char strftime_buf[6];

    time(&now); // Отримуємо поточний час
    localtime_r(&now, &timeinfo); // Перетворюємо його в локальний час

    // Форматуємо час у рядок для виводу в лог
    strftime(strftime_buf, sizeof(strftime_buf), "%H:%M", &timeinfo);
    lv_label_set_text(uic_label_clock, strftime_buf);

    
}


peer_info_t hal_display_get_peer_by_index( uint16_t index){

    if (index >= 0 && index < ui_total_list_count) {
        return ui_peer_list_cache[index]; // Повертаємо копію структури
    }

    ESP_LOGE(TAG, "Attempted to get peer with invalid index: %d", index);
    peer_info_t empty_peer = {0}; 
    return empty_peer;

}


static void roller_check_timer_cb(lv_timer_t * timer) {

    static uint16_t last_selected_index = 0xFFFF; // Ініціалізуємо невалідним значенням

    // Перевіряємо, чи віджет взагалі існує (про всяк випадок)
    if (!uic_roller_peer_list) {
        ESP_LOGE(TAG, "uic_roller_peer_list DOES't EXIST");
        return;
    }

    uint16_t current_selected_index = lv_roller_get_selected(uic_roller_peer_list);

    if (current_selected_index != last_selected_index) {
        last_selected_index = current_selected_index;
        
        peer_info_t peer_data = hal_display_get_peer_by_index(current_selected_index);

        ESP_LOGD(TAG, "GUI_ROLLER_CHANGED to -> %s", peer_data.name);

        app_event_t event = {
            .source = EVENT_SOURCE_GUI,
            .CmdCode = GUI_EVENT_PEER_SELECTED, // Додайте цей тип в app_events.h
            .payload.gui.selected_peer = current_selected_index, // Передаємо індекс вибраного елемента
            .payload.gui.peer_info = peer_data,
        };
        xQueueSend(main_queue_event, &event, 0);
    }
}


// Цю функцію потрібно буде викликати з main.c після init_gui()
void hal_display_roller_timer_init(void) {
    // ESP_LOGW(TAG, "xEventGroupWaitBits START"); //clean
    xEventGroupWaitBits(system_event_group, GUI_INITIALIZED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    // ESP_LOGW(TAG, "xEventGroupWaitBits STOP");


    ESP_LOGD("DISPLAY_LOGIC", "Initializing UI logic and timers...");
    lv_timer_create(roller_check_timer_cb, 100, NULL);

    hal_display_update_peer_list_widget();
}

void hal_display_focus_on_peer(uint64_t peer_sn){    
    if (peer_sn != ui_peer_list_cache[lv_roller_get_selected(uic_roller_peer_list)].sn){        
        // Шукаємо індекс учасника з потрібним SN в нашому кеші
        for (int i = 0; i < ui_total_list_count; i++) {
            if (ui_peer_list_cache[i].sn == peer_sn) {
                // Знайшли! Програмно встановлюємо вибір на роллері
                if (lvgl_port_lock(0)) {
                    lv_roller_set_selected(uic_roller_peer_list, i, LV_ANIM_ON);
                    lvgl_port_unlock();
                }
                ESP_LOGD(TAG, "Встановлено фокус на '%s'", ui_peer_list_cache[i].name);
                break; // Виходимо з циклу
            }
        }            
    }
}

void hal_display_set_rcv_img(bool onAir){ // todo look at if is flag is set
    if (lvgl_port_lock(0)) {
         if (onAir) {
            lv_obj_clear_flag(uic_img_rx_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(uic_img_rx_status, LV_OBJ_FLAG_HIDDEN);
        }

        lvgl_port_unlock();
    }
}


void hal_display_set_wifi_img(bool wifiON){ // todo look at if is flag is set    
    if (lvgl_port_lock(0)) {
         if (wifiON) {
            lv_obj_clear_flag(uic_img_wifi_status, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(uic_img_wifi_status, LV_OBJ_FLAG_HIDDEN);
        }

        lvgl_port_unlock();
    }
}


void hal_display_set_tx_mode(bool is_tx_active) { // todo look at if is flag is set
    if (lvgl_port_lock(0)) {
        
        if (is_tx_active) {
            // РОБИМО ОВЕРЛЕЙ ВИДИМИМ
            lv_obj_clear_flag(uic_panel_tx_overlay, LV_OBJ_FLAG_HIDDEN);            

        } else {
            // ХОВАЄМО ОВЕРЛЕЙ
            lv_obj_add_flag(uic_panel_tx_overlay, LV_OBJ_FLAG_HIDDEN);
        }

        lvgl_port_unlock();
    }
}


static int map_voltage_to_percentage(int voltage_mv) {
    // Визначаємо діапазони
    const int min_voltage = 3400; // 0%
    const int max_voltage = 4200; // 100%

    // Крок 1: Обмежити значення (clamp), щоб уникнути виходу за межі 0-100%
    if (voltage_mv <= min_voltage) {
        return 0;
    }
    if (voltage_mv >= max_voltage) {
        return 100;
    }

    // Крок 2: Виконуємо лінійне мапування
    int percentage = ((voltage_mv - min_voltage) * 100) / (max_voltage - min_voltage);

    return percentage;
}