/* Supplementary app fakes, not actual provider/Runtime proof. */
#define main original_test_main
#include "stopwatch_test.c"
#undef main
static bool acq_life(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){bool ok=acq(n,v,id,g);if(ok&&!strcmp(n,"ui.scene"))g->api=&scene_life.base;return ok;}
static int32_t controlled_next(void*c,uint64_t s,risc_scene_event_v1*e){(void)c;(void)s;polls++;mono+=100;*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.document_revision=document.revision,.sequence=polls};
 if(polls==1){e->kind=RISC_SCENE_ACTION_EVENT;e->node=3;e->action=TOGGLE;return 0;}
 if(polls==2){assert(updates==1);snapshot_flags=RISC_SCENE_INPUT_BUSY;return RISC_SCENE_IDLE;}
 if(polls==3){assert(updates==1);snapshot_flags=RISC_SCENE_PRESENTING;return RISC_SCENE_IDLE;}
 if(polls==4){assert(updates==1);snapshot_flags=0;return RISC_SCENE_IDLE;}
 assert(updates==2);e->kind=RISC_SCENE_SUSPEND_EVENT;return 0;
}
static int32_t checkpoint_fake(uint64_t inv,const risc_resident_request_v1*q,risc_resident_reply_v1*p){assert(inv==1);assert(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);assert(closes==1&&session==0);p->flags=0;mono+=200;return RISC_RESIDENT_OK;}
int main(void){
 original_test_main();setup();runtime.acquire=acq_life;scene_life.base.next=controlled_next;app_main();assert(polls==5&&configured==1&&updates==2&&releases==4&&closes==1);puts("Independent loop coalescing passed");
 setup();ui=&scene_life.base;lifecycle=&scene_life;resident.enabled=true;resident.client.invocation=1;resident.client.checkpoint=checkpoint_fake;navigation=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(navigation),.depth=1,.routes={1}};restore();toggle();declare(&document);document.revision=1;session=1;assert(!checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS));assert(session==1&&clock_state.running&&clock_state.elapsed_ms==200&&writes==1&&closes==1&&configured==1);puts("Independent resident close-dispatch-reopen passed");return 0;
}
