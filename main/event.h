#ifndef IMS_EVENT_H_
#define IMS_EVENT_H_

#include <esp_event.h>

ESP_EVENT_DECLARE_BASE(IMS_EVENT_BASE);

enum ims_event {
    IMS_EVENT_AD5933_START_SWEEP = 0x1,
    IMS_EVENT_AD5933_DATA_READY = 0x2,
    IMS_EVENT_AD5933_ERROR = 0x4,
};

#endif // IMS_EVENT_H_
