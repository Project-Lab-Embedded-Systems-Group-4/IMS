set(APP_SRCS 
    "hello_world_main.c" 
    "board.c"
    "cmd/cmd_system.c"
    "cmd/cmd_gpio_pin.c"
    "cmd/cmd_i2c_tool.c"
)
set(APP_INC_DIRS "include" "cmd")

# Option to enable GPIO test (can also be passed via: idf.py build -DGPIO_TEST=ON)
#set(GPIO_TEST ON) 
#set(I2C_TEST ON)
#set(SPI_TEST ON)
#set(UART_TEST ON)

include("${CMAKE_CURRENT_LIST_DIR}/test/test.cmake")


set(priv_requires
    esp_driver_gpio 
    esp_driver_i2c 
    esp_driver_spi 
    esp_driver_uart
    spi_flash 
    espidf-drivers 
    console 
    esp_partition 
    esp_timer
)
idf_component_register(
    SRCS "${APP_SRCS}"
    PRIV_REQUIRES "${priv_requires}"
    INCLUDE_DIRS "${APP_INC_DIRS}"
)

if(GPIO_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE GPIO_TEST=1)
endif()

if(I2C_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE I2C_TEST=1)
endif()

if(SPI_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SPI_TEST=1)
endif()

if(UART_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE UART_TEST=1)
endif()
