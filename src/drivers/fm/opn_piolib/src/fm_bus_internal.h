//
// Copyright (c) 2026 46nori All rights reserved.
//
// This code is licensed under the MIT License.
// See LICENSE file for details.
//
#pragma once

/*
 * fm_bus_internal.h — GPIO pin assignments, FIFO word constructors,
 * raw PIO helper declarations, and spinlock utilities.
 *
 * NOT part of the public API.  Include only from fm_bus.c / fm_opn.c.
 */

#include "opn_piolib.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "hardware/clocks.h"
#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * GPIO assignments (FM bus: D0-D7, A0/A1, CS0/CS1, /WR, /RD)
 * These are tightly coupled to the PIO program pin configuration.
 * Changing any value here requires updating the PIO programs too.
 * --------------------------------------------------------------------------*/
#define FM_GPIO_D0   2u
#define FM_GPIO_A0  10u
#define FM_GPIO_A1  11u
#define FM_GPIO_CS0 12u
#define FM_GPIO_CS1 13u
#define FM_GPIO_WR  14u
#define FM_GPIO_RD  15u

_Static_assert(FM_GPIO_A0  == FM_GPIO_D0 + 8u,  "A0 must be D0+8");
_Static_assert(FM_GPIO_A1  == FM_GPIO_D0 + 9u,  "A1 must be D0+9");
_Static_assert(FM_GPIO_CS0 == FM_GPIO_D0 + 10u, "CS0 must be D0+10");
_Static_assert(FM_GPIO_CS1 == FM_GPIO_D0 + 11u, "CS1 must be D0+11");
_Static_assert(FM_GPIO_WR  == FM_GPIO_D0 + 12u, "WR must be D0+12");
_Static_assert(FM_GPIO_RD  == FM_GPIO_WR + 1u,  "RD must be WR+1");

/* ---------------------------------------------------------------------------
 * SM_WRITE FIFO word constructor (write transaction word)
 *
 * Bit layout (LSB-first shift to GPIO):
 *   bits[ 7: 0]  D0-D7
 *   bit [ 8]     A0
 *   bit [ 9]     A1
 *   bits[11:10]  CS0-CS1  (= chip_id[1:0])
 *   bits[31:12]  W_count  (20-bit PIO wait counter)
 * --------------------------------------------------------------------------*/
static inline uint32_t fm_make_write_word(uint8_t byte, uint8_t chip_id,
                                          uint8_t a0, uint8_t a1,
                                          uint32_t w_count)
{
    /* valid range 0-3 */
    assert(chip_id <= 3u);
    uint32_t lower = ((uint32_t)byte)
                   | ((uint32_t)(a0 & 1u)    << 8u)
                   | ((uint32_t)(a1 & 1u)    << 9u)
                   | ((uint32_t)(chip_id & 3u) << 10u);
    return lower | (w_count << 12u);
}

/* ---------------------------------------------------------------------------
 * SM_READ_STATUS command word constructor (status read, 1 FIFO word)
 *
 * Uses the same write-word lower-12-bit pin layout as read_reg Word 2:
 *   bits[ 7: 0]  D0-D7 output latch value (0; D pins are CPU-set input)
 *   bit [ 8]     A0=0 (status read)
 *   bit [ 9]     A1
 *   bits[11:10]  CS0-CS1
 *   bits[31:12]  Tacc_count
 * --------------------------------------------------------------------------*/
static inline uint32_t fm_make_read_status_word(uint8_t chip_id, uint8_t a1,
                                                uint32_t tacc_count)
{
    assert(chip_id <= 3u);
    return fm_make_write_word(0u, chip_id, /*a0=*/0u, a1, tacc_count);
}

/* ---------------------------------------------------------------------------
 * SM_READ_REG word constructors (register read: addr write + data read)
 * --------------------------------------------------------------------------*/

/* Word 1 – addr write phase (A0=0 fixed); upper 20 bits unused by unified PIO */
static inline uint32_t fm_make_read_reg_word1(uint8_t addr, uint8_t chip_id,
                                              uint8_t a1)
{
    return fm_make_write_word(addr, chip_id, /*a0=*/0u, a1, /*w_count=*/0u);
}

/* Word 2 – data read phase (A0=1, CS=chip_id).
 * Write pin layout (bits[11:0]) for reg_entry `out pins, 12`.
 * Tacc in bits[31:12]; PIO read_strobe loads 12 bits into X (sufficient at
 * supported pio_hz for SSG/ADPCM Tacc requirements). */
static inline uint32_t fm_make_read_reg_word2(uint8_t chip_id, uint8_t a1,
                                              uint32_t tacc_count)
{
    return fm_make_write_word(0, chip_id, /*a0=*/1u, a1, tacc_count);
}

/* ---------------------------------------------------------------------------
 * Tacc_count from ns and pio_hz: ceil(ns_min * pio_hz / 1e9) — read_status / read_reg use this
 * --------------------------------------------------------------------------*/
static inline uint32_t fm_tacc_count(uint32_t ns_min, uint32_t pio_hz)
{
    /* ceil(ns_min * pio_hz / 1e9) */
    uint64_t cycles = ((uint64_t)ns_min * (uint64_t)pio_hz + 999999999u) / 1000000000u;
    return (uint32_t)cycles;
}

/* ---------------------------------------------------------------------------
 * Wait until SM_WRITE is truly idle
 * Uses FDEBUG.TXSTALL — set when TX FIFO is empty AND SM stalled on pull.
 * --------------------------------------------------------------------------*/
static inline void fm_bus_wait_write_idle(const fm_bus_t *bus)
{
    uint32_t mask = 1u << (PIO_FDEBUG_TXSTALL_LSB + bus->sm);
    /* Clear the stall flag first, then wait for it to be set again */
    bus->pio->fdebug = mask;
    while (!(bus->pio->fdebug & mask)) {
        tight_loop_contents();
    }
}

/* ---------------------------------------------------------------------------
 * Spinlock helpers
 * --------------------------------------------------------------------------*/
static inline uint32_t fm_bus_lock(const fm_bus_t *bus)
{
    return spin_lock_blocking(spin_lock_instance(bus->spinlock_num));
}

static inline void fm_bus_unlock(const fm_bus_t *bus, uint32_t irq_state)
{
    spin_unlock(spin_lock_instance(bus->spinlock_num), irq_state);
}

/* ---------------------------------------------------------------------------
 * Raw (internal) primitive declarations
 * Implemented in fm_bus.c
 * --------------------------------------------------------------------------*/

/**
 * Push 2 words (addr-phase, data-phase) into SM_WRITE TX FIFO.
 * Caller must hold the spinlock.
 */
void fm_write_reg_raw(fm_bus_t *bus, uint32_t addr_word, uint32_t data_word);

/** Status read. Caller must hold the bus spinlock. */
uint8_t fm_read_status_raw(fm_bus_t *bus, uint8_t chip_id,
                           uint8_t a1, uint32_t tacc_count);

/** Register read (addr write + data read). Caller must hold the bus spinlock. */
uint8_t fm_read_reg_raw(fm_bus_t *bus, uint8_t addr, uint8_t chip_id,
                        uint8_t a1, uint32_t tacc_count);

/** Pattern-2 data-phase register read (reg addr latched). Caller must hold the bus spinlock. */
uint8_t fm_read_reg_data_raw(fm_bus_t *bus, uint8_t chip_id,
                             uint8_t a1, uint32_t tacc_count);

#ifdef __cplusplus
}
#endif
