#include <assert.h>
#include "../../Apps/ble_buttons_scene.c"
static const risc_runtime_api_v1 *current_runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){return v==1?current_runtime:NULL;}
static unsigned retains,endpoint_calls,confirmed,closed,key_bits;static bool close_ok=true,release_ok=true;
static unsigned diagnostics,diag_reads;static bool diag_lose;static char diag_text[192];
static bool rt_diagnostic(const char*s){diagnostics++;snprintf(diag_text,sizeof(diag_text),"%s",s);if(diag_lose)current_runtime=NULL;return true;}
static bool rt_retain(void){retains++;return true;}
static bool rt_clock(uint32_t *n){*n=1000;return true;}
static void rt_yield(uint32_t n){(void)n;}
static bool rt_acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){(void)n;(void)v;(void)i;(void)g;return false;}
static bool rt_release(risc_runtime_capability_v1*g){*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 runtime_api={.api_version=1,.struct_size=sizeof(runtime_api),.diagnostic=rt_diagnostic,.retain_invocation=rt_retain,.monotonic_ms=rt_clock,.yield_ms=rt_yield,.acquire=rt_acquire,.release=rt_release};
static bool h_open(void*c,const char*n,bool b,uint64_t*t){(void)c;(void)n;(void)b;endpoint_calls++;*t=1;return true;}
static bool h_poll(void*c,uint64_t t,uint32_t n){(void)c;(void)t;(void)n;endpoint_calls++;return true;}
static bool h_status(void*c,uint64_t t,risc_bluetooth_hid_status_v1*s){(void)c;(void)t;endpoint_calls++;*s=hid.status;return true;}
static bool h_confirm(void*c,uint64_t t,bool b){(void)c;(void)t;(void)b;endpoint_calls++;confirmed++;return true;}
static bool h_key(void*c,uint64_t t,uint8_t m,const uint8_t*k){(void)c;(void)t;(void)m;(void)k;endpoint_calls++;return true;}
static bool h_mouse(void*c,uint64_t t,uint8_t b,int8_t x,int8_t y,int8_t w){(void)c;(void)t;(void)b;(void)x;(void)y;(void)w;endpoint_calls++;return true;}
static bool h_release(void*c,uint64_t t){(void)c;(void)t;endpoint_calls++;return release_ok;}
static bool h_close(void*c,uint64_t t){(void)c;(void)t;endpoint_calls++;closed++;return close_ok;}
static bool h_forget(void*c){(void)c;endpoint_calls++;return true;}
static bool h_battery(void*c,uint64_t t,uint8_t p){(void)c;(void)t;(void)p;endpoint_calls++;return true;}
static risc_bluetooth_hid_v1 h_api={1,sizeof(h_api),NULL,h_open,h_poll,h_status,h_confirm,h_key,h_mouse,h_release,h_close,h_forget,h_battery};
static bool k_read(void*c,uint32_t*out){(void)c;*out=key_bits;return true;}
static risc_power_key_api_v1 k_api={1,sizeof(k_api),NULL,k_read};
static bool h_diagnostics(void*c,uint64_t t,risc_bluetooth_hid_diagnostics_v1*d){(void)c;(void)t;diag_reads++;d->port_fault=7;return true;}
static void reset_root(void){
 diagnostics=diag_reads=0;diag_lose=false;
 current_runtime=&runtime_api;assert(bbo_init(&owner));bbs_init(&hid,&h_api,bbo_live,&owner);bbe_init(&editor,bbo_live,&owner);view=(ble_buttons_view){.hold_available=true,.hid_available=true,.storage_available=true,.session=&hid,.editor=&editor};resident=(risc_scene_resident_v1){0};alarm=(ble_buttons_alarm){0};policy=(ble_buttons_policy){0};background_ready=leave=paired_press=local_fault=false;session=last_event=hold_generation=0;pending_checkpoint=0;notice[0]=0;document=(risc_components_document_v1){0};close_ok=release_ok=true;retains=endpoint_calls=confirmed=closed=0;
 for(unsigned i=0;i<5;i++)grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
}
static void doc(void){bbp_build(&document,&view,1);}
static risc_scene_hold_event_v1 event(unsigned action,unsigned phase,uint64_t seq){
 risc_scene_hold_event_v1 h={.struct_size=sizeof(h),.phase=phase,.generation=55,.held_ms=100,.event={.struct_size=sizeof(risc_scene_event_v1),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=document.revision,.sequence=seq,.action=action}};
 for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==action)h.event.node=document.nodes[i].id;
 return h;
}
int main(void){
 reset_root();view.page=BB_PAIR;hid.token=1;hid.status=(risc_bluetooth_hid_status_v1){sizeof(hid.status),RISC_HID_PAIR_CONFIRM,0,123456,0,7};pair_generation=7;pair_number=123456;doc();risc_scene_hold_event_v1 h=event(BB_PAIR_ACCEPT,RISC_HOLD_PRESS,1);input(&h,1000);assert(paired_press);hid.status.pairing_number=654321;h.phase=RISC_HOLD_RELEASE;input(&h,1100);assert(!confirmed&&!paired_press);
 reset_root();view.page=BB_PAIR;hid.token=1;hid.status=(risc_bluetooth_hid_status_v1){sizeof(hid.status),RISC_HID_PAIR_CONFIRM,0,123456,0,7};pair_generation=7;pair_number=123456;doc();h=event(BB_PAIR_ACCEPT,RISC_HOLD_PRESS,1);input(&h,1000);h.phase=RISC_HOLD_RELEASE;input(&h,1100);assert(confirmed==1&&hid.pair_accepted);
 reset_root();hid.token=1;hid.holding=true;h=(risc_scene_hold_event_v1){.struct_size=sizeof(h),.phase=RISC_HOLD_CANCEL,.event={.document_revision=999}};input(&h,0);assert(!hid.holding&&endpoint_calls==1);
 reset_root();hid.token=1;hid.holding=true;h.flags=RISC_HOLD_LOCAL_FAULT_PENDING;input(&h,0);assert(local_fault&&closed==1&&!hid.token&&!owner.terminal);
 reset_root();hid.token=1;hid.holding=true;close_ok=false;input(&h,0);assert(local_fault&&!owner.terminal&&retains==0&&hid.closing);close_ok=true;assert(bbs_poll(&hid,20)&&!hid.token);
 reset_root();hid.token=1;hid.holding=true;release_ok=close_ok=false;input(&h,0);assert(local_fault&&hid.closing&&!owner.terminal);unsigned before=endpoint_calls;assert(!bbs_poll(&hid,10)&&endpoint_calls==before+1);close_ok=true;assert(bbs_poll(&hid,20)&&!hid.token);
 reset_root();alarm.reserved=true;action(BB_RECONNECT,0);assert(!endpoint_calls&&!hid.token);action(BB_PAIR_START,0);assert(!endpoint_calls&&!hid.token);
 reset_root();view.page=BB_PAIR;back();assert(leave);
 reset_root();view.page=BB_MODIFIERS;editor.editing=true;back();assert(view.page==BB_EDIT&&editor.editing&&!leave);back();assert(view.page==BB_MAIN&&!editor.editing&&!leave);back();assert(leave);
 reset_root();view.page=BB_ALERT;back();assert(!leave&&view.page==BB_ALERT);
 reset_root();bool go=false;key_bits=2;assert(bbpolicy_key(&policy,&owner,&k_api,&go,1)&&!go);key_bits=0;assert(bbpolicy_key(&policy,&owner,&k_api,&go,2)&&!go);key_bits=2;assert(bbpolicy_key(&policy,&owner,&k_api,&go,3)&&go);assert(bbpolicy_key(&policy,&owner,&k_api,&go,4)&&!go);key_bits=0;assert(bbpolicy_key(&policy,&owner,&k_api,&go,5));key_bits=3;assert(bbpolicy_key(&policy,&owner,&k_api,&go,6)&&!go);key_bits=2;assert(bbpolicy_key(&policy,&owner,&k_api,&go,7)&&!go);
 reset_root();policy.neutral=true;assert(bbpolicy_load(&policy,&owner,100));assert(!policy.neutral);key_bits=2;assert(bbpolicy_key(&policy,&owner,&k_api,&go,101)&&!go);
 reset_root();policy.idle_known=true;policy.idle_ms=5000;policy.activity=100;assert(!bbpolicy_due(&policy,5099)&&bbpolicy_due(&policy,5100));policy.idle_known=false;assert(!bbpolicy_due(&policy,10000));
 reset_root();doc();h=event(BB_SETTINGS_OPEN,RISC_HOLD_NONE,1);view.page=BB_EDIT;input(&h,0);assert(view.page==BB_EDIT);
 reset_root();current_runtime=NULL;unsigned n=endpoint_calls;assert(!bbs_close(&hid)&&endpoint_calls==n&&hid.terminal);
 reset_root();hid.status.pairing_number=123456;diagnostic("state");assert(diagnostics==1&&!strstr(diag_text,"123456"));
 reset_root();risc_bluetooth_hid_diagnostics_api_v1 ext={.base=h_api,.diagnostics=h_diagnostics};ext.base.struct_size=sizeof(ext);hid.api=&ext.base;hid.token=1;diagnostic("state");assert(diagnostics==3&&diag_reads==1);
 reset_root();hid.api=&ext.base;hid.token=1;hid.closing=true;diagnostic("failure");assert(diagnostics==1&&!diag_reads);
 reset_root();hid.api=&ext.base;hid.token=1;diag_lose=true;diagnostic("state");assert(diagnostics==1&&!diag_reads&&owner.terminal);diagnostic("again");assert(diagnostics==1);
 puts("Buttons root focused assertions PASS");return 0;
}
