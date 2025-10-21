idf_component_register(SRCS "hal_i2c_bus.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver bsp esp_driver_i2c log)
