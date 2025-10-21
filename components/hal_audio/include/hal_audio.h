#pragma once
#include <stdio.h>
#include "esp_err.h"

#define AIC3120_I2C_ADDR        0x18    

// Адреси регістрів для нових функцій
#define SPK_AMP_ADDR            32  // Page 1, Register 32: Speaker Amplifier Control Register
#define SPL_DRIVER_ADDR         42  // Page 1, Register 42: Speaker Driver Control Register
#define SPL_ATT_REG             38  // Page 1, Register 38: Left Speaker Volume Control Register (припускаємо, що це для лівого або моно)
                                    // Якщо потрібен правий, то це регістр 39 (0x27)
#define FILTER_REG              60  // Page 0, Register 60: ADC Digital Filter Control Register
#define BEEP_REG                71  // Page 0, Register 71: Beep Generator Control Register


#define I2S_DMA_BUF_SIZE 320
#define I2S_DMA_BUF_NUM 8
#define I2S_SAMPLE_RATE 16000
#define I2S_TIMEOUT ((I2S_DMA_BUF_SIZE*1000)/I2S_SAMPLE_RATE + 1)


extern const uint8_t AICstartupSettings[][2];

esp_err_t hal_audio_init(void);
void PlayFile(char *fname);
void play_sine_wave(int freq_hz, int duration_ms);
void aic3120_set_speaker_volume(uint8_t volume);
void audio_task(void* pvParameters);