/* Targeted-key reads through the actual mixed service, not codec stubs. */
#define POINTS_MIXED_TEST_ENTRY mixed_regression_main
#include "points_mixed_clock_edit_test.c"
#undef POINTS_MIXED_TEST_ENTRY
static const char *fault_key;
static int32_t fault_result;
static int32_t selective_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 if(fault_key&&!strcmp(k,fault_key)){assert(c==blobs&&!forbid);get_count++;*n=0;return fault_result;}
 return get_blob(c,k,b,cap,n);
}
static const risc_bound_key_value_v1 selective_bound={2,sizeof(selective_bound),blobs,selective_get,put_blob};
static void ready_with_authorization(void){
 fault_key=NULL;fault_result=0;setup_mixed();alarm_clock_edit_permit_v2 p=begin_edit(seconds-86400);finish_edit(&p);kv=&selective_bound;
}
static void context_result_is(int result){
 const char utc[40]="UTC";points_service_context out;memset(&out,0xa5,sizeof(out));out.struct_size=sizeof(out);points_service_context unchanged=out;
 assert(points_service_read_context(client,&points_configured,utc,0,&out)==result);
 if(result)assert(!memcmp(&out,&unchanged,sizeof(out)));
}
static void rejected_read(int expected){
 unsigned writes=put_count;alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};
 assert(editor()->begin(NULL,seconds,&p)==expected);assert(put_count==writes&&!edit_permit_valid&&custody_retained==(expected==ALARM_RETAINED));
 if(expected==ALARM_BUSY){assert(phase==IDLE&&!error);context_result_is(ALARM_OK);}
 else{assert(phase==BLOCKED&&error==expected);context_result_is(expected);
  unsigned calls=get_count+reads+clocks;assert(editor()->begin(NULL,seconds,&p)==expected);assert(get_count+reads+clocks==calls);}
}
static void failed_clock_snapshot(void){
 for(unsigned kind=0;kind<8;kind++){
  ready_with_authorization();uint32_t requested=seconds;unsigned writes=put_count;
  switch(kind){case 0:read_result=RISC_REALTIME_INVALID;break;case 1:read_result=RISC_REALTIME_IO;break;
   case 2:valid=false;break;case 3:malformed=1;break;case 4:malformed=2;break;case 5:malformed=5;break;
   case 6:read_result=RISC_REALTIME_CONTEXT;break;default:read_result=-1234;break;}
  alarm_clock_edit_permit_v2 p={.struct_size=sizeof(p)};int expected=kind<6?ALARM_RTC:ALARM_RETAINED;
  assert(editor()->begin(NULL,requested,&p)==expected&&!edit_permit_valid&&put_count==writes);context_result_is(expected);
  if(kind>=6){unsigned calls=get_count+put_count+reads+clocks;forbid=true;assert(editor()->begin(NULL,requested,&p)==ALARM_RETAINED);context_result_is(ALARM_RETAINED);assert(calls==get_count+put_count+reads+clocks);restart();continue;}
  assert(error==ALARM_RTC&&phase==BLOCKED);
  /* A new explicit Begin can still repair a typed RTC-only block even while
   * the old clock remains invalid. Only Finish confirms the later valid seed. */
  p=begin_edit(requested);read_result=0;valid=true;malformed=0;finish_edit(&p);context_result_is(ALARM_OK);
 }
}
int main(void){
 failed_clock_snapshot();
 /* Missing and malformed previously verified authority must not remain a
  * settled copied projection after Begin has observed the contradiction. */
 for(unsigned malformed=0;malformed<2;malformed++){
  ready_with_authorization();uint8_t authority[EDIT_BYTES];unsigned size=sizes[9];memcpy(authority,blobs[9],size);
  if(malformed)blobs[9][size-1]^=1;else sizes[9]=0;
  rejected_read(ALARM_STORAGE);
  memcpy(blobs[9],authority,size);sizes[9]=size;client->refresh(NULL);until(ALARM_STATE_READY);context_result_is(ALARM_OK);
 }
 for(unsigned key=0;key<2;key++){
  ready_with_authorization();unsigned index=key?8:5;if(key)zone("UTC");
  uint8_t before[64];unsigned size=sizes[index];memcpy(before,blobs[index],size);blobs[index][0]^=1;
  rejected_read(ALARM_STORAGE);memcpy(blobs[index],before,size);client->refresh(NULL);until(ALARM_STATE_READY);context_result_is(ALARM_OK);
 }
 const char *keys[]={ALARM_CONFIG_KEY,POINTS_CONFIG_KEY,"time_zone",ALARM_CLOCK_EDIT_KEY};
 const int results[]={RISC_BOUND_KEY_VALUE_IO,RISC_BOUND_KEY_VALUE_INVALID,RISC_BOUND_KEY_VALUE_BUSY,RISC_BOUND_KEY_VALUE_CONTEXT,-1234};
 for(unsigned key=0;key<4;key++)for(unsigned r=0;r<sizeof(results)/sizeof(results[0]);r++){
  ready_with_authorization();fault_key=keys[key];fault_result=results[r];
  rejected_read(results[r]==RISC_BOUND_KEY_VALUE_BUSY?ALARM_BUSY:r>=3?ALARM_RETAINED:ALARM_STORAGE);
  if(r>=3){unsigned calls=get_count+put_count+reads+clocks;forbid=true;assert(client->step(NULL)==ALARM_RETAINED);context_result_is(ALARM_RETAINED);assert(calls==get_count+put_count+reads+clocks);fault_key=NULL;restart();continue;}
  fault_key=NULL;client->refresh(NULL);until(ALARM_STATE_READY);context_result_is(ALARM_OK);
 }
 puts("Mixed Begin targeted reads: invalidated known-failed snapshots, BUSY clean refusal and explicit recovery passed");return 0;
}
