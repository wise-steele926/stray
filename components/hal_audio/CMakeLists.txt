idf_component_register(SRCS "hal_audio.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver hal_i2c_bus bsp esp_driver_i2c esp_driver_i2s common_defs esp_timer
                    PRIV_REQUIRES    fs )
