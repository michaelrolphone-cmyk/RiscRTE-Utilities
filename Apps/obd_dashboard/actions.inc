static void ask(obd_app*a,unsigned action_id){a->confirm_action=action_id;go(a,CONFIRM);}
static void action(obd_app*a,unsigned id,int value){
 if(id==ACT_SCAN){a->reconnecting=false;a->reconnect_attempts=0;a->stack_count=0;start_scan(a);return;}
 if(id==ACT_EXIT){a->done=true;return;}
 if(id==ACT_KEEP){a->confirm_action=0;back_screen(a);return;}
 if(id==ACT_BACK){
  if(a->screen==ALERT_MODAL){db_ack(&a->data,a->now,false);back_screen(a);return;}
  if(a->stack_count){a->confirm_action=0;back_screen(a);return;}
  if(a->screen==INTRO){a->done=true;return;}
  a->reconnecting=false;if(stop_link(a,"Disconnected"))show(a,INTRO);return;
 }
 if(id==ACT_DISCONNECT_ASK){ask(a,ACT_DISCONNECT);return;}
 if(id==ACT_FORGET&&a->screen!=CONFIRM){ask(a,ACT_FORGET);return;}
 if(id==ACT_DISCONNECT||id==ACT_FORGET){
  if(id==ACT_FORGET&&(a->screen!=CONFIRM||a->confirm_action!=id))return;
  a->reconnecting=false;a->confirm_action=0;if(!stop_link(a,"Disconnected"))return;
  if(id==ACT_FORGET){a->have_peer=false;memset(&a->selected_peer,0,sizeof(a->selected_peer));a->peer_name[0]=0;}
  a->stack_count=0;show(a,INTRO);return;
 }
 if(id==ACT_UNITS&&value>=0&&value<=1){a->data.cfg.imperial=(unsigned)value;persist(a);return;}
 if(id==ACT_UPDATE&&value>=0&&value<=2){static const unsigned ms[]={1000,2000,5000};a->data.cfg.update_ms=ms[value];persist(a);return;}
 if(id==ACT_PROTOCOL&&value>=0&&value<=7){
  static const unsigned protocols[]={0,6,7,8,9,5,4,3};a->data.cfg.protocol=protocols[value];persist(a);
  feedback(a,"Protocol takes effect on next connection");return;
 }
 if(id==ACT_TAB&&value>=0&&value<=3){static const unsigned screens[]={DASHBOARD,GRAPH,FAULTS,INFO};a->tab=(unsigned)value;a->stack_count=0;a->data.paused=false;show(a,screens[value]);return;}
 if(id==ACT_POLL_PAUSE||id==ACT_PAUSE){a->elm.paused=!a->elm.paused;a->dirty=true;return;}
 if(id==ACT_SETTINGS){go(a,SETTINGS);return;}if(id==ACT_CUSTOM){go(a,CUSTOM);return;}
 if(id==ACT_GRAPH_ADD){go(a,GRAPH_PICK);return;}
 if(id==ACT_GRAPH_PAUSE){db_freeze(&a->data,a->now);a->dirty=true;return;}
 if(id==ACT_GRAPH_SPAN&&value>=0&&value<=2){a->graph_span=(unsigned)value;a->dirty=true;return;}
 if(id==ACT_DETAIL_SPAN&&value>=0&&value<=2){a->detail_span=(unsigned)value;a->dirty=true;return;}
 if(id==ACT_THRESHOLD){a->data.cfg.limits[a->selected_metric].enabled=value!=0;persist(a);return;}
 if(id==ACT_LIMIT_HIGH||id==ACT_LIMIT_LOW){
  unsigned i=a->selected_metric;db_limit next=a->data.cfg.limits[i];int64_t v=(int64_t)db_specs[i].minimum+(int64_t)(value-1)*db_specs[i].step;
  if(value<0||(value&&v>db_specs[i].maximum))return;
  if(id==ACT_LIMIT_HIGH){next.has_high=value!=0;if(value)next.high=(int32_t)v;}else{next.has_low=value!=0;if(value)next.low=(int32_t)v;}
  if(next.has_high&&next.has_low&&next.low>=next.high){feedback(a,"Low limit must be below high limit");return;}
  next.enabled=1;a->data.cfg.limits[i]=next;persist(a);return;
 }
 if(id==ACT_RESCAN){obd_request_faults(&a->elm);a->dirty=true;return;}
 if(id==ACT_RESCAN+1000&&value>=0&&value<=2){a->codes_tab=(unsigned)value;a->dirty=true;return;}
 if(id==ACT_READINESS){a->elm.samples[OBD_MIL].due_ms=0;go(a,READINESS);return;}
 if(id==ACT_LOG){go(a,ADLOG);return;}if(id==ACT_ALERTS){go(a,ALERTS);return;}
 if(id==ACT_ALERT_SWITCH){a->data.cfg.alerts=value!=0;persist(a);return;}
 if(id==ACT_LOG_TRIP){if(value&&!a->log_file)(void)log_start(a);else if(!value)(void)log_close(a);a->dirty=true;return;}
 if(id==ACT_CLEAR_ASK){ask(a,ACT_CLEAR);return;}
 if(id==ACT_CLEAR){
  if(a->screen!=CONFIRM||a->confirm_action!=ACT_CLEAR)return;a->confirm_action=0;back_screen(a);
  if(obd_confirm_clear(&a->elm,a->now))feedback(a,"Clear requested; waiting for ECU reply");
  else feedback(a,"Need fresh zero speed and zero RPM");return;
 }
 if(id==ACT_ALERT_OK||id==ACT_MUTE||id==ACT_ALERT_GRAPH){
  if(a->screen!=ALERT_MODAL)return;unsigned i=a->data.alert.metric;db_ack(&a->data,a->now,id==ACT_MUTE);back_screen(a);
  if(id==ACT_ALERT_GRAPH){a->selected_metric=i;go(a,DETAIL);}return;
 }
 if(id>=ACT_PID&&id<ACT_PID+OBD_METRICS){a->selected_metric=id-ACT_PID;go(a,DETAIL);return;}
 if(id>=ACT_ON_DASH&&id<ACT_ON_DASH+OBD_METRICS){
  unsigned k=id-ACT_ON_DASH;if(!db_specs[k].selectable||!a->elm.samples[k].supported)return;
  unsigned at=0;while(at<a->data.cfg.dash_count&&a->data.cfg.dash[at]!=k)at++;
  if(at<a->data.cfg.dash_count){for(unsigned j=at+1;j<a->data.cfg.dash_count;j++)a->data.cfg.dash[j-1]=a->data.cfg.dash[j];a->data.cfg.dash[--a->data.cfg.dash_count]=0;}
  else if(a->data.cfg.dash_count<8)a->data.cfg.dash[a->data.cfg.dash_count++]=k;else{feedback(a,"Max 8 sensors; remove one first");return;}
  persist(a);return;
 }
 if(id>=ACT_GRAPH_SENSOR&&id<ACT_GRAPH_SENSOR+OBD_METRICS){
  unsigned k=id-ACT_GRAPH_SENSOR;if(!a->elm.samples[k].supported)return;
  unsigned at=0;while(at<a->data.cfg.graph_count&&a->data.cfg.graph[at]!=k)at++;
  if(at<a->data.cfg.graph_count){if(a->data.cfg.graph_count==1){feedback(a,"Keep at least one graph sensor");return;}for(unsigned j=at+1;j<a->data.cfg.graph_count;j++)a->data.cfg.graph[j-1]=a->data.cfg.graph[j];a->data.cfg.graph[--a->data.cfg.graph_count]=0;}
  else if(a->data.cfg.graph_count<3)a->data.cfg.graph[a->data.cfg.graph_count++]=k;else{feedback(a,"Max 3 sensors on a graph");return;}
  a->data.paused=false;persist(a);return;
 }
 if(id>=ACT_DTC&&id<ACT_DTC+OBD_DTC_MAX&&a->screen==FAULTS){
  unsigned at=id-ACT_DTC;const obd_codes*c=&a->elm.codes[a->codes_tab];if(c->status!=OBD_DIAG_VALID||at>=c->count)return;
  a->selected_code=c->codes[at];obd_request_freeze(&a->elm,a->selected_code);go(a,DTC_DETAIL);return;
 }
 if(id>=ACT_ALERT_ITEM&&id<ACT_ALERT_ITEM+DB_ALERTS){unsigned at=id-ACT_ALERT_ITEM;if(at<a->data.alert_count){a->selected_metric=a->data.alert_log[at].metric;go(a,DETAIL);}return;}
 if(id==ACT_MORE){a->first+=8;a->dirty=true;return;}
 if(id==ACT_PREVIOUS){if(a->first>=8)a->first-=8;a->dirty=true;return;}
 if(id>=ACT_PEER&&id<ACT_PEER+8&&a->screen==PEERS){
  a->selected_peer=a->peer_rows[id-ACT_PEER];a->have_peer=true;copy(a->peer_name,sizeof(a->peer_name),a->selected_peer.name[0]?a->selected_peer.name:"BLE adapter");go(a,SELECTED);return;
 }
 if(id==ACT_CONNECT&&a->screen==SELECTED&&a->have_peer){
  bool ok=a->ble->connect(a->ble->context,a->token,&a->selected_peer);if(!authority(a)){retain_app(a);return;}
  if(!ok){if(stop_link(a,"Connect failed"))notice(a,"Connection failed. Search and select again.");return;}
  a->stack_count=0;show(a,CONNECTING);read_status(a);return;
 }
 if(id>=ACT_CHANNEL&&id<ACT_CHANNEL+8&&a->screen==CHANNELS){
  bool ok=a->ble->select_channel(a->ble->context,a->token,a->first+id-ACT_CHANNEL);if(!authority(a)){retain_app(a);return;}
  if(!ok){if(stop_link(a,"Channel unavailable"))notice(a,"Serial channel selection failed");return;}show(a,CONNECTING);read_status(a);
 }
}
