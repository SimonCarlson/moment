extern "C"
{
#include <wifi.h>
#include <vpn.h>
}
#include <nats.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "psa/crypto.h"

#include "microlink.h"

const char *TAG = "wifi station";

// Handle to tailscale configured netif
static struct netif *ts_lwip_netif = NULL;

microlink_t *ml = NULL;

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

// Needed to provide Tailscale routing for destination hosts on that subnet
// Microlink doesn't register routes to the standard netif
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
