add_library(fatfs STATIC hw_config.c)
target_link_libraries(fatfs PUBLIC no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
