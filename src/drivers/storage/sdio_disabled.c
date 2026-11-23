// SDIO interface of no-OS-FatFS-SD-SDIO-SPI-RPi-Pico is not built.
// The SD card is connected via SPI only (see hw_config.c), and the SDIO driver
// sources are removed from the build in extern/CMakeLists.txt. The common part
// (sd_card.c) still references the two functions below, so they are defined here.
// Neither is reached with SD_IF_SPI cards.

#include "pico/stdlib.h"
#include "sd_card.h"
#include "SDIO/SdioCard.h"

void sd_sdio_ctor(sd_card_t *sd_card_p) {
    (void)sd_card_p;
    panic("SDIO interface is not supported");
}

bool rp2040_sdio_get_sd_status(sd_card_t *sd_card_p, uint8_t response[64]) {
    (void)sd_card_p;
    (void)response;
    return false;
}
