if(GPIO_TEST)
  list(APPEND APP_SRCS "test/src/test_gpio.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()

if(I2C_TEST)
  list(APPEND APP_SRCS "test/src/test_i2c.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()

if(SPI_TEST)
  list(APPEND APP_SRCS "test/src/test_spi.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()

if(UART_TEST)
  list(APPEND APP_SRCS "test/src/test_uart.c")
  list(APPEND APP_INC_DIRS "test/include")
endif()
