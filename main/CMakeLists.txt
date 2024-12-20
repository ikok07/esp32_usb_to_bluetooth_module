file(GLOB_RECURSE SRC_FILES *.c)

idf_component_register(SRCS main.c ${SRC_FILES}
        INCLUDE_DIRS "."
        REQUIRES bt nvs_flash usb esp_driver_gpio)