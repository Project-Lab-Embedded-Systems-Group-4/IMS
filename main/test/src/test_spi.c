#include "test_spi.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "espidf-drivers/spi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ims-mcu-driver/spi.h"
#include <stdio.h>
#include <string.h>

// Master Pins (SPI2)
#define MASTER_HOST SPI2_HOST
#define MASTER_MOSI_IO 13
#define MASTER_MISO_IO 12
#define MASTER_SCLK_IO 14
#define MASTER_CS_IO 15

// Slave Pins (SPI3)
#define SLAVE_HOST SPI3_HOST
#define SLAVE_MOSI_IO 23
#define SLAVE_MISO_IO 19
#define SLAVE_SCLK_IO 18
#define SLAVE_CS_IO 5

#define BUFFER_SIZE 32

void test_spi_run(void) {
    printf("Starting SPI Master-Slave Test...\n");

    // 1. Initialize Slave (SPI3) using raw ESP-IDF
    spi_bus_config_t slave_buscfg = {
        .mosi_io_num = SLAVE_MOSI_IO,
        .miso_io_num = SLAVE_MISO_IO,
        .sclk_io_num = SLAVE_SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_slave_interface_config_t slave_slvcfg = {
        .mode = 0,
        .spics_io_num = SLAVE_CS_IO,
        .queue_size = 3,
        .flags = 0,
    };
    esp_err_t err = spi_slave_initialize(SLAVE_HOST, &slave_buscfg,
                                         &slave_slvcfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK)
        printf("Slave init failed\n");

    // 2. Initialize Master (SPI2) using IMS driver
    struct espidf_spibus_config master_bus_cfg = {
        .port = MASTER_HOST,
        .config =
            {
                .mosi_io_num = MASTER_MOSI_IO,
                .miso_io_num = MASTER_MISO_IO,
                .sclk_io_num = MASTER_SCLK_IO,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
            },
        .dma_chan = SPI_DMA_CH_AUTO,
    };

    err = espidf_spibus_init(&master_bus_cfg);

    static struct ims_device master_dev = {.name = "spi-master"};
    static struct espidf_spidev_config master_dev_cfg = {
        .port = MASTER_HOST,
        .config =
            {
                .mode = 0,
                .clock_speed_hz = 1000000,
                .spics_io_num = MASTER_CS_IO,
                .queue_size = 7,
            },
    };
    static struct espidf_spidev_data master_data;
    err = espidf_spidev_init(&master_dev, &master_dev_cfg, &master_data);

    printf("SPI Master and Slave initialized\n");

    // 3. Prepare DMA-capable Buffers
    uint8_t *master_send_buf = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
    uint8_t *master_recv_buf = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
    uint8_t *slave_send_buf = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);
    uint8_t *slave_recv_buf = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA);

    if (!master_send_buf || !master_recv_buf || !slave_send_buf ||
        !slave_recv_buf) {
        printf("Failed to allocate DMA buffers\n");
        return;
    }

    memset(master_send_buf, 0, BUFFER_SIZE);
    memset(master_recv_buf, 0, BUFFER_SIZE);
    memset(slave_send_buf, 0, BUFFER_SIZE);
    memset(slave_recv_buf, 0, BUFFER_SIZE);

    strcpy((char *)master_send_buf, "Master to Slave");
    strcpy((char *)slave_send_buf, "Slave to Master");

    spi_slave_transaction_t slave_trans;
    memset(&slave_trans, 0, sizeof(slave_trans));
    slave_trans.length = BUFFER_SIZE * 8;
    slave_trans.tx_buffer = slave_send_buf;
    slave_trans.rx_buffer = slave_recv_buf;

    // 4. Queue Slave Transaction
    spi_slave_queue_trans(SLAVE_HOST, &slave_trans, portMAX_DELAY);

    // 5. Master Transfer
    printf("Master Sending: %s\n", (char *)master_send_buf);
    ims_spi_transfer(&master_dev, master_send_buf, master_recv_buf,
                     BUFFER_SIZE);

    // 6. Wait for Slave to complete
    spi_slave_transaction_t *ret_trans;
    spi_slave_get_trans_result(SLAVE_HOST, &ret_trans, portMAX_DELAY);

    printf("Master Received: %s\n", (char *)master_recv_buf);
    printf("Slave Received: %s\n", (char *)slave_recv_buf);

    // 7. Cleanup
    free(master_send_buf);
    free(master_recv_buf);
    free(slave_send_buf);
    free(slave_recv_buf);
}
