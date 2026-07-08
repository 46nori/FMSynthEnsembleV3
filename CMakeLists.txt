# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# ====================================================================================
# Project Build Configuration
#   These options control which features are compiled in.
#   Keep in sync with CMakePresets.json cacheVariables.
# ====================================================================================
option(USB_MIDI_IRQ_DRIVEN  "Enable TinyUSB event-driven wait mode for USB MIDI" ON)
option(BUILD_MIDI_PANEL     "Enable MIDI panel controller"                       ON)
option(BUILD_SD_CARD        "Enable SD card stack"                               OFF)
# ====================================================================================

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(FMSynthEnsembleV3 C CXX ASM)

# clangd does not execute the cross compiler unless each developer configures
# --query-driver. CMake knows the cross compiler's implicit C++ include paths,
# but normally omits them from compile_commands.json, so emit them explicitly.
foreach(_cxx_implicit_include IN LISTS CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-isystem${_cxx_implicit_include}>")
endforeach()

# USB_MIDI_IRQ_DRIVEN: TinyUSB OSAL mode must be fixed before pico_sdk_init()
if(USB_MIDI_IRQ_DRIVEN)
    set(TINYUSB_OPT_OS OPT_OS_FREERTOS CACHE STRING "TinyUSB OSAL mode" FORCE)
else()
    set(TINYUSB_OPT_OS OPT_OS_PICO CACHE STRING "TinyUSB OSAL mode" FORCE)
endif()

message(STATUS "USB_MIDI_IRQ_DRIVEN: ${USB_MIDI_IRQ_DRIVEN}")
message(STATUS "BUILD_MIDI_PANEL:    ${BUILD_MIDI_PANEL}")
message(STATUS "BUILD_SD_CARD:       ${BUILD_SD_CARD}")
message(STATUS "TINYUSB_OPT_OS:      ${TINYUSB_OPT_OS}")

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# FreeRTOS (official Raspberry Pi layout only).
# Priority:
#   1) User-specified FREERTOS_KERNEL_PATH (cache/env)
#   2) ${PICO_SDK_PATH}/../FreeRTOS-Kernel
#   3) ${PICO_SDK_PATH}/../../FreeRTOS-Kernel
if(NOT FREERTOS_KERNEL_PATH)
    set(_freertos_candidate_a "${PICO_SDK_PATH}/../FreeRTOS-Kernel")
    set(_freertos_candidate_b "${PICO_SDK_PATH}/../../FreeRTOS-Kernel")
    if(EXISTS "${_freertos_candidate_a}")
        set(FREERTOS_KERNEL_PATH "${_freertos_candidate_a}" CACHE PATH "Path to the FreeRTOS Kernel")
    elseif(EXISTS "${_freertos_candidate_b}")
        set(FREERTOS_KERNEL_PATH "${_freertos_candidate_b}" CACHE PATH "Path to the FreeRTOS Kernel")
    endif()
endif()
if(NOT FREERTOS_KERNEL_PATH OR NOT EXISTS "${FREERTOS_KERNEL_PATH}")
    message(FATAL_ERROR
        "Official FreeRTOS-Kernel not found. Set FREERTOS_KERNEL_PATH or place it at "
        "'${PICO_SDK_PATH}/../FreeRTOS-Kernel' (or '../../FreeRTOS-Kernel').")
endif()
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src/platform CACHE PATH "FreeRTOS config file directory")
include(FreeRTOS_Kernel_import.cmake)

# External libraries (git submodules)
add_subdirectory(extern)

if(USB_MIDI_IRQ_DRIVEN)
    add_compile_definitions(USB_MIDI_IRQ_DRIVEN=1)
else()
    add_compile_definitions(USB_MIDI_IRQ_DRIVEN=0)
endif()

# Device drivers
add_subdirectory(src/drivers)

# Platform (RP2350/Pico specific)
add_subdirectory(src/platform)

# Synthesizer core
add_subdirectory(src/synth)

# MIDI parsing / routing
add_subdirectory(src/midi)

# Add executable. Default name is the project name, version 0.1

add_executable(FMSynthEnsembleV3
    src/app/main.cpp
    src/app/sample_task.cpp
    src/app/debugger.cpp
    src/app/debugger_task.cpp
    src/app/midi_ipc.cpp
    src/app/csm_ipc.cpp
    src/app/csm_frame_task.cpp
    src/app/midi_engine_task.cpp
    src/app/usb_midi_task.cpp
    src/app/midi_panel_task.cpp
)

pico_set_program_name(FMSynthEnsembleV3 "FMSynthEnsembleV3")
pico_set_program_version(FMSynthEnsembleV3 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(FMSynthEnsembleV3 1)
pico_enable_stdio_usb(FMSynthEnsembleV3 0)

# tusb_config.h を TinyUSB スタックに認識させるため include パスを追加
target_include_directories(FMSynthEnsembleV3 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/drivers/usb)

# Add the standard library to the build
target_link_libraries(FMSynthEnsembleV3
        pico_stdlib
        nju72343
        fm
        midi
        platform
        usb
        synth
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4)
if(BUILD_SD_CARD)
    target_link_libraries(FMSynthEnsembleV3 fatfs)
endif()

# Add the standard include files to the build
target_include_directories(FMSynthEnsembleV3 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/app
)

if(USB_MIDI_IRQ_DRIVEN)
    if(PICO_PLATFORM STREQUAL "rp2040")
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040")
    elseif(PICO_PLATFORM STREQUAL "rp2350-riscv")
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_RISC-V")
    else()
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2350_ARM_NTZ/non_secure")
    endif()

    target_include_directories(FMSynthEnsembleV3 PRIVATE
            ${FREERTOS_KERNEL_PATH}/include
            ${FREERTOS_TINYUSB_PORT_DIR}
    )

    # Include as "FreeRTOS.h" (no prefix path).
    target_compile_definitions(FMSynthEnsembleV3 PRIVATE CFG_TUSB_OS_INC_PATH=)
endif()

pico_add_extra_outputs(FMSynthEnsembleV3)

