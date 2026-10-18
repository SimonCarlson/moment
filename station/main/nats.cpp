#include <nats.h>

#include "espidf_nats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mbedtls/base64.h"
#include <cJSON.h>
#include "esp_mac.h"
#include "esp_heap_caps.h"

static NATS *nats = NULL;

// #define NATS_SERVER "100.76.38.123"
#define NATS_SERVER "192.168.0.27"
// #define NATS_SERVER CONFIG_NATS_SERVER
#define NATS_PORT CONFIG_NATS_PORT
#define NATS_USER CONFIG_NATS_USER
#define NATS_PASS CONFIG_NATS_PASS

#define NOTIF_DONE_BIT (1 << 0)
#define OBJ_NUID_BIT (1 << 1)
#define OBJ_FETCH_BIT (1 << 2)
#define OBJ_CONSUMER_CREATE_BIT (1 << 3)
#define OBJ_CONSUMER_DELETE_BIT (1 << 4)

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
    xEventGroupClearBits(nats_event_group, NOTIF_DONE_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_NUID_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_FETCH_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_CONSUMER_CREATE_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_CONSUMER_DELETE_BIT);
}

// TODO: Put in config
#define NATS_NOTIF_SUBJECT "EXAMPLE_NOTIF"
// #define NATS_NOTIF_SUBJECT CONFIG_NATS_NOTIF_SUBJECT

static char *nuid = NULL;
bool nats_check_for_message()
{
    if (nats == NULL)
    {
        return false;
    }

    nats->jetstream_pull("MOMENT_NOTIF", "parents", 1,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Got NATS NOTIF msg size=%d %.*s", msg.size, msg.size, msg.data);
            cJSON *msg_json = cJSON_Parse(msg.data);
            cJSON *nuid_item = cJSON_GetObjectItem(msg_json, "nuid");
            if (cJSON_IsString(nuid_item) && nuid_item->valuestring != NULL)
            {
                nuid = strdup(nuid_item->valuestring);
                ESP_LOGI(TAG, "NUID: %s size=%d", nuid, strlen(nuid));
            }
            cJSON_Delete(msg_json);
            ESP_LOGI(TAG, "Message reply=%s subject=%s", msg.reply, msg.subject);
            // if (msg.reply != NULL && msg.reply[0] != '\0')
            // {
            //     nats->jetstream_ack(msg.reply);
            //     ESP_LOGI(TAG, "Acked message on reply %s", msg.reply);
            // }
            xEventGroupSetBits(nats_event_group, NOTIF_DONE_BIT);
        }
    );

    process_nats_call(nats_event_group, NOTIF_DONE_BIT);

    return nuid != NULL;
}

struct MomentNotification
{
    char *nuid;
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

// TODO: Put in config
#define NATS_OBJ_BUCKET "MOMENT"

static MomentNotification notif;
static MomentDownload download;
bool nats_object_get()
{
    if (nats == NULL || nuid == NULL)
    {
        return false;
    }

    ESP_LOGI(TAG, "Getting NATS OBJ for nuid %.*s size=%d", strlen(nuid), nuid, strlen(nuid));


    // TODO: Maybe move this part into notif function since it is metadata and not data chunks
    nats->obj_get_info(NATS_OBJ_BUCKET, nuid,
        [](nats_msg_t msg){
            ESP_LOGI(TAG, "Got NATS OBJ msg size=%d %.*s", msg.size, msg.size, msg.data);
            cJSON *msg_json = cJSON_Parse(msg.data);
            cJSON *msg_message_item = cJSON_GetObjectItem(msg_json, "message");

            cJSON *msg_data_item = cJSON_GetObjectItem(msg_message_item, "data");
            if (cJSON_IsString(msg_data_item) && msg_data_item->valuestring != NULL)
            {
                ESP_LOGI(TAG, "Data to decode: %.*s", strlen(msg_data_item->valuestring), msg_data_item->valuestring);
                size_t expected_len;
                mbedtls_base64_decode(NULL, 0, &expected_len, (unsigned char*)msg_data_item->valuestring, strlen(msg_data_item->valuestring));

                char *dst = (char*)malloc(expected_len+1);
                size_t written_len = 0;
                int decode_ret = mbedtls_base64_decode((unsigned char*)dst, expected_len, &written_len, (unsigned char*)msg_data_item->valuestring, strlen(msg_data_item->valuestring));
                if (decode_ret != 0)
                {
                    // TODO: Handle failed decoding
                }
                dst[written_len] = '\0';

                ESP_LOGI(TAG, "decode_ret: %d", decode_ret);
                ESP_LOGI(TAG, "DECODED BASE64 %.*s", strlen(dst), dst);


                cJSON *data_json = cJSON_Parse(dst);
                cJSON *data_options_chunk = cJSON_GetObjectItem(data_json, "chunks");
                if (cJSON_IsNumber(data_options_chunk) && data_options_chunk->valueint > 0)
                {
                    notif.chunks = data_options_chunk->valueint;
                }
                cJSON *data_options_size = cJSON_GetObjectItem(data_json, "size");
                if (cJSON_IsNumber(data_options_size) && data_options_size->valueint > 0)
                {
                    notif.size = data_options_size->valueint;
                }
                cJSON *data_options_nuid = cJSON_GetObjectItem(data_json, "nuid");
                if (cJSON_IsString(data_options_nuid) && data_options_nuid->valuestring != NULL)
                {
                    notif.nuid = strdup(data_options_nuid->valuestring);
                }

                cJSON_Delete(data_json);
                free(dst);
            }
            ESP_LOGI(TAG, "NOTIF: chunks=%d size=%d nuid=%.*s", notif.chunks, notif.size, strlen(notif.nuid), notif.nuid);

            cJSON_Delete(msg_json);
            xEventGroupSetBits(nats_event_group, OBJ_NUID_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_NUID_BIT);

    if (notif.nuid == NULL)
    {
        return false;
    }


    char subject[1024];
    snprintf(subject, sizeof(subject), "$O.%s.C.%s", NATS_OBJ_BUCKET, notif.nuid);
    char consumer_name[32];
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(consumer_name, sizeof(consumer_name), "esp32_%02x%02x%02x", mac[3], mac[4], mac[5]);

    nats->jetstream_consumer_delete("OBJ_MOMENT", consumer_name,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Consumer deleted");
            xEventGroupSetBits(nats_event_group, OBJ_CONSUMER_DELETE_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_CONSUMER_DELETE_BIT);

    jetstream_consumer_config_t consumer_config{
        .stream_name = "OBJ_MOMENT",
        .durable_name = consumer_name,
        .filter_subject = subject,
        .deliver_all = true,
        .ack_policy = "explicit",
        .ack_wait = 30000000000LL, // 30 seconds
        .max_deliver = -1,
        .replay_policy = "instant",
        .ordered = false,
    };
    nats->jetstream_consumer_create(&consumer_config,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Consumer created");
            xEventGroupSetBits(nats_event_group, OBJ_CONSUMER_CREATE_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_CONSUMER_CREATE_BIT);

    download = MomentDownload{
        .buffer = (uint8_t*)heap_caps_malloc(notif.size, MALLOC_CAP_SPIRAM),
        .size = notif.size,
        .received = 0,
        .expected_chunks = notif.chunks,
        .received_chunks = 0,
        .error = false,
    };
    nats->jetstream_pull("OBJ_MOMENT", consumer_name, notif.chunks,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Got chunk subject=%s size=%d", msg.subject, msg.size);
            if (msg.size + download.received <= download.size && download.received_chunks < download.expected_chunks)
            {
                memcpy(&download.buffer[download.received], msg.data, msg.size);
                download.received += msg.size;
                download.received_chunks++;
                // if (msg.reply != NULL && msg.reply[0] != '\0')
                // {
                //     nats->jetstream_ack(msg.reply);
                //     ESP_LOGI(TAG, "Acked message on reply %s", msg.reply);
                // }
                if (download.received_chunks == download.expected_chunks && download.received == download.size)
                {
                    xEventGroupSetBits(nats_event_group, OBJ_FETCH_BIT);
                }
            }
            nats->jetstream_ack(msg.reply);
        }
    );

    // TODO: Should return bool and bail if false
    process_nats_call(nats_event_group, OBJ_FETCH_BIT, 30000);

    nats->jetstream_consumer_delete("OBJ_MOMENT", consumer_name,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Consumer deleted");
            xEventGroupSetBits(nats_event_group, OBJ_CONSUMER_DELETE_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_CONSUMER_DELETE_BIT);

    return true;
}

void process_nats_call(const EventGroupHandle_t event_group, int bit, const int timeout)
{
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout);
    const TickType_t start_tick = xTaskGetTickCount();
    while ((xEventGroupGetBits(event_group) & bit) == 0)
    {
        nats->process();
        vTaskDelay(pdMS_TO_TICKS(10));

        if ((xTaskGetTickCount() - start_tick) > timeout_ticks)
        {
            ESP_LOGE(TAG, "Hard timeout waiting for NATS response");
            break;
        }
    }
}