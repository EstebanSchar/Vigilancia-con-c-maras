idf_component_register(SRCS "actuadores.c" "app_wifi.c" "Camera.c" "stream_server.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_event nvs_flash lwip driver esp_timer esp_http_server esp_psram)