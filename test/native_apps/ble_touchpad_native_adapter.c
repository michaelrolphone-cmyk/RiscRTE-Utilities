/* Test-only readback of actual adapter overlay state. */
#include BLE_TOUCHPAD_ADAPTER_SOURCE
bool bb_diag_quick_visible(void){return pqa_visible(&quick.ui);}
