#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdlib.h>

#include "event.h"
#include "board_utils.h"
#include "services/ad7680/ad7680_service.h"

#define TAG "ad7680_srv"

struct ad7680_service_data {
    uint16_t last_value;
    uint16_t iterations;
    SemaphoreHandle_t data_mutex;
    SemaphoreHandle_t start_sem;
};

#define DEFAULT_RM_RANGE BOARD_RM_RANGE_2
#define DEFAULT_RM_REF_RESISTOR RM_REF_RES_VALUES[DEFAULT_RM_RANGE]
#define DEFAULT_RM_CAL_CHANNEL BOARD_SUBJ_CH_CAL1

static struct service *g_ad7680_srv = NULL;

static void ad7680_event_handler(void *arg, esp_event_base_t base,
                                 int32_t event_id, void *event_data) {
    struct service *s = (struct service *)arg;
    struct ad7680_service_data *data = (struct ad7680_service_data *)s->data;

    if (base == IMS_EVENT_BASE) {
        switch (event_id) {
        case IMS_EVENT_AD7680_START_READ:
            if (event_data != NULL) {
                data->iterations = *(uint16_t *)event_data;
            } else {
                data->iterations = 1;
            }
            xSemaphoreGive(data->start_sem);
            break;
        case IMS_EVENT_AD7680_DATA_READY: {
            uint16_t val = *(uint16_t *)event_data;
            printf("AD7680 Raw Value: %u\n", val);

            // Calculate Resistance
            struct board_utils_info info;
            board_utils_get_info(&info);
            if (info.rm_range_index >= 0 && info.rm_range_index < 4) {
                const uint32_t ref_res[] = RM_REF_RES_VALUES;
                uint32_t r_ref = ref_res[info.rm_range_index];
                
                if (val < 65535) {
                    double resistance = (double)r_ref * (double)val / (65536.0 - (double)val);
                    printf("Measured Resistance: %.2f Ohm (Range Ref: %" PRIu32 " Ohm)\n",
                           resistance, r_ref);
                } else {
                    printf("Measured Resistance: Over Range\n");
                }
            } else {
                printf("Measured Resistance: Unknown Range\n");
            }
            break;
        }
        default:
            break;
        }
    }
}

static void ad7680_service_task(void *arg) {
    struct service *s = (struct service *)arg;
    struct ad7680_service_config *cfg =
        (struct ad7680_service_config *)s->config;
    struct ad7680_service_data *data = (struct ad7680_service_data *)s->data;

    ESP_LOGI(TAG, "AD7680 Service Task Started");

    while (service_need_stop(s) != ESP_OK) {
        if (xSemaphoreTake(data->start_sem, portMAX_DELAY) == pdTRUE) {
            uint16_t val;
            if (ad7680_read_averaging(cfg->ad7680_dev, &val, data->iterations) == 0) {
                xSemaphoreTake(data->data_mutex, portMAX_DELAY);
                data->last_value = val;
                xSemaphoreGive(data->data_mutex);

                esp_event_post_to(cfg->loop, IMS_EVENT_BASE,
                                  IMS_EVENT_AD7680_DATA_READY, &val, sizeof(val),
                                  portMAX_DELAY);
            } else {
                ESP_LOGW(TAG, "Failed to read AD7680");
            }
        }
    }

    service_mark_done(s);
    vTaskDelete(NULL);
}

static esp_err_t ad7680_run(struct service *s) {
    struct ad7680_service_config *cfg =
        (struct ad7680_service_config *)s->config;

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, IMS_EVENT_AD7680_START_READ,
        ad7680_event_handler, s, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, IMS_EVENT_AD7680_DATA_READY,
        ad7680_event_handler, s, NULL));

    BaseType_t ret =
        xTaskCreate(ad7680_service_task, s->name, 4096, s, 5, &s->handle);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

static const struct service_api ad7680_api = {
    .run = ad7680_run,
};

esp_err_t ad7680_service_init(struct service *s,
                              struct ad7680_service_config *config) {
    struct ad7680_service_data *data =
        calloc(1, sizeof(struct ad7680_service_data));
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    data->data_mutex = xSemaphoreCreateMutex();
    data->start_sem = xSemaphoreCreateBinary();
    if (data->data_mutex == NULL || data->start_sem == NULL) {
        if (data->data_mutex)
            vSemaphoreDelete(data->data_mutex);
        if (data->start_sem)
            vSemaphoreDelete(data->start_sem);
        free(data);
        return ESP_ERR_NO_MEM;
    }

    s->api = &ad7680_api;
    s->config = config;
    s->data = data;
    s->name = "ad7680_srv";

    g_ad7680_srv = s;

    return service_run(s);
}

esp_err_t ad7680_service_get_value(uint16_t *out_value) {
    if (g_ad7680_srv == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    struct ad7680_service_data *data = (struct ad7680_service_data *)g_ad7680_srv->data;

    xSemaphoreTake(data->data_mutex, portMAX_DELAY);
    *out_value = data->last_value;
    xSemaphoreGive(data->data_mutex);

    return ESP_OK;
}
