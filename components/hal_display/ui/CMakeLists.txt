SET(SOURCES screens/ui_screenmain_.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_wifi_png.c
    images/ui_img_rx_png.c
    images/ui_img_tx_png.c
    images/ui_img_trans_png.c)

add_library(ui ${SOURCES})
