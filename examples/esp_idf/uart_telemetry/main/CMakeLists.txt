# SPDX-License-Identifier: MIT
# ESP-IDF 5.3 split the legacy driver component; esp_driver_uart holds the UART driver.
set(uart_component driver)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(uart_component esp_driver_uart)
endif()

idf_component_register(SRCS "main.cpp"
                       PRIV_REQUIRES ${uart_component} esp_timer)
