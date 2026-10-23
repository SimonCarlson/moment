#include <nats.h>

#include "espidf_nats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mbedtls/base64.h"
#include <cJSON.h>
#include "esp_mac.h"
#include "esp_heap_caps.h"

static const char *TAG = "moment_nats";

static NATS *nats = NULL;

// #define NATS_SERVER "100.76.38.123"
#define NATS_SERVER "192.168.0.27"
// #define NATS_SERVER CONFIG_NATS_SERVER
#define NATS_PORT CONFIG_NATS_PORT
#define NATS_USER CONFIG_NATS_USER
#define NATS_PASS CONFIG_NATS_PASS
#define NATS_NOTIF_STREAM CONFIG_NATS_NOTIF_STREAM
#define NATS_NOTIF_CONSUMER CONFIG_NATS_NOTIF_CONSUMER
#define NATS_OBJ_BUCKET CONFIG_NATS_OBJ_BUCKET
#define NATS_OBJ_STREAM "OBJ_" NATS_OBJ_BUCKET

#define NOTIF_GET_BIT BIT0
#define OBJ_NUID_GET_BIT BIT1
#define OBJ_FETCH_BIT BIT2
#define OBJ_CONSUMER_CREATE_BIT BIT3
#define OBJ_CONSUMER_DELETE_BIT BIT4

static EventGroupHandle_t nats_event_group = NULL;

bool nats_init()
{
    nats = new NATS(NATS_SERVER, NATS_PORT, NATS_USER, NATS_PASS);
    if (nats == NULL)
    {
        ESP_LOGE(TAG, "Failed to create NATS client");
        return false;
    }

    nats->on_connect = []()
    { ESP_LOGI(TAG, "Connected to NATS"); };
    nats->on_disconnect = []()
    { ESP_LOGW(TAG, "Disconnected from NATS"); };
    nats->on_error = []()
    { ESP_LOGE(TAG, "NATS error: %s", nats->last_error_string()); };

    ESP_LOGI(TAG, "Connecting to NATS server %s:%d", NATS_SERVER, NATS_PORT);
    const int retries = 3;
    for (int i = 1; i <= retries; i++)
    {
        if (!nats->connect())
        {
            ESP_LOGE(TAG, "Failed to connect to NATS server: %s, attempt %d/%d", nats->last_error_string(), i, retries);
            if (i == retries)
            {
                return false;
            }
        }
    }

    if (nats_event_group == NULL)
    {
        nats_event_group = xEventGroupCreate();
    }
    xEventGroupClearBits(nats_event_group, NOTIF_GET_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_NUID_GET_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_FETCH_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_CONSUMER_CREATE_BIT);
    xEventGroupClearBits(nats_event_group, OBJ_CONSUMER_DELETE_BIT);

    return true;
}


static char *nuid = NULL;
bool nats_check_for_notification()
{
    if (nats == NULL)
    {
        return false;
    }

    nats->jetstream_pull(NATS_NOTIF_STREAM, NATS_NOTIF_CONSUMER, 1,
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
            // TODO: Reinstate
            // if (msg.reply != NULL && msg.reply[0] != '\0')
            // {
            //     nats->jetstream_ack(msg.reply);
            //     ESP_LOGI(TAG, "Acked message on reply %s", msg.reply);
            // }
            xEventGroupSetBits(nats_event_group, NOTIF_GET_BIT);
        }
    );

    process_nats_call(nats_event_group, NOTIF_GET_BIT);

    if (nuid == NULL)
    {
        return false;
    }
    return nats_get_notification();
}

static MomentNotification notif;
bool nats_get_notification()
{
    if (nats == NULL)
    {
        return false;
    }

    nats->obj_get_info(NATS_OBJ_BUCKET, nuid,
        [](nats_msg_t msg){
            ESP_LOGI(TAG, "Got NATS OBJ msg size=%d %.*s", msg.size, msg.size, msg.data);
            cJSON *msg_json = cJSON_Parse(msg.data);
            cJSON *msg_message_item = cJSON_GetObjectItem(msg_json, "message");

            cJSON *msg_data_item = cJSON_GetObjectItem(msg_message_item, "data");
            if (cJSON_IsString(msg_data_item) && msg_data_item->valuestring != NULL)
            {
                char *obj_msg = msg_data_item->valuestring;
                int msg_len = strlen(obj_msg);
                ESP_LOGI(TAG, "Data to decode: %.*s", msg_len, obj_msg);
                size_t expected_len;
                mbedtls_base64_decode(NULL, 0, &expected_len, (unsigned char*)obj_msg, msg_len);

                char *dst = (char*)malloc(expected_len+1);
                size_t written_len = 0;
                int decode_ret = mbedtls_base64_decode((unsigned char*)dst, expected_len, &written_len, (unsigned char*)obj_msg, msg_len);
                if (decode_ret != 0)
                {
                    // TODO: Report back failed decoding
                    return;
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
            xEventGroupSetBits(nats_event_group, OBJ_NUID_GET_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_NUID_GET_BIT);

    return notif.chunks > 0 && notif.size > 0 && notif.nuid != NULL;
}


static MomentDownload *active_download = NULL;
bool nats_get_object(MomentDownload *download)
{
    if (nats == NULL || nuid == NULL)
    {
        return false;
    }
    active_download = download;

    ESP_LOGI(TAG, "Getting NATS OBJ for nuid %.*s size=%d", strlen(nuid), nuid, strlen(nuid));
    char subject[1024];
    snprintf(subject, sizeof(subject), "$O.%s.C.%s", NATS_OBJ_BUCKET, notif.nuid);
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char consumer_name[32];
    snprintf(consumer_name, sizeof(consumer_name), "esp32_%02x%02x%02x", mac[3], mac[4], mac[5]);

    // We create new consumers from the client since the subject changes dynamically (it is the base64 of the uploaded image name)
    // Deleting a non-existing consumer is handled gracefully by the NATS client lib while
    // trying to create an already existing consumer is not handled gracefully
    nats->jetstream_consumer_delete(NATS_OBJ_STREAM, consumer_name,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Consumer deleted");
            xEventGroupSetBits(nats_event_group, OBJ_CONSUMER_DELETE_BIT);
        }
    );

    process_nats_call(nats_event_group, OBJ_CONSUMER_DELETE_BIT);

    jetstream_consumer_config_t consumer_config{
        .stream_name = NATS_OBJ_STREAM,
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

    // TODO: Check the notif.size, if it's too big then bail and report an error
    active_download->buffer = (uint8_t*)heap_caps_malloc(notif.size, MALLOC_CAP_SPIRAM);
    active_download->size = notif.size;
    active_download->received_bytes = 0;
    active_download->expected_chunks = notif.chunks;
    active_download->received_chunks = 0;
    active_download->error = false;
    nats->jetstream_pull(NATS_OBJ_STREAM, consumer_name, notif.chunks,
        [](nats_msg_t msg)
        {
            ESP_LOGI(TAG, "Got chunk subject=%s size=%d", msg.subject, msg.size);
            if (active_download->received_bytes + msg.size <= active_download->size
                && active_download->received_chunks < active_download->expected_chunks
            )
            {
                memcpy(&active_download->buffer[active_download->received_bytes], msg.data, msg.size);
                active_download->received_bytes += msg.size;
                active_download->received_chunks++;
                // TODO: Reinstate
                // if (msg.reply != NULL && msg.reply[0] != '\0')
                // {
                //     nats->jetstream_ack(msg.reply);
                //     ESP_LOGI(TAG, "Acked message on reply %s", msg.reply);
                // }
                if (active_download->received_chunks == active_download->expected_chunks && active_download->received_bytes == active_download->size)
                {
                    xEventGroupSetBits(nats_event_group, OBJ_FETCH_BIT);
                }
            }
            nats->jetstream_ack(msg.reply);
        }
    );

    // TODO: Should return bool and bail if false
    process_nats_call(nats_event_group, OBJ_FETCH_BIT, 30000);

    nats->jetstream_consumer_delete(NATS_OBJ_STREAM, consumer_name,
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