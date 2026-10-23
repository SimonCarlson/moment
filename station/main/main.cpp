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

static const char *TAG = "moment";

// Handle to tailscale configured netif
static struct netif *ts_lwip_netif = NULL;

microlink_t *ml = NULL;

#define LOCAL_DEBUG 1

extern "C" void app_main(void)
{
    // Initialize RNG
    psa_crypto_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // esp_log_level_set("wifi", ESP_LOG_WARN);
    // esp_log_level_set("ml_wg_mgr", ESP_LOG_WARN);
    // esp_log_level_set("ml_net_io", ESP_LOG_WARN);
    // esp_log_level_set("ml_derp", ESP_LOG_WARN);

    if (!wifi_init_sta())
    {
        ESP_LOGE(TAG, "Failed to connect to wifi, aborting");
        // TODO: Graceful shutdown
        return;
    }

    if (!vpn_init(ml))
    {
        ESP_LOGE(TAG, "Failed to connect to wifi, aborting");
        // TODO: Graceful shutdown
        return;
    }
#ifndef LOCAL_DEBUG
    ESP_LOGI(TAG, "Waiting for Microlink to connect");
    while (!microlink_is_connected(ml))
    {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
#endif
    ESP_LOGI(TAG, "Microlink has connected");
    ts_lwip_netif = (netif *)microlink_get_netif_impl(ml);

#ifndef LOCAL_DEBUG
    ESP_LOGI(TAG, "Waiting for TS NETIF to come UP");
    while (ts_lwip_netif == NULL || !netif_is_up(ts_lwip_netif) || !netif_is_link_up(ts_lwip_netif))
    {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "TS NETIF is UP");
#endif

    if (!nats_init())
    {
        ESP_LOGE(TAG, "Failed to connect to NATS, aborting");
        // TODO: Graceful shutdown
        return;
    }

    if (!nats_check_for_notification())
    {
        ESP_LOGE(TAG, "No notification message, aborting");
        // TODO: Graceful shutdown
        // TODO: Report back
        return;
    }

    MomentDownload download;
    if (!nats_get_object(&download))
    {
        ESP_LOGE(TAG, "Could not get image, aborting");
        // TODO: Graceful shutdown
        // TODO: Report back
        return;
    }

    // TODO: Write to screen
    // TODO: Report back
    // TODO: Graceful shutdown
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Needed to provide Tailscale routing for destination hosts on that subnet
// Microlink doesn't register routes to the standard netif
extern "C" struct netif *ts_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest)
{
    // Tailscale uses CGNAT 100.64.0.0/10
    // 100.64.0.0  -> 0x64400000
    // 255.192.0.0 -> 0xFFC00000
    uint32_t ts_network = PP_HTONL(0x64400000);
    uint32_t ts_mask = PP_HTONL(0xFFC00000);

    // Irrelevant address
    if ((dest->addr & ts_mask) != (ts_network & ts_mask))
    {
        return NULL;
    }
    if (ts_lwip_netif == NULL)
    {
        ESP_LOGW(TAG, "HOOK: Triggered but netif is null");
        return NULL;
    }

    // Return the tailscale netif if dest matches the tailscale subnet
    if (!netif_is_up(ts_lwip_netif))
    {
        ESP_LOGW(TAG, "HOOK: Triggered but netif is down");
        return NULL;
    }
    if (!netif_is_link_up(ts_lwip_netif))
    {
        ESP_LOGW(TAG, "HOOK: Triggered but link is down");
        return NULL;
    }

    return ts_lwip_netif;
}
