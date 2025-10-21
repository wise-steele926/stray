// components/bsp/bsp_rev1.c

#include "bsp.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
static const char *TAG = "components\bsp\bsp_stray_v1.c";

// I2S AIC3120 pins
#define I2S_PIN_BCK	33
#define I2S_PIN_WS	25
#define I2S_PIN_OUT	26
#define I2S_PIN_IN	27
#define AIC_RESET 12 // GPIO_NUM_12

// + I2C on:
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_FREQ_HZ  400000

// SPI lcd
#define ST7789_DRIVER  
#define SPI_READ_FREQUENCY  20000000

#define TFT_WIDTH  240
#define TFT_HEIGHT 240 

// --------- 	 |  i/o |	pin		|
#define TFT_MISO  9	 	// xxx      |
#define TFT_MOSI  23 	// 37       |
#define TFT_SCLK  18 	// 30       |
#define TFT_CS    5  	// 29       |
#define TFT_DC    15 	// 15 (А0)  |
#define TFT_RST   2  	// 24       |
#define TFT_BL    13 	// 16       |

// INPUT PINS
#define GPIO_PTT_BTN         19 // off 1 | on 0
#define GPIO_POWER_BTN       35 // off 0 | on 1
#define GPIO_SELF_POWER      32 // off 0 | on 1

// 74HC164PW
#define GPIO_BUTTONS_CLK     16
#define GPIO_BUTTONS_INT     0
/*KB codes
KB_KEY_1	-	0x7C
KB_KEY_2	-	0x7E
KB_KEY_3	-	0x78
KB_KEY_4	-	0x7F
KB_KEY_5	-	0x70
KB_KEY_6	-	0x60
KB_KEY_7	-	0x40
*/

// BATTERY adc PIN
#define BATTERY_MONITOR_PIN ADC_CHANNEL_6 // GPIO34 - це ADC1_CHANNEL_6
static void battery_monitor_init(void);

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;


// AS5601 encoder
#define ENC_ADDR            0x36 
#define GPIO_ENCODER_A       36
#define GPIO_ENCODER_B       39

extern QueueHandle_t main_queue_event;


// --- Статичні функції-реалізації для Ревізії 1 ---
static void bsp_rev1_init(void) {

// self power 
    gpio_set_direction(GPIO_SELF_POWER, GPIO_MODE_OUTPUT);    
    gpio_set_level(GPIO_SELF_POWER, 1);

// separate buttons
    gpio_set_direction(GPIO_PTT_BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_PTT_BTN, GPIO_PULLUP_ONLY);

    gpio_set_direction(GPIO_POWER_BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_POWER_BTN, GPIO_PULLUP_ONLY);
    
// keyboard 74HC164PW
    gpio_set_direction(GPIO_BUTTONS_CLK, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_BUTTONS_CLK, 0);
    
    gpio_set_direction(GPIO_BUTTONS_INT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_BUTTONS_INT, GPIO_PULLUP_ONLY);
// AIC RESET
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL<<GPIO_NUM_12),
        .pull_down_en = 0,
        .pull_up_en = 0,			
        };
    gpio_config(&io_conf);
    

    gpio_set_level(AIC_RESET, 0);    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(AIC_RESET, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    battery_monitor_init();

    printf("INIT buttons done\n");
}





static void battery_monitor_init(void) {
    // ----- 1. Ініціалізація блоку ADC1 -----
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    // ----- 2. Конфігурація каналу -----
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, // Максимальна розрядність (зазвичай 12 біт -> 0-4095)
        .atten = ADC_ATTEN_DB_12,         // Максимальний діапазон вхідної напруги (~0-3.3V)
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_MONITOR_PIN, &config));

    // ----- 3. Створення об'єкта калібрування -----
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Калібрування не вдалося, буде використовуватися сире значення.");
    }
}

static int battery_monitor_get_voltage_mv(void) {
    int adc_raw;
    int voltage_mv = 0;

    // Читаємо "сире" значення
    esp_err_t read_err = adc_oneshot_read(adc1_handle, BATTERY_MONITOR_PIN, &adc_raw);
    if (read_err != ESP_OK) {
        ESP_LOGE(TAG, "Помилка читання ADC!");
        return 0; // Повертаємо 0 у випадку помилки
    }
    ESP_LOGW(TAG, "ADC1 Channel[%d] Raw Data: %d", BATTERY_MONITOR_PIN, adc_raw);

    // Якщо калібрування було успішно створено, перетворюємо raw в мілівольти
    if (adc1_cali_handle != NULL) {
        esp_err_t cali_err = adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv);
        if (cali_err != ESP_OK) {
            ESP_LOGW(TAG, "Помилка перетворення каліброваного значення, повертаю 0.");
            return 0;
        }
        ESP_LOGI(TAG, "Напруга батареї (калібрована): %d mV", voltage_mv);
    } else {
        // Fallback, якщо калібрування не підтримується/не вдалося.
        // Тут можна використати вашу стару формулу, але це не рекомендовано.
        ESP_LOGW(TAG, "Калібрування недоступне, використовуються сирі дані.");
        voltage_mv = adc_raw; // У цьому випадку повертаємо сирі дані для відладки
    }
    
    return voltage_mv;
}


static void bsp_rev1_power_latch_off(void) {
    gpio_set_level(GPIO_SELF_POWER, 0);
}


static void bsp_rev1_get_display_config(bsp_display_config_t* config) {
    // config->spi_host = REV1_DISPLAY_SPI_HOST;
    config->pin_mosi = TFT_MISO; 
    config->pin_sclk = TFT_SCLK;
    config->pin_cs = TFT_CS;
    config->pin_dc = TFT_DC;
    config->pin_rst = TFT_RST;
}

static void bsp_rev1_get_i2c_config(bsp_i2c_config_t* config) {
    config->i2c_port = I2C_MASTER_NUM;
    config->i2c_freq = I2C_MASTER_FREQ_HZ;
    config->pin_sda = I2C_MASTER_SDA_IO;
    config->pin_scl = I2C_MASTER_SCL_IO;
}

static void bsp_rev1_get_i2s_config(bsp_i2s_config_t* config) {
    config->i2s_BCK	 = I2S_PIN_BCK;
    config->i2s_WS	 = I2S_PIN_WS;
    config->i2s_OUT	 = I2S_PIN_OUT;
    config->i2s_IN	 = I2S_PIN_IN;
}

static bool bsp_rev1_PTT_status (void){   
    return !gpio_get_level(GPIO_PTT_BTN);
}
static bool bsp_rev1_POWER_status (void){
    return gpio_get_level(GPIO_POWER_BTN);
}

static uint8_t bsp_rev1_keyboard_scan(void) {

    if (gpio_get_level(GPIO_BUTTONS_INT) != 0) {
        return 0x00; // Жоден біт не встановлено
    }
    
    uint8_t key_mask = 0;

    gpio_set_level(GPIO_BUTTONS_CLK, 0); esp_rom_delay_us(100);
    gpio_set_level(GPIO_BUTTONS_CLK, 1); esp_rom_delay_us(100);

    // Проходимо по всіх 7 клавішах
    for (int i = 0; i < 7; i++) {
        gpio_set_level(GPIO_BUTTONS_CLK, 0); esp_rom_delay_us(1);
        gpio_set_level(GPIO_BUTTONS_CLK, 1); esp_rom_delay_us(1);
        
        if (gpio_get_level(GPIO_BUTTONS_INT) == 1) {
            key_mask |= (1 << i);
        }
    }

    gpio_set_level(GPIO_BUTTONS_CLK, 0); esp_rom_delay_us(100);
    return key_mask;
}


// --- Створення екземпляру інтерфейсу для Ревізії 1 ---
static const bsp_t bsp_rev1_impl = {
    .init = bsp_rev1_init,
    .power_latch_off = bsp_rev1_power_latch_off,
    .get_display_config = bsp_rev1_get_display_config,
    .get_i2c_config = bsp_rev1_get_i2c_config,
    .get_i2s_config = bsp_rev1_get_i2s_config,
    .PTT_status = bsp_rev1_PTT_status,
    .POWER_status = bsp_rev1_POWER_status,
    .keyboard_scan = bsp_rev1_keyboard_scan,
    .battery_status = battery_monitor_get_voltage_mv,
};

// "Getter" функція, що повертає вказівник на нашу реалізацію
const bsp_t* bsp_get_rev1_interface(void) {
    return &bsp_rev1_impl;
}