#ifndef IMS_MCU_DRIVER_UART_H_
#define IMS_MCU_DRIVER_UART_H_

#include <stdint.h>

#include "ims-mcu-driver/assert.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/util.h"
#include "errno.h"

#ifdef __cplusplus
/**
 * @brief
 *
 */
#endif
typedef void (*ims_uart_rx_isr_t)(uint8_t ch, void *arg);
typedef void (*ims_uart_tx_isr_t)(uint8_t ch, void *arg);

struct ims_uart_driver_api {
    /**
     * @brief Start sending data to USART transmitter.
     *
     * @param wbuf Pointer to buffer with data to send to USART transmitter
     * @param num_bytes Number of data items to send
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     */
    int (*send)(const struct ims_device *dev, const uint8_t *wbuf,
                size_t num_bytes, uint32_t timeout_ms);
    /**
     * @brief Start receiving data from USART receiver.
     *
     * @param rbuf Pointer to buffer for data to receive from USART receiver
     * @param num_bytes Number of data items to receive
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     */

    int (*receive)(const struct ims_device *dev, uint8_t *rbuf, size_t num_bytes,
                   uint32_t timeout_ms);

    /**
     * @brief Start sending/receiving data to/from USART transmitter/receiver.
     *
     * @param wbuf Pointer to buffer with data to send to USART transmitter
     * @param rbuf Pointer to buffer for data to receive from USART receiver
     * @param num_bytes Number of data items to transfer
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     */
    int (*transfer)(const struct ims_device *dev, const uint8_t *wbuf,
                    uint8_t *rbuf, size_t num_bytes, uint32_t timeout_ms);

    void (*irq_callback_set)(const struct ims_device *dev,
                             ims_uart_rx_isr_t rx_isr, ims_uart_tx_isr_t tx_isr,
                             void *arg);

    /** Interrupt driven transfer enabling function */
    void (*irq_tx_enable)(const struct ims_device *dev);

    /** Interrupt driven transfer disabling function */
    void (*irq_tx_disable)(const struct ims_device *dev);

    /** Interrupt driven receiver enabling function */
    void (*irq_rx_enable)(const struct ims_device *dev);

    /** Interrupt driven receiver disabling function */
    void (*irq_rx_disable)(const struct ims_device *dev);
};

/**
 * @brief UART write read.
 *
 * @param dev[in] Device pointer provided by platform
 * @param wbuf[in] Address of write data
 * @param rbuf[out] Address of read data
 * @param num_bytes[in] Number of bytes of this transfer
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio config timeout
 */
static inline int ims_uart_write_read(const struct ims_device *dev, uint8_t *wbuf,
                                     uint8_t *rbuf, uint32_t num_bytes,
                                     uint32_t timeout_ms) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->transfer == NULL) {
        return -EINVAL;
    }
    return api->transfer(dev, wbuf, rbuf, num_bytes, timeout_ms);
}

/**
 * @brief Read data from UART device.
 *
 * This function reads the specified number of bytes from the UART device and
 * stores them in the provided buffer. The function blocks until all the bytes
 * are read or the specified timeout is reached.
 *
 * @param dev The UART device to read from.
 * @param rbuf Pointer to the buffer to store the read data.
 * @param num_bytes The number of bytes to read.
 * @param timeout_ms The timeout value in milliseconds.
 * @return Returns the number of bytes read on success, or a negative error code
 * on failure.
 */
static inline int ims_uart_read(const struct ims_device *dev, uint8_t *rbuf,
                               size_t num_bytes, uint32_t timeout_ms) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;

    return api->receive(dev, rbuf, num_bytes, timeout_ms);
}

/**
 * @brief Writes data to the UART device.
 *
 * This function writes the specified number of bytes from the given buffer to
 * the UART device. The function blocks until all the bytes are written or the
 * specified timeout is reached.
 *
 * @param dev The UART device to write to.
 * @param wbuf Pointer to the buffer containing the data to be written.
 * @param num_bytes The number of bytes to write.
 * @param timeout_ms The timeout value in milliseconds.
 * @return Returns the number of bytes written on success, or a negative error
 * code on failure.
 */
static inline int ims_uart_write(const struct ims_device *dev,
                                const uint8_t *wbuf, size_t num_bytes,
                                uint32_t timeout_ms) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;

    return api->send(dev, wbuf, num_bytes, timeout_ms);
}

static void ims_uart_isr_register(const struct ims_device *dev,
                                 ims_uart_rx_isr_t rx_isr,
                                 ims_uart_tx_isr_t tx_isr, void *arg) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->irq_callback_set == NULL) {
        return;
    }

    return api->irq_callback_set(dev, rx_isr, tx_isr, arg);
}

static void ims_uart_irq_tx_enable(const struct ims_device *dev) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->irq_tx_enable == NULL) {
        return;
    }

    return api->irq_tx_enable(dev);
}

static void ims_uart_irq_tx_disable(const struct ims_device *dev) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->irq_tx_disable == NULL) {
        return;
    }

    return api->irq_tx_disable(dev);
}

static void ims_uart_irq_rx_enable(const struct ims_device *dev) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->irq_rx_enable == NULL) {
        return;
    }

    return api->irq_rx_enable(dev);
}

static void ims_uart_irq_rx_disable(const struct ims_device *dev) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_uart_driver_api *api = dev->api;
    if (api->irq_rx_disable == NULL) {
        return;
    }

    return api->irq_rx_disable(dev);
}

#ifdef __cplusplus
}
#endif

#endif // IMS_MCU_DRIVER_UART_H_
