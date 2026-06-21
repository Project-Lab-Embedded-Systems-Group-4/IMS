#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board_utils.h"
#include "event.h"
#include "ims-mcu-driver/sensor/ad5933.h"
#include "service.h"
#include "services/ad5933/ad5933_service.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "ad5933_srv"

/* Default Sweep Configuration */
#define AD5933_DEFAULT_START_FREQ_HZ 5000 
#define AD5933_DEFAULT_INC_FREQ_HZ 10
#define AD5933_DEFAULT_NUM_INC 10 
#define AD5933_DEFAULT_SETTLE_CYCLES (AD5933_DEFAULT_START_FREQ_HZ / 2000 + 1)
#define AD5933_DEFAULT_VOLTAGE_RANGE AD5933_RANGE_200MV_PP
#define AD5933_DEFAULT_PGA_GAIN AD5933_PGA_GAIN_X1
#define AD5933_READ_TIMEOUT_MS 100

/* Calibration Defaults */
#define AD5933_CALI_CHANNEL BOARD_SUBJ_CH_CAL2
#define AD5933_CALI_RESISTOR 49900.0
#define AD5933_CALI_FB_INDEX 1

#define AD5933_DEFAULT_CH1_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH2_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH3_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH4_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH5_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH6_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH7_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH8_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH9_OFFSET_OHM 0.0
#define AD5933_DEFAULT_CH10_OFFSET_OHM 0.0

static const double ad5933_default_offsets[10] = {
    AD5933_DEFAULT_CH1_OFFSET_OHM,
    AD5933_DEFAULT_CH2_OFFSET_OHM,
    AD5933_DEFAULT_CH3_OFFSET_OHM,
    AD5933_DEFAULT_CH4_OFFSET_OHM,
    AD5933_DEFAULT_CH5_OFFSET_OHM,
    AD5933_DEFAULT_CH6_OFFSET_OHM,
    AD5933_DEFAULT_CH7_OFFSET_OHM,
    AD5933_DEFAULT_CH8_OFFSET_OHM,
    AD5933_DEFAULT_CH9_OFFSET_OHM,
    AD5933_DEFAULT_CH10_OFFSET_OHM
};

enum ad5933_service_state {
    AD5933_STATE_IDLE = 0,
    AD5933_STATE_STANDBY,         /* Step 1 / Wait state */
    AD5933_STATE_CALIBRATE,       /* Startup Calibration */
    AD5933_STATE_INIT_START_FREQ, /* Step 2 */
    AD5933_STATE_SWEEPING,        /* Step 3 */
    AD5933_STATE_READ_DATA,       /* Data Valid Poll */
    AD5933_STATE_MAX
};

static const char *state_names[] = {
    "IDLE", "STANDBY", "CALIBRATE", "INIT_START_FREQ",
    "SWEEPING", "READ_DATA"};

#define MAX_SAMPLES 512

struct ad5933_service_data {
    enum ad5933_service_state now_state;
    enum ad5933_service_state pre_state;
    SemaphoreHandle_t start_sem;
    SemaphoreHandle_t data_mutex;

    struct ad5933_sample_data samples[MAX_SAMPLES];
    double gain_factors[MAX_SAMPLES];
    uint16_t sample_count;

    bool is_calibrating;
    uint8_t cal_fb_index;
    enum board_subj_channel cal_channel;
    double cal_resistor;
    bool continuous;
    int interval_ms;
    int average_times;
    uint16_t orig_num_inc;
    uint32_t orig_inc_freq;
    bool has_saved_orig;
    bool stop_requested;
    double offsets[10];
};

static struct service *g_ad5933_srv = NULL;

static void ad5933_event_handler(void *arg, esp_event_base_t base,
                                 int32_t event_id, void *event_data) {
    struct service *s = (struct service *)arg;
    struct ad5933_service_data *data = (struct ad5933_service_data *)s->data;

    if (base == IMS_EVENT_BASE) {
        switch (event_id) {
        case IMS_EVENT_AD5933_START_SWEEP:
            data->is_calibrating = false;
            data->stop_requested = false;
            if (event_data != NULL) {
                struct ad5933_sweep_params *params = (struct ad5933_sweep_params *)event_data;
                data->continuous = params->continuous;
                data->interval_ms = params->interval_ms;
                data->average_times = params->average_times;
            } else {
                data->continuous = false;
                data->interval_ms = 1000;
                data->average_times = 1;
            }
            xSemaphoreGive(data->start_sem);
            break;
        case IMS_EVENT_AD5933_START_CAL: {
            uint8_t fb_index = AD5933_CALI_FB_INDEX;
            if (event_data != NULL) {
                fb_index = *(uint8_t *)event_data;
            }

            const uint32_t zm_cal_res[] = ZM_CAL_RES_VALUES;
            data->cal_fb_index = fb_index;

            if (fb_index == 0 || fb_index == 1) {
                data->cal_channel = BOARD_SUBJ_CH_CAL1;
                data->cal_resistor = (double)zm_cal_res[0];
            } else if (fb_index == 2) {
                data->cal_channel = BOARD_SUBJ_CH_CAL2;
                data->cal_resistor = (double)zm_cal_res[1];
            } else if (fb_index == 3) {
                data->cal_channel = BOARD_SUBJ_CH_CAL3;
                data->cal_resistor = (double)zm_cal_res[2];
            } else {
                // Invalid index, use defaults
                data->cal_fb_index = AD5933_CALI_FB_INDEX;
                data->cal_channel = AD5933_CALI_CHANNEL;
                data->cal_resistor = AD5933_CALI_RESISTOR;
            }

            data->is_calibrating = true;
            data->continuous = false;
            data->interval_ms = 1000;
            data->average_times = 1;
            xSemaphoreGive(data->start_sem);
            break;
        }
        case IMS_EVENT_AD5933_STOP_SWEEP:
            data->stop_requested = true;
            printf("Continuous sweep stopping gracefully...\n");
            break;
        case IMS_EVENT_AD5933_DATA_READY: {
            bool was_cal = false;
            bool was_continuous = false;
            if (event_data != NULL) {
                struct ad5933_data_ready_params *params = (struct ad5933_data_ready_params *)event_data;
                was_cal = params->is_cal;
                was_continuous = params->is_continuous;
            }

            if (was_cal) {
                printf("Calibration complete. Calculating gain factors...\n");
                for (int i = 0; i < data->sample_count; i++) {
                    double magnitude =
                        sqrt((double)data->samples[i].real * data->samples[i].real +
                             (double)data->samples[i].imag * data->samples[i].imag);

                    if (i >= sizeof(data->gain_factors) / sizeof(data->gain_factors[0])) {
                        ESP_LOGW(TAG, "Sample count exceeds gain factor array size");
                        break;
                    }

                    if (magnitude != 0) {
                        data->gain_factors[i] = 1.0 / (magnitude * data->cal_resistor);
                    } else {
                        data->gain_factors[i] = 0;
                    }
                }
            }

            struct board_utils_info board_info;
            board_utils_get_info(&board_info);
            double offset = 0;
            if (board_info.subj_channel >= 0 && board_info.subj_channel <= 9) {
                offset = data->offsets[board_info.subj_channel];
            }

            if (was_continuous) {
                double sum_real = 0;
                double sum_imag = 0;
                for (int i = 0; i < data->sample_count; i++) {
                    sum_real += data->samples[i].real;
                    sum_imag += data->samples[i].imag;
                }
                double avg_real = data->sample_count > 0 ? (sum_real / data->sample_count) : 0;
                double avg_imag = data->sample_count > 0 ? (sum_imag / data->sample_count) : 0;
                double magnitude = sqrt(avg_real * avg_real + avg_imag * avg_imag);

                double gf = data->gain_factors[0];
                double impedance = 0;
                if (gf != 0 && magnitude != 0) {
                    impedance = 1.0 / (gf * magnitude);
                    impedance -= offset;
                    if (impedance < 0) impedance = 0;
                }

                static uint32_t sweep_num = 0;
                sweep_num++;
                printf("#%" PRIu32 " | Real: %.2f | Imag: %.2f | Mag: %.2f | Imp: %.2f Ohm\n",
                       sweep_num, avg_real, avg_imag, magnitude, impedance);
            } else {
                printf("\n\n%-5s | %-10s | %-10s | %-12s | %-12s | %-15s\n", "Index", "Real",
                       "Imag", "Magnitude", "Gain Factor", "Impedance (Ohm)");
                printf("------|------------|------------|--------------|--------------|-----------------\n");
                for (int i = 0; i < data->sample_count; i++) {
                    double magnitude =
                        sqrt((double)data->samples[i].real * data->samples[i].real +
                             (double)data->samples[i].imag * data->samples[i].imag);
                    double gf = data->gain_factors[i];
                    double impedance = 0;
                    if (gf != 0 && magnitude != 0) {
                        impedance = 1.0 / (gf * magnitude);
                        impedance -= offset;
                        if (impedance < 0) impedance = 0;
                    }
                    printf("%-5d | %-10d | %-10d | %-12.2f | %-12.2e | %-15.2f\n", i,
                           data->samples[i].real, data->samples[i].imag, magnitude,
                           gf, impedance);
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

static void ad5933_service_task(void *arg) {
    struct service *s = (struct service *)arg;
    struct ad5933_service_config *cfg =
        (struct ad5933_service_config *)s->config;
    struct ad5933_service_data *data = (struct ad5933_service_data *)s->data;
    const struct ims_device *ad_dev = cfg->ad5933_dev;

    ESP_LOGI(TAG, "AD5933 Service Task Started");

    while (service_need_stop(s) != ESP_OK) {
        ad5933_ctrl_reg1_t ctrl1;
        ad5933_status_reg_t status;
        uint32_t delay_ms = 0;

        if (data->now_state != data->pre_state) {
            ESP_LOGD(TAG, "State transition: %s -> %s",
                     state_names[data->pre_state],
                     state_names[data->now_state]);
            data->pre_state = data->now_state;
        }

        switch (data->now_state) {
        case AD5933_STATE_IDLE:
            ESP_ERROR_CHECK(ad5933_reset(ad_dev));

            xSemaphoreTake(data->start_sem, portMAX_DELAY);
            if (data->is_calibrating) {
                data->now_state = AD5933_STATE_CALIBRATE;
            } else {
                data->now_state = AD5933_STATE_STANDBY;
            }

            /* Lock shared board resources before starting measurement */
            if (board_utils_lock(portMAX_DELAY) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to lock board resources");
                break;
            }

            delay_ms = 0;
            break;

        case AD5933_STATE_CALIBRATE:
            ESP_LOGI(TAG, "Starting Auto-Calibration (FB:%d, CH:%d, R:%.0f)...", 
                     data->cal_fb_index, data->cal_channel, data->cal_resistor);
            board_set_subj_channel(data->cal_channel);
            board_set_zm_fb(data->cal_fb_index);

            data->now_state = AD5933_STATE_STANDBY;
            delay_ms = 0;
            break;

        case AD5933_STATE_STANDBY:
            if (data->continuous && data->average_times > 1 && !data->has_saved_orig) {
                ad5933_get_num_inc(ad_dev, &data->orig_num_inc);
                ad5933_get_inc_freq(ad_dev, &data->orig_inc_freq);
                data->has_saved_orig = true;

                ad5933_set_num_inc(ad_dev, data->average_times);
                ad5933_set_inc_freq(ad_dev, 0);
            }

            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_STANDBY;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            xSemaphoreTake(data->data_mutex, portMAX_DELAY);
            data->sample_count = 0;
            xSemaphoreGive(data->data_mutex);

            //ad5933_set_start_freq(ad_dev, drv_data->start_freq);

            board_measure_enable(true);

            data->now_state = AD5933_STATE_INIT_START_FREQ;
            delay_ms = 0;
            break;

        case AD5933_STATE_INIT_START_FREQ:
            /* Step 2: Program initialize with start frequency */
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_INIT_START_FREQ;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            // TODO: Add timeout handling here
            delay_ms = 100;
            data->now_state = AD5933_STATE_SWEEPING;
            break;

        case AD5933_STATE_SWEEPING:
            /* Step 3: Program start frequency sweep */
            ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
            ctrl1.function_code = AD5933_FUNC_START_FREQ_SWEEP;
            ad5933_set_ctrl_reg1(ad_dev, ctrl1);

            struct ad5933_data *d = (struct ad5933_data *)ad_dev->data;
            d->sweep_index = 0;

            data->now_state = AD5933_STATE_READ_DATA;
            delay_ms = 1;
            break;

        case AD5933_STATE_READ_DATA: {
            uint64_t wait_start = esp_timer_get_time();
            bool timeout = false;
            while (1) {
                ad5933_get_status_reg(ad_dev, &status);
                if (status.data_valid)
                    break;

                if ((esp_timer_get_time() - wait_start) >
                    (AD5933_READ_TIMEOUT_MS * 1000)) {
                    ESP_LOGE(TAG, "Timeout waiting for data_valid");
                    timeout = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            if (timeout) {
                board_measure_enable(false);
                board_utils_unlock();
                if (data->has_saved_orig) {
                    ad5933_set_num_inc(ad_dev, data->orig_num_inc);
                    ad5933_set_inc_freq(ad_dev, data->orig_inc_freq);
                    data->has_saved_orig = false;
                }
                data->now_state = AD5933_STATE_IDLE;
                break;
            }

            int16_t real, imag;
            if (ad5933_get_complex_data(ad_dev, &real, &imag, true) == 0) {
                xSemaphoreTake(data->data_mutex, portMAX_DELAY);
                if (data->sample_count < MAX_SAMPLES) {
                    data->samples[data->sample_count].real = real;
                    data->samples[data->sample_count].imag = imag;
                    data->sample_count++;
                }
                xSemaphoreGive(data->data_mutex);
            }

            bool sweep_done = status.sweep_complete;

            if (sweep_done) {
                /* Finish: Power down and release resources */
                ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
                ctrl1.function_code = AD5933_FUNC_POWER_DOWN;
                ad5933_set_ctrl_reg1(ad_dev, ctrl1);

                board_measure_enable(false);
                board_utils_unlock();

                struct ad5933_data_ready_params ready_params = {
                    .is_cal = data->is_calibrating,
                    .is_continuous = data->continuous,
                };
                esp_event_post_to(cfg->loop, IMS_EVENT_BASE,
                                  IMS_EVENT_AD5933_DATA_READY, &ready_params,
                                  sizeof(ready_params), portMAX_DELAY);

                if (data->stop_requested) {
                    data->continuous = false;
                    data->stop_requested = false;
                }

                if (data->continuous) {
                    vTaskDelay(pdMS_TO_TICKS(data->interval_ms));
                    data->now_state = AD5933_STATE_IDLE;
                    xSemaphoreGive(data->start_sem);
                } else {
                    if (data->has_saved_orig) {
                        ad5933_set_num_inc(ad_dev, data->orig_num_inc);
                        ad5933_set_inc_freq(ad_dev, data->orig_inc_freq);
                        data->has_saved_orig = false;
                    }
                    data->now_state = AD5933_STATE_IDLE;
                    data->is_calibrating = false;
                }
            } else {
                /* Increment frequency for the next point */
                ad5933_get_ctrl_reg1(ad_dev, &ctrl1);
                ctrl1.function_code = AD5933_FUNC_INCREMENT_FREQ;
                ad5933_set_ctrl_reg1(ad_dev, ctrl1);

                struct ad5933_data *d_drv = (struct ad5933_data *)ad_dev->data;
                d_drv->sweep_index++;

                data->now_state = AD5933_STATE_READ_DATA;
            }
            delay_ms = 0;
            break;
        }

        default:
            data->now_state = AD5933_STATE_IDLE;
            delay_ms = 10;
            break;
        }

        if (delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    service_mark_done(s);
    vTaskDelete(NULL);
}

static esp_err_t ad5933_run(struct service *s) {
    struct ad5933_service_config *cfg =
        (struct ad5933_service_config *)s->config;

    ESP_ERROR_CHECK(esp_event_handler_instance_register_with(
        cfg->loop, IMS_EVENT_BASE, ESP_EVENT_ANY_ID,
        ad5933_event_handler, s, NULL));

    BaseType_t ret =
        xTaskCreate(ad5933_service_task, s->name, 4096, s, 5, &s->handle);
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}

static const struct service_api ad5933_api = {
    .run = ad5933_run,
};

static void ad5933_load_or_init_nvs_settings(const struct ims_device *ad_dev, struct ad5933_service_data *data) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ad5933", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle! Using hardcoded defaults.", esp_err_to_name(err));
        ad5933_set_start_freq(ad_dev, AD5933_DEFAULT_START_FREQ_HZ);
        ad5933_set_inc_freq(ad_dev, AD5933_DEFAULT_INC_FREQ_HZ);
        ad5933_set_num_inc(ad_dev, AD5933_DEFAULT_NUM_INC);
        ad5933_set_voltage_range(ad_dev, AD5933_DEFAULT_VOLTAGE_RANGE);
        ad5933_set_pga_gain(ad_dev, AD5933_DEFAULT_PGA_GAIN);
        memcpy(data->offsets, ad5933_default_offsets, sizeof(ad5933_default_offsets));
        return;
    }

    uint32_t start_freq = AD5933_DEFAULT_START_FREQ_HZ;
    err = nvs_get_u32(my_handle, "start_freq", &start_freq);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u32(my_handle, "start_freq", start_freq);
    }
    ad5933_set_start_freq(ad_dev, start_freq);

    uint32_t inc_freq = AD5933_DEFAULT_INC_FREQ_HZ;
    err = nvs_get_u32(my_handle, "inc_freq", &inc_freq);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u32(my_handle, "inc_freq", inc_freq);
    }
    ad5933_set_inc_freq(ad_dev, inc_freq);

    uint16_t num_inc = AD5933_DEFAULT_NUM_INC;
    err = nvs_get_u16(my_handle, "num_inc", &num_inc);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u16(my_handle, "num_inc", num_inc);
    }
    ad5933_set_num_inc(ad_dev, num_inc);

    uint8_t voltage_range = AD5933_DEFAULT_VOLTAGE_RANGE;
    err = nvs_get_u8(my_handle, "voltage_range", &voltage_range);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u8(my_handle, "voltage_range", voltage_range);
    }
    ad5933_set_voltage_range(ad_dev, (enum ad5933_voltage_range)voltage_range);

    uint8_t pga_gain = AD5933_DEFAULT_PGA_GAIN;
    err = nvs_get_u8(my_handle, "pga_gain", &pga_gain);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u8(my_handle, "pga_gain", pga_gain);
    }
    ad5933_set_pga_gain(ad_dev, (enum ad5933_pga_gain)pga_gain);

    // Load offsets
    for (uint8_t i = 0; i < 10; i++) {
        char key[16];
        snprintf(key, sizeof(key), "offset_%u", i);
        uint64_t val_u64;
        err = nvs_get_u64(my_handle, key, &val_u64);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            double default_offset = ad5933_default_offsets[i];
            memcpy(&val_u64, &default_offset, sizeof(double));
            nvs_set_u64(my_handle, key, val_u64);
            data->offsets[i] = default_offset;
        } else {
            double offset_val;
            memcpy(&offset_val, &val_u64, sizeof(double));
            data->offsets[i] = offset_val;
        }
    }

    nvs_commit(my_handle);
    nvs_close(my_handle);
}

esp_err_t ad5933_service_get_offsets(double *out_offsets, uint8_t count) {
    if (g_ad5933_srv == NULL) return ESP_ERR_INVALID_STATE;
    struct ad5933_service_data *data = (struct ad5933_service_data *)g_ad5933_srv->data;
    if (count > 10) count = 10;
    xSemaphoreTake(data->data_mutex, portMAX_DELAY);
    memcpy(out_offsets, data->offsets, count * sizeof(double));
    xSemaphoreGive(data->data_mutex);
    return ESP_OK;
}

esp_err_t ad5933_service_set_offset(uint8_t channel_index, double offset_ohm) {
    if (g_ad5933_srv == NULL) return ESP_ERR_INVALID_STATE;
    if (channel_index >= 10) return ESP_ERR_INVALID_ARG;
    struct ad5933_service_data *data = (struct ad5933_service_data *)g_ad5933_srv->data;

    xSemaphoreTake(data->data_mutex, portMAX_DELAY);
    data->offsets[channel_index] = offset_ohm;
    xSemaphoreGive(data->data_mutex);

    // Save to NVS
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("ad5933", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        char key[16];
        snprintf(key, sizeof(key), "offset_%u", channel_index);
        uint64_t val_u64;
        memcpy(&val_u64, &offset_ohm, sizeof(double));
        nvs_set_u64(my_handle, key, val_u64);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
    return ESP_OK;
}

esp_err_t ad5933_service_init(struct service *s,
                              struct ad5933_service_config *config) {
    struct ad5933_service_data *data =
        calloc(1, sizeof(struct ad5933_service_data));
    if (data == NULL)
        return ESP_ERR_NO_MEM;

    data->start_sem = xSemaphoreCreateBinary();
    data->data_mutex = xSemaphoreCreateMutex();
    data->now_state = AD5933_STATE_IDLE;
    data->pre_state = AD5933_STATE_IDLE;

    data->cal_fb_index = AD5933_CALI_FB_INDEX;
    data->cal_channel = AD5933_CALI_CHANNEL;
    data->cal_resistor = AD5933_CALI_RESISTOR;
    data->is_calibrating = true;
    xSemaphoreGive(data->start_sem);

    s->api = &ad5933_api;
    s->config = config;
    s->data = data;
    s->name = "ad5933_srv";

    /* Driver Initialization with defaults */
    const struct ims_device *ad_dev = config->ad5933_dev;
    ad5933_load_or_init_nvs_settings(ad_dev, data);
    ad5933_set_settling_cycles(ad_dev, AD5933_DEFAULT_SETTLE_CYCLES, AD5933_SETTLE_X1);

    g_ad5933_srv = s;

    return service_run(s);
}

esp_err_t ad5933_service_get_results(struct ad5933_sample_data **out_samples,
                                     double **out_gain_factors,
                                     uint16_t *out_count) {
    if (g_ad5933_srv == NULL)
        return ESP_ERR_INVALID_STATE;
    struct ad5933_service_data *data =
        (struct ad5933_service_data *)g_ad5933_srv->data;

    if (data->now_state != AD5933_STATE_IDLE) {
        return ESP_ERR_INVALID_STATE; // Sweep in progress
    }

    *out_samples = data->samples;
    if (out_gain_factors) {
        *out_gain_factors = data->gain_factors;
    }
    *out_count = data->sample_count;
    return ESP_OK;
}
