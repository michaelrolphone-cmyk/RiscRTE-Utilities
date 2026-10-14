/* Native declaration of the owner's NOVA-7 OBD mockup. No coordinates, raster
 * font selection, simulated vehicle values or domain code in the presenter. */
static const uint32_t spans[]={30,120,600};
static const char*const span_names[]={"30 S","2 MIN","10 MIN"};
static void go(obd_app*a,unsigned screen){if(a->stack_count<8)a->stack[a->stack_count++]=a->screen;show(a,screen);}
static void back_screen(obd_app*a){show(a,a->stack_count?a->stack[--a->stack_count]:DASHBOARD);}
static void feedback(obd_app*a,const char*s){copy(a->toast,sizeof(a->toast),s);a->toast_token++;a->dirty=true;}
static void persist(obd_app*a){a->save_config=true;a->dirty=true;a->imperial=a->data.cfg.imperial!=0;}
static void title(obd_app*a,const char*t,const char*sub){copy(a->candidate.routes[0].title,40,t);copy(a->candidate.subtitle,72,sub);}
static void header_button(obd_app*a,unsigned act,unsigned icon){risc_scene_node_v1*n=node(a,RISC_COMPONENT_HEADER_ACTION,"","",act);a->candidate.details[n->id-1].symbol=icon;}
static void button(obd_app*a,const char*t,unsigned action,bool primary){risc_scene_node_v1*n=node(a,RISC_SCENE_ACTION,t,"",action);if(primary)n->flags|=RISC_SCENE_PRIMARY;}
static void toggle(obd_app*a,const char*t,const char*detail,unsigned action,bool on){risc_scene_node_v1*n=node(a,RISC_COMPONENT_SWITCH,t,detail,action);n->maximum=n->step=1;n->value=on;}
static void checked(obd_app*a,const char*t,const char*detail,unsigned action,bool on,bool disabled){risc_scene_node_v1*n=node(a,RISC_COMPONENT_CHECK_ROW,t,detail,action);n->maximum=n->step=1;n->value=on;if(disabled)n->flags|=RISC_SCENE_DISABLED;}
static void stepper(obd_app*a,const char*t,const char*v,unsigned act,unsigned value,unsigned max){risc_scene_node_v1*n=node(a,RISC_COMPONENT_STEPPER,t,v,act);n->maximum=(int32_t)max;n->value=(int32_t)value;n->step=1;}
static bool on_dashboard(obd_app*a,unsigned id){for(unsigned i=0;i<a->data.cfg.dash_count;i++)if(a->data.cfg.dash[i]==id)return true;return false;}
static bool on_graph(obd_app*a,unsigned id){for(unsigned i=0;i<a->data.cfg.graph_count;i++)if(a->data.cfg.graph[i]==id)return true;return false;}
static unsigned known_codes(obd_app*a){unsigned n=0;for(unsigned i=0;i<3;i++)if(a->elm.codes[i].status==OBD_DIAG_VALID)n+=a->elm.codes[i].count;return n;}
static void tabs(obd_app*a,unsigned selected){
 risc_scene_node_v1*n=node(a,RISC_COMPONENT_TAB_BAR,"","",ACT_TAB);n->maximum=3;n->step=1;n->value=(int32_t)selected;
 copy(a->candidate.details[n->id-1].choices,72,"LIVE|GRAPH|FAULTS|INFO");unsigned count=known_codes(a);if(count)snprintf(a->candidate.details[n->id-1].badge,16,"%u",count);
 a->candidate.flags|=RISC_COMPONENTS_ROOT_HEADER;
}
static void metric_value(obd_app*a,unsigned i,char*v,size_t size){
 if(obd_fresh(&a->elm,i,a->now))obd_value(i,a->elm.samples[i].milli,a->imperial,v,size);else copy(v,size,"--");
}
static const char*diag_state(unsigned state){return state==OBD_DIAG_IDLE?"NOT REQUESTED":state==OBD_DIAG_LOADING?"READING...":state==OBD_DIAG_UNAVAILABLE?"NOT REPORTED":state==OBD_DIAG_ERROR?"READ FAILED":"VALID";}
static void curve(obd_app*a,unsigned node_id,unsigned metric,unsigned style,unsigned count,uint32_t span,bool frozen,int32_t lo,int32_t hi){
 if(a->plot_candidate.count>=RISC_TELEMETRY_CURVES)return;
 risc_scene_curve_v1*c=&a->plot_candidate.curves[a->plot_candidate.count++];c->node=node_id;c->style=style;c->count=count;
 db_series(&a->data,metric,a->now,span,frozen,lo,hi,c->points,count);
}
static unsigned plot(obd_app*a,unsigned metric,uint32_t span,bool detail,bool frozen){
 int32_t mn=0,mx=0,av=0,lo=db_specs[metric].minimum,hi=db_specs[metric].maximum;
 if(detail&&db_stats(&a->data,metric,a->now,span,false,&mn,&mx,&av)){
  int32_t pad=(mx-mn)/5;if(pad<500)pad=500;lo=mn-pad;hi=mx+pad;
 }
 risc_scene_node_v1*n=node(a,RISC_COMPONENT_PLOT,"","",0);n->value=(int32_t)span;
 if(detail){
  char high[20],low[20];obd_value(metric,hi,a->imperial,high,sizeof(high));obd_value(metric,lo,a->imperial,low,sizeof(low));snprintf(a->candidate.details[n->id-1].choices,72,"%s|%s",high,low);
  db_limit*t=&a->data.cfg.limits[metric];if(t->enabled){
   if(t->has_high&&t->high>=lo&&t->high<=hi){n->step|=1;n->maximum=(int32_t)((int64_t)(t->high-lo)*10000/(hi-lo));}
   if(t->has_low&&t->low>=lo&&t->low<=hi){n->step|=2;n->minimum=(int32_t)((int64_t)(t->low-lo)*10000/(hi-lo));}}
 }
 curve(a,n->id,metric,0,RISC_TELEMETRY_POINTS,span,frozen,lo,hi);return n->id;
}
static void economy(obd_app*a,char*out,size_t size,char*caption,size_t nc){
 copy(out,size,"--");copy(caption,nc,"ECONOMY");
 if(!obd_fresh(&a->elm,OBD_SPEED,a->now)||a->elm.samples[OBD_SPEED].milli<=3000)return;
 int32_t rate=0;
 if(obd_fresh(&a->elm,OBD_RATE,a->now))rate=a->elm.samples[OBD_RATE].milli;
 else if(obd_fresh(&a->elm,OBD_MAF,a->now)&&obd_fresh(&a->elm,OBD_FUEL_TYPE,a->now)&&a->elm.samples[OBD_FUEL_TYPE].milli==1000){rate=(int32_t)((int64_t)a->elm.samples[OBD_MAF].milli*36000/109515);copy(caption,nc,"ECONOMY EST");}
 if(rate<=0)return;int32_t l100=(int32_t)((int64_t)rate*100000/a->elm.samples[OBD_SPEED].milli);if(l100<=0)return;
 int32_t v=a->imperial?235214583/l100:l100;
 snprintf(out,size,"%ld.%ld",(long)(v/1000),(long)(v/100%10));copy(caption,nc,a->imperial?(strstr(caption,"EST")?"MPG (EST)":"ECONOMY MPG"):(strstr(caption,"EST")?"L/100 (EST)":"ECONOMY L/100"));
}
static void live_page(obd_app*a){
 char link[72];snprintf(link,sizeof(link),"BLE|%s",a->elm.state==OBD_LIVE?obd_protocol_name(a->elm.protocol):"WAITING");title(a,"LIVE",link);
 header_button(a,ACT_SETTINGS,RISC_SYMBOL_GEAR);header_button(a,ACT_CUSTOM,RISC_SYMBOL_SLIDERS);
 char speed[24],eco[24],caption[24],rpm[24];metric_value(a,OBD_SPEED,speed,sizeof(speed));
 if(strcmp(speed,"--")&&a->elm.samples[OBD_SPEED].milli>=0)snprintf(speed,sizeof(speed),"%03ld",(long)(obd_convert(OBD_SPEED,a->elm.samples[OBD_SPEED].milli,a->imperial)/1000));
 economy(a,eco,sizeof(eco),caption,sizeof(caption));risc_scene_node_v1*n=node(a,RISC_COMPONENT_INSTRUMENT,speed,obd_unit(OBD_SPEED,a->imperial),0);
 snprintf(a->candidate.details[n->id-1].choices,72,"GEAR (EST)|--|%s|%s",caption,eco);
 if(a->reconnecting){a->candidate.details[n->id-1].tone=RISC_TONE_DANGER;copy(n->text,72,"LINK LOST / RECONNECTING...");}
 metric_value(a,OBD_RPM,rpm,sizeof(rpm));char rpm_value[40];snprintf(rpm_value,sizeof(rpm_value),"%s RPM",rpm);n=node(a,RISC_COMPONENT_BAR_GAUGE,"ENGINE SPEED",rpm_value,0);n->maximum=8000;
 if(obd_fresh(&a->elm,OBD_RPM,a->now)){n->value=a->elm.samples[OBD_RPM].milli/1000;if(n->value>8000)n->value=8000;}
 for(unsigned j=0;j<a->data.cfg.dash_count;j++){
  unsigned i=a->data.cfg.dash[j];char value[24];metric_value(a,i,value,sizeof(value));n=node(a,RISC_COMPONENT_METRIC_TILE,db_specs[i].short_name,value,ACT_PID+i);
  risc_component_detail_v1*d=&a->candidate.details[n->id-1];copy(d->badge,16,obd_unit(i,a->imperial));
  if(!obd_fresh(&a->elm,i,a->now))d->tone=RISC_TONE_MUTED;else if(db_warning(&a->data,&a->elm,i,a->now))d->tone=RISC_TONE_DANGER;
  curve(a,n->id,i,0,48,48,false,db_specs[i].minimum,db_specs[i].maximum);
 }
 tabs(a,0);
 if(a->elm.state!=OBD_LIVE&&!a->reconnecting){
  /* No demo zeroes while connecting. The real setup stage is visible. */
  a->candidate.toast_token=a->elm.stage+1;copy(a->candidate.toast,40,a->elm.message);
 }
}
static void graph_page(obd_app*a){
 title(a,"GRAPH",a->data.paused?"FROZEN / POLLING CONTINUES":"SCALED TO EACH SENSOR RANGE");header_button(a,ACT_GRAPH_PAUSE,a->data.paused?RISC_SYMBOL_PLAY:RISC_SYMBOL_PAUSE);header_button(a,ACT_GRAPH_ADD,RISC_SYMBOL_PLUS);
 row(a,"MOTION HISTORY","Velocity / acceleration / jerk",ACT_MOTION);
 for(unsigned i=0;i<a->data.cfg.graph_count;i++){unsigned k=a->data.cfg.graph[i];char text[40];snprintf(text,sizeof(text),"%s x",db_specs[k].short_name);node(a,RISC_COMPONENT_CHIP,text,"",ACT_GRAPH_SENSOR+k);}
 unsigned p=0;
 for(unsigned i=0;i<a->data.cfg.graph_count;i++){unsigned k=a->data.cfg.graph[i];if(!i)p=plot(a,k,spans[a->graph_span],false,a->data.paused);else curve(a,p,k,i,RISC_TELEMETRY_POINTS,spans[a->graph_span],a->data.paused,db_specs[k].minimum,db_specs[k].maximum);}
 
 for(unsigned i=0;i<a->data.cfg.graph_count;i++){
  unsigned k=a->data.cfg.graph[i];int32_t mn,mx,av;char value[24],low[20]="--",high[20]="--",text[72];metric_value(a,k,value,sizeof(value));
  if(db_stats(&a->data,k,a->now,spans[a->graph_span],a->data.paused,&mn,&mx,&av)){obd_value(k,mn,a->imperial,low,sizeof(low));obd_value(k,mx,a->imperial,high,sizeof(high));if(a->data.paused){int32_t last;if(db_last(&a->data,k,a->now,true,&last))obd_value(k,last,a->imperial,value,sizeof(value));else copy(value,sizeof(value),"--");}}
  snprintf(text,sizeof(text),"%s-%s",low,high);risc_scene_node_v1*n=node(a,RISC_COMPONENT_TRACE_LEGEND,db_specs[k].short_name,value,ACT_PID+k);n->value=(int)i;copy(a->candidate.details[n->id-1].badge,16,obd_unit(k,a->imperial));copy(a->candidate.details[n->id-1].choices,72,text);
 }
 stepper(a,"SPAN",span_names[a->graph_span],ACT_GRAPH_SPAN,a->graph_span,2);tabs(a,1);
}
static void motion_value(obd_app*a,unsigned k,int32_t v,char*out,size_t n){
 if(!k){obd_value(OBD_SPEED,(int32_t)((int64_t)v*18/5),a->imperial,out,n);return;}
 uint32_t magnitude=v<0?(uint32_t)(-(int64_t)v):(uint32_t)v;
 snprintf(out,n,"%s%lu.%03lu",v<0?"-":"",(unsigned long)(magnitude/1000),(unsigned long)(magnitude%1000));
}
static void motion_page(obd_app*a){
 title(a,"MOTION HISTORY","ECU SPEED / DERIVED / GAPS = NO DATA");
 const char*names[]={"VELOCITY","ACCELERATION","JERK"};
 const char*units[]={obd_unit(OBD_SPEED,a->imperial),"m/s^2","m/s^3"};
 for(unsigned k=0;k<3;k++){
  row(a,names[k],units[k],0);
  risc_scene_node_v1*n=node(a,RISC_COMPONENT_PLOT,"","",0);n->value=(int32_t)spans[a->graph_span];
  risc_scene_curve_v1*c=&a->plot_candidate.curves[a->plot_candidate.count++];c->node=n->id;c->count=RISC_TELEMETRY_POINTS;
  int32_t lo,hi;db_motion_series(&a->data,k,a->now,spans[a->graph_span],c->points,c->count,&lo,&hi);
  char high[24],low[24];motion_value(a,k,hi,high,sizeof(high));motion_value(a,k,lo,low,sizeof(low));
  snprintf(a->candidate.details[n->id-1].choices,72,"%s|%s",high,low);
 }
 stepper(a,"SPAN",span_names[a->graph_span],ACT_GRAPH_SPAN,a->graph_span,2);
}
static void detail_page(obd_app*a){
 unsigned k=a->selected_metric;char sub[32],value[32],units[64];snprintf(sub,sizeof(sub),"PID 01 %02X",obd_specs[k].pid);title(a,obd_specs[k].name,sub);
 metric_value(a,k,value,sizeof(value));snprintf(units,sizeof(units),"%s / %s",obd_unit(k,a->imperial),!a->elm.samples[k].supported?"NOT REPORTED":!obd_fresh(&a->elm,k,a->now)?"STALE / WAITING":db_warning(&a->data,&a->elm,k,a->now)?"OUTSIDE LIMIT":"NOW");node(a,RISC_COMPONENT_VALUE_HERO,value,units,0);
 plot(a,k,spans[a->detail_span],true,false);int32_t mn,mx,av;char min[20]="--",max[20]="--",avg[20]="--",values[72];
 if(db_stats(&a->data,k,a->now,spans[a->detail_span],false,&mn,&mx,&av)){obd_value(k,mn,a->imperial,min,sizeof(min));obd_value(k,mx,a->imperial,max,sizeof(max));obd_value(k,av,a->imperial,avg,sizeof(avg));}
 snprintf(values,sizeof(values),"%s|%s|%s",min,avg,max);risc_scene_node_v1*n=node(a,RISC_COMPONENT_STAT_TRIPLE,"",values,0);copy(a->candidate.details[n->id-1].choices,72,"MIN|AVG|MAX");
 stepper(a,"SPAN",span_names[a->detail_span],ACT_DETAIL_SPAN,a->detail_span,2);node(a,RISC_COMPONENT_SECTION,"ALERT LIMITS","",0);
 db_limit*t=&a->data.cfg.limits[k];toggle(a,"ALERT FOR THIS SENSOR","",ACT_THRESHOLD,t->enabled);
 for(unsigned i=0;i<2;i++){
  bool has=i?t->has_low:t->has_high;int32_t v=i?t->low:t->high;char vtext[40]="OFF";
  if(has){obd_value(k,v,a->imperial,value,sizeof(value));snprintf(vtext,sizeof(vtext),"%s %s",value,obd_unit(k,a->imperial));}
  unsigned max_index=(unsigned)((db_specs[k].maximum-db_specs[k].minimum)/db_specs[k].step)+1,index=has?(unsigned)((v-db_specs[k].minimum)/db_specs[k].step)+1:0;
  stepper(a,i?"ALERT BELOW":"ALERT ABOVE",vtext,i?ACT_LIMIT_LOW:ACT_LIMIT_HIGH,index,max_index);
 }
 if(db_specs[k].selectable)checked(a,"SHOW ON LIVE DASHBOARD","",ACT_ON_DASH+k,on_dashboard(a,k),!a->elm.samples[k].supported&&!on_dashboard(a,k));
}
static const struct {uint16_t code;unsigned priority;const char*description,*causes[3];} definitions[]={
 {0x0300,2,"RANDOM / MULTIPLE CYLINDER MISFIRE",{"Worn spark plugs or coils","Vacuum leak","Low fuel pressure"}},
 {0x0301,2,"CYLINDER 1 MISFIRE DETECTED",{"Spark plug or ignition coil, cylinder 1","Fuel injector, cylinder 1","Low compression"}},
 {0x0171,1,"SYSTEM TOO LEAN (BANK 1)",{"Vacuum or intake leak","Dirty MAF sensor","Weak fuel pump or clogged filter"}},
 {0x0420,1,"CATALYST EFFICIENCY BELOW THRESHOLD B1",{"Aging catalytic converter","Exhaust leak before the sensor","Faulty downstream O2 sensor"}},
 {0x0128,0,"COOLANT BELOW THERMOSTAT REGULATION",{"Thermostat stuck open","Coolant temperature sensor","Low coolant level"}},
 {0x0442,0,"EVAP SYSTEM SMALL LEAK DETECTED",{"Loose or worn fuel cap","Cracked EVAP hose","Purge or vent valve"}},
 {0x0562,1,"SYSTEM VOLTAGE LOW",{"Failing alternator","Weak battery","Corroded terminals"}}
};
static int definition(uint16_t code){for(unsigned i=0;i<sizeof(definitions)/sizeof(definitions[0]);i++)if(code==definitions[i].code)return (int)i;return -1;}
static void faults_page(obd_app*a){
 title(a,"FAULTS","SELECTED ECU / GENERIC OBD-II");header_button(a,ACT_RESCAN,RISC_SYMBOL_REFRESH);
 char counts[3][8],status[72];for(unsigned i=0;i<3;i++){if(a->elm.codes[i].status==OBD_DIAG_VALID)snprintf(counts[i],8,"%u",a->elm.codes[i].count);else copy(counts[i],8,"--");}snprintf(status,sizeof(status),"%s STORED / %s PENDING / %s PERMANENT",counts[0],counts[1],counts[2]);
 bool valid=a->elm.readiness_valid&&(uint32_t)(a->now-a->elm.readiness_at)<30000&&a->elm.state==OBD_LIVE;
 risc_scene_node_v1*mil=node(a,RISC_COMPONENT_CARD,valid?(a->elm.readiness[0]&128?"CHECK ENGINE ON":"CHECK ENGINE OFF"):"MIL STATUS UNAVAILABLE",status,0);a->candidate.details[mil->id-1].symbol=RISC_SYMBOL_WARNING;if(valid&&(a->elm.readiness[0]&128))mil->flags|=RISC_SCENE_PRIMARY;
 segments(a,"STORED|PENDING|PERMANENT",a->codes_tab,2,ACT_RESCAN+1000);
 const obd_codes*c=&a->elm.codes[a->codes_tab];
 if(c->status!=OBD_DIAG_VALID)node(a,RISC_COMPONENT_EMPTY,diag_state(c->status),"Rescan queries the selected ECU; no success is assumed.",0);
 else if(!c->count)node(a,RISC_COMPONENT_EMPTY,"NO CODES REPORTED","The selected ECU returned an empty list.",0);
 else for(unsigned i=0;i<c->count;i++){
  char code[6];obd_code_text(c->codes[i],code);int d=definition(c->codes[i]);risc_scene_node_v1*n=node(a,RISC_COMPONENT_CODE_ROW,code,d>=0?definitions[d].description:"DESCRIPTION NOT IN LOCAL DICTIONARY",ACT_DTC+i);
  const char*priorities[]={"NOTICE","ATTENTION","URGENT"};const char*kinds[]={"STORED","PENDING","PERMANENT"};snprintf(a->candidate.details[n->id-1].choices,72,"%s / %s",d>=0?priorities[definitions[d].priority]:"UNCLASSIFIED",kinds[a->codes_tab]);
 }
 if(a->elm.clear_status!=OBD_DIAG_IDLE)row(a,"CLEAR REQUEST",a->elm.clear_status==OBD_DIAG_VALID?"ECU acknowledged / rescanning":a->elm.clear_status==OBD_DIAG_LOADING?"Waiting for ECU acknowledgement":"Failed or cancelled; check adapter log",0);
 if(c->truncated)node(a,RISC_SCENE_TEXT_NODE,"LIST TRUNCATED","More than 32 codes were reported by this ECU.",0);
 row(a,"READINESS","EMISSIONS MONITORS",ACT_READINESS);button(a,"CLEAR CODES & TURN OFF LIGHT",ACT_CLEAR_ASK,false);tabs(a,2);
}
static void dtc_page(obd_app*a){
 char code[6];obd_code_text(a->selected_code,code);title(a,code,a->codes_tab==0?"STORED":a->codes_tab==1?"PENDING":"PERMANENT");int d=definition(a->selected_code);
 node(a,RISC_COMPONENT_CARD,d>=0?definitions[d].description:"DESCRIPTION NOT IN LOCAL DICTIONARY","Possible causes are examples, not a diagnosis.",0);
 if(d>=0){node(a,RISC_COMPONENT_SECTION,"COMMON CAUSES","",0);for(unsigned j=0;j<3;j++)row(a,definitions[d].causes[j],"",0);}
 node(a,RISC_COMPONENT_SECTION,"FREEZE FRAME / FRAME 0","",0);
 if(a->elm.freeze_status==OBD_DIAG_LOADING)node(a,RISC_COMPONENT_PROGRESS,"READING FREEZE FRAME","Checking the ECU's associated code first",0);
 else if(a->elm.freeze_status!=OBD_DIAG_VALID||a->elm.freeze_code!=a->selected_code)node(a,RISC_SCENE_TEXT_NODE,"NO MATCHING FREEZE FRAME","Frame 0 is unavailable or belongs to a different fault.",0);
 else for(unsigned j=0;j<8;j++){
  const unsigned fm[]={OBD_SPEED,OBD_RPM,OBD_COOLANT,OBD_LOAD,OBD_MAP,OBD_SHORT_TRIM,OBD_LONG_TRIM,OBD_INTAKE};unsigned k=fm[j];char value[24]="--",text[40];if(a->elm.freeze[k].valid)obd_value(k,a->elm.freeze[k].milli,a->imperial,value,sizeof(value));snprintf(text,sizeof(text),"%s %s",value,obd_unit(k,a->imperial));row(a,db_specs[k].short_name,text,0);
 }
}
static void readiness_page(obd_app*a){
 title(a,"READINESS","EMISSIONS MONITORS / SELECTED ECU");
 if(!a->elm.readiness_valid||a->elm.state!=OBD_LIVE||(uint32_t)(a->now-a->elm.readiness_at)>30000){node(a,RISC_COMPONENT_EMPTY,"STATUS NOT AVAILABLE","A current PID 01 reply is required.",0);return;}
 const uint8_t*b=a->elm.readiness;static const char*const continuous[]={"MISFIRE","FUEL SYSTEM","COMPONENTS"};
 for(unsigned i=0;i<3;i++){risc_scene_node_v1*n=node(a,RISC_COMPONENT_STATUS_TILE,continuous[i],"",0);n->value=!(b[1]&(1u<<i))?2:b[1]&(1u<<(i+4))?1:0;}
 static const char*const spark[]={"CATALYST","HEATED CATALYST","EVAP SYSTEM","SECONDARY AIR","A/C REFRIGERANT","O2 SENSOR","O2 HEATER","EGR / VVT"};
 static const char*const diesel[]={"NMHC CATALYST","NOX / SCR","RESERVED","BOOST PRESSURE","RESERVED","EXHAUST SENSOR","PM FILTER","EGR / VVT"};
 for(unsigned i=0;i<8;i++){if((b[1]&8)&&(i==2||i==4))continue;risc_scene_node_v1*n=node(a,RISC_COMPONENT_STATUS_TILE,(b[1]&8)?diesel[i]:spark[i],"",0);n->value=!(b[2]&(1u<<i))?2:b[3]&(1u<<i)?1:0;}
 node(a,RISC_SCENE_TEXT_NODE,"SELF-TEST STATUS","Check: complete. Hourglass: incomplete. Dash: unsupported.",0);node(a,RISC_SCENE_TEXT_NODE,"NOT AN INSPECTION CERTIFICATE","Inspection eligibility also depends on local rules.",0);
}
static void info_page(obd_app*a){
 title(a,"VEHICLE INFO","REPORTED DATA / NO DEMO VALUES");header_button(a,ACT_SETTINGS,RISC_SYMBOL_GEAR);
 row(a,"VIN",a->elm.vin_status==OBD_DIAG_VALID?a->elm.vin:diag_state(a->elm.vin_status),0);
 unsigned info_values[]={OBD_STANDARD,OBD_FUEL_TYPE,OBD_DISTANCE_CLEAR,OBD_DISTANCE_MIL,OBD_RUNTIME};
 for(unsigned j=0;j<5;j++){unsigned k=info_values[j];char value[32],text[64];metric_value(a,k,value,sizeof(value));
  if(k==OBD_FUEL_TYPE&&obd_fresh(&a->elm,k,a->now)){static const char*const fuel[]={"UNAVAILABLE","GASOLINE","METHANOL","ETHANOL","DIESEL","LPG","CNG","PROPANE","ELECTRIC"};unsigned v=(unsigned)(a->elm.samples[k].milli/1000);if(v<9)copy(value,sizeof(value),fuel[v]);}
  if(k==OBD_STANDARD&&obd_fresh(&a->elm,k,a->now)){unsigned v=(unsigned)(a->elm.samples[k].milli/1000);if(v==1)copy(value,sizeof(value),"OBD-II (CARB)");else if(v==2)copy(value,sizeof(value),"OBD (EPA)");else if(v==6)copy(value,sizeof(value),"EOBD");}
  snprintf(text,sizeof(text),"%s %s",value,obd_unit(k,a->imperial));row(a,obd_specs[k].name,text,0);
 }
 row(a,"PROTOCOL",obd_protocol_name(a->elm.protocol),0);char text[72];snprintf(text,sizeof(text),"%lX",(unsigned long)a->elm.ecu);row(a,"SELECTED ECU",a->elm.have_ecu?text:"--",0);
 unsigned supported=0;for(unsigned i=0;i<OBD_METRICS;i++)supported+=a->elm.samples[i].supported;snprintf(text,sizeof(text),"%u OF %u DISPLAYABLE",supported,OBD_METRICS);row(a,"SUPPORTED PIDS",text,0);
 node(a,RISC_COMPONENT_SECTION,"ADAPTER","",0);row(a,"MODULE",a->elm.identity,0);row(a,"BLUETOOTH NAME",a->peer_name,0);
 if(a->elm.voltage_status==OBD_DIAG_VALID)snprintf(text,sizeof(text),"%ld.%ld V / ATRV",(long)(a->elm.adapter_mv/1000),(long)(a->elm.adapter_mv/100%10));else copy(text,sizeof(text),diag_state(a->elm.voltage_status));row(a,"INPUT VOLTAGE",text,0);
 row(a,"LINK",a->elm.state==OBD_LIVE?"ECU READY":a->status.message,0);
 snprintf(text,sizeof(text),"%lu MS / %lu REPLIES / %lu ERRORS",(unsigned long)a->elm.rtt_ms,(unsigned long)a->elm.replies,(unsigned long)a->elm.failed_requests);row(a,"RESPONSE",text,0);
 row(a,"ADAPTER LOG","RAW AT / OBD CONVERSATION",ACT_LOG);tabs(a,3);
}
static void declare(obd_app*a){
 memset(&a->candidate,0,sizeof(a->candidate));memset(&a->plot_candidate,0,sizeof(a->plot_candidate));a->plot_candidate.struct_size=sizeof(a->plot_candidate);
 a->candidate.api_version=1;a->candidate.struct_size=sizeof(a->candidate);a->candidate.revision=a->revision;
 a->candidate.root=1;a->candidate.route_count=1;a->candidate.screen_key=1+a->screen*100+a->selected_metric;
 a->candidate.flags=RISC_COMPONENTS_DOCUMENT|RISC_COMPONENTS_TELEMETRY;a->candidate.routes[0].id=1;a->candidate.routes[0].back_action=ACT_BACK;
 a->candidate.toast_token=a->toast_token;copy(a->candidate.toast,40,a->toast);
 title(a,"OBD-II MODULE","ELM327 BLUETOOTH LE");
 if(a->upgrade_required){
  a->candidate.flags=RISC_COMPONENTS_DOCUMENT|RISC_COMPONENTS_ROOT_HEADER;
  title(a,"UPDATE FIRMWARE","TELEMETRY SCENE PROVIDER REQUIRED");
  node(a,RISC_COMPONENT_EMPTY,"PROVIDER UPDATE NEEDED","Install the paired firmware for OBD Dashboard 0.2.1.",0);
  button(a,"CLOSE",ACT_EXIT,true);return;
 }
 if(a->reload_required){
  a->candidate.flags|=RISC_COMPONENTS_ROOT_HEADER;
  title(a,"RESTART REQUIRED","BLUETOOTH PROVIDER RELOAD REQUIRED");
  risc_scene_node_v1*n=node(a,RISC_COMPONENT_EMPTY,"BLUETOOTH STOPPED",a->status.message,0);
  a->candidate.details[n->id-1].symbol=RISC_SYMBOL_WARNING;
  node(a,RISC_SCENE_TEXT_NODE,"RESTART DEVICE","Close OBD, then restart the device before reconnecting.",0);
  button(a,"CLOSE",ACT_EXIT,true);return;
 }
 if(a->screen==INTRO){
  a->candidate.flags|=RISC_COMPONENTS_ROOT_HEADER;node(a,RISC_COMPONENT_EMPTY,"NO MODULE CONNECTED","Plug in the adapter and switch ignition on, then search.",0);
  row(a,"PLUG INTO THE OBD-II PORT","",0);row(a,"IGNITION ON (ENGINE OPTIONAL)","",0);row(a,"SELECT YOUR BLE MODULE IN THIS APP","No Classic SPP / no PIN assumed",0);
  button(a,"SEARCH FOR MODULE",ACT_SCAN,true);
 }else if(a->screen==PEERS||a->screen==CHANNELS){
  title(a,a->screen==PEERS?"SEARCHING...":"SERIAL CHANNEL","SELECT THE ACTUAL ADVERTISED DEVICE");
  if(a->screen==PEERS&&(a->status.state==RISC_SERIAL_SCANNING||a->status.state==RISC_SERIAL_STARTING)){risc_scene_node_v1*n=node(a,RISC_COMPONENT_PROGRESS,"SEARCHING",a->status.message,0);n->value=(a->now/1000)%3;}
  unsigned count=a->screen==PEERS?a->status.peer_count:a->status.channel_count;
  for(unsigned i=0;i<8&&a->first+i<count;i++){
   if(a->screen==PEERS){risc_serial_peer_v1*p=&a->peer_rows[i];p->struct_size=sizeof(*p);if(!a->ble->peer(a->ble->context,a->token,a->first+i,p)||!authority(a))break;
    char text[72];snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X / %d dBm",p->address[5],p->address[4],p->address[3],p->address[2],p->address[1],p->address[0],p->rssi);risc_scene_node_v1*n=node(a,RISC_COMPONENT_ROW,p->name[0]?p->name:"UNNAMED BLE DEVICE",text,ACT_PEER+i);if(!p->connectable)n->flags|=RISC_SCENE_DISABLED;}
   else{risc_serial_channel_v1*p=&a->channel_rows[i];p->struct_size=sizeof(*p);if(!a->ble->channel(a->ble->context,a->token,a->first+i,p)||!authority(a))break;char text[72];snprintf(text,sizeof(text),"TX %04X / RX %04X | %.20s / %.20s",p->tx_handle,p->rx_handle,p->tx_uuid,p->rx_uuid);row(a,p->service,text,ACT_CHANNEL+i);}
  }
  if(!count&&a->status.state==RISC_SERIAL_SCAN_DONE)node(a,RISC_COMPONENT_EMPTY,"NO BLE DEVICES","Try again near the adapter; close other OBD apps.",0);
  if(a->first)row(a,"PREVIOUS","",ACT_PREVIOUS);if(a->first+8<count)row(a,"MORE","",ACT_MORE);
  button(a,"CANCEL",ACT_DISCONNECT,false);
 }else if(a->screen==SELECTED){
  title(a,"FOUND",a->peer_name);char detail[72];snprintf(detail,sizeof(detail),"%d dBm / Bluetooth LE",a->selected_peer.rssi);row(a,a->peer_name,detail,0);button(a,"CONNECT",ACT_CONNECT,true);
 }else if(a->screen==CONNECTING){
  title(a,"CONNECTING",a->peer_name);
  const char*steps[]={"BLUETOOTH LINK","ADAPTER RESET / ATZ","ECHO & LINE SETUP / HEADERS ON","VEHICLE PROTOCOL SEARCH","ECU HANDSHAKE / SUPPORTED PIDS","READING VEHICLE INFO / VIN"};
  unsigned stage=!a->elm_started?0:a->elm.stage<1?1:a->elm.stage<5?2:a->elm.stage<6?3:a->elm.stage<10?4:5;
  for(unsigned i=0;i<6;i++){risc_scene_node_v1*n=node(a,RISC_COMPONENT_STAT_ROW,steps[i],i==stage?a->elm.message:"",0);a->candidate.details[n->id-1].symbol=i<stage?RISC_SYMBOL_CHECK:RISC_SYMBOL_CLOCK;}
  risc_scene_node_v1*n=node(a,RISC_COMPONENT_PROGRESS,"","",0);n->maximum=6;n->value=(int)stage;
  button(a,"CANCEL",ACT_DISCONNECT,false);
 }else if(a->screen==NOTICE){node(a,RISC_COMPONENT_EMPTY,"NOT CONNECTED",a->notice,0);button(a,"SEARCH AGAIN",ACT_SCAN,true);button(a,"CLOSE",ACT_EXIT,false);}
 else if(a->screen==DASHBOARD)live_page(a);
 else if(a->screen==GRAPH)graph_page(a);
 else if(a->screen==MOTION)motion_page(a);
 else if(a->screen==DETAIL)detail_page(a);
 else if(a->screen==FAULTS)faults_page(a);
 else if(a->screen==DTC_DETAIL)dtc_page(a);
 else if(a->screen==READINESS)readiness_page(a);
 else if(a->screen==INFO)info_page(a);
 else if(a->screen==CUSTOM||a->screen==GRAPH_PICK){
  bool custom=a->screen==CUSTOM;char text[64];snprintf(text,sizeof(text),"%u OF %u SHOWN",custom?a->data.cfg.dash_count:a->data.cfg.graph_count,custom?8:3);title(a,custom?"DASHBOARD SENSORS":"ADD TO GRAPH",text);
  for(unsigned i=0;i<OBD_METRICS;i++)if(custom?db_specs[i].selectable:(db_specs[i].selectable||i==OBD_RPM||i==OBD_SPEED)){
   snprintf(text,sizeof(text),"PID %02X%s",obd_specs[i].pid,a->elm.samples[i].supported?"":" / NOT REPORTED BY THIS VEHICLE");checked(a,obd_specs[i].name,text,(custom?ACT_ON_DASH:ACT_GRAPH_SENSOR)+i,custom?on_dashboard(a,i):on_graph(a,i),!a->elm.samples[i].supported&&!(custom?on_dashboard(a,i):on_graph(a,i)));
  }
  node(a,RISC_SCENE_TEXT_NODE,custom?"SPEED AND RPM ALWAYS SHOWN":"MAXIMUM THREE SERIES","Fewer selected sensors allow faster adapter polling.",0);
 }else if(a->screen==ADLOG){
  title(a,"ADAPTER LOG","ELM327 / LAST 30 LINES");
  if(!a->elm.log_count)node(a,RISC_COMPONENT_EMPTY,"NO TRAFFIC YET","The log records actual commands and received lines.",0);
  for(unsigned i=0;i<a->elm.log_count;i++){unsigned at=(a->elm.log_head+OBD_LOG_LINES-a->elm.log_count+i)%OBD_LOG_LINES;node(a,RISC_COMPONENT_LOG_LINE,"",a->elm.logs[at],0);}
 }else if(a->screen==SETTINGS){
  title(a,"SETTINGS","DASHBOARD & ADAPTER");segments(a,"KM/H|MPH",a->data.cfg.imperial,1,ACT_UNITS);
  if(a->telemetry_api&&a->telemetry_api->telemetry_version>=2)toggle(a,"LANDSCAPE","This session",ACT_LANDSCAPE,a->landscape);
  else row(a,"LANDSCAPE","Requires updated scene provider",0);
  stepper(a,"SCREEN UPDATE EVERY",a->data.cfg.update_ms==1000?"1 S":a->data.cfg.update_ms==2000?"2 S":"5 S",ACT_UPDATE,a->data.cfg.update_ms==1000?0:a->data.cfg.update_ms==2000?1:2,2);
  static const unsigned ps[]={0,6,7,8,9,5,4,3};unsigned pi=0;for(unsigned i=0;i<8;i++)if(ps[i]==a->data.cfg.protocol)pi=i;stepper(a,"PROTOCOL",obd_protocol_name(a->data.cfg.protocol),ACT_PROTOCOL,pi,7);
  toggle(a,"THRESHOLD ALERTS","",ACT_ALERT_SWITCH,a->data.cfg.alerts);char text[72];snprintf(text,sizeof(text),"%u THIS DRIVE",a->data.alert_count);row(a,"ALERT HISTORY",text,ACT_ALERTS);
  snprintf(text,sizeof(text),"%s / %lu BYTES",a->log_file?"RECORDING":"OFF",(unsigned long)a->log_bytes);toggle(a,"LOG TRIP TO FILE",text,ACT_LOG_TRIP,a->log_file!=0);
  row(a,"ADAPTER LOG","RAW AT / OBD CONVERSATION",ACT_LOG);button(a,a->elm.paused?"RESUME POLLING":"PAUSE POLLING",ACT_POLL_PAUSE,false);
  row(a,"DISCONNECT ADAPTER","",ACT_DISCONNECT_ASK);row(a,"FORGET ADAPTER","Current session selection",ACT_FORGET);
  node(a,RISC_SCENE_TEXT_NODE,"ESTIMATES & COMPATIBILITY","Gear needs vehicle ratios. Economy estimates require gasoline.",0);
 }else if(a->screen==ALERTS){
  title(a,"ALERT HISTORY","THIS DRIVE / ELAPSED TIME");
  if(!a->data.alert_count)node(a,RISC_COMPONENT_EMPTY,"NO ALERTS","Limits use actual fresh samples while connected.",0);
  for(unsigned i=0;i<a->data.alert_count;i++){const db_alert*e=&a->data.alert_log[i];char label[40],val[24],text[72];obd_value(e->metric,e->value,a->imperial,val,sizeof(val));snprintf(label,sizeof(label),"%s %s",db_specs[e->metric].short_name,e->high?"HIGH":"LOW");snprintf(text,sizeof(text),"%s %s / +%lu S",val,obd_unit(e->metric,a->imperial),(unsigned long)((e->at_ms-a->data.started_ms)/1000));row(a,label,text,ACT_ALERT_ITEM+i);}
 }else if(a->screen==CONFIRM){
  a->candidate.flags=RISC_COMPONENTS_TELEMETRY|RISC_COMPONENTS_CONFIRM;a->candidate.cancel_action=ACT_KEEP;title(a,"CONFIRM","");
  node(a,RISC_SCENE_TEXT_NODE,a->confirm_action==ACT_CLEAR?"CLEAR ALL CODES?":a->confirm_action==ACT_FORGET?"FORGET ADAPTER?":"DISCONNECT ADAPTER?",a->confirm_action==ACT_CLEAR?"Erases freeze frames, resets readiness. Permanent codes remain.":"The current Bluetooth session will end.",0);
  button(a,"KEEP",ACT_KEEP,true);button(a,a->confirm_action==ACT_CLEAR?"CLEAR":a->confirm_action==ACT_FORGET?"FORGET":"DISCONNECT",a->confirm_action,false);a->candidate.nodes[a->candidate.node_count-1].flags|=RISC_SCENE_DESTRUCTIVE;
 }else if(a->screen==ALERT_MODAL){
  const db_alert*e=&a->data.alert;char label[40],val[20],limit[20],text[72];obd_value(e->metric,e->value,a->imperial,val,sizeof(val));obd_value(e->metric,e->limit,a->imperial,limit,sizeof(limit));snprintf(label,sizeof(label),"%s TOO %s",db_specs[e->metric].short_name,e->high?"HIGH":"LOW");snprintf(text,sizeof(text),"%s %s / LIMIT %s",val,obd_unit(e->metric,a->imperial),limit);
  a->candidate.flags=RISC_COMPONENTS_TELEMETRY|RISC_COMPONENTS_ALERT;a->candidate.cancel_action=ACT_ALERT_OK;title(a,"ALERT","");node(a,RISC_SCENE_TEXT_NODE,label,text,0);
  button(a,"OK",ACT_ALERT_OK,true);button(a,"MUTE 5 MIN",ACT_MUTE,false);button(a,"SEE GRAPH",ACT_ALERT_GRAPH,false);
 }
 if(a->landscape)a->candidate.flags|=RISC_COMPONENTS_LANDSCAPE;
}
