idf_component_register(SRCS "wifi_provisioning.c" "hal_sntp.c" "hal_net.c" 
                    INCLUDE_DIRS "include" 
                    REQUIRES esp-tls esp_wifi  common_defs esp_http_server nvs_flash fs) #channels
