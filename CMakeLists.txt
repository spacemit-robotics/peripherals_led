cmake_minimum_required(VERSION 3.10)
project(led LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
)

set(LED_CORE_SOURCES
    src/led_core.c
)

set(SROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS "" CACHE STRING "Enabled led drivers (semicolon-separated). Empty means default.")
if("${SROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS}" STREQUAL "")
    set(_led_enabled_drivers "drv_generic")
else()
    set(_led_enabled_drivers "${SROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS}")
endif()

set(LED_DRIVER_SOURCES "")
foreach(_drv IN LISTS _led_enabled_drivers)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${_drv}.c")
        list(APPEND LED_DRIVER_SOURCES "src/drivers/${_drv}.c")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/${_drv}/${_drv}.c")
        list(APPEND LED_DRIVER_SOURCES "src/drivers/${_drv}/${_drv}.c")
    else()
        message(FATAL_ERROR
            "led: enabled driver '${_drv}' not found. Tried:\n"
            "  - src/drivers/${_drv}.c\n"
            "  - src/drivers/${_drv}/${_drv}.c"
        )
    endif()
endforeach()

set(LED_SOURCES
    ${LED_CORE_SOURCES}
    ${LED_DRIVER_SOURCES}
)

add_library(led SHARED ${LED_SOURCES})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(led PRIVATE
        -Wall
        -Wextra
    )
endif()

install(TARGETS led
    EXPORT ${SROBOTIS_EXPORT_SET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES include/led.h
    DESTINATION include
)

option(BUILD_TESTS "Build test programs" OFF)

if(BUILD_TESTS)
    add_executable(test_led_generic test/test_led_generic.c)
    target_link_libraries(test_led_generic led)
    set_target_properties(test_led_generic PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_led_generic RUNTIME DESTINATION bin)

    add_executable(test_led_ws2812 test/test_led_ws2812.c)
    target_link_libraries(test_led_ws2812 led)
    set_target_properties(test_led_ws2812 PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_led_ws2812 RUNTIME DESTINATION bin)
endif()
