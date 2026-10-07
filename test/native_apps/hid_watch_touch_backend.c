/* The production Watch touch provider is linked unchanged. Only physical I2C,
 * GPIO and time are simulated; event kinds, sequences and snapshots are real. */
#include "RiscGpioBankV1.h"
#include "RiscI2cBusV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscHardwareConfigV1.h"
#include "RiscTouchV1.h"
#include <assert.h>
#include <string.h>

const risc_driver_v2 *t5_driver_get(uint32_t abi);
void hid_renderer_watch_report(risc_touch_snapshot_v1 *sample);
uint64_t hid_renderer_watch_millis(void);
static const risc_driver_v2 *provider;
static bool bus_owned, gpio_owned;
static bool claim_bus(void *c, uint8_t address, uint64_t *token) {
    (void)c; assert(address == 0x38 && !bus_owned);
    bus_owned = true; *token = 1; return true;
}
static bool release_bus(void *c, uint64_t token) {
    (void)c; assert(token == 1 && bus_owned); bus_owned = false; return true;
}
static bool transact(void *c, uint64_t token, const uint8_t *w, size_t wn,
                     uint8_t *r, size_t rn, uint32_t timeout) {
    (void)c; assert(bus_owned && token == 1 && wn == 1 && w[0] == 2);
    assert(rn == 13 && timeout == 30);
    risc_touch_snapshot_v1 sample; hid_renderer_watch_report(&sample);
    assert(sample.contact_count <= 2); memset(r, 0, rn); r[0] = sample.contact_count;
    for (unsigned i = 0; i < sample.contact_count; ++i) {
        const risc_touch_contact_v1 *contact = sample.contacts + i;
        uint8_t *p = r + 1 + 6 * i;
        p[0] = (uint8_t)(contact->x >> 8); p[1] = (uint8_t)contact->x;
        p[2] = (uint8_t)((contact->id << 4) | (contact->y >> 8));
        p[3] = (uint8_t)contact->y;
    }
    return true;
}
static bool claim_gpio(void *c, uint8_t pin, uint32_t flags, uint64_t *token) {
    (void)c; assert(pin == 16 && flags == (RISC_GPIO_INPUT | RISC_GPIO_PULLUP) && !gpio_owned);
    gpio_owned = true; *token = 2; return true;
}
static bool release_gpio(void *c, uint64_t token) {
    (void)c; assert(token == 2 && gpio_owned); gpio_owned = false; return true;
}
static bool read_gpio(void *c, uint64_t token, bool *level) {
    (void)c; assert(token == 2 && gpio_owned); *level = false; return true;
}
static bool write_gpio(void *c, uint64_t token, bool level) {
    (void)c; (void)token; (void)level; assert(!"Touch must never write the IRQ pin"); return false;
}
static uint64_t clock_now(void *c) { (void)c; return hid_renderer_watch_millis(); }
static void clock_sleep(void *c, uint32_t ms) { (void)c; (void)ms; assert(!"Unexpected touch sleep"); }
static const risc_i2c_bus_api_v1 bus = {1, sizeof(bus), NULL, claim_bus, transact, release_bus};
static const risc_gpio_bank_api_v1 gpio = {
    .api_version=1, .struct_size=sizeof(gpio), .claim=claim_gpio,
    .read=read_gpio, .write=write_gpio, .release=release_gpio
};
static const risc_platform_clock_api_v1 clock_api = {1, sizeof(clock_api), NULL, clock_now, clock_sleep};
static const risc_hw_i2c_touch_v1 config = {
    .struct_size=sizeof(config), .bus={.struct_size=sizeof(risc_hw_bus_v1),
        .kind=RISC_HW_BUS_I2C, .instance_id=3, .controller=1, .frequency_hz=400000,
        .sda=39, .scl=40, .sclk=-1, .mosi=-1, .miso=-1},
    .address=0x38, .irq=16, .reset=-1, .irq_pull_up=1, .width=240, .height=240
};
static const risc_hardware_device_v1 device = {
    .api_version=1, .struct_size=sizeof(device), .instance_id=6,
    .compatible="focaltech,ft6336u", .revision="unspecified", .config_type="touch.i2c",
    .config_version=1, .config_size=sizeof(config), .config=&config
};
const risc_touch_api_v1 *hid_watch_touch_start(void) {
    const risc_provider_dependency_v1 dependencies[] = {
        {"hardware.device",1,&device}, {"i2c.bus",1,&bus},
        {"gpio.bank",1,&gpio}, {"platform.clock",1,&clock_api}
    };
    provider=t5_driver_get(2); assert(provider && provider->start(dependencies,4));
    return provider->capability;
}
void hid_watch_touch_stop(void) {
    assert(provider->quiesce() && !bus_owned && !gpio_owned);
}
