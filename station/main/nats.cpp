#include <nats.h>

#include "espidf_nats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mbedtls/base64.h"
#include <cJSON.h>

static NATS *nats = NULL;

// #define NATS_SERVER "100.76.38.123"
#define NATS_SERVER "192.168.0.27"
// #define NATS_SERVER CONFIG_NATS_SERVER
#define NATS_PORT CONFIG_NATS_PORT
#define NATS_USER CONFIG_NATS_USER
#define NATS_PASS CONFIG_NATS_PASS

#define OBJ_DONE_BIT (1 << 0)
#define NOTIF_DONE_BIT (1 << 1)

static EventGroupHandle_t nats_event_group = NULL;
// static bool obj_fetch_success = false;

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
    if (nats_event_group == NULL)
    {
        nats_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(nats_event_group, OBJ_DONE_BIT);
    xEventGroupClearBits(nats_event_group, NOTIF_DONE_BIT);
}

// TODO: Put in config
#define NATS_NOTIF_SUBJECT "EXAMPLE_NOTIF"
// #define NATS_NOTIF_SUBJECT CONFIG_NATS_NOTIF_SUBJECT

static char *nuid = NULL;
char *nats_check_for_message()
{
    if (nats == NULL)
    {
        return nuid;
    }

    nats->jetstream_pull("MOMENT_NOTIF", "parents", 1,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Got NATS NOTIF msg size=%d %.*s", msg.size, msg.size, msg.data);
            cJSON* msg_json = cJSON_Parse(msg.data);
            cJSON* nuid_item = cJSON_GetObjectItem(msg_json, "nuid");
            if (cJSON_IsString(nuid_item) && nuid_item->valuestring != NULL)
            {
                nuid = nuid_item->valuestring;
                ESP_LOGI(TAG, "NUID: %s", nuid);
            }
            cJSON_Delete(msg_json);
            ESP_LOGI(TAG, "Message reply=%s subject=%s", msg.reply, msg.subject);
            if (msg.reply != NULL && msg.reply[0] != '\0')
            {
                nats->jetstream_ack(msg.reply);
                ESP_LOGI(TAG, "Acked message on reply %s", msg.reply);
            }
            xEventGroupSetBits(nats_event_group, NOTIF_DONE_BIT);
        }
    );

    TickType_t start_tick = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(6000);

    while ((xEventGroupGetBits(nats_event_group) & NOTIF_DONE_BIT) == 0)
    {
        nats->process();
        vTaskDelay(pdMS_TO_TICKS(10));

        // if ((xTaskGetTickCount() - start_tick) > timeout_ticks)
        // {
        //     ESP_LOGE(TAG, "Hard timeout waiting for notif message");
        //     break;
        // }
    }

    return nuid;
}

struct MomentNotification
{
    char nuid[128];
    size_t size;
    uint8_t chunks;
};

struct MomentDownload
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    size_t received = 0;
    uint8_t expected_chunks = 0;
    uint8_t received_chunks = 0;
    bool error = false;

};

bool nats_object_get()
{
//     if (nats == NULL)
//     {
//         return false;
//     }

//     ESP_LOGI(TAG, "GETTING NATS OBJECT");
//     // TODO: Must get the object metadata name in notification
//     char *subject = "";
//     nats->subscribe(subject,
//         [](nats_msg_t msg)
//         {

//         }
//     );

//             obj_fetch_success = false;
//             xEventGroupSetBits(obj_event_group, OBJ_DONE_BIT);

//     TickType_t start_tick = xTaskGetTickCount();
//     const TickType_t timeout_ticks = pdMS_TO_TICKS(6000);

//     while ((xEventGroupGetBits(obj_event_group) & OBJ_DONE_BIT) == 0)
//     {
//         nats->process();
//         vTaskDelay(pdMS_TO_TICKS(10));

//         if ((xTaskGetTickCount() - start_tick) > timeout_ticks)
//         {
//             ESP_LOGE(TAG, "Hard timeout waiting for NATS response");
//             break;
//         }
//     }

//     // TODO: Handle 404 on object
//     ESP_LOGI(TAG, "obj_ret %d", obj_ret);
//     ESP_LOGI(TAG, "obj_fetch_success %d", obj_fetch_success);
//     return obj_fetch_success;
    return false;
}