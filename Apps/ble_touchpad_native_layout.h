#ifndef BLE_TOUCHPAD_NATIVE_LAYOUT_H
#define BLE_TOUCHPAD_NATIVE_LAYOUT_H
/* Shared capability-pixel control grid; Touchpad supplies its own gesture area.
 * Existing Buttons geometry and legacy/paper paths are unchanged. */
#include "ble_buttons_native_layout.h"
static inline hid_native_rect hid_touchpad_native_pad(hid_native_layout l){
 int top=l.status.y+l.status.h+l.gap/2;
 int bottom=l.content.y+l.content.h;
 return (hid_native_rect){l.content.x,top,l.content.w,bottom>top?bottom-top:0};
}
#endif
