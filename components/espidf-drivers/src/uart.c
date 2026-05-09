#include <errno.h>
#include <esp_log.h>

#include "espidf-drivers/uart.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/uart.h"

#define TAG "espidf_uart"

static int espidf_uart_send(const struct ims_device *dev, const uint8_t *wbuf,
                            size_t num_bytes, uint32_t timeout_ms) {
    const struct espidf_uart_config *config = dev->config;
    int len = uart_write_bytes(config->uart_port, (const char *)wbuf, num_bytes);
    if (len < 0) {
        return -EIO;
    }
    return 0;
}

static int espidf_uart_receive(const struct ims_device *dev, uint8_t *rbuf,
                               size_t num_bytes, uint32_t timeout_ms) {
    const struct espidf_uart_config *config = dev->config;
    int len = uart_read_bytes(config->uart_port, rbuf, num_bytes, timeout_ms / portTICK_PERIOD_MS);
    if (len < 0) {
        return -EIO;
    }
    return len; // Returns number of bytes read
}

static int espidf_uart_transfer(const struct ims_device *dev, const uint8_t *wbuf,
                                uint8_t *rbuf, size_t num_bytes, uint32_t timeout_ms) {
    int ret = espidf_uart_send(dev, wbuf, num_bytes, timeout_ms);
    if (ret != 0) return ret;
    return espidf_uart_receive(dev, rbuf, num_bytes, timeout_ms);
}

static const struct ims_uart_driver_api espidf_uart_api = {
    .send = espidf_uart_send,
    .receive = espidf_uart_receive,
    .transfer = espidf_uart_transfer,
};

esp_err_t espidf_uart_init(struct ims_device *dev,
                           const struct espidf_uart_config *config,
                           struct espidf_uart_data *data) {
    dev->api = &espidf_uart_api;
    dev->config = config;
    dev->data = data;

    esp_err_t esp_err;
    
    // 1. Configure parameters first
    esp_err = uart_param_config(config->uart_port, &config->uart_config);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "param config: %s", esp_err_to_name(esp_err));
        return esp_err;
    }

    // 2. Set pins
    esp_err = uart_set_pin(config->uart_port, config->tx_pin, config->rx_pin,
                           config->rts_pin, config->cts_pin);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "set pin: %s", esp_err_to_name(esp_err));
        return esp_err;
    }

    // 3. Install driver (Handle event queue pointer safely)
    QueueHandle_t *event_queue_ptr = (config->event_queue_size > 0) ? &data->event_queue : NULL;
    esp_err = uart_driver_install(
        config->uart_port, config->rx_buffer_size, config->tx_buffer_size,
        config->event_queue_size, event_queue_ptr, 0);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "driver install: %s", esp_err_to_name(esp_err));
        return esp_err;
    }

    // 4. HW Flow control if needed
    if (config->uart_config.flow_ctrl == UART_HW_FLOWCTRL_CTS_RTS) {
        esp_err = uart_set_hw_flow_ctrl(
            config->uart_port, UART_HW_FLOWCTRL_CTS_RTS, SOC_UART_FIFO_LEN - 8);
    }
    return 0;
}

uart_port_t espidf_uart_get_port(const struct ims_device *dev) {
    const struct espidf_uart_config *cfg = dev->config;
    return cfg->uart_port;
}
