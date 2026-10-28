/* Explicit mixed profile. Durable bytes are versioned participant records;
 * legacy ACE2 is a read-only compatibility view until an explicit edit. */
#include "ClockEditLegacyImport.h"
#define EDIT_BYTES CLOCK_PARTICIPANT_MAX_BYTES
static const clock_participant_type edit_types[]={
 {1,1,32,32,clock_absolute_participant},{2,1,32,32,clock_countdown_participant},
 {3,1,64,64,clock_points_participant}};
static uint8_t edit_record[EDIT_BYTES],edit_prior_record[EDIT_BYTES];
static uint8_t edit_io[EDIT_BYTES],edit_encode_scratch[EDIT_BYTES];
static size_t edit_size,edit_prior_size;
static clock_participant_record edit_view;
static bool edit_loaded;
static alarm_clock_edit_permit_v2 edit_permit;
static uint64_t edit_started_ms;
static void edit_config_bytes(const alarm_config*c,uint8_t*b){if(c->revision)alarm_config_encode(c,b);else memset(b,0,32);}
static void edit_occ_bytes(const alarm_occurrence*o,uint8_t*b){if(o->generation)alarm_occurrence_encode(o,b);else memset(b,0,32);}
static void edit_points_bytes(const points_config*c,const points_ledger*l,uint8_t*b,uint8_t*h){
 if(c->revision)points_config_encode(c,b);else memset(b,0,64);
 if(l->generation)points_ledger_encode(l,h);else memset(h,0,64);
}
static bool edit_decode_view(clock_participant_record*out,const uint8_t*b,size_t n){
 return clock_participant_import_ace2(out,b,n)||clock_participant_decode(out,b,n,edit_types,3,ALARM_RTC_MAX);
}
static void edit_reset(void){memset(edit_record,0,sizeof(edit_record));memset(edit_prior_record,0,sizeof(edit_prior_record));memset(&edit_view,0,sizeof(edit_view));memset(&edit_permit,0,sizeof(edit_permit));edit_size=edit_prior_size=0;edit_loaded=edit_recovery=edit_permit_valid=false;edit_started_ms=0;}
static int32_t edit_read_error(void){return custody_retained?ALARM_RETAINED:mixed_storage_result==RISC_BOUND_KEY_VALUE_BUSY?ALARM_BUSY:ALARM_STORAGE;}
static int32_t edit_load(void){
 uint8_t *b=edit_io;uint32_t n=0;clock_participant_record decoded;
 int32_t r=storage_get(kv->context,ALARM_CLOCK_EDIT_KEY,b,EDIT_BYTES,&n);
 if(custody_retained)return ALARM_RETAINED;
 if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){if(edit_loaded)return ALARM_STORAGE;return ALARM_OK;}
 if(r!=RISC_BOUND_KEY_VALUE_OK)return edit_read_error();
 if(!edit_decode_view(&decoded,b,n))return ALARM_STORAGE;
 if(edit_loaded&&(decoded.generation<edit_view.generation||(decoded.generation==edit_view.generation&&(n!=edit_size||memcmp(b,edit_record,n)))))return ALARM_STORAGE;
 memcpy(edit_record,b,n);edit_size=n;edit_loaded=true;
 if(!edit_decode_view(&edit_view,edit_record,edit_size))return ALARM_STORAGE;
 edit_recovery=edit_view.phase==ALARM_CLOCK_EDIT_INTENT&&!edit_permit_valid;return ALARM_OK;
}
static bool edit_alarm_history(const uint8_t*h,const alarm_occurrence*o){
 if(clock_schedule_zero(h,32))return true;
 alarm_occurrence old;if(!alarm_occurrence_decode(&old,h,32,h[4])||o->generation<old.generation)return false;
 if(o->generation==old.generation){
  uint8_t bytes[32];edit_occ_bytes(o,bytes);if(!memcmp(bytes,h,32))return true;
  return old.state==ALARM_OCC_PENDING&&o->kind==old.kind&&o->revision==old.revision&&o->deadline==old.deadline&&
   o->recovery_until==old.recovery_until&&o->mode==old.mode&&o->silenced>=old.silenced&&
   (o->state==ALARM_OCC_PENDING||o->state==ALARM_OCC_ACKED||o->state==ALARM_OCC_EXPIRED);
 }
 return !(old.state!=ALARM_OCC_PENDING&&o->revision==old.revision&&o->deadline==old.deadline&&o->state!=old.state);
}
static bool edit_history_valid(const alarm_config*c,const alarm_occurrence*o){
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];if(r->kind>2)continue;
  unsigned i=r->kind-1;uint8_t bytes[32];edit_config_bytes(&c[i],bytes);
  if(!memcmp(bytes,r->config,32)&&!edit_alarm_history(r->history,&o[i]))return false;
 }return true;
}
static bool edit_points_history_valid(const points_config*c,const points_ledger*l,bool saved){
 uint8_t config_bytes[64],history[64];edit_points_bytes(c,l,config_bytes,history);
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];
  if(r->kind!=3||memcmp(r->config,config_bytes,64)||r->provenance!=(saved?CLOCK_CONFIG_SAVED:CLOCK_CONFIG_VIRTUAL))continue;
  if(!clock_schedule_zero(r->history,64)){points_ledger old;
   if(!points_ledger_decode(&old,r->history,64)||!clock_points_history_successor(&old,l))return false;
  }
 }return true;
}
/* A later explicit edit replaces the live projection floor. It must not make
 * an already authenticated pending/ACK/expired occurrence fail reconstruction.
 * This exception is only for that exact durable occurrence, never fresh due,
 * preview or wake selection; caller still verifies its original UTC deadline. */
static bool edit_points_recorded_config(const points_config*c,const points_ledger*l,points_config*out){
 if(!edit_loaded||!edit_view.phase||!l->generation||!l->state)return false;
 uint8_t bytes[64];points_config_encode(c,bytes);
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];points_ledger old;
  if(r->kind!=3||memcmp(r->config,bytes,64)||r->provenance!=(points_configured_saved?CLOCK_CONFIG_SAVED:CLOCK_CONFIG_VIRTUAL)||
     !points_ledger_decode(&old,r->history,64)||!clock_points_same_event(&old,l)||!clock_points_history_successor(&old,l))continue;
  *out=*c;out->created=0;return true;
 }
 return false;
}
static uint32_t edit_floor(unsigned i,const alarm_config*c){
 uint32_t floor=c->created;uint8_t bytes[32];edit_config_bytes(c,bytes);
 if(!edit_loaded||!edit_view.phase)return floor;
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];
  if(r->kind==i+1&&!memcmp(r->config,bytes,32)&&r->floor<floor)floor=r->floor;
 }return floor;
}
/* UTC floor follows config/provenance, like its immutable created boundary.
 * Zone is captured for transaction-race checks and occurrence reconstruction;
 * changing the future recurrence zone does not rewrite that UTC boundary. */
static uint32_t edit_points_floor(const points_config*c){
 uint32_t floor=c->created;uint8_t bytes[64];points_config_encode(c,bytes);
 if(!edit_loaded||!edit_view.phase)return floor;
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];
  if(r->kind==3&&!memcmp(r->config,bytes,64)&&r->provenance==(points_configured_saved?CLOCK_CONFIG_SAVED:CLOCK_CONFIG_VIRTUAL)&&r->floor<floor)floor=r->floor;
 }return floor;
}
static bool edit_time_authorized(unsigned i,uint32_t now,const alarm_config*c,const alarm_occurrence*o){
 uint8_t bytes[32];edit_config_bytes(c,bytes);
 if(!edit_loaded||edit_recovery||edit_view.phase!=ALARM_CLOCK_EDIT_VERIFIED)return false;
 for(unsigned n=0;n<edit_view.count;n++){const clock_participant_row*r=&edit_view.rows[n];
  if(!r->group&&r->kind==i+1&&!memcmp(bytes,r->config,32)&&edit_alarm_history(r->history,o)&&now>=r->floor)return true;
 }return false;
}
static int32_t edit_store(const uint8_t*b,size_t size,bool before_clock){
 int32_t r=storage_put(kv->context,ALARM_CLOCK_EDIT_KEY,b,(uint32_t)size);
 if(custody_retained)return ALARM_RETAINED;
 if(before_clock&&(r==RISC_BOUND_KEY_VALUE_BUSY||r==RISC_BOUND_KEY_VALUE_INVALID))return r==RISC_BOUND_KEY_VALUE_BUSY?ALARM_BUSY:ALARM_STORAGE;
 if(r!=RISC_BOUND_KEY_VALUE_OK&&r!=RISC_BOUND_KEY_VALUE_IO){custody_retained=true;return ALARM_RETAINED;}
 uint8_t *actual=edit_io;uint32_t n=0;r=storage_get(kv->context,ALARM_CLOCK_EDIT_KEY,actual,EDIT_BYTES,&n);
 if(custody_retained)return ALARM_RETAINED;
 if(r!=RISC_BOUND_KEY_VALUE_OK||n!=size||memcmp(actual,b,size)){custody_retained=true;return ALARM_RETAINED;}
 memcpy(edit_record,b,size);edit_size=size;edit_loaded=true;
 if(!edit_decode_view(&edit_view,edit_record,edit_size)){custody_retained=true;return ALARM_RETAINED;}
 return ALARM_OK;
}
