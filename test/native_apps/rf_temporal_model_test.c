#include <assert.h>
#include <stdio.h>
#include "../../Apps/rf_temporal_files.h"
#include "../../Apps/rf_neural_store.h"

static rt_library library,decoded,before;
static rn_trainer trainer,repeat,trainer_before;
static uint8_t bytes[RT_BANK_MAX],saved[RT_BANK_MAX],checkpoint[RN_RECORD_SIZE];
static void crc_bank(size_t n){rf_signature_put32(bytes+n-4,rf_signature_crc(bytes,n-4));}
static void crc_neural(void){rf_signature_put32(checkpoint+RN_RECORD_SIZE-4,rf_signature_crc(checkpoint,RN_RECORD_SIZE-4));}
static rt_example pattern(unsigned band,unsigned stretch,bool reverse){
 rt_example e={.id=1,.count=16,.kind=RT_POSITIVE};
 const int levels[16]={-7000,-4000,-1500,-2000,-2500,-3000,-4000,-4500,-2000,-2500,-3000,-4000,-5000,-6000,-8000,-10000};
 for(unsigned i=0;i<e.count;i++){unsigned j=reverse?15-i:i;rt_frame*f=&e.frames[i];f->timestamp_ms=1234+i*100*stretch;f->level_db=(int16_t)levels[j];f->peak_coord=rt_bin_coord(band*2);f->flags=RT_ACTIVE|RT_TONAL|RT_GAP_BEFORE;f->flux=(j==1||j==8)?255:0;rt_set_nibble(f,band,15);rt_set_nibble(f,band+1,8);}
 rt_summarize(&e);assert(rt_example_valid(&e));return e;
}
static unsigned cost(const rt_example*a,const rt_example*b,unsigned shift){rt_dtw d;rt_dtw_start(&d,a,b,shift);unsigned rounds=0;while(!d.done){unsigned cells=d.cells;rt_dtw_step(&d,a,b,3);assert(d.cells-cells<=3*(2*RT_DTW_RADIUS+1));assert(++rounds<64);}return d.cost;}
static rt_matcher match(const rt_example *e){rt_matcher m;rt_match_begin(&m,e);unsigned ticks=0;while(m.running){unsigned work=m.work_units;rt_match_tick(&m,&library,64);assert(m.work_units-work<=64);assert(++ticks<100);}return m;}
static void test_frequency_and_time(void){
 uint32_t p[128]={0},previous[128]={0};
 for(unsigned i=0;i<128;i++){memset(p,0,sizeof(p));p[i]=1234567;rt_frame f=rt_frame_make(p,previous,true,1000+i*100,true);assert(rt_frame_valid(&f));assert(f.timestamp_ms==1000+i*100&&f.peak_coord==rt_bin_coord(i));assert(rt_nibble(&f,(int)(i/2))==15&&(f.flags&RT_GAP_BEFORE));assert(f.level_db%100==0);rf_capture_identity id=rf_identity_default();uint32_t from_coord=rf_coord_hz(&id,f.peak_coord),from_bin=rf_signature_band_hz(&id,i);assert((from_coord>from_bin?from_coord-from_bin:from_bin-from_coord)<id.sample_rate_hz/65534u);}
 rt_example a=pattern(30,1,false),slow=pattern(30,2,false),too_slow=pattern(30,3,false),shifted=pattern(31,1,false),other=pattern(44,1,false),reversed=pattern(30,1,true);
 assert(cost(&a,&a,1)==0&&cost(&slow,&a,1)<100&&cost(&too_slow,&a,1)==1000);assert(cost(&shifted,&a,1)<80&&cost(&shifted,&a,0)>200);assert(cost(&other,&a,2)==1000&&cost(&reversed,&a,1)>200);
 assert(a.duration_ms==1501&&slow.duration_ms==3001);rt_example bad=a;bad.frames[1].timestamp_ms=bad.frames[0].timestamp_ms;assert(!rt_example_valid(&bad));bad=a;bad.frames[1].timestamp_ms=UINT32_MAX;assert(!rt_example_valid(&bad));
 /* Long retained-display gaps are exact observations with unknown intervals. */
 rt_segmenter s={0};rt_frame q={.level_db=-12000,.flags=RT_GAP_BEFORE};uint32_t now=100;
 for(unsigned i=0;i<4;i++){q.timestamp_ms=now;now+=100;rt_segment_observe(&s,&q);}
 for(unsigned i=0;i<a.count;i++){rt_frame f=a.frames[i];f.timestamp_ms=now;now+=(i==5?1800:100);rt_segment_observe(&s,&f);}
 for(unsigned i=0;i<4;i++){q.timestamp_ms=now;now+=100;rt_segment_observe(&s,&q);}
 assert(s.ready&&s.event.pre==4&&s.event.count==24&&!s.event.flags&&s.event.duration_ms==3201);assert(rt_example_valid(&s.event));
 assert(s.event.frames[10].timestamp_ms-s.event.frames[9].timestamp_ms==1800);
 for(unsigned i=0;i<s.event.count;i++)assert(s.event.frames[i].flags&RT_GAP_BEFORE);
 rt_segment_reset(&s);for(unsigned i=0;i<64;i++){rt_frame f=a.frames[0];f.timestamp_ms=now;now+=100;rt_segment_observe(&s,&f);}assert(s.ready&&s.event.count==64&&rt_partial(&s.event));s.event.flags|=RT_CONFIRMED_END;assert(!rt_partial(&s.event)&&rt_example_valid(&s.event));
 rt_segment_reset(&s);for(unsigned i=0;i<3;i++){rt_frame f=a.frames[i];rt_segment_observe(&s,&f);}rt_frame f=a.frames[3];f.timestamp_ms+=70000;rt_segment_observe(&s,&f);assert(s.ready&&rt_partial(&s.event)&&s.event.count==3);
 library=(rt_library){.identity=rf_identity_default()};rt_label*l=&library.labels[0];l->present=true;l->shift_limit=1;strcpy(l->name,"RF pulse");l->next_id=3;l->examples[0]=a;l->examples[1]=slow;l->examples[1].id=2;
 rt_matcher m=match(&shifted);assert(m.selected==0&&m.reason==RT_RESULT_MATCH&&m.shift[0]==1);
 l->examples[2]=shifted;l->examples[2].id=3;l->examples[2].kind=RT_NEGATIVE;l->next_id=4;m=match(&shifted);assert(m.selected<0&&m.reason==RT_RESULT_NEGATIVE);
 memset(&l->examples[2],0,sizeof(rt_example));library.labels[1]=*l;strcpy(library.labels[1].name,"Other pulse");m=match(&a);assert(m.reason==RT_RESULT_AMBIGUOUS);
 rt_match_begin(&m,&a);rt_match_tick(&m,&library,8);assert(m.running);library.generation[0]++;rt_match_tick(&m,&library,8);assert(m.complete&&!m.running&&m.selected<0&&m.reason==RT_RESULT_UNKNOWN);
}
static void test_storage(void){
 size_t n=rt_bank_encode(&library,0,bytes,sizeof(bytes));assert(n>RT_BANK_MIN&&n<RT_BANK_MAX);memcpy(saved,bytes,n);assert(rt_bank_decode(&decoded,0,bytes,n));assert(!memcmp(&library,&decoded,sizeof(library)));before=decoded;
 for(size_t i=0;i<n;i+=17){bytes[i]^=1;assert(!rt_bank_decode(&decoded,0,bytes,n));assert(!memcmp(&decoded,&before,sizeof(decoded)));bytes[i]^=1;}
 assert(!rt_bank_decode(&decoded,1,bytes,n)&&!rt_bank_decode(&decoded,0,bytes,n-1));
 size_t first=RT_BANK_HEADER+RT_LABEL_BYTES+RT_EXAMPLE_BYTES;
 const unsigned bad_offsets[]={12,28,96,RT_BANK_HEADER+2,RT_BANK_HEADER+RT_LABEL_BYTES+38};
 for(unsigned i=0;i<sizeof(bad_offsets)/sizeof(bad_offsets[0]);i++){memcpy(bytes,saved,n);bytes[bad_offsets[i]]=1;crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));assert(!memcmp(&decoded,&before,sizeof(decoded)));}
 memcpy(bytes,saved,n);bytes[first+37]=1;crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 memcpy(bytes,saved,n);bytes[first+39+37]=bytes[first+39+38]=0;crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 memcpy(bytes,saved,n);rf_signature_put32(bytes+RT_BANK_HEADER+RT_LABEL_BYTES+34,UINT32_MAX-50);crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 memcpy(bytes,saved,n);bytes[first+32]=131;crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 memcpy(bytes,saved,n);bytes[first+36]|=128;crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 memcpy(bytes,saved,n);rf_capture_identity other=library.identity;other.lo_hz+=1000000;rf_identity_encode(&other,bytes+32);crc_bank(n);assert(!rt_bank_decode(&decoded,0,bytes,n));
 /* Full eight-label, six-example,64-observation quota and deletion reclaim. */
 rt_example full=pattern(30,1,false);full.count=64;full.flags=RT_CLIPPED|RT_CONFIRMED_END;
 for(unsigned i=0;i<64;i++){full.frames[i]=full.frames[i%16];full.frames[i].timestamp_ms=1000+i*60000u;}rt_summarize(&full);assert(rt_example_valid(&full)&&full.duration_ms==3780001u);
 for(unsigned i=0;i<RT_LABELS;i++){rt_label*l=&library.labels[i];memset(l,0,sizeof(*l));l->present=true;l->next_id=7;snprintf(l->name,sizeof(l->name),"RF %u",i);for(unsigned j=0;j<RT_EXAMPLES;j++){l->examples[j]=full;l->examples[j].id=j+1;l->examples[j].kind=j<3?RT_POSITIVE:RT_NEGATIVE;}}
 for(unsigned b=0;b<2;b++){n=rt_bank_encode(&library,b,bytes,sizeof(bytes));assert(n==RT_BANK_MAX&&n<65536);assert(rt_bank_decode(&decoded,b,bytes,n));}
 assert(!memcmp(&library,&decoded,sizeof(library)));assert(2u*RT_BANK_MAX+RN_RECORD_SIZE==129004u&&2u*RT_BANK_MAX+RN_RECORD_SIZE<=RISC_APP_DATA_NAMESPACE_MAX);
 memset(&library.labels[0].examples[0],0,sizeof(rt_example));assert(rt_bank_encode(&library,0,bytes,sizeof(bytes))==RT_BANK_MAX-64u*RT_FRAME_BYTES);
}
static rt_example fixture(unsigned identity,unsigned recording,unsigned kind){
 rt_segmenter s={0};uint32_t power[128]={0},previous[128]={0},now=1000;
 for(unsigned i=0;i<4;i++){rt_frame f=rt_frame_make(power,previous,false,now,true);now+=100;rt_segment_observe(&s,&f);}
 const unsigned envelope[]={100,70,48,31,21,11,5,1};
 for(unsigned i=0;i<8;i++){
  unsigned amplitude=(envelope[i]+(recording==2&&i>0?1u:0u))*(10+recording);memset(power,0,sizeof(power));power[15]=amplitude*10000;power[18]=amplitude*8000;
  if(i==2&&identity<2)power[identity?40:30]=amplitude*8000;
  if(i==2&&identity==2)power[30]=power[40]=amplitude*4000;
  if(identity==3){memset(power,0,sizeof(power));power[64]=amplitude*10000;}
  rt_frame f=rt_frame_make(power,previous,true,now,true);now+=100;rt_segment_observe(&s,&f);memcpy(previous,power,sizeof(power));
 }
 for(unsigned i=0;i<4;i++){memset(power,0,sizeof(power));rt_frame f=rt_frame_make(power,previous,false,now,true);now+=100;rt_segment_observe(&s,&f);memcpy(previous,power,sizeof(power));}
 s.event.id=recording+1;s.event.kind=(uint8_t)kind;assert(rt_example_valid(&s.event));return s.event;
}
static void setup_neural(void){
 library=(rt_library){.identity=rf_identity_default(),.generation={3,5}};
 for(unsigned i=0;i<2;i++){rt_label*l=&library.labels[i];l->present=true;l->next_id=6;snprintf(l->name,sizeof(l->name),"RF label %u",i);for(unsigned j=0;j<3;j++)l->examples[j]=fixture(i,j,RT_POSITIVE);l->examples[3]=fixture(2,3,RT_NEGATIVE);l->examples[4]=fixture(2,4,RT_NEGATIVE);assert(rt_label_valid(l));}
}
static void finish_neural(rn_trainer*t){unsigned steps=0;while(t->state==RN_PREPARING||t->state==RN_TRAINING||t->state==RN_CHECKING){unsigned updates=t->updates;rn_tick(t,&library);assert(t->updates-updates<=1&&++steps<4000);}}
static void test_neural(void){
 setup_neural();before=library;rn_reset(&trainer,&library,255);assert(trainer.state==RN_PREPARING&&trainer.eligible==3&&trainer.held[0]==20&&trainer.held[1]==20);finish_neural(&trainer);
 printf("RF neural validation: state %u held %u/%u baseline %u/%u full %u/%u versus %u/%u regressions %u\n",trainer.state,trainer.candidate_correct,trainer.checks,trainer.baseline_correct,trainer.checks,trainer.full_candidate_correct,trainer.full_checks,trainer.full_baseline_correct,trainer.full_checks,trainer.regressions);
 assert(trainer.has_active&&trainer.state==RN_ACTIVE&&trainer.checks==4&&trainer.candidate_correct>trainer.baseline_correct&&trainer.full_candidate_correct>trainer.full_baseline_correct&&!trainer.regressions);
 assert(trainer.updates==RN_EPOCHS*6&&trainer.epoch==RN_EPOCHS&&!memcmp(&library,&before,sizeof(library)));
 for(unsigned i=0;i<2;i++){rt_matcher m;rt_match_begin(&m,&library.labels[i].examples[2]);memcpy(m.excluded_examples,trainer.held,sizeof(m.excluded_examples));while(m.running)rt_match_tick(&m,&library,64);assert(m.reason==RT_RESULT_AMBIGUOUS&&rn_apply(&trainer,&m)&&m.selected==(int)i);}
 uint32_t crc[2]={123,456};assert(rn_record_encode(&trainer,&library,crc,checkpoint,sizeof(checkpoint)));memcpy(saved,checkpoint,sizeof(checkpoint));rn_reset(&repeat,&library,255);assert(rn_record_load(&repeat,&library,crc,checkpoint,sizeof(checkpoint)));assert(!memcmp(&repeat.active,&trainer.active,sizeof(rn_model)));trainer_before=repeat;
 for(unsigned i=0;i<RN_RECORD_SIZE;i++){checkpoint[i]^=1;assert(!rn_record_load(&repeat,&library,crc,checkpoint,sizeof(checkpoint)));assert(!memcmp(&repeat,&trainer_before,sizeof(repeat)));checkpoint[i]^=1;}
 rf_signature_put32(checkpoint+RN_RECORD_HEADER,0x7fc00000u);crc_neural();assert(!rn_record_valid(checkpoint,sizeof(checkpoint)));memcpy(checkpoint,saved,sizeof(checkpoint));
 crc[0]++;assert(!rn_record_load(&repeat,&library,crc,checkpoint,sizeof(checkpoint)));crc[0]--;
 library.identity.raw_gain++;assert(!rn_record_load(&repeat,&library,crc,checkpoint,sizeof(checkpoint)));rn_tick(&repeat,&library);assert(!repeat.has_active&&repeat.state==RN_NEEDS_EXAMPLES);library.identity.raw_gain--;
 rt_matcher m=match(&library.labels[0].examples[2]);library.generation[0]++;assert(!rn_apply(&trainer,&m));rn_tick(&trainer,&library);assert(!trainer.has_active&&trainer.state==RN_NEEDS_EXAMPLES);
 setup_neural();library.labels[0].examples[4]=fixture(0,4,RT_NEGATIVE);rn_reset(&repeat,&library,255);finish_neural(&repeat);assert(!repeat.has_active&&repeat.state==RN_BASELINE&&repeat.regressions);
 setup_neural();library.labels[0].examples[4]=fixture(3,4,RT_NEGATIVE);rn_reset(&repeat,&library,255);finish_neural(&repeat);assert(!repeat.has_active&&repeat.state==RN_BASELINE&&repeat.calibrated!=repeat.eligible);
 setup_neural();memset(&library.labels[1].examples[2],0,sizeof(rt_example));rn_reset(&repeat,&library,255);assert(repeat.state==RN_NEEDS_EXAMPLES);
}
int main(void){test_frequency_and_time();test_storage();test_neural();printf("RF temporal and neural: linear frequency, exact sparse timestamps, long gaps, clip review, bounded DTW, strict atomic codecs, quota, held-out promotion and stale-model invalidation PASS (%zu-byte library, %zu-byte trainer)\n",sizeof(rt_library),sizeof(rn_trainer));return 0;}
