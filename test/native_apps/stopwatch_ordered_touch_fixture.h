/* Host-only adapter for the existing poll-indexed paper/controller scenarios.
 * Each hardware poll consumes 20 ms, retaining the scenarios' elapsed-time
 * assumptions even when the current adapter polls repeatedly during a wait.
 * Queue edges and snapshots represent exactly the same hardware reports.
 */
static risc_touch_snapshot_v1 stopwatch_touch_state;
static risc_touch_event_v1 stopwatch_touch_events[RISC_TOUCH_QUEUE_LENGTH];
static unsigned stopwatch_touch_head,stopwatch_touch_count;
static void stopwatch_touch_edge(unsigned kind,risc_touch_contact_v1 point) {
 assert(stopwatch_touch_count<RISC_TOUCH_QUEUE_LENGTH);
 stopwatch_touch_events[(stopwatch_touch_head+stopwatch_touch_count++)%RISC_TOUCH_QUEUE_LENGTH]=
  (risc_touch_event_v1){.sequence=++stopwatch_touch_state.sequence,.timestamp_ms=ticks,
   .kind=kind,.id=point.id,.x=point.x,.y=point.y};
}
static bool fake_touch_poll(void*c,size_t n) {
 (void)c;assert(n==1);polls++;ticks+=20;
 risc_touch_snapshot_v1 next={.width=paper_profile?480:240,.height=paper_profile?800:240};
 if(raw_home_at&&polls==raw_home_at)next.buttons=RISC_TOUCH_BUTTON_PRIMARY;
 for(unsigned i=0;i<action_count;i++)if(actions[i].at==polls){
  next.contact_count=1;next.contacts[0]=(risc_touch_contact_v1){.id=1,.x=actions[i].x,.y=actions[i].y};
  if(cancel_contact){next.contact_count=2;next.contacts[1]=next.contacts[0];next.contacts[1].id=2;}
 }
 if(next.buttons!=stopwatch_touch_state.buttons)
  stopwatch_touch_edge(next.buttons?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,(risc_touch_contact_v1){.id=0});
 for(unsigned i=0;i<stopwatch_touch_state.contact_count;i++){
  risc_touch_contact_v1 old=stopwatch_touch_state.contacts[i];bool found=false;
  for(unsigned j=0;j<next.contact_count;j++)if(next.contacts[j].id==old.id)found=true;
  if(!found)stopwatch_touch_edge(RISC_TOUCH_EVENT_UP,old);
 }
 for(unsigned i=0;i<next.contact_count;i++){
  risc_touch_contact_v1 point=next.contacts[i];int at=-1;
  for(unsigned j=0;j<stopwatch_touch_state.contact_count;j++)if(stopwatch_touch_state.contacts[j].id==point.id)at=(int)j;
  if(at<0)stopwatch_touch_edge(RISC_TOUCH_EVENT_DOWN,point);
  else if(stopwatch_touch_state.contacts[at].x!=point.x||stopwatch_touch_state.contacts[at].y!=point.y)
   stopwatch_touch_edge(RISC_TOUCH_EVENT_MOVE,point);
 }
 next.sequence=stopwatch_touch_state.sequence;next.timestamp_ms=ticks;stopwatch_touch_state=next;return true;
}
static int32_t fake_next(void*c,uint64_t n,risc_touch_event_v1*out) {
 (void)c;assert(n==1);if(!stopwatch_touch_count)return 0;
 *out=stopwatch_touch_events[stopwatch_touch_head];stopwatch_touch_head=(stopwatch_touch_head+1)%RISC_TOUCH_QUEUE_LENGTH;
 --stopwatch_touch_count;return 1;
}
static bool fake_snapshot(void*c,risc_touch_snapshot_v1*out) {
 (void)c;if(!stopwatch_touch_state.width){stopwatch_touch_state.width=paper_profile?480:240;stopwatch_touch_state.height=paper_profile?800:240;}
 *out=stopwatch_touch_state;return true;
}
