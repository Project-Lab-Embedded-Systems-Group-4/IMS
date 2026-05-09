#include <driver/i2c.h>
#include <freertos/FreeRTOS.h>

#include "espidf-drivers/i2c.h"

static int espidf_i2c_transfer(const struct ims_device *dev, uint16_t addr,
                               struct ims_i2c_msg *msgs, uint8_t num_msgs,
                               uint32_t timeout_ms) {

#define DBG_ESPIDF_I2C_TRANSFER 0
    int err = 0;
    esp_err_t esp_err;

    struct espidf_i2c_data *data = (struct espidf_i2c_data *)dev->data;
    if (data != NULL && data->mutex != NULL) {
        ims_mutex_lock(data->mutex);
    }

    i2c_cmd_handle_t *cmd = i2c_cmd_link_create();

    // payload
    for (int i = 0; i < num_msgs; i++) {
        const struct ims_i2c_msg *msg = &msgs[i];
        bool isRead = (msg->flags & IMS_I2C_MSG_READ) != 0;
        bool isStop = (msg->flags & IMS_I2C_MSG_STOP) != 0;

        // start/restart bit
        esp_err = i2c_master_start(cmd);
        if (esp_err != ESP_OK) {
            err = -EINVAL;
            goto RELEASE_CMD;
        }

        // i2c address + read/write
        esp_err = i2c_master_write_byte(
            cmd, (addr << 1) | ((isRead) ? I2C_MASTER_READ : I2C_MASTER_WRITE),
            true);
        if (esp_err != ESP_OK) {
            err = -EINVAL;
            goto RELEASE_CMD;
        }

        // payload
        if (msg->len > 0) {
            if (isRead && isStop) {
                if (msg->len > 1) {
                    esp_err = i2c_master_read(cmd, msg->buf, msg->len - 1,
                                              I2C_MASTER_ACK);
                    if (esp_err != ESP_OK) {
                        err = -EINVAL;
                        goto RELEASE_CMD;
                    }
                }
                esp_err = i2c_master_read(cmd, msg->buf + msg->len - 1, 1,
                                          I2C_MASTER_NACK);
                if (esp_err != ESP_OK) {
                    err = -EINVAL;
                    goto RELEASE_CMD;
                }
            } else if (isRead && !isStop) {
                esp_err =
                    i2c_master_read(cmd, msg->buf, msg->len, I2C_MASTER_ACK);
                if (esp_err != ESP_OK) {
                    err = -EINVAL;
                    goto RELEASE_CMD;
                }
            } else {
                esp_err = i2c_master_write(cmd, msg->buf, msg->len, true);
                if (esp_err != ESP_OK) {
                    err = -EINVAL;
                    goto RELEASE_CMD;
                }
            }
        }
    }

    // i2c stop bit
    esp_err = i2c_master_stop(cmd);
    if (esp_err != ESP_OK) {
        err = -EINVAL;
        goto RELEASE_CMD;
    }

    struct espidf_i2c_config *cfg = (struct espidf_i2c_config *)dev->config;
    esp_err =
        i2c_master_cmd_begin(cfg->port, cmd, timeout_ms / portTICK_PERIOD_MS);
    switch (esp_err) {
    case ESP_OK:
        break;
    case ESP_ERR_TIMEOUT:
        err = -ETIMEDOUT;
        goto RELEASE_CMD;
    default:
        err = -EIO;
        goto RELEASE_CMD;
    }

#if (DBG_ESPIDF_I2C_TRANSFER)
    for (int i = 0; i < num_msgs; i++) {
        const struct ims_i2c_msg *msg = &msgs[i];

        for (int m = 0; m < msg->len; ++m) {
            printf("num_msgs[%d]->data[%d] = 0x%02X address: %p\n", i, m,
                   msg->buf[m], (void *)&msg->buf[m]);
        }
    }
#endif

RELEASE_CMD:
    i2c_cmd_link_delete(cmd);
    if (data != NULL && data->mutex != NULL) {
        ims_mutex_unlock(data->mutex);
    }
    return err;
}

static const struct ims_i2c_driver_api espidf_i2c_api = {
    .transfer = espidf_i2c_transfer,
};

esp_err_t espidf_i2c_init(struct ims_device *dev,
                          const struct espidf_i2c_config *config,
                          struct espidf_i2c_data *data) {
    i2c_param_config(config->port, &config->config);
    i2c_driver_install(config->port, config->config.mode, config->rxbuf_size,
                       config->txbuf_size, 0);

    if (config->clkstretch_timeout_tick) {
        i2c_set_timeout(config->port, config->clkstretch_timeout_tick);
    }

    dev->api = &espidf_i2c_api;
    dev->config = config;
    dev->data = data;
    return ESP_OK;
}
