set(srcs "src/gpio.c" "src/i2c.c" "src/spi.c" "src/uart.c" "src/ad5933.c")

set(required_components driver soc esp_eth esp_rom hal esp_common)

list(APPEND required_components 
    esp_driver_gpio 
    esp_driver_i2c 
    esp_driver_spi 
    esp_driver_uart
    esp_timer
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    REQUIRES ${required_components}
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC IMS_MCU_DRIVER_ASSERT_ON=1)
