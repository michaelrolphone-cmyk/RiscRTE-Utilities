#include <assert.h>
#include "../../Apps/ble_touchpad_scene.c"
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return NULL;}
int main(void){app_main();assert(!owner.runtime&&!session);puts("Touchpad actual root no Runtime PASS");return 0;}
