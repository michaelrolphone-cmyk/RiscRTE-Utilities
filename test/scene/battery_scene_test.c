/* Production scene controller and real broadcast helper; semantic service doubles.
 * Includes the helper owner's exact atomic fixture, not replacement helper logic. */
#define runtime helper_rt
#define other helper_other
#define extended helper_extended
#define main broadcast_fixture_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "battery_broadcast_test.c"
#pragma GCC diagnostic pop
#undef main
#undef extended
#undef other
#undef runtime
#include "../../Apps/battery_scene.c"
static risc_scene_components_api_v1 ui;
static risc_battery_gauge_api_v1 gauge_api;
static risc_battery_sample_v1 measured;
static bool gauge_ok,no_ui,replace_read,replace_dispatch;
static unsigned opens,closes,updates,configs,polls,dispatches,navigations,rt_retains;
static uint32_t busy;
static int close_result,dispatch_result,nav_result;
static bool rt_hold(void){rt_retains++;return true;}
static void rt_yield(uint32_t ms){uptime+=ms;}
static bool gauge_read(void*c,risc_battery_sample_v1*out){(void)c;*out=measured;if(replace_read)owner=false;return gauge_ok;}
static bool scene_acquire(const char*k,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(!strcmp(k,RISC_SCENE_CAPABILITY)){if(no_ui)return false;assert(v==1&&id==0);g->api=&ui;g->slot=3;g->generation=1;return true;}
 if(!strcmp(k,RISC_BATTERY_GAUGE_CAPABILITY)){assert(v==1&&id==0);g->api=&gauge_api;g->slot=4;g->generation=1;return true;}
 return acquire(k,v,id,g);
}
static int32_t scene_open(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;(void)d;(void)n;(void)s;assert(!"component interface required");return -1;}
static int32_t scene_update(void*c,uint64_t s,const risc_scene_document_v1*d){(void)c;(void)s;(void)d;assert(!"component interface required");return -1;}
static int32_t comp_open(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*s){(void)c;assert(!session&&d->node_count>=19&&n->depth);opens++;*s=7;return 0;}
static int32_t comp_update(void*c,uint64_t s,const risc_components_document_v1*d){(void)c;assert(s==7&&d->revision>document.revision&&!busy);updates++;return 0;}
static int32_t scene_close(void*c,uint64_t s){(void)c;assert(s==7);closes++;return close_result;}
static int32_t configure(void*c,uint64_t s,uint32_t f){(void)c;(void)f;assert(s==7);configs++;return 0;}
static int32_t snap(void*c,uint64_t s,risc_scene_navigation_v1*n,uint32_t*f){(void)c;assert(s==7&&n->struct_size==sizeof(*n));*f=busy;return 0;}
static int32_t nav(void*c,uint64_t s,uint32_t op,uint32_t route){(void)c;assert(s==7);navigations++;if(nav_result)return nav_result;if(op==RISC_SCENE_ROOT)navigation.depth=1;else{assert(op==RISC_SCENE_PUSH);navigation.depth=2;navigation.routes[1]=route;}return 0;}
static int32_t next_event(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;assert(s==7);polls++;if(polls==1){busy=RISC_SCENE_INPUT_BUSY;notice[0]='X';return RISC_SCENE_IDLE;}if(polls==2){assert(!updates);busy=RISC_SCENE_PRESENTING;return RISC_SCENE_IDLE;}if(polls==3){assert(!updates);busy=0;return RISC_SCENE_IDLE;}assert(polls==4&&updates==1);*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_SUSPEND_EVENT,.document_revision=document.revision,.sequence=1};return 0;}
static int32_t dispatch(uint64_t inv,const risc_resident_request_v1*q,risc_resident_reply_v1*p){assert(inv==1&&q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS&&session==0&&closes==1);dispatches++;p->flags=0;if(replace_dispatch)owner=false;return dispatch_result;}
static bool bad_bind(risc_resident_client_v1*c){owner=false;c->api_version=0;return true;}
static bool current_status(void*c,telemetry_broadcast_status_v1*out){(void)c;*out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out),.state=TELEMETRY_BROADCAST_PAUSED,.enabled=stored&&value[2]!=0};return true;}
static void setup(void){
 reset();service.status=current_status;helper_extended.base.status=current_status;helper_rt.acquire=scene_acquire;helper_rt.retain_invocation=rt_hold;helper_rt.yield_ms=rt_yield;runtime=&helper_rt;helper_other=helper_rt;
 ui=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(ui),.open=scene_open,.update=scene_update,.next=next_event,.snapshot=snap,.close=scene_close,.navigate=nav},.configure=configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=comp_open,.update=comp_update};
 gauge_api=(risc_battery_gauge_api_v1){.api_version=1,.struct_size=sizeof(gauge_api),.read=gauge_read};scene=&ui.lifecycle.base;lifecycle=&ui.lifecycle;components=&ui;gauge=&gauge_api;
 resident=(risc_scene_resident_v1){.enabled=true,.client={.invocation=1,.checkpoint=dispatch}};terminal=false;session=7;last_event=0;read_ok=true;detail_page=0;broadcast_ready=false;requested=save_failed=false;broadcast_result=0;notice[0]=0;document=(risc_components_document_v1){0};navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={OVERVIEW}};sample=(risc_battery_sample_v1){4000,70,PORTABLE_POWER_STATUS_VALID|PORTABLE_POWER_BATTERY_PRESENT};measured=sample;gauge_ok=true;no_ui=replace_read=replace_dispatch=false;opens=closes=updates=configs=polls=dispatches=navigations=rt_retains=busy=0;close_result=dispatch_result=nav_result=0;navigation_notice[0]=0;memset(acquired,0,sizeof(acquired));declare();document=candidate;
}
static const risc_scene_node_v1 *find(unsigned id){for(unsigned i=0;i<candidate.node_count;i++)if(candidate.nodes[i].id==id)return &candidate.nodes[i];assert(0);return NULL;}
static bool event(unsigned id,unsigned action){risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.node=id,.action=action,.document_revision=document.revision,.sequence=last_event+1};return handle_event(&e);}
int main(void){
 unsigned count=0;
 setup();sample.flags=PORTABLE_POWER_THERMAL_LIMIT|PORTABLE_POWER_CHARGE_DONE;declare();assert(!strcmp(yes_no(PORTABLE_POWER_CHARGE_DONE),"Unknown")&&!strcmp(power_title(),"BATTERY STATUS"));count++;
 setup();sample.percent=100;assert(strcmp(power_title(),"CHARGE COMPLETE"));count++;
 setup();sample.flags|=PORTABLE_POWER_THERMAL_LIMIT|PORTABLE_POWER_CHARGE_DONE;assert(!strcmp(power_title(),"THERMAL LIMIT"));sample.flags&=~PORTABLE_POWER_BATTERY_PRESENT;assert(!strcmp(power_title(),"NO BATTERY"));count++;
 setup();sample.percent=10;assert(!strcmp(power_title(),"BATTERY CRITICAL"));sample.percent=20;assert(!strcmp(power_title(),"LOW BATTERY"));sample.flags|=PORTABLE_POWER_INPUT_READY;assert(!strcmp(power_title(),"CHARGE DISABLED"));count++;
 setup();gauge_ok=false;assert(refresh_status()&&!read_ok&&sample.percent==255&&sample.millivolts==0);declare();assert(!strcmp(find(5)->label,"RETRY")&&find(1)->maximum==0);count++;
 setup();assert(!event(4,OPEN_DETAILS)&&navigation.routes[1]==DETAILS);assert(!event(10,PREVIOUS)&&detail_page==3);declare();assert(strstr(find(7)->text,"PMIC")&&strstr(find(8)->text,"No cell temperature"));assert(!event(11,NEXT)&&detail_page==0);assert(!event(0,ROOT_BACK)&&navigation.depth==1);assert(event(0,LEAVE_APP));assert(!submits&&!puts_count);count++;
 setup();navigation.depth=2;navigation.routes[1]=DETAILS;for(unsigned i=0;i<4;i++)assert(!event(11,NEXT));assert(detail_page==0&&!submits&&!puts_count);count++;
 setup();lifecycle=NULL;declare();assert(strstr(find(19)->text,"AUTOMATIC REFRESH"));lifecycle=&ui.lifecycle;resident.enabled=false;declare();assert(!strcmp(find(19)->text,"SHARED CONTROLS UNAVAILABLE"));count++;
 setup();assert(!handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&closes==1&&dispatches==1&&opens==1&&last_event==0);count++;
 setup();dispatch_result=RISC_RESIDENT_RETAINED;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&rt_retains==1&&!opens);count++;
 setup();replace_dispatch=true;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&terminal&&!rt_retains&&!opens);count++;
 setup();close_result=RISC_SCENE_AGAIN;assert(handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS)&&closes==250&&rt_retains==1&&!dispatches);count++;
 setup();replace_read=true;assert(!refresh_status()&&terminal&&!rt_retains);count++;
 setup();no_ui=true;app_main();assert(!opens&&!submits&&!puts_count);count++;
 setup();missing_service=true;app_main();assert(polls==4&&updates==1&&closes==1&&!submits&&!puts_count);count++;
 setup();helper_rt.resident_shell=bad_bind;app_main();assert(terminal&&!rt_retains&&!opens);count++;
 setup();broadcast_ready=true;assert(!battery_broadcast_init(&broadcast,runtime,helper_alive,helper_retain,NULL));assert(refresh_status()&&broadcast_status.enabled);navigation.depth=2;navigation.routes[1]=BROADCAST;declare();document=candidate;assert(!event(18,TOGGLE)&&broadcast_result==3&&!requested&&submits==1);pending=2;assert(pause_broadcast()&&collects==3&&broadcast_result==0&&!broadcast_status.enabled&&!value[2]);assert(!event(18,TOGGLE)&&requested&&submits==2);assert(pause_broadcast()&&value[2]&&broadcast_status.enabled&&!puts_count);count++;
 setup();broadcast_ready=true;assert(!battery_broadcast_init(&broadcast,runtime,helper_alive,helper_retain,NULL));assert(refresh_status());navigation.depth=2;navigation.routes[1]=BROADCAST;declare();document=candidate;assert(!event(18,TOGGLE));pending=2;close_dependencies();assert(!terminal&&!broadcast_ready&&cancels==1&&collects==3&&!broadcast.edit.token&&!broadcast.preference.api);count++;
 setup();broadcast_ready=true;assert(!battery_broadcast_init(&broadcast,runtime,helper_alive,helper_retain,NULL));assert(refresh_status());navigation.depth=2;navigation.routes[1]=BROADCAST;declare();document=candidate;receipt_result=RISC_ATOMIC_UNCERTAIN;assert(!event(18,TOGGLE));assert(pause_broadcast()&&save_failed&&!requested);receipt_result=0;assert(!event(18,TOGGLE)&&!requested&&submits==2);assert(pause_broadcast()&&!value[2]);count++;
 setup();read_ok=false;assert(power_tone()==RISC_TONE_DANGER);count++;
 setup();sample.flags|=PORTABLE_POWER_THERMAL_LIMIT|PORTABLE_POWER_CHARGE_DONE;assert(power_tone()==RISC_TONE_WARNING);count++;
 setup();sample.percent=20;assert(power_tone()==RISC_TONE_WARNING);sample.percent=10;sample.flags|=RISC_BATTERY_CHARGING;assert(power_tone()==RISC_TONE_ACCENT);sample.flags&=~RISC_BATTERY_CHARGING;sample.flags|=PORTABLE_POWER_INPUT_READY;assert(power_tone()==RISC_TONE_WARNING);count++;
 setup();detail_page=3;assert(!event(4,OPEN_DETAILS)&&detail_page==0);count++;
 setup();nav_result=RISC_SCENE_AGAIN;assert(!event(4,OPEN_DETAILS)&&navigation.depth==1&&!terminal);declare();for(unsigned i=20;i<=22;i++)assert(strstr(find(i)->text,"NAVIGATION FAILED")&&!(find(i)->flags&RISC_SCENE_HIDDEN));nav_result=0;assert(!event(4,OPEN_DETAILS)&&navigation.depth==2&&!navigation_notice[0]);nav_result=RISC_SCENE_AGAIN;assert(!event(0,ROOT_BACK)&&navigation.depth==2);declare();assert(strstr(find(21)->text,"BACK FAILED"));count++;
 setup();broadcast_ready=true;assert(!battery_broadcast_init(&broadcast,runtime,helper_alive,helper_retain,NULL));assert(refresh_status()&&broadcast_status.enabled);navigation.depth=2;navigation.routes[1]=BROADCAST;declare();document=candidate;assert(!event(18,TOGGLE));assert(refresh_status()&&broadcast_status.enabled&&broadcast_result==3);assert(pause_broadcast()&&!broadcast_status.enabled);count++;
 setup();const unsigned states[]={TELEMETRY_BROADCAST_OFF,TELEMETRY_BROADCAST_BLE_OFF,TELEMETRY_BROADCAST_PAUSED,TELEMETRY_BROADCAST_STARTING,TELEMETRY_BROADCAST_LIVE,TELEMETRY_BROADCAST_RETRY,TELEMETRY_BROADCAST_RETAINED};const char*labels[]={"Disabled","Bluetooth is off","Paused","Starting","Broadcasting","Waiting / retrying","Cleanup pending"};for(unsigned i=0;i<7;i++){broadcast_status.state=states[i];assert(!strcmp(broadcast_label(),labels[i]));}count++;
 printf("Battery scene: %u production controller scenarios passed\n",count);return 0;
}
