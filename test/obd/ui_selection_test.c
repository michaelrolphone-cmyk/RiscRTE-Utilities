/* Value requests are idempotent; selected unavailable sensors can be removed. */
#define main existing_lifecycle_main
#include "lifecycle_test.c"
#undef main
static unsigned next_calls,snap_calls;
static uint32_t test_now;
static bool budget_clock(uint32_t*out){*out=test_now;return true;}
static int32_t budget_next(void*c,uint64_t token,risc_scene_event_v1*e){(void)c;(void)token;(void)e;next_calls++;test_now++;return RISC_SCENE_IDLE;}
static int32_t budget_snapshot(void*c,uint64_t token,risc_scene_navigation_v1*n,uint32_t*f){(void)c;(void)token;(void)n;snap_calls++;*f=RISC_SCENE_PRESENTING|RISC_SCENE_ACTIVITY;return RISC_SCENE_OK;}
static int32_t event_next(void*c,uint64_t token,risc_scene_event_v1*e){(void)c;(void)token;next_calls++;e->kind=RISC_SCENE_VALUE_EVENT;return RISC_SCENE_OK;}
int main(void){
 obd_app a;init_app(&a);a.screen=CUSTOM;a.elm.samples[OBD_COOLANT].supported=true;
 assert(on_dashboard(&a,OBD_COOLANT));action(&a,ACT_ON_DASH+OBD_COOLANT,0);assert(!on_dashboard(&a,OBD_COOLANT));
 action(&a,ACT_ON_DASH+OBD_COOLANT,0);assert(!on_dashboard(&a,OBD_COOLANT));
 action(&a,ACT_ON_DASH+OBD_COOLANT,1);action(&a,ACT_ON_DASH+OBD_COOLANT,1);assert(on_dashboard(&a,OBD_COOLANT));
 puts("Repeated checkbox value requests are idempotent PASS");
 a.elm.samples[OBD_COOLANT].supported=false;declare(&a);bool found=false;
 for(unsigned i=0;i<a.candidate.node_count;i++)if(a.candidate.nodes[i].action==ACT_ON_DASH+OBD_COOLANT){assert(!(a.candidate.nodes[i].flags&RISC_SCENE_DISABLED));found=true;}
 assert(found);action(&a,ACT_ON_DASH+OBD_COOLANT,0);assert(!on_dashboard(&a,OBD_COOLANT));
 action(&a,ACT_ON_DASH+OBD_COOLANT,1);assert(!on_dashboard(&a,OBD_COOLANT));
 puts("Selected unsupported sensor can be disabled, never newly enabled PASS");
 a.screen=GRAPH_PICK;a.data.cfg.graph_count=2;a.data.cfg.graph[0]=OBD_RPM;a.data.cfg.graph[1]=OBD_COOLANT;
 action(&a,ACT_GRAPH_SENSOR+OBD_COOLANT,0);assert(!on_graph(&a,OBD_COOLANT));
 action(&a,ACT_GRAPH_SENSOR+OBD_COOLANT,0);assert(!on_graph(&a,OBD_COOLANT));
 action(&a,ACT_GRAPH_SENSOR+OBD_RPM,0);assert(on_graph(&a,OBD_RPM));
 puts("Graph checkbox idempotency and last-series protection PASS");
 risc_runtime_api_v1 r=runtime;r.monotonic_ms=budget_clock;
 risc_scene_api_v1 s={.next=budget_next,.snapshot=budget_snapshot};a.rt=&r;a.scene=&s;
 risc_scene_event_v1 e={.struct_size=sizeof(e)};
 assert(next_scene(&a,&e)==RISC_SCENE_IDLE);assert(next_calls==4&&snap_calls==4);assert(a.scene_activity&RISC_SCENE_ACTIVITY);
 next_calls=snap_calls=0;s.next=event_next;assert(next_scene(&a,&e)==RISC_SCENE_OK);assert(next_calls==1&&!snap_calls);
 puts("Raster work bounded at 4 ms between calls; activity retained; first event returned immediately PASS");
 return 0;
}
