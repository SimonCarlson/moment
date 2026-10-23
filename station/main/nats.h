#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

struct MomentNotification
{
    char *nuid;
    size_t size = 0;
    uint8_t chunks = 0;
};

struct MomentDownload
{
    uint8_t *buffer = nullptr;
    size_t size = 0;
    size_t received_bytes = 0;
    uint8_t expected_chunks = 0;
    uint8_t received_chunks = 0;
    bool error = false;
};

bool nats_init();
void process_nats_call(const EventGroupHandle_t event_group, int bit, const int timeout = 5000);
bool nats_check_for_notification();
bool nats_get_notification();
bool nats_get_object(MomentDownload *download);