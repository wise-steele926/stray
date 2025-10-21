// #include "shared_resources.h"

#pragma once

#ifndef SH_RES_H
#define SH_RES_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_mac.h"

// 2 secconds between updates for UI
#define PERIODIC_TASK_TIME 2000 

// system_event_group BITS
#define WIFI_CONNECTED_BIT BIT0 
#define PTT_PRESSED_BIT  BIT1
#define PTT_RELEASED_BIT  BIT2
#define GUI_INITIALIZED_BIT BIT3
#define LAN_TASKS_INITIALIZED_BIT BIT4
#define I2C_BUS_INIT_BIT BIT5
 
// Структура, що описує один шматок аудіо
// extern QueueHandle_t audio_queue;

#define NUM_AUDIO_BUFFERS 10
#define AUDIO_BUFFER_SIZE (320*4)

#define LOCAL_PORT 4500

extern QueueHandle_t mic_to_net_queue;
extern QueueHandle_t net_to_speaker_queue;
extern QueueHandle_t shared_buffer_pool;

extern EventGroupHandle_t system_event_group;

typedef struct {
    bool is_wifi_connected;
    bool is_ptt_pressed;
    bool is_speaker_on;

    uint8_t speaker_volume;
    
    char my_ip_str[16];
} app_state_t;

extern app_state_t stray_cur_state;
extern SemaphoreHandle_t cur_state_mutex;

typedef struct {
    uint8_t* buffer; // Вказівник на динамічно виділений буфер
    size_t   len;    // Кількість байтів у буфері
} audio_chunk_t;

extern char my_ip_str[16];



/*
PEERS
*/
#define MAX_PEERS 5         // Максимальна кількість учасників у списку
#define PEER_NAME_LEN 16     // Відповідає sender_name[16] з вашого udp_protocol.h
#define PEER_PING_PERIOD 15000
#define PEER_INACTIVITY_TIMEOUT_MS (PEER_PING_PERIOD * 3 * 1000ULL)


// Структура для зберігання інформації про одного учасника
typedef struct {
    bool is_active;                      // Чи активний цей запис?
    uint64_t sn;                         // Унікальний серійний номер (як у header->sender_sn)
    char name[PEER_NAME_LEN];            // Ім'я учасника
    uint32_t ip_address;                 // IP-адреса у вигляді числа
    int64_t last_seen_timestamp_us;      // Час останнього контакту (в мікросекундах)
} peer_info_t;


/*
FORMER APP_EVENTS
*/
typedef enum {
    EVENT_SOURCE_INPUT,
    EVENT_SOURCE_WIFI,
    EVENT_SOURCE_CHANNEL,
    EVENT_SOURCE_GUI,
    EVENT_SOURCE_SNTP
    // ... інші ваші модулі
} app_event_source_t;

// Типи подій для кожного джерела
typedef enum { INPUT_ENCODER_TURN, INPUT_KEY_PRESS } input_event_type_t;
typedef enum { WIFI_EVENT_CONNECTED, WIFI_EVENT_DISCONNECTED, WIFI_EVENT_STA_READY} wifi_event_type_t;
typedef enum { SNTP_EVENT_TIME_SYNCED } sntp_event_type_t;
typedef enum { CHANNEL_EVENT_PEER_SPEAKING, CHANNEL_EVENT_PEER_SPEAKING_END , CHANNEL_EVENT_PEER_LIST_CHANGED} channel_event_type_t;
typedef enum { GUI_EVENT_CONNECT_TO_SERVER, GUI_EVENT_PEER_SELECTED } gui_event_type_t;


typedef union {
    // Дані від модуля вводу
    struct {
        uint8_t key_code;
        int16_t press_type;
        uint32_t timestamp_ms;
    } input;
    
    // Дані від модуля Wi-Fi
    struct {
        bool is_connected;
    } wifi;
    
    struct{
        uint64_t peer_speaking;
    }channel;
    
    struct{
        uint16_t selected_peer; 
        peer_info_t peer_info;
    }gui;

} app_event_payload_t;

typedef struct {
    app_event_source_t source;
    uint8_t CmdCode; // Може бути input_event_type_t, wifi_event_type_t, тощо
    app_event_payload_t payload;
} app_event_t;

#endif