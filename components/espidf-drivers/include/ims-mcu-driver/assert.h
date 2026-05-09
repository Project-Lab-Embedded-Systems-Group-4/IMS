#ifndef IMS_MCU_DRIVER_ASSERT_H_
#define IMS_MCU_DRIVER_ASSERT_H_

#include <stdbool.h>

#include "os_port.h"
#include "ims-mcu-driver/util.h"

#if IMS_MCU_DRIVER_ASSERT_ON

#if IMS_MCU_DRIVER_ASSERT_ON_STOP
#include <assert.h>
#define IMS_MCU_DRIVER_ASSERT_PRINT(fmt, ...)                                   \
    do {                                                                       \
        ims_loge(fmt, ##__VA_ARGS__);                                           \
        assert(false);                                                         \
    } while (0)
#else /* IMS_MCU_DRIVER_ASSERT_ON_STOP */
#define IMS_MCU_DRIVER_ASSERT_PRINT(fmt, ...) ims_loge(fmt, ##__VA_ARGS__)
#endif /* IMS_MCU_DRIVER_ASSERT_ON_STOP */

#else /* IMS_MCU_DRIVER_ASSERT_ON */
#define IMS_MCU_DRIVER_ASSERT_PRINT(fmt, ...)
#endif /* IMS_MCU_DRIVER_ASSERT_ON */

#define IMS_MCU_DRIVER_ASSERT_MSG_INFO(fmt, ...)                                \
    IMS_MCU_DRIVER_ASSERT_PRINT("\t" fmt "\n", ##__VA_ARGS__)

#define IMS_MCU_DRIVER_ASSERT_LOC(test)                                         \
    IMS_MCU_DRIVER_ASSERT_PRINT("ASSERTION FAIL [%s] @ %s:%d\n",                \
                               IMS_MCU_DRIVER_STR(test), __FILE__, __LINE__)

#define IMS_MCU_DRIVER_ASSERT_MSG(test, fmt, ...)                               \
    do {                                                                       \
        if (!(test)) {                                                         \
            IMS_MCU_DRIVER_ASSERT_LOC(test);                                    \
            IMS_MCU_DRIVER_ASSERT_MSG_INFO(fmt, ##__VA_ARGS__);                 \
        }                                                                      \
    } while (false)

#define IMS_MCU_DRIVER_ASSERT(test)                                             \
    do {                                                                       \
        if (!(test)) {                                                         \
            IMS_MCU_DRIVER_ASSERT_LOC(test);                                    \
        }                                                                      \
    } while (false)

#endif /* IMS_MCU_DRIVER_ASSERT_H_ */
