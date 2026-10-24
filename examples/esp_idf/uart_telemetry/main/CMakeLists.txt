# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
# ESP-IDF 5.3 split the legacy driver component; esp_driver_uart holds the UART driver.
set(uart_component driver)
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_GREATER_EQUAL "5.3")
    set(uart_component esp_driver_uart)
endif()

idf_component_register(SRCS "main.cpp"
                       PRIV_REQUIRES ${uart_component} esp_timer)
