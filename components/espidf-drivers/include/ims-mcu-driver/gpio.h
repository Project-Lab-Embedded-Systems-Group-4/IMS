/*
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 * Copyright (c) 2019 Piotr Mienkowski
 * Copyright (c) 2017 ARM Ltd
 * Copyright (c) 2015-2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// inspired from zephyr
// https://github.com/zephyrproject-rtos/zephyr/blob/master/include/drivers/gpio.h

#ifndef IMS_MCU_DRIVER_GPIO_H_
#define IMS_MCU_DRIVER_GPIO_H_

#include <stdint.h>

#include "ims-mcu-driver/assert.h"
#include "ims-mcu-driver/device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMS_GPIO_FLAGS_DISCONNECTED 0

/* Configures GPIO output in single-ended mode (open drain or open source). */
#define IMS_GPIO_FLAGS_SINGLE_ENDED (1 << 1)
/* Configures GPIO output in push-pull mode */
#define IMS_GPIO_FLAGS_PUSH_PULL (0 << 1)

/* Indicates single ended open drain mode (wired AND). */
#define IMS_GPIO_FLAGS_LINE_OPEN_DRAIN (1 << 2)
/* Indicates single ended open source mode (wired OR). */
#define IMS_GPIO_FLAGS_LINE_OPEN_SOURCE (0 << 2)

/** Enables GPIO pin pull-up. */
#define IMS_GPIO_FLAGS_INPUT_PULL_UP (1U << 4)

/** Enable GPIO pin pull-down. */
#define IMS_GPIO_FLAGS_INPUT_PULL_DOWN (1U << 5)

/** Enables pin as input. */
#define IMS_GPIO_FLAGS_INPUT (1U << 8)

/** Enables pin as output, no change to the output state. */
#define IMS_GPIO_FLAGS_OUTPUT (1U << 9)

/* Initializes output to a low state. */
#define IMS_GPIO_FLAGS_OUTPUT_INIT_LOW (1U << 10)

/* Initializes output to a high state. */
#define IMS_GPIO_FLAGS_OUTPUT_INIT_HIGH (1U << 11)

/**
 * @name GPIO interrupt configuration flags
 * The `GPIO_INT_*` flags are used to specify how input GPIO pins will trigger
 * interrupts. The interrupts can be sensitive to pin physical or logical level.
 * Interrupts sensitive to pin logical level take into account GPIO_ACTIVE_LOW
 * flag. If a pin was configured as Active Low, physical level low will be
 * considered as logical level 1 (an active state), physical level high will
 * be considered as logical level 0 (an inactive state).
 * @{
 */

/** Disables GPIO pin interrupt. */
#define IMS_GPIO_INT_DISABLE (1U << 13)

/** @cond INTERNAL_HIDDEN */

/* Enables GPIO pin interrupt. */
#define IMS_GPIO_INT_ENABLE (1U << 14)

/* GPIO interrupt is sensitive to logical levels.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define IMS_GPIO_INT_LEVELS_LOGICAL (1U << 15)

/* GPIO interrupt is edge sensitive.
 *
 * Note: by default interrupts are level sensitive.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define IMS_GPIO_INT_EDGE (1U << 16)

/* Trigger detection when input state is (or transitions to) physical low or
 * logical 0 level.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define IMS_GPIO_INT_LOW_0 (1U << 17)

/* Trigger detection on input state is (or transitions to) physical high or
 * logical 1 level.
 *
 * This is a component flag that should be combined with other
 * `GPIO_INT_*` flags to produce a meaningful configuration.
 */
#define IMS_GPIO_INT_HIGH_1 (1U << 18)

/** @endcond */

/** @} */

/* Used by driver api function pin_interrupt_configure, these are defined
 * in terms of the public flags so we can just mask and pass them
 * through to the driver api
 */
enum ims_gpio_int_mode {
    IMS_GPIO_INT_MODE_DISABLED = IMS_GPIO_INT_DISABLE,
    IMS_GPIO_INT_MODE_LEVEL = IMS_GPIO_INT_ENABLE,
    IMS_GPIO_INT_MODE_EDGE = IMS_GPIO_INT_ENABLE | IMS_GPIO_INT_EDGE,
};

enum ims_gpio_int_trig {
    /* Trigger detection when input state is (or transitions to)
     * physical low. (Edge Failing or Active Low) */
    IMS_GPIO_INT_TRIG_LOW = IMS_GPIO_INT_LOW_0,
    /* Trigger detection when input state is (or transitions to)
     * physical high. (Edge Rising or Active High) */
    IMS_GPIO_INT_TRIG_HIGH = IMS_GPIO_INT_HIGH_1,
    /* Trigger detection on pin rising or falling edge. */
    IMS_GPIO_INT_TRIG_BOTH = IMS_GPIO_INT_LOW_0 | IMS_GPIO_INT_HIGH_1,
};

/** GPIO level value */
#define IMS_GPIO_LEVEL_LOW 0
#define IMS_GPIO_LEVEL_HIGH 1

/**
 * @brief Provides a type to hold a GPIO pin index.
 *
 * This reduced-size type is sufficient to record a pin number,
 * e.g. from a devicetree GPIOS property.
 */
typedef uint8_t ims_gpio_pin_t;

/**
 * @brief Identifies a set of pins associated with a port.
 *
 * The pin with index n is present in the set if and only if the bit
 * identified by (1U << n) is set.
 */

typedef uint32_t ims_gpio_port_pin_t;
/**
 * @brief Provides a type to hold a GPIO port index.
 *
 * e.g. from a devicetree GPIOS property.
 */
typedef uint8_t ims_gpio_port_t;

/**
 * @brief Provides a type to hold a GPIO port value.
 *
 * e.g. from a devicetree GPIOS property.
 */
typedef uint32_t ims_gpio_port_value_t;

/**
 * @brief Provides a type to hold GPIO configuration flags.
 *
 * This type is sufficient to hold all flags used to control GPIO
 * configuration, whether pin or interrupt.
 */
typedef uint32_t ims_gpio_flags_t;

/**
 * @brief Identifies a set of pins associated with a port.
 *
 * The pin with index n is present in the set if and only if the bit
 * identified by (1U << n) is set.
 */
typedef uint32_t ims_gpio_port_pins_t;

/**
 * @brief gpio isr callback interface
 */
typedef void (*ims_gpio_pin_isr_t)(void *arg);

/**
 * Generic GPIO port driver interface
 * All gpio device need implement ims_gpio_driver_api
 */
struct ims_gpio_port_driver_api {

    /**
     * @brief Configure GPIO (Required)
     *
     * @param dev[in] Device pointer provided by platform.
     * @param pin[in] GPIO pin number
     * @param flags[in] GPIO configuration. Can be multiple GPIO_FLAGS_*.
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     * @retval -EINVAL if pin number is invalid
     * @retval -ETIMEOUT if gpio config timeout
     */
    int (*pin_set_config)(const struct ims_device *dev, ims_gpio_pin_t pin,
                          ims_gpio_flags_t flags);

    /**
     * @brief Get Configure of GPIO (Required)
     *
     * @param dev[in] Device pointer provided by platform.
     * @param pin[in] GPIO pin number
     * @param flags[out] address of GPIO configuration.
     *
     * @retval 0 if successful
     * @retval -EIO if generic I/O error
     * @retval -EINVAL if pin number is invalid
     * @retval -ETIMEOUT if gpio config timeout
     */
    int (*pin_get_config)(const struct ims_device *dev, ims_gpio_pin_t pin,
                          ims_gpio_flags_t *flags);

    /**
     * @brief Get pin level (Required)
     *
     * @param dev[in] Device pointer provided by platform.
     * @param pin[in] GPIO pin number
     */
    int (*pin_get)(const struct ims_device *dev, ims_gpio_pin_t pin);

    /**
     * @brief Set pin level (Required)
     *
     * @param dev[in] Device pointer provided by platform.
     * @param pin[in] GPIO pin number
     */
    int (*pin_set)(const struct ims_device *dev, ims_gpio_pin_t pin, int level);

    /**
     * @brief Get port value
     *
     * @param dev[in] Device pointer provided by platform.
     * @param port[in] GPIO port number
     * @param mask[in] Mask indicating which pins will be ignore.
     *
     * @retval Value assigned to the pins status.
     * @retval Pin with index n is represented by bit n in mask and value.
     * @retval 0 represent the pin is a low physical level.
     * @retval 1 represent the pin is a high physical level.
     */
    ims_gpio_port_value_t (*port_get)(const struct ims_device *dev,
                                     ims_gpio_port_t port,
                                     ims_gpio_port_pin_t mask);

    /**
     * @brief Set port value
     *
     * @param dev[in] Device pointer provided by platform.
     * @param port[in] GPIO port number
     * @param mask[in]  Mask indicating which pins will be modified.
     * @param value[in] Value assigned to the output pins.
     *
     * @retval 0 if success
     * @retval -EIO if generic I/O error
     * @retval -ETIMEOUT if gpio busy
     */
    int (*port_set)(const struct ims_device *dev, ims_gpio_port_t port,
                    ims_gpio_port_pin_t mask, ims_gpio_port_value_t value);

    /**
     * @brief Configure GPIO interrupt (Required)
     *
     * @param dev[in] Device pointer provided by platform.
     * @param pin[in] GPIO pin number
     */
    int (*pin_interrupt_configure)(const struct ims_device *dev,
                                   ims_gpio_pin_t pin,
                                   enum ims_gpio_int_mode mode,
                                   enum ims_gpio_int_trig trig);

    /**
     * @brief Register gpio interrupt
     *
     * @param dev[in] Device pointer provided by platform
     * @param pin[in] GPIO pin number
     * @param isr[in] Pointer of function called when interrupt
     * @param arg[in] Argument pass into isr callback
     *
     * @retval 0 if success
     * @retval -EIO if generic I/O error
     * @retval -ETIMEOUT if gpio busy
     */
    int (*pin_isr_register)(const struct ims_device *dev, ims_gpio_pin_t pin,
                            ims_gpio_pin_isr_t isr, void *arg);
};

/**
 * @brief Get gpio configuration
 *
 * @param dev[in] Device pointer provided by platform.
 * @param pin[in] GPIO pin number
 * @param flags[out] GPIO configuration. Can be multiple GPIO_FLAGS_*.
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio config timeout
 */
static inline int ims_gpio_port_pin_get_config(const struct ims_device *dev,
                                              ims_gpio_pin_t pin,
                                              ims_gpio_flags_t *flags) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_get_config(dev, pin, flags);
}

/**
 * @brief Set gpio configuration
 *
 * @param dev[in] Device pointer provided by platform.
 * @param pin[in] GPIO pin number
 * @param flags[in] GPIO configuration. Can be multiple GPIO_FLAGS_*.
 *
 * @retval 0 if successful
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio config timeout
 */
static inline int ims_gpio_port_pin_set_config(const struct ims_device *dev,
                                              ims_gpio_pin_t pin,
                                              ims_gpio_flags_t flags) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_set_config(dev, pin, flags);
}

/**
 * @brief Get gpio level
 *
 * @param dev[in] Device pointer provided by platform
 * @param pin[in] GPIO pin number
 *
 * @retval GIIO_LEVEL_HIGH/GPIO_LEVEL_LOW if success
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio busy
 */
static inline int ims_gpio_port_pin_get(const struct ims_device *dev,
                                       ims_gpio_pin_t pin) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_get(dev, pin);
}

/**
 * @brief Set gpio level
 *
 * @param dev[in] Device pointer provided by platform
 * @param pin[in] GPIO pin number
 * @param level[in] GPIO level
 *
 * @retval 0 if success
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio busy
 */
static inline int ims_gpio_port_pin_set(const struct ims_device *dev,
                                       ims_gpio_pin_t pin, int level) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_set(dev, pin, level);
}

/**
 * @brief Get port value
 *
 * @param dev[in] Device pointer provided by platform.
 * @param port[in] GPIO port number
 * @param mask[in] Mask indicating which pins will be ignore.
 *
 * @retval Value assigned to the pins status.
 * @retval Pin with index n is represented by bit n in mask and value.
 * @retval 0 represent the pin is a low physical level.
 * @retval 1 represent the pin is a high physical level.
 */
static inline ims_gpio_port_value_t
ims_gpio_port_get_masked(const struct ims_device *dev, ims_gpio_port_t port,
                        ims_gpio_port_pin_t mask) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->port_get(dev, port, mask);
}

/**
 * @brief Set gpio port value
 *
 * @param dev[in] Device pointer provided by platform
 * @param port[in] GPIO port number
 * @param mask[in]  Mask indicating which pins will be modified.
 * @param value[in] Value assigned to the output pins.
 *
 * @retval 0 if success
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio busy
 */
static inline int ims_gpio_port_set_masked(const struct ims_device *dev,
                                          ims_gpio_port_t port,
                                          ims_gpio_port_pin_t mask,
                                          ims_gpio_port_value_t value) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->port_set(dev, port, mask, value);
}

/**
 * @brief Configure gpio interrupt
 *
 * @param dev[in] Device pointer provided by platform
 * @param pin[in] GPIO pin number
 * @param level[in] GPIO level
 *
 * @retval 0 if success
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio busy
 */
static inline int ims_gpio_port_pin_interrupt_configure(
    const struct ims_device *dev, ims_gpio_pin_t pin, enum ims_gpio_int_mode mode,
    enum ims_gpio_int_trig trig) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_interrupt_configure(dev, pin, mode, trig);
}

/**
 * @brief Register gpio interrupt
 *
 * @param dev[in] Device pointer provided by platform
 * @param pin[in] GPIO pin number
 * @param isr[in] Pointer of function called when interrupt
 * @param arg[in] Argument pass into isr callback
 *
 * @retval 0 if success
 * @retval -EIO if generic I/O error
 * @retval -ETIMEOUT if gpio busy
 */
static inline int ims_gpio_port_pin_isr_register(const struct ims_device *dev,
                                                ims_gpio_pin_t pin,
                                                ims_gpio_pin_isr_t isr,
                                                void *arg) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->api != NULL);
    const struct ims_gpio_port_driver_api *api = dev->api;

    return api->pin_isr_register(dev, pin, isr, arg);
}

// Generic gpio pin device, just a wrapper for gpio port

struct ims_gpio_pin_config {
    const struct ims_device *dev;
    ims_gpio_pin_t pin;
};

struct ims_gpio_port_config {
    const struct ims_device *dev;
    ims_gpio_port_t port;
};

static inline int ims_gpio_pin_init(struct ims_device *dev,
                                   struct ims_gpio_pin_config *config) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(config != NULL);
    dev->config = config;
    return 0;
}

static inline int ims_gpio_port_init(struct ims_device *dev,
                                    struct ims_gpio_port_config *config) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(config != NULL);
    dev->config = config;
    return 0;
}

static inline int ims_gpio_pin_set(const struct ims_device *dev, int level) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_set(cfg->dev, cfg->pin, level);
}

static inline int ims_gpio_pin_get(const struct ims_device *dev) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_get(cfg->dev, cfg->pin);
}

static inline int ims_gpio_port_set(const struct ims_device *dev,
                                   ims_gpio_port_pin_t mask,
                                   ims_gpio_port_value_t value) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_port_config *cfg = dev->config;
    return ims_gpio_port_set_masked(cfg->dev, cfg->port, mask, value);
}

static inline ims_gpio_port_value_t ims_gpio_port_get(const struct ims_device *dev,
                                                    ims_gpio_port_pin_t mask) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_port_config *cfg = dev->config;
    return ims_gpio_port_get_masked(cfg->dev, cfg->port, mask);
}

static inline int ims_gpio_pin_get_config(const struct ims_device *dev,
                                         ims_gpio_flags_t *flags) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_get_config(cfg->dev, cfg->pin, flags);
}

static inline int ims_gpio_pin_set_config(const struct ims_device *dev,
                                         ims_gpio_flags_t flags) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_set_config(cfg->dev, cfg->pin, flags);
}

static inline int ims_gpio_pin_interrupt_configure(const struct ims_device *dev,
                                                  enum ims_gpio_int_mode mode,
                                                  enum ims_gpio_int_trig trig) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_interrupt_configure(cfg->dev, cfg->pin, mode, trig);
}

static inline int ims_gpio_pin_isr_register(const struct ims_device *dev,
                                           ims_gpio_pin_isr_t isr, void *arg) {
    IMS_MCU_DRIVER_ASSERT(dev != NULL);
    IMS_MCU_DRIVER_ASSERT(dev->config != NULL);
    const struct ims_gpio_pin_config *cfg = dev->config;
    return ims_gpio_port_pin_isr_register(cfg->dev, cfg->pin, isr, arg);
}

#ifdef __cplusplus
}
#endif

#endif // IMS_MCU_DRIVER_GPIO_H_
