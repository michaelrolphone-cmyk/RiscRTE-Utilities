#ifndef S3_IQ_BURST_H
#define S3_IQ_BURST_H
/*
 * One SRAM dump window from the ESP32-S3 Wi-Fi modem, receive only.
 *
 * Register addresses, the 32-bit pair layout, and the dump start/stop order
 * are copied from h0m3us3r/eSpDR (0BSD) commit
 * f279bf823eee41796dfd1ac21f13e1ed9b418c82:
 *   esp32s3/src/board.h
 *   esp32s3/src/capture.c capture_run()
 *   esp32s3/src/radio.c radio_dump_control() / configure_receiver()
 * The FPGA dedicated-GPIO stream in transmit.S is not used. There is no
 * transmitter path.
 *
 * This header does not bring the receiver up. radio_init() in that tree calls
 * register_chipv7_phy, esp_rom_regi2c_read/write, phy_init_param_set,
 * phy_bbpll_en_usb, and the Wi-Fi clock-gate helpers. None of those symbols
 * are in the watch ELF import allowlist, so they are not called here and no
 * substitute register sequence is invented. Until s3_iq_radio_ready is set by
 * a real bring-up, s3_iq_take_burst does not touch the modem.
 */
#include <stdint.h>

#define S3_IQ_OK 0u
#define S3_IQ_NEED_RADIO_BRINGUP 1u
#define S3_IQ_BAD_ARGUMENT 2u

#define S3_IQ_PAIRS 256u

/* eSpDR board.h */
#define S3_IQ_CAPTURE_BANK_BASE 0x3FCB0000u
#define S3_IQ_DUMP_CTRL_REG 0x60033D5Cu
#define S3_IQ_DUMP_BANK_SELECT_REG 0x600C101Cu
#define S3_IQ_DUMP_CTRL_RUN 0x80000000u
#define S3_IQ_DUMP_CTRL_CIRCULAR 0x00024000u /* 80 Msps circular ring, IQ source 0 */

/* Set only after the published radio_init sequence has returned success.
 * Nothing in this app sets it. */
static unsigned s3_iq_radio_ready;

static volatile uint32_t *s3_iq_reg(uint32_t address) {
    return (volatile uint32_t *)(uintptr_t)address;
}

/* Published window, valid only after radio_init has programmed DUMP_CONFIG
 * and the receive path. One short run, then the writer is stopped. */
static unsigned s3_iq_copy_configured_window(uint32_t *pairs, unsigned count) {
    volatile uint32_t *ctrl = s3_iq_reg(S3_IQ_DUMP_CTRL_REG);
    volatile uint32_t *bank = s3_iq_reg(S3_IQ_DUMP_BANK_SELECT_REG);
    volatile uint32_t *samples = s3_iq_reg(S3_IQ_CAPTURE_BANK_BASE);
    *ctrl = S3_IQ_DUMP_CTRL_CIRCULAR;
    *bank = (*bank & ~15u) | 1u;
    *ctrl = S3_IQ_DUMP_CTRL_CIRCULAR | S3_IQ_DUMP_CTRL_RUN;
    for (volatile unsigned spin = 0; spin < 4000u; ++spin) {
    }
    *ctrl = S3_IQ_DUMP_CTRL_CIRCULAR;
    *bank &= ~15u;
    for (unsigned i = 0; i < count; ++i) pairs[i] = samples[i];
    return S3_IQ_OK;
}

static unsigned s3_iq_take_burst(uint32_t *pairs, unsigned count) {
    if (!pairs || count == 0 || count > S3_IQ_PAIRS) return S3_IQ_BAD_ARGUMENT;
    if (!s3_iq_radio_ready) return S3_IQ_NEED_RADIO_BRINGUP;
    return s3_iq_copy_configured_window(pairs, count);
}
#endif
