cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(StateDetector)

pico_sdk_init()

add_executable(StateDetector)

target_sources(StateDetector PRIVATE main.cc)

pico_generate_pio_header(StateDetector ${CMAKE_CURRENT_LIST_DIR}/as_detector.pio)

target_link_libraries(StateDetector pico_stdlib hardware_pio)

pico_add_extra_outputs(StateDetector)
