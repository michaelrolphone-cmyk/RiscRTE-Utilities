/* Advance input and bounded raster work independently of slow display completion.
 * next() owns touch capture and event ordering. Stop at the first event; never
 * discard or batch application actions. The presenter keeps submitted frames
 * immutable while accepting newer logical documents. */
#define OBD_SCENE_WORK_LIMIT 8u
#define OBD_SCENE_WORK_BUDGET_MS 4u
static int32_t next_scene(obd_app *a,risc_scene_event_v1 *event){
 uint32_t began;
 if(!a->rt->monotonic_ms(&began)||!authority(a))return RISC_SCENE_UNAVAILABLE;
 for(unsigned work=0;work<OBD_SCENE_WORK_LIMIT;work++){
  int32_t rc=a->scene->next(a->scene->context,a->session,event);
  if(!authority(a))return RISC_SCENE_RETAINED;
  if(rc!=RISC_SCENE_IDLE)return rc;
  risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t flags=0;
  rc=a->scene->snapshot(a->scene->context,a->session,&nav,&flags);
  if(!authority(a))return RISC_SCENE_RETAINED;
  if(rc!=RISC_SCENE_OK)return rc;
  /* snapshot consumes activity; preserve it for the resident lifecycle poll. */
  a->scene_activity|=flags&RISC_SCENE_ACTIVITY;
  if(!(flags&RISC_SCENE_PRESENTING))return RISC_SCENE_IDLE;
  uint32_t now;
  if(!a->rt->monotonic_ms(&now)||!authority(a))return RISC_SCENE_UNAVAILABLE;
  if((uint32_t)(now-began)>=OBD_SCENE_WORK_BUDGET_MS)break;
 }
 return RISC_SCENE_IDLE;
}
static bool editing_sensor_selection(const obd_app *a){
 return a->screen==CUSTOM||a->screen==GRAPH_PICK;
}
