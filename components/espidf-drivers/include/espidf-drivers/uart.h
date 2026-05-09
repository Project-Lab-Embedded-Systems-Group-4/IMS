#ifndef ESP_IMS_MCU_DRIVER_APP_ESPIDF_DRIVERS_UART_H_
#define ESP_IMS_MCU_DRIVER_APP_ESPIDF_DRIVERS_UART_H_

#include <driver/uart.h>

#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/gpio.h"

struct espidf_uart_config {
    uart_config_t uart_config;
    uart_port_t uart_port;
    int tx_pin;
    int rx_pin;
    int cts_pin;
    int rts_pin;
    size_t rx_buffer_size;
    size_t tx_buffer_size;
    size_t event_queue_size;
};

struct espidf_uart_data {
    QueueHandle_t event_queue;
};

esp_err_t espidf_uart_init(struct ims_device *dev,
                           const struct espidf_uart_config *config,
                           struct espidf_uart_data *data);

// FIXME: for esp-modem temporary, should be remove in the future
uart_port_t espidf_uart_get_port(const struct ims_device *dev);

#endif // ESP_IMS_MCU_DRIVER_APP_ESPIDF_DRIVERS_UART_H_
