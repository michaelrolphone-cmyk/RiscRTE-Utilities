#pragma once
/* Capture checkpoints only form real bounded feature windows. DTW and neural
 * refinement run here solely when the ordinary full policy step calls ct_tick. */
static bool ct_enabled(unsigned i){return ct_m[i].details.temporal_state==CONTEXTS_IMPORT_READY&&ct_m[i].details.positive_examples;}
static bool ct_freeze(unsigned i){return ct_enabled(i)&&(ct_m[i].known_hold||(i?ct_r.match.running:ct_a.match.running));}
static void ct_audio_observe(bool ready,bool active,const uint32_t power[128],uint64_t now){
 if(!ct_enabled(0))return;
 ct_metadata *m=&ct_m[0];
 for(unsigned b=0;b<128;b++)ct_a.pair_power[b]+=power[b];
 ct_a.pair_active|=active;if(++ct_a.pair_count<2)return;
 uint32_t averaged[128];for(unsigned b=0;b<128;b++){averaged[b]=(uint32_t)(ct_a.pair_power[b]/2u);ct_a.pair_power[b]=0;}
 ct_a.pair_count=0;st_frame frame=st_frame_make(averaged,ct_a.previous_power,ct_a.pair_active);
 memcpy(ct_a.previous_power,averaged,sizeof(averaged));ct_a.pair_active=false;
 if(!ready){ct_reset(0);return;}
 if(!(frame.flags&ST_ACTIVE)){if(m->quiet<4)++m->quiet;if(m->quiet>=4)m->known_hold=false;}else m->quiet=0;
 if(ct_a.segment.ready&&ct_a.match.running&&(frame.flags&ST_ACTIVE)&&!m->previous_active&&m->details.missed_events<UINT32_MAX)++m->details.missed_events;
 m->previous_active=!!(frame.flags&ST_ACTIVE);
 if(!ct_a.segment.ready){st_segment_observe(&ct_a.segment,&frame);if(ct_a.segment.ready)m->segment_at=now;}
 if(ct_a.segment.ready&&!ct_a.match.running){
  st_match_begin(&ct_a.match,&ct_a.segment.event);m->query_at=m->segment_at;st_segment_rearm(&ct_a.segment);
  m->details.match_pending=ct_a.match.running;
 }
}
static void ct_radio_observe(bool ready,bool active,const uint32_t power[128],const rf_capture_identity *identity,uint64_t now){
 if(!ct_enabled(1))return;
 ct_metadata *m=&ct_m[1];
 if(!rf_identity_equal(identity,&ct_r.library.identity)){ct_reset(1);return;}
 if(ct_r.have_previous&&(now<=ct_r.previous_at||(now>>32)!=(ct_r.previous_at>>32)||now-ct_r.previous_at>RT_DELTA_MAX_MS))ct_reset(1);
 ct_r.previous_at=now;ct_r.have_previous=true;
 rt_frame frame=rt_frame_make(power,ct_r.previous_power,active,(uint32_t)now,true);
 memcpy(ct_r.previous_power,power,sizeof(ct_r.previous_power));
 if(!ready){ct_reset(1);return;}
 if(!(frame.flags&RT_ACTIVE)){if(m->quiet<4)++m->quiet;if(m->quiet>=4)m->known_hold=false;}else m->quiet=0;
 if(ct_r.segment.ready&&ct_r.match.running&&(frame.flags&RT_ACTIVE)&&!m->previous_active&&m->details.missed_events<UINT32_MAX)++m->details.missed_events;
 m->previous_active=!!(frame.flags&RT_ACTIVE);
 if(!ct_r.segment.ready){rt_segment_observe(&ct_r.segment,&frame);if(ct_r.segment.ready)m->segment_at=now;}
 if(ct_r.segment.ready&&!ct_r.match.running){
  rt_match_begin(&ct_r.match,&ct_r.segment.event);m->query_at=m->segment_at;rt_segment_rearm(&ct_r.segment);
  m->details.match_pending=ct_r.match.running;
 }
}
static void ct_tick(unsigned i){
 if(!ct_enabled(i))return;
 ct_metadata *m=&ct_m[i];bool neural=false;int selected=-1;unsigned reason=0,score=0;
 if(i==0){
  if(!ct_a.match.running)return;
  st_match_tick(&ct_a.match,&ct_a.library,8);m->details.match_work_units=ct_a.match.work_units;m->details.match_pending=ct_a.match.running;
  if(!ct_a.match.complete)return;
  selected=ct_a.match.selected;reason=ct_a.match.reason;score=ct_a.match.score;
  if(m->details.neural_state==CONTEXTS_IMPORT_READY){int refined=sn_resolve(&ct_a.neural,m->neural_eligible,&ct_a.match);if(refined>=0&&refined!=selected){selected=refined;reason=ST_RESULT_MATCH;score=ct_a.match.positive[selected];neural=true;}}
 }else{
  if(!ct_r.match.running)return;
  rt_match_tick(&ct_r.match,&ct_r.library,8);m->details.match_work_units=ct_r.match.work_units;m->details.match_pending=ct_r.match.running;
  if(!ct_r.match.complete)return;
  selected=ct_r.match.selected;reason=ct_r.match.reason;score=ct_r.match.score;
  if(m->details.neural_state==CONTEXTS_IMPORT_READY){int refined=rn_resolve(&ct_r.neural,m->neural_eligible,&ct_r.match);if(refined>=0&&refined!=selected){selected=refined;reason=RT_RESULT_MATCH;score=ct_r.match.positive[selected];neural=true;}}
 }
 m->details.event_engine=neural?CONTEXTS_EVENT_NEURAL:CONTEXTS_EVENT_TEMPORAL;
 m->event_at=m->query_at;m->event_valid=selected>=0;m->event_slot=selected;m->event_score=score;
 m->event_ambiguous=reason==(i?RT_RESULT_AMBIGUOUS:ST_RESULT_AMBIGUOUS);m->known_hold=m->event_valid;
 memset(m->event_name,0,sizeof(m->event_name));
 if(selected>=0)memcpy(m->event_name,i?ct_r.library.labels[selected].name:ct_a.library.labels[selected].name,17);
}
static void ct_publish(unsigned i,contexts_source_status_v1 *s,uint64_t now,uint64_t signature_at,uint32_t hold_ms){
 ct_metadata *m=&ct_m[i];
 if(!ct_enabled(i)){m->details.event_engine=s->event_valid?CONTEXTS_EVENT_SIGNATURE:CONTEXTS_EVENT_NONE;m->event_at=signature_at;return;}
 bool fresh=s->current&&now>=m->event_at&&now-m->event_at<=hold_ms;
 s->event_valid=fresh&&m->event_valid;s->event_ambiguous=fresh&&m->event_ambiguous;
 s->event_slot=m->event_slot;s->event_confidence=m->event_score;memcpy(s->event_name,m->event_name,17);
}
