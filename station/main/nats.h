#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

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

bool nats_init();
void process_nats_call(const EventGroupHandle_t event_group, int bit, const int timeout = 5000);
bool nats_check_for_message();
bool nats_object_get(MomentDownload *download);