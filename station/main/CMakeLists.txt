idf_component_register(SRCS "wifi.c" "main.cpp"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
