#pragma once
/* Stable Buttons facade over the reusable shared HID session controller. */
#include "ble_hid_session.h"
typedef ble_hid_session ble_buttons_session;
#define bbs_live bhs_live
#define bbs_init bhs_init
#define bbs_release bhs_release
#define bbs_close bhs_close
#define bbs_ready bhs_ready
#define bbs_report bhs_report
#define bbs_neutral bhs_neutral
#define bbs_press bhs_press
#define bbs_poll bhs_poll
#define bbs_confirm bhs_confirm
#define bbs_forget bhs_forget
#define bbs_battery bhs_battery
static inline bool bbs_open(ble_buttons_session *s,bool pairing,uint64_t now){return bhs_open(s,"BLE Buttons",pairing,now);}
