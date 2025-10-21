#ifndef LAN_TASK_H
#define LAN_TASK_H

#include <stdint.h> // Для типу uint16_t
#include <stddef.h>
#include "shared_resources.h"

// void lan_task(void* pvParameters);
void lan_manager_init(int port);
void lan_set_tx_target (peer_info_t *peer);

#endif 