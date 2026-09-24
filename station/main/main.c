#include <wifi.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "microlink.h"
#include "microlink_internal.h"

const char *TAG = "wifi station";

#define MSG_PORT 9000

static microlink_t *ml = NULL;
static microlink_udp_socket_t *udp_sock = NULL;

void vpn_init();

void app_main(void)
{
    /* Initialize RNG */
    psa_crypto_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL)
    {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    vpn_init();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void on_state_change(microlink_t *ml_handle, microlink_state_t state, void *user_data)
{
    const char *state_names[] = {
        "IDLE", "WIFI_WAIT", "CONNECTING", "REGISTERING",
        "CONNECTED", "RECONNECTING", "ERROR"};
    const char *name = (state < sizeof(state_names) / sizeof(state_names[0]))
                           ? state_names[state]
                           : "UNKNOWN";
    ESP_LOGI(TAG, "MicroLink state: %s", name);

    if (state == ML_STATE_CONNECTED)
    {
        uint32_t ip = microlink_get_vpn_ip(ml_handle);
        char ip_str[16];
        microlink_ip_to_str(ip, ip_str);
        ESP_LOGI(TAG, "Connected! VPN IP: %s", ip_str);
    }
}

static void on_peer_update(microlink_t *ml_handle, const microlink_peer_info_t *peer,
                           void *user_data)
{
    char ip_str[16];
    microlink_ip_to_str(peer->vpn_ip, ip_str);
    ESP_LOGI(TAG, "Peer: %s (%s) online=%d direct=%d",
             peer->hostname, ip_str, peer->online, peer->direct_path);
}

static uint32_t msg_rx_count = 0;
static void on_udp_rx(microlink_udp_socket_t *sock, uint32_t src_ip, uint16_t src_port,
                      const uint8_t *data, size_t len, void *user_data)
{
    msg_rx_count++;
    char ip_str[16];
    microlink_ip_to_str(src_ip, ip_str);

    /* Log the message (null-terminate for safe printing) */
    char msg[256];
    size_t copy_len = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
    memcpy(msg, data, copy_len);
    msg[copy_len] = '\0';
    /* Strip trailing newline if present */
    if (copy_len > 0 && msg[copy_len - 1] == '\n')
        msg[copy_len - 1] = '\0';

    ESP_LOGI(TAG, "UDP RX #%lu from %s:%u [%d bytes]: \"%s\"",
             (unsigned long)msg_rx_count, ip_str, src_port, (int)len, msg);

    /* Echo back with prefix */
    char reply[300];
    int reply_len = snprintf(reply, sizeof(reply), "ECHO: %s", msg);
    if (reply_len > 0)
    {
        esp_err_t err = microlink_udp_send(sock, src_ip, src_port, reply, reply_len);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "UDP TX echo -> %s:%u", ip_str, src_port);
        }
        else
        {
            ESP_LOGW(TAG, "UDP TX echo failed: %d (handshake in progress)", err);
        }
    }
}

void vpn_init()
{
    microlink_config_t config = {
        .auth_key = CONFIG_ML_TAILSCALE_AUTH_KEY,
        .device_name = CONFIG_ML_DEVICE_NAME,
        .enable_derp = true,
        .enable_stun = true,
        .enable_disco = true,
        .max_peers = CONFIG_ML_MAX_PEERS,
        .wifi_tx_power_dbm = 13, /* Reduced for thermal management */
    };
    ml = microlink_init(&config);
    if (!ml)
    {
        ESP_LOGE(TAG, "Failed to initialize MicroLink");
        return;
    }
    microlink_set_state_callback(ml, on_state_change, NULL);
    microlink_set_peer_callback(ml, on_peer_update, NULL);

    /* Start connecting */
    ESP_ERROR_CHECK(microlink_start(ml));
    /* Wait for CONNECTED state before creating UDP socket */
    while (!microlink_is_connected(ml))
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    /* Create UDP socket on port 9000 */
    udp_sock = microlink_udp_create(ml, MSG_PORT);
    if (!udp_sock)
    {
        ESP_LOGE(TAG, "Failed to create UDP socket");
    }
    else
    {
        ESP_LOGI(TAG, "UDP socket listening on port %d", MSG_PORT);
        microlink_udp_set_rx_callback(udp_sock, on_udp_rx, NULL);
    }
}