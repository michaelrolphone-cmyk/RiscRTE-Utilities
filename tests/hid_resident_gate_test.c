/* Actual controller, including its original lifecycle/security regression. */
#define PORTABLE_RADIO_CONTINUOUS_CAPTURE
#define main original_hid_main
#include "hid_app_test.c"
#undef main
int main(void){
 assert(original_hid_main()==0);
 reset();assert(!portable_radio_capture_active());
 const unsigned states[]={RISC_HID_ADVERTISING,RISC_HID_CONNECTED,RISC_HID_PAIR_CONFIRM,RISC_HID_READY};
 for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);i++){
  start_session(true);fake_status.state=states[i];fake_status.pairing_number=42;
  step(0,0,0);assert(token && portable_radio_capture_active());
  tick+=61000;step(0,0,0);assert(token && portable_radio_capture_active());
  stop_session("Stopped");assert(!portable_radio_capture_active());
 }
 /* A provider timeout exits pending comparison and permits idle again. */
 start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;step(0,0,0);
 assert(portable_radio_capture_active());fake_status.state=RISC_HID_ADVERTISING;
 step(0,0,0);assert(!token&&!portable_radio_capture_active()&&strstr(message,"timed out"));
 start_session(true);fake_status.state=RISC_HID_PAIR_CONFIRM;step(0,0,0);
 step(1,30,220);step(0,0,0);assert(!token&&!portable_radio_capture_active());
 /* Error and retry, plus failed close and grant release preserve custody. */
 start_session(false);poll_bad=true;step(0,0,0);assert(!token&&!portable_radio_capture_active());poll_bad=false;
 start_session(false);assert(portable_radio_capture_active());close_fail=1;
 assert(!close_session()&&token&&portable_radio_capture_active());
 assert(close_session()&&!portable_radio_capture_active());
 start_session(true);grant_fail=1;assert(!close_session()&&!token&&closing&&portable_radio_capture_active());
 assert(close_session()&&!portable_radio_capture_active());
 start_session(true);unsub_fail=1;assert(!close_session()&&closing&&portable_radio_capture_active());
 assert(close_session()&&!portable_radio_capture_active());
 assert(!grants&&!raw_subs&&!have_token);
 puts("HID resident gate: advertising, comparison, secure link, timeout, reject, fault, retry and checked cleanup PASS");
 return 0;
}
