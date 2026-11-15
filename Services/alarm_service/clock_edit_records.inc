/* ACE2 v1. No native structs are persisted. All integers little-endian.
 * Header40 + four68-byte rows + FNV checksum4 =316 bytes. */
#define EDIT_BYTES 316u
#define EDIT_ROW_BYTES 68u
#define EDIT_HEADER 40u
static uint8_t edit_record[EDIT_BYTES],edit_prior_record[EDIT_BYTES];
static bool edit_loaded;
static alarm_clock_edit_permit_v2 edit_permit;
static uint64_t edit_started_ms;
static uint32_t edit_word(const uint8_t*b,unsigned n){return alarm_read32(b+n);}
static uint32_t edit_sum(const uint8_t*b){uint32_t h=2166136261u;for(unsigned n=0;n<EDIT_BYTES-4;n++)h=(h^b[n])*16777619u;return h;}
static bool edit_zero(const uint8_t*b,unsigned n){for(unsigned i=0;i<n;i++)if(b[i])return false;return true;}
static uint8_t*edit_row(uint8_t*b,unsigned group,unsigned kind){return b+EDIT_HEADER+(group*2+kind)*EDIT_ROW_BYTES;}
static const uint8_t*edit_crow(const uint8_t*b,unsigned group,unsigned kind){return b+EDIT_HEADER+(group*2+kind)*EDIT_ROW_BYTES;}
static void edit_config_bytes(const alarm_config*c,uint8_t*b){if(c->revision)alarm_config_encode(c,b);else memset(b,0,32);}
static void edit_occ_bytes(const alarm_occurrence*o,uint8_t*b){if(o->generation)alarm_occurrence_encode(o,b);else memset(b,0,32);}
static bool edit_row_valid(const uint8_t*b,unsigned kind){
 if(edit_zero(b,EDIT_ROW_BYTES))return true;
 alarm_config c;alarm_occurrence o;
 if(!alarm_config_decode(&c,b,32,(uint8_t)(kind+1))||edit_word(b,64)>c.created)return false;
 return edit_zero(b+32,32)||alarm_occurrence_decode(&o,b+32,32,(uint8_t)(kind+1));
}
static bool edit_decode(const uint8_t*b){
 if(memcmp(b,"ACE2",4)||edit_word(b,4)!=1||edit_word(b,8)>ALARM_CLOCK_EDIT_VERIFIED||!edit_word(b,12)||
    edit_word(b,16)>ALARM_RTC_MAX||edit_word(b,20)>ALARM_RTC_MAX||edit_word(b,24)>ALARM_CLOCK_EDIT_VERIFIED||
    edit_word(b,28)>ALARM_RTC_MAX||edit_word(b,32)>ALARM_RTC_MAX||edit_word(b,36)||edit_word(b,EDIT_BYTES-4)!=edit_sum(b))return false;
 for(unsigned g=0;g<2;g++)for(unsigned i=0;i<2;i++)if(!edit_row_valid(edit_crow(b,g,i),i))return false;
 unsigned state=edit_word(b,8);
 if(state!=ALARM_CLOCK_EDIT_INTENT&&(!edit_zero(b+24,16)||!edit_zero(edit_crow(b,1,0),2*EDIT_ROW_BYTES)))return false;
 if(state==ALARM_CLOCK_EDIT_NONE&&(!edit_zero(b+16,8)||!edit_zero(edit_crow(b,0,0),2*EDIT_ROW_BYTES)))return false;
 if(state!=ALARM_CLOCK_EDIT_NONE)for(unsigned i=0;i<2;i++){
  const uint8_t*r=edit_crow(b,0,i);if(!edit_zero(r,EDIT_ROW_BYTES)){
   uint32_t created=edit_word(r,16),wanted=edit_word(b,16);if(edit_word(r,64)!=(created<wanted?created:wanted))return false;
  }
 }
 return true;
}
static void edit_seal(uint8_t*b){alarm_write32(b+EDIT_BYTES-4,edit_sum(b));}
static void edit_reset(void){memset(edit_record,0,sizeof(edit_record));memset(&edit_permit,0,sizeof(edit_permit));memset(edit_prior_record,0,sizeof(edit_prior_record));edit_loaded=edit_recovery=edit_permit_valid=false;edit_started_ms=0;}
static int32_t edit_load(void){
 uint8_t b[EDIT_BYTES];uint32_t n=0;int32_t r=storage_get(kv->context,ALARM_CLOCK_EDIT_KEY,b,sizeof(b),&n);
 if(custody_retained)return ALARM_RETAINED;
 if(r==RISC_BOUND_KEY_VALUE_NOT_FOUND){if(edit_loaded)return ALARM_STORAGE;memset(edit_record,0,sizeof(edit_record));edit_recovery=false;return ALARM_OK;}
 if(r!=RISC_BOUND_KEY_VALUE_OK||n!=EDIT_BYTES||!edit_decode(b))return ALARM_STORAGE;
 if(edit_loaded&&(edit_word(b,12)<edit_word(edit_record,12)||(edit_word(b,12)==edit_word(edit_record,12)&&memcmp(b,edit_record,EDIT_BYTES))))return ALARM_STORAGE;
 memcpy(edit_record,b,sizeof(b));edit_loaded=true;edit_recovery=edit_word(b,8)==ALARM_CLOCK_EDIT_INTENT&&!edit_permit_valid;return ALARM_OK;
}
static bool edit_row_matches(const uint8_t*b,const alarm_config*c){uint8_t encoded[32];edit_config_bytes(c,encoded);return c->revision&&!memcmp(b,encoded,32);}
static bool edit_row_history(const uint8_t*b,const alarm_occurrence*o){
 if(edit_zero(b+32,32))return true;
 alarm_occurrence old;if(!alarm_occurrence_decode(&old,b+32,32,o->kind?o->kind:b[4]))return false;
 if(o->generation<old.generation)return false;
 if(o->generation==old.generation){
  uint8_t encoded[32];edit_occ_bytes(o,encoded);if(!memcmp(b+32,encoded,32))return true;
  /* A pending occurrence may legitimately complete with the SAME identity.
   * ACK/expiry are monotonic; neither can change back or into the other. DND
   * may only add the durable silenced marker, never remove it. */
  return old.state==ALARM_OCC_PENDING&&o->kind==old.kind&&o->revision==old.revision&&
   o->deadline==old.deadline&&o->recovery_until==old.recovery_until&&o->mode==old.mode&&
   o->silenced>=old.silenced&&(o->state==ALARM_OCC_PENDING||o->state==ALARM_OCC_ACKED||o->state==ALARM_OCC_EXPIRED);
 }return true;
}
static bool edit_history_valid(const alarm_config*c,const alarm_occurrence*o){
 if(!edit_loaded)return true;
 unsigned groups=edit_word(edit_record,8)==ALARM_CLOCK_EDIT_INTENT?2:1;
 for(unsigned g=0;g<groups;g++)for(unsigned i=0;i<2;i++){const uint8_t*r=edit_crow(edit_record,g,i);if(edit_row_matches(r,&c[i])&&!edit_row_history(r,&o[i]))return false;}
 return true;
}
static uint32_t edit_floor(unsigned i,const alarm_config*c){
 uint32_t floor=c->created;if(!edit_loaded||edit_word(edit_record,8)==ALARM_CLOCK_EDIT_NONE)return floor;
 unsigned groups=edit_word(edit_record,8)==ALARM_CLOCK_EDIT_INTENT?2:1;
 for(unsigned g=0;g<groups;g++){const uint8_t*r=edit_crow(edit_record,g,i);if(edit_row_matches(r,c)&&edit_word(r,64)<floor)floor=edit_word(r,64);}return floor;
}
static bool edit_time_authorized(unsigned i,uint32_t now,const alarm_config*c,const alarm_occurrence*o){
 if(!edit_loaded||edit_recovery||edit_word(edit_record,8)!=ALARM_CLOCK_EDIT_VERIFIED)return false;
 const uint8_t*r=edit_crow(edit_record,0,i);return edit_row_matches(r,c)&&edit_row_history(r,o)&&now>=edit_word(r,64);
}
static int32_t edit_store(const uint8_t*b,bool before_clock){
 int32_t r=storage_put(kv->context,ALARM_CLOCK_EDIT_KEY,b,EDIT_BYTES);
 if(custody_retained)return ALARM_RETAINED;
 if(before_clock&&(r==RISC_BOUND_KEY_VALUE_BUSY||r==RISC_BOUND_KEY_VALUE_INVALID))return r==RISC_BOUND_KEY_VALUE_BUSY?ALARM_BUSY:ALARM_STORAGE;
 if(r!=RISC_BOUND_KEY_VALUE_OK&&r!=RISC_BOUND_KEY_VALUE_IO){custody_retained=true;return ALARM_RETAINED;}
 uint8_t actual[EDIT_BYTES];uint32_t n=0;r=storage_get(kv->context,ALARM_CLOCK_EDIT_KEY,actual,sizeof(actual),&n);
 if(custody_retained)return ALARM_RETAINED;
 if(r!=RISC_BOUND_KEY_VALUE_OK||n!=EDIT_BYTES||memcmp(actual,b,EDIT_BYTES)){custody_retained=true;return ALARM_RETAINED;}
 memcpy(edit_record,b,EDIT_BYTES);edit_loaded=true;return ALARM_OK;
}
