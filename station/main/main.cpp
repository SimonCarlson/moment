extern "C"
{
#include <wifi.h>
#include <vpn.h>
}
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "microlink.h"

#include "espidf_nats.h"

const char *TAG = "wifi station";

// Handle to tailscale configured netif
static struct netif *ts_lwip_netif = NULL;

#define NATS_SERVER "100.76.38.123"
// #define NATS_SERVER CONFIG_NATS_SERVER
#define NATS_PORT CONFIG_NATS_PORT
#define NATS_USER CONFIG_NATS_USER
#define NATS_PASS CONFIG_NATS_PASS

microlink_t *ml = NULL;
static NATS *nats = NULL;

void nats_init()
{
    nats = new NATS(NATS_SERVER, NATS_PORT, NATS_USER, NATS_PASS);
    if (nats == NULL)
    {
        ESP_LOGE(TAG, "Failed to create NATS client");
        return;
    }

    nats->on_connect = []()
    { ESP_LOGI(TAG, "Connected to NATS"); };
    nats->on_disconnect = []()
    { ESP_LOGW(TAG, "Disconnected from NATS"); };
    nats->on_error = []()
    { ESP_LOGE(TAG, "️NATS error: %s", nats->last_error_string()); };

    ESP_LOGI(TAG, "Connecting to NATS server %s:%d", NATS_SERVER, NATS_PORT);
    // For some reason it seems like every other connection attempt succeeds. Don't know why
    // TODO: Should at least wrap this in some retries
    if (!nats->connect())
    {
        ESP_LOGE(TAG, "Failed to connect to NATS server: %s", nats->last_error_string());
        return;
    }
}

extern "C" void app_main(void)
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

    // if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL)
    // {
    /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
     * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("ml_wg_mgr", ESP_LOG_WARN);
    esp_log_level_set("ml_net_io", ESP_LOG_WARN);
    esp_log_level_set("ml_derp", ESP_LOG_WARN);
    // }

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    vpn_init();

    ts_lwip_netif = (netif *)microlink_get_netif_impl(ml);

    nats_init();
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" struct netif *ts_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    if (ts_lwip_netif == NULL)
    {
        return NULL;
    }

    // Tailscale uses CGNAT 100.64.0.0/10
    // 100.64.0.0 -> 0x64400000
    // 255.192.0.0 -> 0xFFC000000
    uint32_t ts_network = PP_HTONL(0x64400000);
    uint32_t ts_mask = PP_HTONL(0xFFC00000);

    // Return the tailscale netif if dest matches the tailscale subnet
    if ((dest->addr & ts_mask) == (ts_network & ts_mask))
    {
        char ip_str[16];
        microlink_ip_to_str(dest->addr, ip_str);
        ESP_LOGI(TAG, "Returning tailscale netif for %s", ip_str);
        return ts_lwip_netif;
    }

    // Fall back to the default netif otherwise
    return NULL;
}
