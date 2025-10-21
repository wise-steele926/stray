idf_component_register(SRCS "hal_input.c" "hal_encoder.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver bsp esp_timer hal_i2c_bus esp_driver_i2c common_defs
                    )
