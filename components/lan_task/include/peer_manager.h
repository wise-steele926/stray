//#include "peer_manager.h"
#ifndef PEER_MANAGER_H
#define PEER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "shared_resources.h"


// // Структура для зберігання інформації про одного учасника
// typedef struct {
//     bool is_active;                      // Чи активний цей запис?
//     uint64_t sn;                         // Унікальний серійний номер (як у header->sender_sn)
//     char name[PEER_NAME_LEN];            // Ім'я учасника
//     uint32_t ip_address;                 // IP-адреса у вигляді числа
//     int64_t last_seen_timestamp_us;      // Час останнього контакту (в мікросекундах)
// } peer_info_t;

// --- Публічні функції ---

// Ініціалізує менеджер: створює м'ютекс та очищує список
void peer_manager_init(void);

// Оновлює інформацію про учасника або додає нового.
// Це основна функція, яку буде викликати lan_rx_task.
void peer_manager_update_peer(uint64_t sn, const char* name, uint32_t ip_address);

// Видаляє неактивних учасників зі списку.
// Цю функцію буде періодично викликати lan_ping_task.
void peer_manager_cleanup_inactive(void);

// Функція для отримання копії списку активних учасників (наприклад, для GUI)
// Повертає кількість скопійованих учасників.
int8_t peer_manager_get_peers(peer_info_t* peer_list, int max_peers);


#endif // PEER_MANAGER_H