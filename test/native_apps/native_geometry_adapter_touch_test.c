/* Real production adapter + Calculator, ordered provider events. */
#define risc_runtime_get_api inherited_runtime_get_api
#define main inherited_capture_main
#include "nova/peripherals.h"
#undef main
#undef risc_runtime_get_api
#include "../../Apps/calculator.c"
static risc_touch_event_v1 pending_event;
static risc_touch_snapshot_v1 touch_state={.width=NOVA_FIXTURE_WIDTH,.height=NOVA_FIXTURE_HEIGHT};
static bool event_pending;
static unsigned checked;
static void emit(unsigned kind,unsigned x,unsigned y){
 x+=NOVA_FIXTURE_LEFT;y+=NOVA_FIXTURE_TOP;
 touch_state.timestamp_ms=ticks;
 pending_event=(risc_touch_event_v1){++touch_state.sequence,ticks,(uint8_t)kind,1,(uint16_t)x,(uint16_t)y};event_pending=true;
 touch_state.contact_count=kind==RISC_TOUCH_EVENT_UP?0:1;
 touch_state.contacts[0]=(risc_touch_contact_v1){.id=1,.x=x,.y=y};
}
static bool ordered_poll(void*c,size_t n){
 const int cw=NOVA_FIXTURE_WIDTH-NOVA_FIXTURE_LEFT-NOVA_FIXTURE_RIGHT;
 const int ch=NOVA_FIXTURE_HEIGHT-NOVA_FIXTURE_TOP-NOVA_FIXTURE_BOTTOM;
 /* Independent native layout contract: 4 columns, 5 rows below one third. */
 const int key2x=4+3*(cw-8)/8, key2y=ch/3+7*(ch-ch/3-4)/10;
 const int key3x=4+5*(cw-8)/8;
 (void)c;(void)n;polls++;
 /* Header point outside native Back but inside inherited Back at width240. */
 if(polls==100)emit(1,20,42);
 if(polls==110)emit(3,20,42);
 if(polls==150){assert(!strcmp(calculator_state.text,"0"));checked++;}
 /* Horizontal/vertical gestures must not synthesize legacy navigation. */
 if(polls==200)emit(1,100,60);
 if(polls==210)emit(2,150,60);
 if(polls==220)emit(3,150,60);
 if(polls==300)emit(1,100,60);
 if(polls==310)emit(2,100,100);
 if(polls==320)emit(3,100,100);
 if(polls==360){assert(calculator_selected==0&&!strcmp(calculator_state.text,"0"));checked++;}
 /* Key2 center, held contact exposed to controller, release activates once. */
 if(polls==400)emit(1,key2x,key2y);
 if(polls==420){assert(calculator_pressed==13);checked++;}
 if(polls==440)emit(3,key2x,key2y);
 if(polls==480){assert(calculator_pressed==-1&&!strcmp(calculator_state.text,"2"));checked++;}
 /* Move off key cancels activation and pressed highlight. */
 if(polls==500)emit(1,key2x,key2y);
 if(polls==510)emit(2,key3x,key2y);
 if(polls==520)emit(3,key3x,key2y);
 if(polls==560){assert(calculator_pressed==-1&&!strcmp(calculator_state.text,"2"));checked++;}
 /* Queue gap cancels, never invents an activation from the snapshot. */
 if(polls==600)emit(1,key2x,key2y);
 if(polls==620){event_pending=false;touch_state.contact_count=0;touch_state.sequence++;}
 if(polls==660){assert(calculator_pressed==-1&&!strcmp(calculator_state.text,"2"));checked++;}
 /* Real native Back far edge x59 is outside inherited width240 Back. */
 if(polls==700)emit(1,59,20);
 if(polls==710)emit(3,59,20);
 return true;
}
static int32_t ordered_next(void*c,uint64_t n,risc_touch_event_v1*e){(void)c;(void)n;
 if(polls==620){polls++;return -1;}if(!event_pending)return 0;*e=pending_event;event_pending=false;return 1;}
static bool ordered_snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;*s=touch_state;return true;}
static const risc_touch_api_v1 ordered_touch={1,sizeof(ordered_touch),NULL,fake_sub,fake_unsub,ordered_poll,ordered_next,ordered_snapshot};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){if(!strcmp(n,"input.touch.raw")&&v==1){g->api=&ordered_touch;grants++;return true;}return fake_acquire(n,v,id,g);}
static const risc_runtime_api_v1 ordered_runtime={1,sizeof(ordered_runtime),fake_health,fake_yield,fake_diag,fake_launch,acquire,fake_release};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&ordered_runtime:NULL;}
int main(int argc,char**argv){assert(argc==2);directory=argv[1];stop_poll=1200;memset(pixels,0xa5,sizeof(pixels));assert(!app_module_init());app_main();assert(checked==6&&polls<800);app_module_fini();assert(!grants&&!frames&&!subs);puts("PASS actual adapter native Back boundaries, swipe isolation, Calculator contact/release/move/gap cancellation");}
