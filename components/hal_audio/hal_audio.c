// components/hal_audio/hal_audio.c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

#include "hal_audio.h"
#include "driver/i2s_std.h"
#include "hal_i2c_bus.h"
#include "driver/gpio.h"
#include "bsp.h"

#include "esp_log.h"
#include <esp_err.h>
#include "esp_timer.h" 

#include "shared_resources.h"

#include <math.h>
#define SINE_WAVE_AMPLITUDE 20000  // Гучність (макс. 32767 для int16_t)
#define SAMPLES_PER_BUFFER 256     // Кількість семплів в одному буфері

static bool is_speaker_on = false;
static int64_t last_audio_time_us = 0;

#define SPEAKER_OFF_TIMEOUT_US (200 * 1000)

static const char *TAG = "hal_audio.c: ";

const uint8_t AICstartupSettings[][2] = {
    //addr, data DEC
    //Page 0 //for 44100
    {0, 0}, {3, 2}, {4, 7}, {5, 147}, {6, 56}, {7, 0}, {8, 0},
    {11, 131}, {12, 142}, {13, 0}, {14, 128}, {15, 128}, {16, 8},
    {18, 131}, {19, 135}, {20, 0}, {21, 128}, {22, 4}, {25, 0},
    {26, 1}, {27, 0}, {28, 0}, {29, 0}, {30, 129}, {31, 0},
    {32, 0}, {33, 0}, {34, 0}, {36, 0}, {37, 0}, {38, 0},
    {39, 0}, {44, 0}, {45, 0}, {46, 0}, {47, 0}, {48, 0},
    {49, 0}, {50, 0}, {51, 0}, {53, 18}, {54, 2}, {60, 25},
    {61, 5}, {62, 0}, {63, 214}, {64, 0}, {65, 0}, {66, 0},
    {67, 0}, {68, 0}, {69, 56}, {70, 0}, {71, 0}, {72, 128},
    {73, 0}, {74, 8}, {75, 157}, {76, 20}, {77, 210}, {78, 126},
    {79, 74}, {81, 128}, {82, 0}, {83, 0}, {86, 128}, {87, 8},
    {88, 70}, {89, 40}, {90, 36}, {91, 19}, {92, 1}, {93, 60},
    {102, 0}, {103, 0}, {104, 0}, {105, 0}, {106, 0}, {116, 0},
    {117, 0},

    //page1
    {0, 1}, {30, 0}, {31, 194}, {32, 198}, {33, 78}, {34, 0},
    {35, 64}, {36, 33}, {37, 0}, {38, 225}, {39, 127}, {40, 54},
    {41, 2}, {42, 5}, {43, 0}, {44, 32}, {45, 134}, {46, 9},
    {47, 24}, {48, 0x80}, {49, 0x20}, {50, 0},

    //page 3
    {0, 3}, {16, 129},
};

static i2c_master_dev_handle_t i2c_codec_dev_handle = NULL; // Локальний хендл для кодека


static i2s_chan_handle_t tx_handle;
static i2s_chan_handle_t rx_handle;

static void i2s_init(void){
    
    ESP_LOGD(TAG, "Запускаємо i2s_init");
    bsp_i2s_config_t i2s_config;
    g_bsp->get_i2s_config(&i2s_config);

    /* Allocate a pair of I2S channel */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    /* Allocate for TX and RX channel at the same time, then they will work in full-duplex mode */
    i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);

    /* Set the configurations for BOTH TWO channels, since TX and RX channel have to be same in full-duplex mode */
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        // .slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_8BIT, I2S_SLOT_MODE_MONO ),
        // .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),

        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = i2s_config.i2s_BCK,
            .ws = 	i2s_config.i2s_WS,
            .dout = i2s_config.i2s_OUT,
            .din = 	i2s_config.i2s_IN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    

    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_init_std_mode(rx_handle, &std_cfg);

    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);
    
    ESP_LOGD(TAG, "i2s_init DONE");
}

esp_err_t hal_audio_init(void) {
    
    ESP_LOGD(TAG, "Запускаємо hal_audio_init");
    i2s_init();

    esp_err_t ret;

    EventBits_t bits = xEventGroupWaitBits(system_event_group, I2C_BUS_INIT_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(500));
    if (bits & I2C_BUS_INIT_BIT) {
        ESP_LOGD(TAG, "I2C_BUS_INIT_BIT");
    } else {    
        ESP_LOGD(TAG, "Запускаємо i2c_bus_init");
        bsp_i2c_config_t i2c_config;
        g_bsp->get_i2c_config(&i2c_config);
        ret = i2c_bus_init(&i2c_config);
    }


    ret = i2c_bus_add_device(AIC3120_I2C_ADDR, &i2c_codec_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add codec to I2C bus: %s\n", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGD(TAG, "i2c_bus_add_device added");

    uint8_t write_buf[][2] = {{0x00, 0x00},{0x01, 0x01}}; // Вибір сторінки 0

    ret = i2c_master_transmit(
        i2c_codec_dev_handle, 
        write_buf[0],
        sizeof(write_buf[0]),
         -1    );

    if (ret != ESP_OK) {       
        ESP_LOGE(TAG, "I2C transaction failed: %s", esp_err_to_name(ret));
        return ret; // Повертаємо код помилки
    }

    ret = i2c_master_transmit(
        i2c_codec_dev_handle, 
        write_buf[1],
        sizeof(write_buf[1]),
         -1    );

    if (ret != ESP_OK) {       
        ESP_LOGE(TAG, "I2C transaction failed: %s", esp_err_to_name(ret));
        return ret; // Повертаємо код помилки
    }

   
    vTaskDelay(pdMS_TO_TICKS(20)); 
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGD(TAG, "AIC3120 Reset done.");

    ESP_LOGD(TAG, "Configuring AIC3120 with startup settings...");
    for (int i = 0; i < (sizeof(AICstartupSettings) / sizeof(AICstartupSettings[0])); i++) {
        
        ret = i2c_master_transmit(
            i2c_codec_dev_handle, 
            AICstartupSettings[i],
            sizeof(AICstartupSettings[i]),
            -1    );
        if (ret != ESP_OK) {       
            ESP_LOGE(TAG, "Failed to write AICstartupSettings[%d]: reg 0x%02X, data 0x%02X", i, AICstartupSettings[i][0], AICstartupSettings[i][1]);
            return ret; 
        } 
    }
    ESP_LOGD(TAG, "AIC3120 configuration complete.");

    aic3120_set_speaker_volume(80);
    PlayFile("/spiffs/on.wav");
    // play_sine_wave(3500, 2000);

    // xTaskCreate(record_task, "Record Task", 4096, NULL, 5, NULL);

    return ret;
}


void aic3120_on_off_speaker(bool IsON) {
    

    uint8_t tmp_data_reg = 0;
    uint8_t write_buf [] = {0x00, 0x01};

    ESP_ERROR_CHECK(i2c_master_transmit( // Перехід на сторінку 1
        i2c_codec_dev_handle, 
        write_buf,
        sizeof(write_buf),
         -1    ));

    // Керування живленням драйвера динаміка (P1_R42, SPL_DRIVER_ADDR)
    // D2 (SPK_EN): 1 = Speaker Driver увімкнено, 0 = вимкнено 
    uint8_t spl_driver_reg = SPL_DRIVER_ADDR;   
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_codec_dev_handle, &spl_driver_reg, 1, &tmp_data_reg, 1, -1));
   
    if (IsON) 
        tmp_data_reg |= 0x04;    // D2 = 1
    else 
        tmp_data_reg &= ~(0x04); // D2 = 0

    uint8_t spl_driver_write_buf[] = {spl_driver_reg, tmp_data_reg};
    ESP_ERROR_CHECK(i2c_master_transmit(
        i2c_codec_dev_handle, 
        spl_driver_write_buf,
        sizeof(spl_driver_write_buf),
         -1    ));
    
    // Керування живленням підсилювача динаміка (P1_R32, SPK_AMP_ADDR)
    // D7 (SPK_RAMP): 1 = Speaker Driver увімкнено, 0 = вимкнено
    uint8_t spk_amp_reg = SPK_AMP_ADDR;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_codec_dev_handle, &spk_amp_reg, sizeof(spk_amp_reg), &tmp_data_reg, 1, -1));
    
    if (IsON) 
        tmp_data_reg |= 0x80;    // D7 = 1
    else 
        tmp_data_reg &= ~(0x80); // D7 = 0
    
    uint8_t spk_amp_write_buf[] = {spk_amp_reg, tmp_data_reg};    
    ESP_ERROR_CHECK(i2c_master_transmit(
        i2c_codec_dev_handle, 
        spk_amp_write_buf,
        sizeof(spk_amp_write_buf),
         -1    ));

    if (xSemaphoreTake(cur_state_mutex, portMAX_DELAY) == pdTRUE) {
    // Ми "взяли ключ". Тепер ми в безпечній секції.
    
        stray_cur_state.is_speaker_on = IsON; // Змінюємо стан
        
        // Ми "повернули ключ". Інші задачі тепер можуть отримати доступ.
        xSemaphoreGive(cur_state_mutex);
    }
    ESP_LOGD(TAG, "Speaker %s", IsON ? "ON" : "OFF");
}


static void speaker_handle_playback(void) {
    if (!is_speaker_on) {
        // ESP_LOGD(TAG, "Speaker ON");
        aic3120_on_off_speaker(true);
        is_speaker_on = true;
    }
    // Оновлюємо час останньої активності
    last_audio_time_us = esp_timer_get_time();
    // ESP_LOGD(TAG, "last_audio_time_us %llu",last_audio_time_us);
}

static void speaker_handle_transmit_start(void) {
    if (is_speaker_on) {
        ESP_LOGD(TAG, "PTT pressed, Speaker OFF");
        aic3120_on_off_speaker(false);
        is_speaker_on = false;
    }
}
static void speaker_handle_timeout(void) {

    int64_t diff = esp_timer_get_time() - last_audio_time_us;

    if (is_speaker_on && (diff > SPEAKER_OFF_TIMEOUT_US)) {
        ESP_LOGD(TAG, "Таймаут тиші diff %llu , вимикаю динамік.",diff);
        aic3120_on_off_speaker(false);
        is_speaker_on = false;
    }
}

void PlayFile(char *fname)
{
    // Буфер для 16-бітних стерео-семплів. 
    // `static`, щоб уникнути переповнення стеку.
    #define PLAY_BUF_SIZE 1024 // Розмір у байтах, має бути парним
    static uint8_t buffer[PLAY_BUF_SIZE]; 

    // speaker_handle_playback();//maybe

    size_t bytes_written;
    size_t bytes_read;
    
    // Відкриваємо файл у БІНАРНОМУ режимі ("rb") - це важливо
    FILE* f = fopen(fname, "rb");
    if (f != NULL)
    {
        ESP_LOGD(TAG, "Playing raw 16-bit stereo file: %s", fname);
        

        // Читаємо файл порціями, доки не закінчиться
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
        {
            speaker_handle_playback();
            // --- КЛЮЧОВА ЗМІНА ---
            // Відправляємо в I2S ТОЧНО ту кількість байтів,
            // яку ми реально прочитали з файлу.
            esp_err_t ret = i2s_channel_write(
                tx_handle,
                buffer,
                bytes_read,       // <--- ВИКОРИСТОВУЄМО РЕАЛЬНО ПРОЧИТАНИЙ РОЗМІР
                &bytes_written,
                portMAX_DELAY
            );
            
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
                break; // Виходимо з циклу при помилці
            }
        }
        
        fclose(f);
        // i2s_channel_zero_dma_buffer(tx_handle); // Очищуємо DMA-буфери після відтворення
        // aic3120_on_off_speaker(false);

    } else {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", fname);
    }
}

static long map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void aic3120_set_speaker_volume(uint8_t volume) {
    // uint8_t tmp_data_reg = 0;
    // esp_err_t err;

    if (volume > 100) 
        volume = 100;

    uint8_t vol_mapped = map_value(volume, 0, 100, 0, 127); // 0-127
    // uint8_t vol_mapped = 255 - (volume * 255 / 100);

    if (volume == 0)
        aic3120_on_off_speaker(false);
    else    
        aic3120_on_off_speaker(true);
            
    uint8_t att_val;
    if (volume == 0) {
        att_val = 0x7F; 
    } else {       
        att_val = 0x80 | (uint8_t)(127 - vol_mapped);
    }
    
    uint8_t spl_att_reg = SPL_ATT_REG;  
    uint8_t spl_driver_write_buf[] = {spl_att_reg, att_val};
    ESP_ERROR_CHECK(i2c_master_transmit(
        i2c_codec_dev_handle, 
        spl_driver_write_buf,
        sizeof(spl_driver_write_buf),
         -1    ));

    ESP_LOGD(TAG, "Speaker volume set to %d%% (mapped: %d), Reg 0x%02X (P1_R%d) = 0x%02X", volume, vol_mapped, SPL_ATT_REG, SPL_ATT_REG, att_val);
    return;
}


static esp_err_t hal_audio_read(uint8_t* buffer, size_t size, size_t* bytes_read, uint32_t timeout_ms) {
    // Читаємо дані з RX каналу I2S
    return i2s_channel_read(rx_handle, buffer, size, bytes_read, pdMS_TO_TICKS(timeout_ms));
}

static esp_err_t hal_audio_write(const uint8_t* buffer, size_t size, size_t* bytes_written) {
    // Просто викликаємо стандартну функцію запису в TX канал I2S
    return i2s_channel_write(tx_handle, buffer, size, bytes_written, portMAX_DELAY);
}



static void convert_stereo_to_mono(int16_t* dest_mono, const int16_t* src_stereo, int num_stereo_frames)
{
    for (int i = 0; i < num_stereo_frames; i++) {
        // Просто копіюємо кожен перший семпл з пари (Лівий канал)
        dest_mono[i] = src_stereo[i * 2];
    }
}

void convert_mono_to_stereo(int16_t* dest_stereo, const int16_t* src_mono, int num_mono_samples)
{
    for (int i = 0; i < num_mono_samples; i++) {
        // Копіюємо один моно-семпл в обидва канали стерео-пари
        dest_stereo[i * 2]     = src_mono[i]; // Лівий канал
        dest_stereo[i * 2 + 1] = src_mono[i]; // Правий канал
    }
}


void audio_task(void* pvParameters) {
    
    ESP_LOGD(TAG, "Запускаємо audio_task");
    ESP_ERROR_CHECK(hal_audio_init());
    
    // int16_t stereo_buffer[AUDIO_BUFFER_SIZE / 2];
    static int16_t stereo_buffer[AUDIO_BUFFER_SIZE / 2];
    size_t bytes_read;
    EventBits_t bits;

    // static int64_t last_audio_time_us = 0;

    ESP_LOGD(TAG, "Audio task: в режимі очікування...");

    int64_t start_time, elapsed_time; //cleanof

    while (1) {
        // 1. Чекаємо на натискання PTT, АЛЕ з невеликим таймаутом 20 мс
        bits = xEventGroupWaitBits( system_event_group, PTT_PRESSED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));

        // 2. Перевіряємо, чи ми прокинулись саме через натискання PTT
        if ((bits & PTT_PRESSED_BIT) != 0) {
            ESP_LOGD(TAG, "PTT натиснута, починаю запис...");

            speaker_handle_transmit_start();

            while (1) {
                // Перевіряємо, чи не відпустили кнопку
                bits = xEventGroupWaitBits(system_event_group, PTT_RELEASED_BIT, pdTRUE, pdFALSE, 0);
                if ((bits & PTT_RELEASED_BIT) != 0) {
                    ESP_LOGD(TAG, "PTT відпущена, вихід з режиму передачі.");
                    break; // Виходимо з внутрішнього циклу і повертаємось до очікування
                }
                // Логіка запису та відправки в чергу (з пулом пам'яті)
                uint8_t* mono_buffer;
                if (xQueueReceive(shared_buffer_pool, &mono_buffer, pdMS_TO_TICKS(100)) == pdTRUE) {
                    hal_audio_read((uint8_t*)stereo_buffer, AUDIO_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
                    if (bytes_read > 0) {
                        int actual_frames_read = bytes_read / 4;
                        convert_stereo_to_mono((int16_t*)mono_buffer, stereo_buffer, actual_frames_read);
                        audio_chunk_t chunk = { .buffer = mono_buffer, .len = bytes_read / 2 };
                        xQueueSend(mic_to_net_queue, &chunk, 0);
                    } else {
                        xQueueSend(shared_buffer_pool, &mono_buffer, 0);
                    }
                } else {
                    ESP_LOGW(TAG, "Немає вільних буферів для запису!");
                }
            }
        }
        else {
            // Це наш РЕЖИМ ПРИЙОМУ
            audio_chunk_t received_chunk;            
            // Перевіряємо вхідну чергу (можна без таймауту, бо цикл і так періодичний)
            if (xQueueReceive(net_to_speaker_queue, &received_chunk, 0) == pdTRUE) {
                start_time = esp_timer_get_time();
                size_t stereo_size = received_chunk.len * 2;
                // int16_t stereo_buffer[stereo_size / sizeof(int16_t)];

                int mono_samples = received_chunk.len / sizeof(int16_t);
                convert_mono_to_stereo(stereo_buffer, (const int16_t*)received_chunk.buffer, mono_samples);

                size_t bytes_written;

                speaker_handle_playback();

                ESP_ERROR_CHECK_WITHOUT_ABORT(hal_audio_write((uint8_t*)stereo_buffer, stereo_size, &bytes_written));

                xQueueSend(shared_buffer_pool, &received_chunk.buffer, 0);

                last_audio_time_us = esp_timer_get_time();

                elapsed_time = last_audio_time_us - start_time;
                ESP_LOGI(TAG, "Функція audio_task()[%lld] виконувалася %lld мікросекунд, до [%lld]",start_time, elapsed_time,last_audio_time_us);
            } else {
                speaker_handle_timeout();
                // // Пакетів у черзі немає. Перевіряємо, чи не час вимкнути динамік.
                // if (stray_cur_state.is_speaker_on && (esp_timer_get_time() - last_audio_time_us > SPEAKER_OFF_TIMEOUT_US)) {
                //     ESP_LOGD(TAG, "Таймаут тиші, вимикаю динамік.");
                // }
            }

        }
    }
}