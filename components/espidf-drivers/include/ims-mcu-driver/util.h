#ifndef IMS_MCU_DRIVER_UTIL_H_
#define IMS_MCU_DRIVER_UTIL_H_

#include "os_port.h"

#define IMS_MCU_DRIVER_STR_HELPER(x) #x
#define IMS_MCU_DRIVER_STR(x) IMS_MCU_DRIVER_STR_HELPER(x)

#ifndef BIT
#define BIT(n) (1 << (n))
#endif

#ifndef BIT_SET
#define BIT_SET(v, n) ((v) |= BIT(n))
#endif

#ifndef BIT_CLEAR
#define BIT_CLEAR(v, n) ((v) &= ~(BIT(n)))
#endif

#ifndef BIT_FLIP
#define BIT_FLIP(v, n) ((v) ^= BIT(n))
#endif

#ifndef BIT_CHECK
#define BIT_CHECK(v, n) ((v)&BIT(n))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif

#define ERROR_CHECK(fn)                                                        \
    do {                                                                       \
        int err;                                                               \
        if ((err = (fn)) != 0) {                                               \
            ims_loge("%s: err=%d\n", __FUNCTION__, err);                        \
            return err;                                                        \
        }                                                                      \
    } while (0)

#endif // IMS_MCU_DRIVER_UTIL_H_
