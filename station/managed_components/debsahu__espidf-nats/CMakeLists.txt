cmake_minimum_required(VERSION 3.10)

# Check if esp_websocket_client component is available
# Note: Managed components use namespace prefix (espressif__esp_websocket_client)
# while built-in components use plain name (esp_websocket_client)
idf_build_get_property(build_components BUILD_COMPONENTS)
if("esp_websocket_client" IN_LIST build_components)
    set(WEBSOCKET_AVAILABLE TRUE)
    set(OPTIONAL_REQUIRES esp_websocket_client)
elseif("espressif__esp_websocket_client" IN_LIST build_components)
    set(WEBSOCKET_AVAILABLE TRUE)
    set(OPTIONAL_REQUIRES espressif__esp_websocket_client)
else()
    set(WEBSOCKET_AVAILABLE FALSE)
    set(OPTIONAL_REQUIRES "")
endif()

# cJSON: ESP-IDF <6.0 ships it as a built-in component named `json`; v6.0+
# moved it to the component registry as `espressif/cjson`.
if("json" IN_LIST build_components)
    set(CJSON_REQUIRES json)
elseif("espressif__cjson" IN_LIST build_components)
    set(CJSON_REQUIRES espressif__cjson)
else()
    set(CJSON_REQUIRES "")
endif()

idf_component_register(
    INCLUDE_DIRS include
    REQUIRES esp-tls lwip ${CJSON_REQUIRES} ${OPTIONAL_REQUIRES}
)

# Define CONFIG_ESP_WEBSOCKET_CLIENT_ENABLE if WebSocket component is available
if(WEBSOCKET_AVAILABLE)
    target_compile_definitions(${COMPONENT_LIB} INTERFACE CONFIG_ESP_WEBSOCKET_CLIENT_ENABLE=1)
    message(STATUS "espidf-nats: WebSocket support ENABLED (esp_websocket_client found)")
else()
    message(STATUS "espidf-nats: WebSocket support DISABLED (esp_websocket_client not found)")
endif()
