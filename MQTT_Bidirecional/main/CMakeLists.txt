idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi nvs_flash esp_event ssd1306 dht
                       EMBED_TXTFILES "hivemq_ca.pem")