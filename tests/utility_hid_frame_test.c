/* Production HID controller; only display and provider I/O are doubles. */
#define main original_hid_test_main
#include "hid_app_test.c"
#undef main
static bool frame_available;
static unsigned frame_submits,frame_checks;
static bool frame_ready(void){frame_checks++;return frame_available;}
static void frame_present(bool full){(void)full;assert(frame_available);frame_submits++;}
int main(void){
 reset();utility_frame_reset();t5_app_api_v1 frame_api=fake_app;
 frame_api.frame_ready=frame_ready;frame_api.present=frame_present;app=&frame_api;
 frame_available=false;draw();assert(dirty&&utility_frame_pending&&!frame_submits);
 start_session(false);step(0,0,0);assert(had_ready);
 if(HID_TOUCHPAD){step(1,100,120);step(0,0,0);assert(mouse_reports==2&&reports[0].buttons==1&&!reports[1].buttons);}
 else{assignments[2]=(hid_assignment){.kind=HID_ACTION_KEY,.modifiers=3,.key=6};step(1,30,160);step(0,0,0);assert(key_reports==2&&reports[0].mod==3&&reports[0].key==6&&!reports[1].key);}
 /* Intermediate pages are logical only while busy. One available lease paints
  * the current settings screen; no old edit/main snapshot is replayed. */
 view=HID_VIEW_EDIT;draft=assignments[0];dirty=true;draw();
 view=HID_VIEW_MODIFIERS;draw();view=HID_VIEW_SETTINGS;draw();
 assert(!frame_submits&&dirty&&utility_frame_pending);
 frame_available=true;draw();assert(frame_submits==1&&!dirty&&!utility_frame_pending&&strstr(drawn,"HID SETTINGS"));
 assert(!strstr(drawn,"MODIFIERS"));
 /* A legacy runtime suffix and null callbacks retain synchronous behavior. */
 frame_api.struct_size=offsetof(t5_app_api_v1,frame_ready);frame_available=false;
 frame_api.present=present;draw();assert(!utility_frame_pending);
 frame_api.struct_size=sizeof(frame_api);frame_api.frame_ready=NULL;draw();assert(!utility_frame_pending);
 stop_session("Done");assert(!grants&&!have_token&&!raw_subs);
 printf("PASS %s busy display: input/reports continue; latest-only frame; legacy suffix; checks=%u\n",HID_TOUCHPAD?"Touchpad":"Buttons/combo",frame_checks);return 0;
}
