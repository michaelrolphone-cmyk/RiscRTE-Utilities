/* Read-only diagnostic wrapper around the unchanged selected adapter source. */
#include BLE_BUTTONS_ADAPTER_SOURCE
bool bb_diag_poll(unsigned wait,t5_app_input_t *out){return poll(out,wait);}
bool bb_diag_drain(void){return portable_paper_frame_drain();}
bool bb_diag_visible(void){return ble_buttons_native_visible;}
uint64_t bb_diag_generation(void){return ble_buttons_native_generation;}
unsigned bb_diag_owner(void){return native_token_owner;}
bool bb_diag_failed(void){return failed;}

bool bb_diag_quick_visible(void){return pqa_visible(&quick.ui);}
