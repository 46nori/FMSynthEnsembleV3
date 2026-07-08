// Hardware configuration for no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
// SD card via SPI0: SCK=GPIO18, MOSI=GPIO19, MISO=GPIO16, CS=GPIO17

#include "hw_config.h"

// SPI0 controller configuration
static spi_t spis[] = {
    {
        .hw_inst   = spi0,
        .miso_gpio = 16,        // SD_MISO
        .mosi_gpio = 19,        // SD_MOSI
        .sck_gpio  = 18,        // SD_CLK
        .baud_rate = 12500000,  // 12.5 MHz
    }
};

// SPI interface for the SD card
static sd_spi_if_t spi_ifs[] = {
    {
        .spi     = &spis[0],
        .ss_gpio = 17,          // /SDCS
    }
};

// SD card configuration
static sd_card_t sd_cards[] = {
    {
        .type     = SD_IF_SPI,
        .spi_if_p = &spi_ifs[0],
    }
};

// Required by the library
size_t sd_get_num() {
    return count_of(sd_cards);
}

sd_card_t *sd_get_by_num(size_t num) {
    if (num < sd_get_num()) {
        return &sd_cards[num];
    }
    return NULL;
}
