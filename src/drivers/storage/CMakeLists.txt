add_library(fatfs STATIC hw_config.c)

# Override extern/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico's ffconf.h without editing
# extern/ (AGENTS.md). This include directory must be added before the
# no-OS-FatFS-SD-SDIO-SPI-RPi-Pico link so it resolves first for every target
# that links fatfs.
target_include_directories(fatfs PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(fatfs PUBLIC no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
