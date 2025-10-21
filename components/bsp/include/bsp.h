// components/bsp/bsp.h

#pragma once
#include <stdbool.h>
#include <stdio.h>

#define EVENT_QUEUE_SIZE     10

// --- ENUMS ---
typedef enum {
    INPUT_NONE = 0,
    KB_KEY_4,
    KB_KEY_2, 
    KB_KEY_1, 
    KB_KEY_3, 
    KB_KEY_5, 
    KB_KEY_6, 
    KB_KEY_7,
    INPUT_ENCODER,
    INPUT_PTT,           // Dedicated PTT input
    INPUT_POWER          // Power button
} key_code_t;

// --- STRUCT ---
typedef struct {
    uint8_t key_code;        // Mapped key value or encoder delta
    int16_t press_type;      // 0 = press, 1 = release, 2 = long_press or any code for encoder
    uint32_t timestamp_ms;   // Timestamp from system tick
} input_event_t;


// Конфігураційні структури для периферії,
// які BSP буде повертати для HAL-рівня.
typedef struct {
    int spi_host;
    int pin_mosi;
    int pin_sclk;
    int pin_cs;
    int pin_dc;
    int pin_rst;
} bsp_display_config_t;


typedef struct {
    int i2c_port;
    uint32_t  i2c_freq;
    int pin_sda;
    int pin_scl;
} bsp_i2c_config_t;

typedef struct {
	 int i2s_BCK;	
	 int i2s_WS	;
	 int i2s_OUT;	
	 int i2s_IN	;
} bsp_i2s_config_t;

// --- Інтерфейс BSP у вигляді структури вказівників на функції ---

typedef struct {
    // Ініціалізація основних функцій плати
    void (*init)(void);

    // Керування живленням
    void (*power_latch_off)(void);

    // Функції для отримання конфігурації пінів
    void (*get_display_config)(bsp_display_config_t* config);
    void (*get_i2c_config)(bsp_i2c_config_t* config);
    void (*get_i2s_config)(bsp_i2s_config_t* config);
    bool (*PTT_status)(void);
    bool (*POWER_status)(void);
    uint8_t (*keyboard_scan)(void);
    int (*battery_status)(void);
} bsp_t;


// --- Глобальний вказівник на активний BSP ---
// Всі модулі системи будуть працювати тільки з цим вказівником.
extern const bsp_t* g_bsp;
