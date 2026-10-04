#ifndef RADIO_IQ_V1_H
#define RADIO_IQ_V1_H
/* Same layout as RiscRTE-Drivers sdk/driver/RiscRadioIqV1.h API 1.
 * The app does not include the driver sources. */
#include <stdint.h>
#define RISC_RADIO_IQ_API_V1 1u
#define RISC_RADIO_IQ_PAIRS 256u
#define RISC_RADIO_IQ_OK 0
#define RISC_RADIO_IQ_NOT_RUNNING 1
#define RISC_RADIO_IQ_PLL_FAILED 2
#define RISC_RADIO_IQ_PBUS_FAILED 3
#define RISC_RADIO_IQ_DUMP_TIMEOUT 4
#define RISC_RADIO_IQ_BAD_ARGUMENT 5
typedef struct {
    uint32_t api_version;
    uint32_t struct_size;
    void *context;
    int (*capture_burst)(void *context, uint32_t *pairs, uint32_t count);
} risc_radio_iq_api_v1;
#endif
