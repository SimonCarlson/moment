#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

extern const char *TAG;

void nats_init();
void process_nats_call(const EventGroupHandle_t event_group, int bit, const int timeout = 5000);
bool nats_check_for_message();
bool nats_object_get();