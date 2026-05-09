/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// copy from zephyr and modify
// https://github.com/zephyrproject-rtos/zephyr/blob/master/include/drivers/i2c.h

#ifndef IMS_MCU_DRIVER_I2C_H_
#define IMS_MCU_DRIVER_I2C_H_

#include <stdint.h>

#include "ims-mcu-driver/assert.h"
#include "ims-mcu-driver/device.h"
#include "ims-mcu-driver/util.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IMS_I2C_MSG_* are I2C Message flags.
 */

/** Write message to I2C bus. */
#define IMS_I2C_MSG_WRITE (0U << 0U)

/** Read message from I2C bus. */
#define IMS_I2C_MSG_READ BIT(0)

/** @cond INTERNAL_HIDDEN */
#define IMS_I2C_MSG_RW_MASK BIT(0)
/** @endcond  */

/** Send STOP after this message. */
#define IMS_I2C_MSG_STOP BIT(1)

/** RESTART I2C transaction for this message.
 *
 * @note Not all I2C drivers have or require explicit support for this
 * feature. Some drivers require this be present on a read message
 * that follows a write, or vice-versa.  Some drivers will merge
 * adjacent fragments into a single transaction using this flag; some
 * will not. */
#define IMS_I2C_MSG_RESTART BIT(2)

/** Use 10-bit addressing for this message.
 *
 * @note Not all SoC I2C implementations support this feature. */
#define IMS_I2C_MSG_ADDR_10_BITS BIT(3)

/**
 * @brief One I2C Message.
 *
 * This defines one I2C message to transact on the I2C bus.
 *
 * @note Some of the configurations supported by this API may not be
 * supported by specific SoC I2C hardware implementations, in
 * particular features related to bus transactions intended to read or
 * write data from different buffers within a single transaction.
 * Invocations of i2c_transfer() may not indicate an error when an
 * unsupported configuration is encountered.  In some cases drivers
 * will generate separate transactions for each message fragment, with
 * or without presence of @ref IMS_I2C_MSG_RESTART in #flags.
 */
struct ims_i2c_msg {
    /** Data buffer in bytes */
    uint8_t *buf;

    /** Length of buffer in bytes */
    uint32_t len;

    /** Flags for this message */
    uint8_t flags;
};

struct ims_i2c_driver_api {
    /**
     * @brief Generic i2c transfer api. Perform data transfer to I2C device in
     * 		  master mode.
     *
     * @param dev[in] Device pointer provided by platform
     * @param addr[in] I2C address
     * @param msgs[in/out] Array of ims_i2c_msg
     * @param num_msgs[in] Number of array of ims_i2c_msg
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     * @retval -ETIMEOUT if i2c transfer timeout
     */
    int (*transfer)(const struct ims_device *dev, uint16_t addr,
                    struct ims_i2c_msg *msgs, uint8_t num_msgs,
                    uint32_t timeout_ms);
};

/**
 * @brief Generic i2c transfer api. Perform data transfer to I2C device in
 * 		  master mode.
 *
 * @param dev[in] Device pointer provided by platform
 * @param addr[in] I2C address
 * @param msgs[in/out] Array of ims_i2c_msg
 * @param num_msgs[in] Number of array of ims_i2c_msg
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if i2c transfer timeout
 */
static inline int ims_i2c_transfer(const struct ims_device *dev, uint16_t addr,
                                  struct ims_i2c_msg *msgs, uint32_t num_msgs,
                                  uint32_t timeout_ms) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    const struct ims_i2c_driver_api *api = dev->api;

    return api->transfer(dev, addr, msgs, num_msgs, timeout_ms);
}

/**
 * @brief Read data from i2c device.
 *        This function should generate below i2c signal
 *
 *  M: [S][addr/r]              [ACK]         [ACK] ...           [NACK][P]
 *  S:            [ACK] [buf0-7]     [buf8-15]      ... [bufx-x+7]
 *
 * @param dev[in] Device pointer provided by platform
 * @param addr[in] I2C address
 * @param buf[out] Buf that store the retrieved data
 * @param num_bytes[in] Number of bytes to read
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if i2c transfer timeout
 */
static inline int ims_i2c_read(const struct ims_device *dev, uint16_t addr,
                              uint8_t *buf, uint32_t num_bytes,
                              uint32_t timeout_ms) {
    struct ims_i2c_msg msg;

    msg.buf = (uint8_t *)buf;
    msg.len = num_bytes;
    msg.flags = IMS_I2C_MSG_READ | IMS_I2C_MSG_STOP;

    return ims_i2c_transfer(dev, addr, &msg, 1, timeout_ms);
}

/**
 * @brief Write data from i2c device
 *        This function should generate below i2c signal
 *
 *  M: [S][addr/w]      [buf0-7]     [buf8-15]      ... [bufx-x+7][P]
 *  S:            [ACK]         [ACK]         [ACK] ...
 *
 * @param dev[in] Device pointer provided by platform
 * @param addr[in] I2C address
 * @param buf[in] Buf that contain data to be written
 * @param num_bytes[in] Number of bytes to write
 *
 * @return
 */
static inline int ims_i2c_write(const struct ims_device *dev, uint16_t addr,
                               const uint8_t *buf, uint32_t num_bytes,
                               uint32_t timeout_ms) {
    struct ims_i2c_msg msg;

    msg.buf = (uint8_t *)buf;
    msg.len = num_bytes;
    msg.flags = IMS_I2C_MSG_WRITE | IMS_I2C_MSG_STOP;

    return ims_i2c_transfer(dev, addr, &msg, 1, timeout_ms);
}

/**
 * @brief Write then read data from i2c device
 *        This function should generate below i2c signal
 *        Please check the received data's sequences from peripheral device's
 * datasheet and should be reconstructed if need
 *
 *  M: [S][addr/w]      [wbuf0-7]     [wbuf8-15]      ... [bufx-x+7]
 *  S:            [ACK]          [ACK]          [ACK] ...           [ACK]
 *  M: [S][addr/r]               [ACK]          [ACK] ...           [NACK][S]
 *  S:            [ACK] [rbuf0-7]     [rbuf8-15]      ... [bufx-x+7]
 *
 * @param dev[in] Device pointer provided by platform
 * @param addr[in] I2C address
 * @param buf[in] Buf that contain data to be written
 * @param num_bytes[in] Number of bytes to write
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if i2c transfer timeout
 */
static inline int ims_i2c_write_read(const struct ims_device *dev, uint16_t addr,
                                    uint8_t *wbuf, uint32_t wnum_bytes,
                                    uint8_t *rbuf, uint32_t rnum_bytes,
                                    uint32_t timeout_ms) {
    struct ims_i2c_msg msgs[2];

    msgs[0].buf = (uint8_t *)wbuf;
    msgs[0].len = wnum_bytes;
    msgs[0].flags = IMS_I2C_MSG_WRITE;

    msgs[1].buf = (uint8_t *)rbuf;
    msgs[1].len = rnum_bytes;
    msgs[1].flags = IMS_I2C_MSG_RESTART | IMS_I2C_MSG_READ | IMS_I2C_MSG_STOP;

    return ims_i2c_transfer(dev, addr, msgs, 2, timeout_ms);
}

#ifdef __cplusplus
}
#endif

#endif // IMS_MCU_DRIVER_I2C_H_
