idf_component_register(SRCS "dht.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver esp_driver_gpio esp_rom esp_timer)
