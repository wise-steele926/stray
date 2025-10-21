idf_component_register(SRCS "peer_manager.c" "lan_task.c"
                    INCLUDE_DIRS "include"
                    REQUIRES common_defs fs esp_timer)
