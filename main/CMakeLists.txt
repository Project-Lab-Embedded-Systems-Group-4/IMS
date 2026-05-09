set(APP_SRCS "hello_world_main.c")
set(APP_INC_DIRS "")

#set(GPIO_TEST ON) 
#set(I2C_TEST ON)
#set(SPI_TEST ON)
#set(UART_TEST ON)

include("${CMAKE_CURRENT_LIST_DIR}/test/test.cmake")

idf_component_register(SRCS "${APP_SRCS}"
                       PRIV_REQUIRES spi_flash espidf-drivers
                       INCLUDE_DIRS "${APP_INC_DIRS}")

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
