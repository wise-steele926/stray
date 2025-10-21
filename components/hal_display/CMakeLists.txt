idf_component_register(SRCS "hal_display_logic.c" "hal_display_helpers.c" "hal_display.c" "ui/ui.c" "ui/ui_helpers.c" "ui/screens/ui_screenmain_.c"  "ui/ui_events.c" "ui/images/ui_img_rx_png.c" "ui/images/ui_img_tx_png.c" "ui/images/ui_img_wifi_png.c" "ui/images/ui_img_trans_png.c"
                    INCLUDE_DIRS "include" "ui"
                    REQUIRES freertos driver esp_lcd common_defs lan_task bsp)
