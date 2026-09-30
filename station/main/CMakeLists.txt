idf_component_register(SRCS "wifi.c" "main.cpp" "vpn.c"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS ".")

idf_component_get_property(lwip_lib lwip COMPONENT_LIB)

# Inject the hooks definition and project include path into lwip's compilation flags
target_compile_definitions(${lwip_lib} PRIVATE "ESP_IDF_LWIP_HOOK_FILENAME=\"lwip_hooks.h\"")
target_include_directories(${lwip_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")