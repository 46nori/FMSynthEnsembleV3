/*
 * fm_opn.c — High-level frequency API: fm_set_freq(), fm_set_freq_ch3()
 *
 * Both functions write 2 registers (Block/F-Num2 first, then F-Num1) inside
 * a single spinlock acquisition, so the 2-register update is atomic on the bus.
 */

#include "fm_bus_internal.h"
#include <assert.h>

/* ---------------------------------------------------------------------------
 * Register address tables (F-Num1 / Block/F-Num2 per channel and CH3 slot)
 * --------------------------------------------------------------------------*/

/* fm_set_freq(): ch 0-2 use A1=0; ch 3-5 use A1=1.
 * ch mod 3 gives the register column index. */
static const uint8_t ch_fnum1_reg[3]  = { 0xa0u, 0xa1u, 0xa2u };
static const uint8_t ch_fnum2_reg[3]  = { 0xa4u, 0xa5u, 0xa6u };

/* fm_set_freq_ch3(): slot 0-3 = S1-S4 (A1=0 fixed) */
static const uint8_t ch3_fnum1_reg[4] = { 0xa9u, 0xaau, 0xa8u, 0xa2u };
static const uint8_t ch3_fnum2_reg[4] = { 0xadu, 0xaeu, 0xacu, 0xa6u };

/* ---------------------------------------------------------------------------
 * fm_set_freq: ch 0-2 use A1=0, ch 3-5 use A1=1 (YM2608)
 * --------------------------------------------------------------------------*/
void fm_set_freq(const fm_device_t *dev, uint8_t ch, uint8_t block,
                 uint16_t fnum)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(ch <= 5u);
    assert(block <= 7u);
    assert(fnum <= 0x7FFu);

    uint8_t a1      = (ch >= 3u) ? 1u : 0u;
    uint8_t col     = ch % 3u;
    uint8_t addr_f2 = ch_fnum2_reg[col];  /* Block/F-Num2 written first */
    uint8_t addr_f1 = ch_fnum1_reg[col];  /* F-Num1 written second */

    uint8_t fn2 = (uint8_t)(((block & 0x07u) << 3u) | ((fnum >> 8u) & 0x07u));
    uint8_t fn1 = (uint8_t)(fnum & 0xFFu);

    uint8_t cid = dev->chip_id;

    /* Pre-compute W1/W2 counts for FM frequency registers (0xa0-0xa6) */
    /* W1=17, W2=47 for addr range 0xa0-0xb6 */
    uint32_t w1 = dev->wait_table.w17;
    uint32_t w2 = dev->wait_table.w47;

    uint32_t aw_f2 = fm_make_write_word(addr_f2, cid, 0u, a1, w1);
    uint32_t dw_f2 = fm_make_write_word(fn2,     cid, 1u, a1, w2);
    uint32_t aw_f1 = fm_make_write_word(addr_f1, cid, 0u, a1, w1);
    uint32_t dw_f1 = fm_make_write_word(fn1,     cid, 1u, a1, w2);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    fm_write_reg_raw(dev->bus, aw_f2, dw_f2);
    fm_write_reg_raw(dev->bus, aw_f1, dw_f1);
    fm_bus_unlock(dev->bus, irq_state);
}

/* ---------------------------------------------------------------------------
 * fm_set_freq_ch3: slot 0-3 = S1-S4, A1=0 fixed (CH3 effect/CSM mode)
 * --------------------------------------------------------------------------*/
void fm_set_freq_ch3(const fm_device_t *dev, uint8_t slot, uint8_t block,
                     uint16_t fnum)
{
    assert(dev != NULL && dev->bus != NULL);
    assert(slot <= 3u);
    assert(block <= 7u);
    assert(fnum <= 0x7FFu);

    uint8_t a1      = 0u;  /* always A1=0 */
    uint8_t addr_f2 = ch3_fnum2_reg[slot];
    uint8_t addr_f1 = ch3_fnum1_reg[slot];

    uint8_t fn2 = (uint8_t)(((block & 0x07u) << 3u) | ((fnum >> 8u) & 0x07u));
    uint8_t fn1 = (uint8_t)(fnum & 0xFFu);

    uint8_t cid = dev->chip_id;

    uint32_t w1 = dev->wait_table.w17;
    uint32_t w2 = dev->wait_table.w47;

    uint32_t aw_f2 = fm_make_write_word(addr_f2, cid, 0u, a1, w1);
    uint32_t dw_f2 = fm_make_write_word(fn2,     cid, 1u, a1, w2);
    uint32_t aw_f1 = fm_make_write_word(addr_f1, cid, 0u, a1, w1);
    uint32_t dw_f1 = fm_make_write_word(fn1,     cid, 1u, a1, w2);

    uint32_t irq_state = fm_bus_lock(dev->bus);
    fm_write_reg_raw(dev->bus, aw_f2, dw_f2);
    fm_write_reg_raw(dev->bus, aw_f1, dw_f1);
    fm_bus_unlock(dev->bus, irq_state);
}
