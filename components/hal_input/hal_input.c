// components/hal_input/hal_input.c
#include "hal_input.h"
#include "shared_resources.h"
// #include "app_events.h"
#include "hal_encoder.h"
#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_timer.h"

#include "esp_log.h"
static const char *TAG = "hal_input: ";

#define ENCODER_I2C_ADDRESS 0x36

extern QueueHandle_t main_queue_event;
extern const bsp_t* g_bsp;



static void send_input_event(input_event_type_t type, key_code_t key_code, int16_t press_type)
{
    // input_event_t event = {
    //     // .CmdCode = type,
    //     .key_code = key_code, 
    //     .press_type = press_type,
    //     .timestamp_ms = esp_timer_get_time() / 1000,
    // };
    app_event_t event = {
        .source = EVENT_SOURCE_INPUT,
        .CmdCode = type, 
        .payload = {
            .input = { 
                .key_code = key_code,
                .press_type = press_type,
                .timestamp_ms = esp_timer_get_time() / 1000,
            }
        }
    };
    xQueueSend(main_queue_event, &event, 0);
}



static void handle_button_state(bool is_currently_pressed,
                                bool* was_pressed_flag,
                                int64_t* press_start_time,
                                key_code_t key_code,
                                uint32_t long_press_duration_ms)
{
    int64_t now = esp_timer_get_time() / 1000;

    // Подія: Кнопку щойно натиснули (змінився стан з "не натиснуто" на "натиснуто")
    if (is_currently_pressed && !(*was_pressed_flag)) {
        *was_pressed_flag = true;
        if (press_start_time) {
            *press_start_time = now;
        }
        send_input_event(INPUT_KEY_PRESS, key_code, 0); // 0 = PRESS
    }
    // Подія: Кнопку щойно відпустили (змінився стан з "натиснуто" на "не натиснуто")
    else if (!is_currently_pressed && (*was_pressed_flag)) {
        *was_pressed_flag = false;
        
        uint8_t press_type = 1; // 1 = RELEASE (за замовчуванням)
        
        // Перевірка на довге натискання, якщо потрібно
        if (press_start_time && long_press_duration_ms > 0) {
            int64_t held_duration = now - (*press_start_time);
            if (held_duration >= long_press_duration_ms) {
                press_type = 2; // 2 = LONG_PRESS
            }
        }
        send_input_event(INPUT_KEY_PRESS, key_code, press_type);
    }
}

static key_code_t translate_raw_to_logical(uint8_t raw_code) {
    switch (raw_code) {
        case 0x7C: return KB_KEY_1; // Використовуйте осмислені імена
        case 0x7E: return KB_KEY_2;
        case 0x78: return KB_KEY_3;
        case 0x7F: return KB_KEY_4;
        case 0x70: return KB_KEY_5;
        case 0x60: return KB_KEY_6;
        case 0x40: return KB_KEY_7;
        default:   return INPUT_NONE; // Всі інші коди (включаючи 0x00) ігноруються
    }
}

void poll_keyboard(void)
{
   static struct {
        key_code_t pressed_key; // Зберігаємо логічний код
        int64_t press_timestamp_ms;
    } state = {0};

    // 1. Отримуємо сирий код від BSP і перетворюємо його на логічний
    uint8_t raw_code = g_bsp->keyboard_scan();
    key_code_t current_key = translate_raw_to_logical(raw_code);
    int64_t now = esp_timer_get_time() / 1000;

    
    if (current_key == state.pressed_key) 
        return;

    // --- Стан змінився! ---

    // ВИпадок А: Клавішу ВІДПУСТИЛИ
    if (state.pressed_key != INPUT_NONE) {
        int64_t held_duration = now - state.press_timestamp_ms;
        uint8_t press_type = (held_duration >= LONG_PRESS_DURATION_MS) ? 2 : 1;
        send_input_event(INPUT_KEY_PRESS, state.pressed_key, press_type);
    }

    // ВИпадок Б: НОВУ клавішу НАТИСНУЛИ
    if (current_key != INPUT_NONE) {
        state.press_timestamp_ms = now;
        send_input_event(INPUT_KEY_PRESS, current_key, 0); // 0 = PRESS
    }
    
    // Оновлюємо стан для наступної ітерації
    state.pressed_key = current_key;
}

void poll_encoder(void) {
    static uint16_t previous_angle = 0;
    uint16_t current_angle;
    // ESP_LOGD(TAG, "poll_encoder: START\n");
    esp_err_t ret = hal_encoder_get_angle(&current_angle);

    if (ret == ESP_OK) {
        
        int16_t delta = current_angle - previous_angle;

        if ((abs(delta) > 25) && (abs(delta) < 1000)) {
            // ESP_LOGD(TAG, "current_angle = %d | previous_angle =  %d | delta =  %d ",current_angle,previous_angle,delta);
            send_input_event(INPUT_ENCODER_TURN, INPUT_ENCODER, delta);
        } 
        previous_angle = current_angle;
    } else
        ESP_LOGE(TAG, "poll_encoder: %s\n", esp_err_to_name(ret));
}


void input_task(void *pvParameters) {
    ESP_LOGD(TAG, "Запускаємо ініціалізацію input_task");
    int64_t press_time_start = 0; 

    static bool ptt_pressed = false;
    static bool power_pressed = false;

    esp_err_t ret = hal_encoder_init();
    
    while (1) {

        handle_button_state(g_bsp->PTT_status(), &ptt_pressed, NULL, INPUT_PTT, 0);

        handle_button_state(g_bsp->POWER_status(), &power_pressed, &press_time_start, INPUT_POWER, LONG_PRESS_DURATION_MS);
        
        poll_keyboard();

        poll_encoder();

        vTaskDelay(pdMS_TO_TICKS(INPUT_SCAN_INTERVAL_MS));
    }
}