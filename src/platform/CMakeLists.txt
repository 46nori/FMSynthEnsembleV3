add_library(platform STATIC
    init.cpp
    isr.cpp
    volume_controller.cpp
    freertos_hooks.cpp
)

target_include_directories(platform PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(platform PRIVATE ${CMAKE_SOURCE_DIR}/src/drivers/usb)
target_include_directories(platform PRIVATE ${CMAKE_SOURCE_DIR}/src/drivers/fm/opn_piolib/include)

target_link_libraries(platform PUBLIC
    pico_stdlib
    hardware_gpio
    hardware_irq
    hardware_sync
    fm
    FreeRTOS-Kernel
)

# Platform dependencies
target_link_libraries(platform PUBLIC
    tinyusb_device
    nju72343
)
if(BUILD_SD_CARD)
    target_compile_definitions(platform PUBLIC BUILD_SD_CARD=1)
    target_link_libraries(platform PUBLIC fatfs)
else()
    target_compile_definitions(platform PUBLIC BUILD_SD_CARD=0)
endif()
