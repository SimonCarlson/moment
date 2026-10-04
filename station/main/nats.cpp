#include <nats.h>

#include "espidf_nats.h"

static NATS *nats = NULL;

#define NATS_SERVER "100.76.38.123"
// #define NATS_SERVER CONFIG_NATS_SERVER
#define NATS_PORT CONFIG_NATS_PORT
#define NATS_USER CONFIG_NATS_USER
#define NATS_PASS CONFIG_NATS_PASS

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