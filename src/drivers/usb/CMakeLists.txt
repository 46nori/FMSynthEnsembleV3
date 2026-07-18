add_library(usb STATIC
    usb_descriptors.cpp
)

target_include_directories(usb PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(USB_MIDI_IRQ_DRIVEN)
    if(PICO_PLATFORM STREQUAL "rp2040")
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include")
    elseif(PICO_PLATFORM STREQUAL "rp2350-riscv")
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V")
    else()
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure")
    endif()

    target_include_directories(usb PRIVATE
            ${FREERTOS_KERNEL_PATH}/include
            ${FREERTOS_TINYUSB_PORT_DIR}
            ${CMAKE_SOURCE_DIR}/src/platform
    )

    target_compile_definitions(usb PRIVATE CFG_TUSB_OS_INC_PATH=)
endif()

target_link_libraries(usb PUBLIC tinyusb_device tinyusb_board pico_stdlib)
