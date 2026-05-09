#ifndef ESP_IMS_MCU_DRIVER_OS_PORT_H_
#define ESP_IMS_MCU_DRIVER_OS_PORT_H_

#include <errno.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#define IMS_MCU_DRIVER_TAG "ims_mcu_driver"

#define ims_msleep(msec)                                                        \
    do {                                                                       \
        vTaskDelay(msec / portTICK_PERIOD_MS);                                 \
    } while (0)

#define ims_mutex_t SemaphoreHandle_t

#define ims_mutex_init(mutex)                                                   \
    do {                                                                       \
        mutex = xSemaphoreCreateMutex();                                       \
    } while (0)

#define ims_mutex_lock(mutex)                                                   \
    do {                                                                       \
        xSemaphoreTake((mutex), portMAX_DELAY);                                \
    } while (0)

#define ims_mutex_unlock(mutex)                                                 \
    do {                                                                       \
        xSemaphoreGive((mutex));                                               \
    } while (0)

#define ims_logd(fmt, ...)                                                      \
    do {                                                                       \
        ESP_LOGD(IMS_MCU_DRIVER_TAG, fmt, ##__VA_ARGS__);                       \
    } while (0)

#define ims_loge(fmt, ...)                                                      \
    do {                                                                       \
        ESP_LOGE(IMS_MCU_DRIVER_TAG, fmt, ##__VA_ARGS__);                       \
    } while (0)

#define ims_logw(fmt, ...)                                                      \
    do {                                                                       \
        ESP_LOGW(IMS_MCU_DRIVER_TAG, fmt, ##__VA_ARGS__);                       \
    } while (0)

#define ims_logi(fmt, ...)                                                      \
    do {                                                                       \
        ESP_LOGI(IMS_MCU_DRIVER_TAG, fmt, ##__VA_ARGS__);                       \
    } while (0)

#endif // ESP_IMS_MCU_DRIVER_OS_PORT_H_
