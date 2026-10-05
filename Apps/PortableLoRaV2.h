#pragma once
/* Consumer-only copy of radio.lora@2 from RiscRTE-T-Watch-S3
 * include/twatch_caps.h at fc4fe03a0ed26222aae4848ff587fb6137390106.
 * This preserves the existing board-local contract; it does not add a Runtime
 * ABI or let applications select pins, buses or the physical antenna band. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TWATCH_RADIO_API_V1 2u
#define TWATCH_RADIO_CAPABILITY "radio.lora"
typedef struct {
    uint32_t frequency_hz, bandwidth_hz;
    uint16_t preamble;
    uint8_t sf, coding_rate;
    int8_t power_dbm;
    uint8_t reserved[3];
} twatch_lora_config_v2;
typedef struct {
    uint8_t state, length;
    int16_t rssi_dbm;
    int8_t snr_quarter_db;
    uint8_t reserved[3];
} twatch_lora_status_v2;
enum {
    TW_LORA_IDLE,
    TW_LORA_TX,
    TW_LORA_RX,
    TW_LORA_SENT,
    TW_LORA_RECEIVED,
    TW_LORA_TIMEOUT,
    TW_LORA_CRC_ERROR,
    TW_LORA_IO_ERROR
};
typedef struct {
    uint32_t struct_size, supported_profiles, selected_profile, reserved;
} twatch_lora_profile_info_v1;
typedef struct {
    uint32_t api_version, struct_size;
    void *context;
    bool (*configure)(void *, const twatch_lora_config_v2 *);
    bool (*send)(void *, const uint8_t *, size_t, uint32_t timeout_ms);
    bool (*receive)(void *, uint32_t timeout_ms);
    bool (*poll)(void *, twatch_lora_status_v2 *);
    bool (*read)(void *, uint8_t *, size_t, size_t *);
    bool (*cancel)(void *);
    /* Optional suffix: profile IDs1..4, mask1<<(id-1),0 explicitly clears.
     * Selection never initializes or transmits. Failed cleanup retains choice. */
    bool (*profile_info)(void *, twatch_lora_profile_info_v1 *);
    bool (*select_profile)(void *, uint32_t);
} twatch_radio_api_v2;

_Static_assert(sizeof(twatch_lora_config_v2)==16,"radio.lora@2 config size");
_Static_assert(sizeof(twatch_lora_status_v2)==8,"radio.lora@2 status size");
_Static_assert(sizeof(twatch_lora_profile_info_v1)==16,"radio selector info size");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(twatch_radio_api_v2,profile_info)==36,"radio.lora@2 preserved prefix");
_Static_assert(offsetof(twatch_radio_api_v2,select_profile)==40,"radio selector offset");
_Static_assert(sizeof(twatch_radio_api_v2)==44,"radio.lora@2 target table");
_Static_assert(offsetof(twatch_radio_api_v2,cancel)==32,"radio.lora@2 cancel offset");
#endif
