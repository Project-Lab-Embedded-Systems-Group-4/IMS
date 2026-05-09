if(GPIO_TEST)
  list(APPEND APP_SRCS "test/src/test_gpio.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()

if(I2C_TEST)
  list(APPEND APP_SRCS "test/src/test_i2c.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()
