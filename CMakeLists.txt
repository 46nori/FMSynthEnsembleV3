# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.18)

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
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# ====================================================================================
# Project Build Configuration
#   ファームウェアの構造（外部ライブラリ・SDK の構成・特定ハードウェアに依存するコンポーネントの
#   有無）を決めるオプション。ここが唯一の定義元で、値は build_config.h としてソースに渡る。
#   アプリケーションの調整値は src/config.h に置く。
# ====================================================================================
include(cmake/build_config.cmake)
fmse_build_option(USB_MIDI_IRQ_DRIVEN "Enable TinyUSB event-driven wait mode for USB MIDI" ON)
fmse_build_option(BUILD_MIDI_PANEL    "Enable MIDI panel controller"                       ON)
fmse_build_option(BUILD_SD_CARD       "Enable SD card stack"                               ON)
fmse_build_option(BUILD_I2C_DISPLAY   "Enable I2C display support"                         ON)
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

message(STATUS "TINYUSB_OPT_OS: ${TINYUSB_OPT_OS}")

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# FreeRTOS (git submodule; the RP2350 ports live in its nested submodule).
# Priority:
#   1) User-specified FREERTOS_KERNEL_PATH (cache/env)
#   2) extern/FreeRTOS-Kernel
if(NOT FREERTOS_KERNEL_PATH AND DEFINED ENV{FREERTOS_KERNEL_PATH})
    set(FREERTOS_KERNEL_PATH "$ENV{FREERTOS_KERNEL_PATH}")
endif()
if(NOT FREERTOS_KERNEL_PATH)
    set(FREERTOS_KERNEL_PATH "${CMAKE_CURRENT_LIST_DIR}/extern/FreeRTOS-Kernel")
endif()
# Submodules must match extern/submodules.lock
include(extern/check_submodules.cmake)
if(NOT EXISTS "${FREERTOS_KERNEL_PATH}/include/FreeRTOS.h")
    message(FATAL_ERROR "FreeRTOS-Kernel not found at '${FREERTOS_KERNEL_PATH}'.")
endif()
if(NOT PICO_PLATFORM STREQUAL "rp2040"
        AND NOT EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC")
    message(FATAL_ERROR
        "The RP2350 ports are missing from '${FREERTOS_KERNEL_PATH}'. They live in a nested "
        "submodule of FreeRTOS-Kernel: run 'git submodule update --init --recursive'.")
endif()
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src/platform CACHE PATH "FreeRTOS config file directory")
include(FreeRTOS_Kernel_import.cmake)

# External libraries (git submodules)
add_subdirectory(extern)

# 構造オプションを build_config.h に書き出し、src/config.h と合わせてソースから参照できるようにする。
# fmse_config を link したターゲット（とその利用者）だけが src/ と生成ディレクトリを include パスに持つ。
set(FMSE_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
fmse_generate_build_config(${FMSE_GENERATED_DIR})
add_library(fmse_config INTERFACE)
target_include_directories(fmse_config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${FMSE_GENERATED_DIR}
)

# Device drivers
add_subdirectory(src/smf)
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
if(BUILD_SD_CARD)
    target_sources(FMSynthEnsembleV3 PRIVATE src/app/smf_player_task.cpp)
endif()
if(BUILD_I2C_DISPLAY)
    target_sources(FMSynthEnsembleV3 PRIVATE
        src/app/info_screen_task.cpp
        src/app/ui/menu_screens.cpp
        src/app/ui/menu_context.cpp
        src/app/ui/screen_settings.cpp
        src/app/ui/screen_volume.cpp
        src/app/ui/screen_system_info.cpp
        src/app/ui/joystick_input_adapter.cpp
    )
    if(BUILD_SD_CARD)
        target_sources(FMSynthEnsembleV3 PRIVATE src/app/ui/screen_smf.cpp)
    endif()
endif()

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
        fmse_config
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
        set(FREERTOS_TINYUSB_PORT_DIR "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/include")
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

# 静的ライブラリごとに SDK 等のソースが重複コンパイルされるのを防ぐ（全ターゲット定義後に呼ぶ）
include(cmake/compile_only_interface_deps.cmake)
fmse_compile_only_interface_deps(FMSynthEnsembleV3)

