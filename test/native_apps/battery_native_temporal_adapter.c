/* Read-only diagnostic wrapper. The included production adapter is unchanged. */
#include BATTERY_DIAG_ADAPTER
#include <stdio.h>
void diag_trace(const char *name) {
 printf("TRACE %s time=%u last_activity=%u idle=%u cancelled=%u raw_contacts=%u down=%u pending=%u progressed=%u sample(valid,down,release,cancel)=%u,%u,%u,%u token=%llu failed=%u retained=%u\n",name,millis_now(),last_activity,portable_idle_ms(),touch.cancel_pending,touch.contact_count,touch.down,input_pending,input_progressed,input_sample.valid,input_sample.down,input_sample.released,input_sample.cancelled,(unsigned long long)paper_token,failed,native_custody_retained);
}
bool diag_poll(unsigned wait,t5_app_input_t *out){return poll(out,wait);}
unsigned diag_activity(void){return last_activity;}
unsigned diag_idle(void){return portable_idle_ms();}
unsigned diag_cancel(void){return touch.cancel_pending;}
unsigned diag_down(void){return touch.down;}
unsigned diag_failed(void){return failed;}
bool diag_drain(void){return portable_paper_frame_drain();}
#if defined(PORTABLE_BATTERY_NATIVE_UI) && !defined(BATTERY_EXPECT_ORIGINAL)
bool diag_visible(void){return battery_native_visible;}
unsigned long long diag_generation(void){return battery_native_generation;}
unsigned diag_owner(void){return native_token_owner;}
#endif

#if defined(PORTABLE_QUICK_NATIVE) && defined(PORTABLE_ALARM_NATIVE)
#include "PortableQuickLayout.h"
pqa_layout diag_quick_layout(void){return pqa_layout_for(&quick.ui);}
unsigned diag_quick_state(unsigned field){
 switch(field){case 0:return quick_modal;case 1:return quick_restore_pending;
 case 2:return quick.ui.native_width;case 3:return quick.ui.logical_height;
 case 4:return quick.brightness;case 5:return quick.volume;
 case 6:return quick.ui.error_flags;case 7:return quick.ui.torch;
 case 8:return quick_atomic_queue_count;case 9:return alarm_modal;
 case 10:return pqa_visible(&quick.ui);default:return 0;}
}
#endif
