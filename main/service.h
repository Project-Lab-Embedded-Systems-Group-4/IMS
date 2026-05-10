#ifndef IMS_SERVICE_H_
#define IMS_SERVICE_H_

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

struct service;

struct service_api {
    esp_err_t (*run)(struct service *s);
};

struct service {
    // service name
    const char *name;
    // general data for service stop
    SemaphoreHandle_t stop_sem;
    SemaphoreHandle_t done_sem;

    TaskHandle_t handle;

    const struct service_api *api;
    void *config;
    void *data;
};

esp_err_t service_run(struct service *s);
esp_err_t service_stop(struct service *s);
esp_err_t service_join(struct service *s, uint32_t timeout_ms);
esp_err_t service_need_stop(struct service *s);
esp_err_t service_mark_done(struct service *s);

#endif // IMS_SERVICE_H_
