/* Actual Waterfall state and suspension, with only provider I/O replaced. */
#include "../../Apps/waterfall.c"
#include <assert.h>
static unsigned suspensions;
static bool stop_radio(void *context){(void)context;suspensions++;return true;}
static const risc_radio_iq_api_v1 radio={.api_version=1,.struct_size=sizeof(radio),.suspend=stop_radio};
int main(void){
 rf_bulk=&rf_resident_bulk;
 assert(!portable_radio_capture_active());
 capture_requested=running=owned=waterfall_enabled=true;assert(portable_radio_capture_active());
 uncertain=true;assert(!portable_radio_capture_active());uncertain=false;
 running=false;assert(!portable_radio_capture_active());running=true;
 waterfall_radio=&radio;
 assert(portable_radio_suspend());
 assert(suspensions==1&&!portable_radio_capture_active()&&!capture_requested&&!running&&!owned);
 portable_rf_capture_resume();
 assert(!running&&!capture_requested&&suspensions==1);
 puts("Waterfall: actual capture admission and no automatic foreground restart PASS");
 return 0;
}
