//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

#include "hardware/pio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Device type
 * --------------------------------------------------------------------------*/
typedef enum {
    FM_DEVICE_YM2203,
    FM_DEVICE_YM2608,
} fm_device_type_t;

typedef uint32_t fm_master_clock_hz_t;
typedef uint8_t  fm_chip_id_t;

/* ---------------------------------------------------------------------------
 * Pre-computed wait count table (built once at fm_device_init())
 * Entries correspond to the 5 distinct W1/W2 φM cycle values in the spec.
 * --------------------------------------------------------------------------*/
typedef struct {
    uint32_t w0;    /* N=0   – SSG / ADPCM */
    uint32_t w17;   /* N=17  – FM/Rhythm addr */
    uint32_t w47;   /* N=47  – FM F-Num data */
    uint32_t w83;   /* N=83  – FM/Rhythm data */
    uint32_t w576;  /* N=576 – Rhythm 0x10 data */
} fm_wait_table_t;

/* ---------------------------------------------------------------------------
 * Bus object  (1 per physical shared bus)
 * --------------------------------------------------------------------------*/
typedef struct fm_bus {
    PIO      pio;
    uint     sm;
    uint32_t pio_hz;
    /** Base offset of unified `fm_bus` PIO program in instruction memory. */
    uint     offset_bus;
    uint     spinlock_num;
    /** Non-zero only after `fm_bus_init()` returns 0; `fm_bus_deinit()` no-ops if zero. */
    uint32_t init_magic;
} fm_bus_t;

/* ---------------------------------------------------------------------------
 * Device object  (1 per FM chip, shares an fm_bus_t)
 * --------------------------------------------------------------------------*/
typedef struct {
    fm_chip_id_t        chip_id;
    fm_device_type_t    device_type;
    fm_master_clock_hz_t master_clock_hz;
    fm_wait_table_t     wait_table;
    fm_bus_t           *bus;
} fm_device_t;

/* ---------------------------------------------------------------------------
 * Initialization / teardown API
 * --------------------------------------------------------------------------*/

/**
 * @brief  Initialize the FM bus (GPIO, PIO, spinlock).
 *
 * Loads the unified `fm_bus` PIO program, assigns GPIO2–15, and enables
 * **sm** as the sole bus state machine (always drives the pads).
 *
 * @param bus     Pointer to caller-allocated fm_bus_t.
 * @param pio     PIO instance (pio0 or pio1).
 * @param sm      State machine index for the bus (0-3).
 * @param pio_hz  Desired PIO clock in Hz (<=150000000).
 * @return  0 on success, -1 if spinlock exhausted, -2 if PIO memory full.
 */
int fm_bus_init(fm_bus_t *bus, PIO pio, uint sm, uint32_t pio_hz);

/**
 * @brief  Shut down the FM bus (stop SMs, release spinlock, unload programs).
 *
 * No-op if bus is NULL, or if `fm_bus_init()` did not complete successfully
 * on this struct (including never called).
 */
void fm_bus_deinit(fm_bus_t *bus);

/**
 * @brief  Initialize a device handle bound to a bus.
 *
 * Pre-computes the wait_table for the given master clock and PIO frequency.
 *
 * @param dev             Pointer to caller-allocated fm_device_t.
 * @param bus             Initialized fm_bus_t.
 * @param chip_id         Chip select index 0-3.
 * @param device_type     FM_DEVICE_YM2203 or FM_DEVICE_YM2608.
 * @param master_clock_hz Actual master clock supplied to the chip (Hz).
 * @return  0 on success, -1 on invalid argument.
 */
int fm_device_init(fm_device_t *dev, fm_bus_t *bus,
                   fm_chip_id_t chip_id, fm_device_type_t device_type,
                   fm_master_clock_hz_t master_clock_hz);

/* ---------------------------------------------------------------------------
 * Register access API
 * --------------------------------------------------------------------------*/

/**
 * @brief  Write a register.
 *
 * Automatically determines W1/W2 from addr/a1 and performs an atomic
 * addr-write + data-write pair via the bus PIO SM.
 *
 * @param dev   Device handle.
 * @param addr  Register address (0x00–0xb6).
 * @param a1    Bank select: 0 = lower bank, 1 = upper bank (YM2608 only).
 * @param data  Data byte to write.
 */
void write_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1, uint8_t data);

/**
 * @brief  Write the data phase only (A0=1), after a register address is latched.
 *
 * Pattern-2 data write (repeated data cycles after a single address cycle):
 * skip the address cycle and
 * emit only the data cycle. The latched address is a YM2608 register (e.g. 0x08
 * ADPCM-DATA), not a DRAM byte address. ADPCM registers use W1=W2=0; transfer
 * pacing after each byte is via Status1 BRDY/EOS (P.53), not W2.
 *
 * @param dev   Device handle.
 * @param a1    Bank select: 0 = lower bank, 1 = upper bank (YM2608 only).
 * @param data  Data byte to write.
 */
void write_reg_data(const fm_device_t *dev, uint8_t a1, uint8_t data);

/**
 * @brief  Read the status register.
 *
 * @param dev  Device handle.
 * @param a1   0 = Status0 (BUSY/flags), 1 = Status1 (YM2608 extended).
 * @return  Status byte.
 */
uint8_t read_status(const fm_device_t *dev, uint8_t a1);

/**
 * @brief  Read a register (W1=0 addresses).
 *
 * Valid addresses:
 *   A1=0, addr 0x00–0x0f  → SSG registers
 *   A1=1, addr 0x08       → ADPCM data
 *   A1=1, addr 0x0f       → PCM data
 *   A1=0, addr 0xff       → Device identification code (YM2608)
 *
 * @param dev   Device handle.
 * @param addr  Register address.
 * @param a1    Bank select.
 * @return  Register value.
 */
uint8_t read_reg(const fm_device_t *dev, uint8_t addr, uint8_t a1);

/**
 * @brief  Read the data phase only (A0=1), after a register address is latched.
 *
 * Pattern-2 data read: skip the address cycle (see write_reg_data). Requires a
 * prior read_reg() (or equivalent addr phase) for the target register.
 *
 * @param dev  Device handle.
 * @param a1   Bank select: 0 = lower bank, 1 = upper bank (YM2608 only).
 * @return  Data byte read.
 */
uint8_t read_reg_data(const fm_device_t *dev, uint8_t a1);

/* ---------------------------------------------------------------------------
 * High-level frequency API
 * --------------------------------------------------------------------------*/

/**
 * @brief  Set the frequency of CH0-5 atomically.
 *
 * Writes Block/F-Num2 first, then F-Num1, both inside a single spinlock
 * acquisition.  ch=0-2 use A1=0; ch=3-5 use A1=1.
 *
 * @param dev    Device handle.
 * @param ch     Channel 0-5.
 * @param block  Block (octave) 0-7.
 * @param fnum   F-Number 0-2047 (11 bits).
 */
void fm_set_freq(const fm_device_t *dev, uint8_t ch, uint8_t block,
                 uint16_t fnum);

/**
 * @brief  Set per-slot frequencies for CH3 effect-sound / CSM mode.
 *
 * Always uses A1=0.  slot=0-3 maps to operators S1-S4 of CH3.
 *
 * @param dev    Device handle.
 * @param slot   Slot index 0-3 (S1-S4).
 * @param block  Block (octave) 0-7.
 * @param fnum   F-Number 0-2047 (11 bits).
 */
void fm_set_freq_ch3(const fm_device_t *dev, uint8_t slot, uint8_t block,
                     uint16_t fnum);

#ifdef __cplusplus
}
#endif
