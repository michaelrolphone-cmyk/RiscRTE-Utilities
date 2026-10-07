/* Continuous content-space settings list. Header and footer stay fixed, and
 * the same pixel offset is used to draw, clip and hit-test every row. */
#define CONTROLS_TOP 48
#define CONTROLS_BOTTOM (ui_height-46)
#define CONTROLS_ROW_HEIGHT 48
#define CONTROLS_COUNT 13
#define CONTROLS_MAX_SCROLL (CONTROLS_COUNT*CONTROLS_ROW_HEIGHT-(CONTROLS_BOTTOM-CONTROLS_TOP))
static void controls_move(int offset){int next=clamp_int(offset,0,CONTROLS_MAX_SCROLL);if(next!=controls_scroll){controls_scroll=next;dirty=true;}}
static bool controls_segment(unsigned setting){return setting==3||setting==4||setting==5;}
static void draw_setting(unsigned setting,int y){
 static const char *captions[]={"","LOW","HIGH","X SCALE","Y SCALE","LABELS","COLORS","GAIN","WINDOW","FFT SIZE","FLOOR","STORAGE","SAMPLES","EVENTS"};
 char value[24]={0};fill(12,y+47,216,1,NOVA_LINE);text(1,14,y+14,75,captions[setting],NOVA_TEXT);
 if(controls_segment(setting)){const char *a=setting==3?"LOG":setting==4?"DB":"ON",*b=setting==5?"OFF":"LIN";bool first=setting==3?prefs.log_frequency:setting==4?prefs.log_amplitude:prefs.show_labels;pill(110,y+4,55,40,a,first);pill(171,y+4,61,40,b,!first);return;}
 switch(setting){case 13:case 12:pill(110,y+4,122,40,"OPEN",false);return;case 1:snprintf(value,sizeof(value),"%u.%uM",prefs.low_hz/1000000u,prefs.low_hz%1000000u/100000u);break;case 2:snprintf(value,sizeof(value),"%u.%uM",dsp_config.high_hz/1000000u,dsp_config.high_hz%1000000u/100000u);break;case 6:snprintf(value,sizeof(value),"%s",palette_names[prefs.palette]);break;case 7:snprintf(value,sizeof(value),"%+d dB",prefs.gain_db);break;case 8:snprintf(value,sizeof(value),"%s",window_names[prefs.window]);break;case 9:snprintf(value,sizeof(value),"%u",prefs.fft_size);break;case 10:snprintf(value,sizeof(value),"%d dB",prefs.threshold_db);break;default:if(pending_save){pill(110,y+4,55,40,"RETRY",false);pill(171,y+4,61,40,storage_discard_confirm?"SURE":"DISCARD",false);}else pill(110,y+4,122,40,(signature_pending||event_files.pending>=0)?"RETRY SAVE":(store_message||signature_errors||event_files.ready!=3)?"RETRY":storage?"SAVED":"RETRY",false);return;}
 pill(90,y+4,32,40,"-",false);center(1,125,y+14,72,value,NOVA_WHITE);pill(200,y+4,32,40,"+",false);
 if(setting==6)for(unsigned x=0;x<66;x++)fill(128+(int)x,y+34,1,3,palette[x*255/65]);
}
static void draw_controls(void){
 for(unsigned row=0;row<CONTROLS_COUNT;row++){int y=CONTROLS_TOP+(int)row*CONTROLS_ROW_HEIGHT-controls_scroll;if(y+CONTROLS_ROW_HEIGHT>CONTROLS_TOP&&y<CONTROLS_BOTTOM)draw_setting(row+1,y);}
 /* Paint chrome last to clip partially visible rows and text without resizing
  * glyphs. Intersecting rows always remain inside the 240px raster surface. */
 fill(0,0,240,CONTROLS_TOP,0);fill(0,CONTROLS_BOTTOM,240,ui_height-CONTROLS_BOTTOM,0);
 text(0,12,7,216,"CONTROLS",NOVA_CYAN);char name[48];snprintf(name,sizeof(name),"RF %uM / RECEIVER SETTINGS >",sample_rate/1000000u);text(1,12,27,220,name,NOVA_CAP);
 fill(12,CONTROLS_TOP-1,216,1,NOVA_DIM);fill(12,CONTROLS_BOTTOM,216,1,NOVA_DIM);
 int track=CONTROLS_BOTTOM-CONTROLS_TOP,thumb=track*track/(CONTROLS_COUNT*CONTROLS_ROW_HEIGHT);fill(236,CONTROLS_TOP,2,track,NOVA_LINE);fill(236,CONTROLS_TOP+controls_scroll*(track-thumb)/CONTROLS_MAX_SCROLL,2,thumb,NOVA_CYAN);
 pill(79,ui_height-39,82,32,"BACK",false);
}

/* Receiver codes are independent from display gain. Changes stop acquisition,
 * preserve all saved examples, and invalidate only derived live analysis. */
#define RF_RECEIVER_ROWS 17u
static const char *receiver_names[RF_RECEIVER_ROWS]={"CENTER","SAMPLE RATE","RF WIDTH","GAIN CODE","RF GAIN","BB GAIN","FILTER I","FILTER Q","DC I0","DC Q0","DC I1","DC Q1","IQ AMP","IQ PHASE","DEFAULTS","EVENT RF","DETAILS"};
static unsigned receiver_control(unsigned field){return field==0?RISC_RADIO_IQ_CONTROL_CENTER:field==1?RISC_RADIO_IQ_CONTROL_RATE:field==2?RISC_RADIO_IQ_CONTROL_BANDWIDTH:field==3?RISC_RADIO_IQ_CONTROL_GAIN:field==4?RISC_RADIO_IQ_CONTROL_RF_GAIN:field==5?RISC_RADIO_IQ_CONTROL_BB_GAIN:field<8?RISC_RADIO_IQ_CONTROL_FILTER:field<12?RISC_RADIO_IQ_CONTROL_DC:RISC_RADIO_IQ_CONTROL_IQ;}
static uint32_t receiver_step_auto(uint32_t value,int direction,uint32_t maximum){if(value==RF_RAW_AUTO)return direction>0?0:maximum;if(direction<0)return value?value-1:RF_RAW_AUTO;return value>=maximum?RF_RAW_AUTO:value+1;}
static bool receiver_change(unsigned field,int direction){
 if(field>=RF_RECEIVER_ROWS)return false;
 if(field==16){notify(receiver_extended?"NOMINAL MHz / RAW CODES / dBFS":"LEGACY @1: DEFAULTS / 256 ONLY");return false;}
 if(load_errors&1u){notify("RETRY PREFERENCE STORAGE");return false;}
 if(signature_goal||signature_pending||temporal_exit_blocked()){notify("SAVE OR CANCEL CAPTURE FIRST");return false;}
 if(!receiver_extended&&field!=14){notify("LEGACY @1 / DEFAULTS ONLY");return false;}
 if(field<14&&!(receiver_caps.controls&receiver_control(field))){notify("RECEIVER CONTROL UNAVAILABLE");return false;}
 rf_capture_identity changed=prefs.identity;
 switch(field){
 case 0:{int64_t center=(int64_t)changed.lo_hz+(int64_t)direction*1000000;uint32_t lo=receiver_caps.center_min_hz,hi=receiver_caps.center_max_hz;if(center<lo)center=lo;if(center>hi)center=hi;changed.lo_hz=(uint32_t)center;break;}
 case 1:changed.sample_rate_hz=direction>0?receiver_caps.sample_rates_hz[1]:receiver_caps.sample_rates_hz[0];break;
 case 2:changed.width_hz=direction>0?receiver_caps.bandwidths_hz[1]:receiver_caps.bandwidths_hz[0];break;
 case 3:changed.raw_gain=(uint32_t)clamp_int((int)changed.raw_gain+direction,0,(int)receiver_caps.gain_selector_max);break;
 case 4:changed.rf_gain=receiver_step_auto(changed.rf_gain,direction,receiver_caps.rf_gain_max);break;
 case 5:changed.bb_gain=receiver_step_auto(changed.bb_gain,direction,receiver_caps.bb_gain_max);break;
 case 6:case 7:{unsigned shift=field==6?0u:8u,mask=(receiver_caps.filter_mask>>shift)&255u,value=(changed.filter>>shift)&255u;value=(unsigned)clamp_int((int)value+direction,0,(int)mask);changed.filter=(changed.filter&~(255u<<shift))|(value<<shift);break;}
 case 8:case 9:case 10:case 11:changed.dc[field-8]=receiver_step_auto(changed.dc[field-8],direction,receiver_caps.dc_max);break;
 case 12:case 13:{unsigned shift=field==12?0u:8u,mask=(receiver_caps.iq_correction_mask>>shift)&255u;if(changed.iq_correction==RF_RAW_AUTO)changed.iq_correction=0;else{unsigned value=(changed.iq_correction>>shift)&255u;if((!value&&direction<0)||(value==mask&&direction>0))changed.iq_correction=RF_RAW_AUTO;else{value=(unsigned)((int)value+direction);changed.iq_correction=(changed.iq_correction&~(255u<<shift))|(value<<shift);}}break;}
 case 14:changed=rf_identity_default();break;
 case 15:if(!rf_identity_valid(&event_library.identity)){notify("NO SAVED EVENT RECEIVER");return false;}changed=event_library.identity;break;
 }
 if(!rf_identity_valid(&changed)){notify("UNSUPPORTED RECEIVER SETTING");return false;}
 stop();prefs.identity=changed;prefs.low_hz=rf_identity_low_hz(&changed);prefs.high_hz=rf_identity_high_hz(&changed);if(!receiver_extended)prefs.fft_size=256;configure_dsp();temporal_start();signature_audio.available=false;rf_room_reset(&room_tracker);event_match.complete=false;event_slot=-1;
 bool empty=true;for(unsigned i=0;i<8;i++)if(event_library.labels[i].present)empty=false;
 if(empty&&!event_files.exists){event_library.identity=changed;event_neural_refresh();}
 else{memset(&event_neural,0,sizeof(event_neural));event_neural_checkpoint=event_neural_used=false;}
 pending_save|=1;persist();notify("RECEIVER SET / START TO CAPTURE");return true;
}
static void receiver_value(unsigned field,char out[32]){
 const rf_capture_identity *i=&prefs.identity;uint32_t value=0;
 switch(field){case 0:snprintf(out,32,"%u.%03u MHz",i->lo_hz/1000000u,i->lo_hz%1000000u/1000u);return;case 1:snprintf(out,32,"%u MSa/s",i->sample_rate_hz/1000000u);return;case 2:snprintf(out,32,"%u MHz",i->width_hz/1000000u);return;case 3:value=i->raw_gain;break;case 4:value=i->rf_gain;break;case 5:value=i->bb_gain;break;case 6:value=i->filter&63u;break;case 7:value=(i->filter>>8)&63u;break;case 8:case 9:case 10:case 11:value=i->dc[field-8];break;case 12:case 13:value=i->iq_correction==RF_RAW_AUTO?RF_RAW_AUTO:field==12?i->iq_correction&31u:(i->iq_correction>>8)&63u;break;case 14:snprintf(out,32,"RESTORE");return;case 15:snprintf(out,32,"RESTORE SAVED");return;default:snprintf(out,32,"NOMINAL / RAW");return;}
 if(value==RF_RAW_AUTO)snprintf(out,32,"AUTO");else snprintf(out,32,"%u RAW",value);
}
static void draw_receiver(void){
 text(0,12,7,216,"RECEIVER SETTINGS",NOVA_CYAN);text(1,12,29,216,receiver_extended?"COHERENT BURSTS / NOMINAL RF":"LEGACY @1 / FIXED DEFAULTS",NOVA_CAP);
 for(unsigned row=0;row<3;row++){unsigned field=receiver_scroll+row;if(field>=RF_RECEIVER_ROWS)break;int y=55+(int)row*44;char value[32];receiver_value(field,value);text(1,12,y,105,receiver_names[field],NOVA_TEXT);text(1,12,y+19,142,value,NOVA_WHITE);if(field<14){pill(151,y,34,35,"-",false);pill(193,y,35,35,"+",false);}else pill(151,y,77,35,"OPEN",false);fill(12,y+41,216,1,NOVA_LINE);}
 text(1,12,189,216,"UNCLASSIFIED / UNCALIBRATED",NOVA_CAP);pill(12,211,31,27,"<",false);pill(49,211,68,27,running?"STOP":"START",false);pill(124,211,63,27,"BACK",false);pill(195,211,33,27,">",false);
}
static void receiver_tap(int x,int y){
 if(y>=55&&y<187){unsigned field=receiver_scroll+(unsigned)(y-55)/44;if(x>=151&&x<185)receiver_change(field,-1);else if(x>=193&&x<228)receiver_change(field,1);else if(field>=14&&x>=151)receiver_change(field,1);return;}
 if(y>=208){if(x<45){if(receiver_scroll)receiver_scroll--;}else if(x<120)toggle();else if(x<190)set_page(PAGE_CONTROLS);else if(receiver_scroll+3<RF_RECEIVER_ROWS)receiver_scroll++;dirty=true;}
}
