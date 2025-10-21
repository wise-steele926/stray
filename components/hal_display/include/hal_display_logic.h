#ifndef HAL_DISPLAY_LOGIC_H
#define HAL_DISPLAY_LOGIC_H

#include <stdint.h>
#include "shared_resources.h"



/**
 * @brief Оновлює всі віджети, які потребують періодичного оновлення.
 * Цю функцію потрібно викликати з головного циклу app_main.
 */
void hal_display_periodic_update(void);

peer_info_t hal_display_get_peer_by_index( uint16_t index);

// TODO: Додати функції для обробки інших подій, наприклад, від мережі
// void hal_display_notify_peer_speaking(uint64_t peer_sn);

void change_tab(uint32_t tab);

void hal_display_focus_on_peer(uint64_t peer_sn);

void hal_display_set_tx_mode(bool is_tx_active);

void hal_display_set_rcv_img(bool onAir);
void hal_display_set_wifi_img(bool wifiON);

void hal_display_roller_timer_init(void);

void hal_display_update_peer_list_widget(void);

#endif // HAL_DISPLAY_LOGIC_H