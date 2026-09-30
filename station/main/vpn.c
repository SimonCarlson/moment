#include <vpn.h>

#include "microlink.h"
#include "microlink_internal.h"
#include "esp_log.h"

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

// TODO: Put in config
#define NATS_HOST_DNS "home.kudu-puffin.ts.net"
static uint32_t nats_peer_ip = 0;
static void on_peer_update(microlink_t *ml_handle, const microlink_peer_info_t *peer,
                           void *user_data)
{
    ESP_LOGI(TAG, "Peer hostname: %s", peer->hostname);
    if (strstr(peer->hostname, NATS_HOST_DNS) != NULL)
    {
        nats_peer_ip = peer->vpn_ip;
        char ip_str[16];
        microlink_ip_to_str(peer->vpn_ip, ip_str);
        ESP_LOGI(TAG, "Found NATS server on: %s (%s) online=%d direct=%d",
                 peer->hostname, ip_str, peer->online, peer->direct_path);
    }
}

void vpn_init()
{
    microlink_config_t config = {};
    config.auth_key = CONFIG_ML_TAILSCALE_AUTH_KEY,
    config.device_name = CONFIG_ML_DEVICE_NAME,
    config.enable_derp = true,
    config.enable_stun = true,
    config.enable_disco = true,
    config.max_peers = CONFIG_ML_MAX_PEERS,
    config.wifi_tx_power_dbm = 13, /* Reduced for thermal management */

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
}