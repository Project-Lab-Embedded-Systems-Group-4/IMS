#include "test_uart.h"
#include "driver/uart.h"
#include "espidf-drivers/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ims-mcu-driver/uart.h"
#include <stdio.h>
#include <string.h>

#define UART1_TX_PIN 17
#define UART1_RX_PIN 16
#define UART2_TX_PIN 33
#define UART2_RX_PIN 32

void test_uart_run(void) {
    printf("Starting UART Master-Slave Test...\n");

    // 1. Initialize UART1
    static struct ims_device uart1_dev = {.name = "uart1"};
    static struct espidf_uart_config uart1_cfg = {
        .uart_port = UART_NUM_1,
        .uart_config =
            {
                .baud_rate = 115200,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .source_clk = UART_SCLK_DEFAULT,
            },
        .tx_pin = UART1_TX_PIN,
        .rx_pin = UART1_RX_PIN,
        .rts_pin = -1,
        .cts_pin = -1,
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
        .event_queue_size = 0,
    };
    static struct espidf_uart_data uart1_data;
    esp_err_t err1 = espidf_uart_init(&uart1_dev, &uart1_cfg, &uart1_data);
    if (err1 != ESP_OK) {
        printf("UART1 init failed: %d\n", err1);
        return;
    }

    // 2. Initialize UART2
    static struct ims_device uart2_dev = {.name = "uart2"};
    static struct espidf_uart_config uart2_cfg = {
        .uart_port = UART_NUM_2,
        .uart_config =
            {
                .baud_rate = 115200,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .source_clk = UART_SCLK_DEFAULT,
            },
        .tx_pin = UART2_TX_PIN,
        .rx_pin = UART2_RX_PIN,
        .rts_pin = -1,
        .cts_pin = -1,
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
        .event_queue_size = 0,
    };
    static struct espidf_uart_data uart2_data;
    esp_err_t err2 = espidf_uart_init(&uart2_dev, &uart2_cfg, &uart2_data);
    if (err2 != ESP_OK) {
        printf("UART2 init failed: %d\n", err2);
        return;
    }

    printf("UART1 and UART2 initialized\n");

    // 3. Test Transfer
    const char *msg = "Hello from UART1!";
    uint8_t rx_buf[32] = {0};

    printf("UART1 Sending: %s\n", msg);
    ims_uart_write(&uart1_dev, (uint8_t *)msg, strlen(msg), 1000);

    // Give some time for transmission
    vTaskDelay(100 / portTICK_PERIOD_MS);

    int len = ims_uart_read(&uart2_dev, rx_buf, strlen(msg), 1000);
    if (len > 0) {
        rx_buf[len] = '\0';
        printf("UART2 Received %d bytes: %s\n", len, (char *)rx_buf);
    } else {
        printf("UART2 Received nothing (check wiring: 17->32)\n");
    }
}
