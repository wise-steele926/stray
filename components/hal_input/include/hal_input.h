// components\hal_input\hal_input.h

#pragma once
#include "bsp.h"


// --- Timings ---
#define INPUT_SCAN_INTERVAL_MS     100       // Polling rate in ms
#define LONG_PRESS_DURATION_MS     400     // Time to trigger long press (ms)

#define INPUT_TASK_STACK_SIZE 4096
#define INPUT_TASK_PRIORITY   5

static const uint8_t KB_translate[8] = {0, KB_KEY_4, KB_KEY_2, KB_KEY_1, KB_KEY_3, KB_KEY_5, KB_KEY_6, KB_KEY_7};

void poll_encoder(void);
void input_task(void *pvParameters);
