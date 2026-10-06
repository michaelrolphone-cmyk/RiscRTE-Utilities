#include <assert.h>
#include <stdio.h>
#include "../Apps/spectrum_temporal_store.h"
static spectrum_background background;
static st_library library,decoded,before;
static uint8_t bytes[ST_BANK_MAX];
static st_matcher matcher;
static st_example pattern(unsigned band,unsigned speed,bool reverse){
 st_example e={.id=1,.count=(uint8_t)(16*speed),.kind=ST_POSITIVE};
 const int levels[16]={-7000,-4000,-1500,-2000,-2500,-3000,-4000,-4500,-2000,-2500,-3000,-4000,-5000,-6000,-8000,-10000};
 for(unsigned i=0;i<e.count;i++){unsigned j=i/speed;if(reverse)j=15-j;st_frame*f=&e.frames[i];f->level_db=(int16_t)levels[j];f->peak_hz=(uint16_t)(1000+band*10);f->flags=ST_ACTIVE;f->flux=(j==1||j==8)?255:0;st_set_nibble(f,band,15);st_set_nibble(f,band+1,8);}
 st_summarize(&e);assert(st_example_valid(&e));return e;
}
static unsigned cost(st_example*a,st_example*b,unsigned shift){st_dtw d;st_dtw_start(&d,a,b,shift);unsigned rounds=0;while(!d.done){unsigned cells=d.cells;st_dtw_step(&d,a,b,3);assert(d.cells-cells<=3*(2*ST_DTW_RADIUS+1));assert(++rounds<64);}assert(d.cells<=64*(2*ST_DTW_RADIUS+1));return d.cost;}
int main(void){
 uint32_t power[128]={0};power[15]=100000;spectrum_background_reset(&background);
 for(unsigned i=0;i<64+100;i++){spectrum_background_observe(&background,power);assert(!background.foreground);int16_t excess,snr;assert(!spectrum_background_label(&background,1000,-90,false,&excess,&snr));}
 power[15]=500000;spectrum_background_observe(&background,power);assert(background.foreground);int16_t excess,snr;assert(spectrum_background_label(&background,1000,-90,false,&excess,&snr)&&snr>=600);power[15]=250000;spectrum_background_observe(&background,power);assert(spectrum_background_label(&background,1000,-90,true,&excess,&snr));power[15]=100000;spectrum_background_observe(&background,power);assert(!spectrum_background_label(&background,1000,-90,true,&excess,&snr));
 power[15]=500000;background.freeze_upward=true;uint32_t baseline=background.slow[15];for(unsigned i=0;i<2000;i++)spectrum_background_observe(&background,power);assert(background.foreground&&background.slow[15]==baseline);background.freeze_upward=false;for(unsigned i=0;i<4000;i++)spectrum_background_observe(&background,power);assert(!background.foreground);memset(power,0,sizeof(power));for(unsigned i=0;i<2000;i++)spectrum_background_observe(&background,power);assert(!spectrum_signature_total(background.slow));
 st_example a=pattern(30,1,false),fast=pattern(30,2,false),shifted=pattern(31,1,false),other=pattern(44,1,false),reversed=pattern(30,1,true);
 assert(cost(&a,&a,1)==0);assert(cost(&fast,&a,1)<160);assert(cost(&shifted,&a,1)<80);assert(cost(&shifted,&a,0)>200);assert(cost(&other,&a,2)>200);assert(cost(&reversed,&a,1)>200);printf("Temporal costs: speed=%u, shift=%u, no-shift=%u, reversed=%u\n",cost(&fast,&a,1),cost(&shifted,&a,1),cost(&shifted,&a,0),cost(&reversed,&a,1));
 st_label*l=&library.labels[0];l->present=true;l->shift_limit=1;strcpy(l->name,"Door");l->next_id=3;l->examples[0]=a;l->examples[1]=fast;l->examples[1].id=2;assert(st_label_valid(l));
 st_match_begin(&matcher,&shifted);while(matcher.running)st_match_tick(&matcher,&library,8);assert(matcher.selected==0&&matcher.shift[0]==1&&matcher.reason==ST_RESULT_MATCH);
 l->examples[2]=shifted;l->examples[2].id=3;l->examples[2].kind=ST_NEGATIVE;l->next_id=4;st_match_begin(&matcher,&shifted);while(matcher.running)st_match_tick(&matcher,&library,64);assert(matcher.reason==ST_RESULT_NEGATIVE&&matcher.selected==-1);
 memset(&l->examples[2],0,sizeof(st_example));library.labels[1]=*l;strcpy(library.labels[1].name,"Other door");st_match_begin(&matcher,&a);while(matcher.running)st_match_tick(&matcher,&library,64);assert(matcher.reason==ST_RESULT_AMBIGUOUS);
 size_t n=st_bank_encode(&library,0,bytes,sizeof(bytes));assert(n>ST_BANK_MIN&&n<ST_BANK_MAX);assert(st_bank_decode(&decoded,0,bytes,n));assert(!memcmp(&library,&decoded,sizeof(library)));before=decoded;
 for(unsigned i=0;i<n;i+=17){bytes[i]^=1;assert(!st_bank_decode(&decoded,0,bytes,n));assert(!memcmp(&decoded,&before,sizeof(decoded)));bytes[i]^=1;}assert(!st_bank_decode(&decoded,1,bytes,n));assert(!st_bank_decode(&decoded,0,bytes,n-1));
 st_segmenter segment;st_segment_reset(&segment);st_frame quiet={.level_db=-12000};for(unsigned i=0;i<8;i++)st_segment_observe(&segment,&quiet);for(unsigned i=0;i<a.count;i++)st_segment_observe(&segment,&a.frames[i]);for(unsigned i=0;i<4;i++)st_segment_observe(&segment,&quiet);assert(segment.ready&&segment.event.pre==4&&segment.event.count==24&&!segment.event.flags);assert(st_example_valid(&segment.event));
 st_segment_reset(&segment);for(unsigned i=0;i<80;i++)st_segment_observe(&segment,&a.frames[2]);assert(segment.ready&&segment.event.count==64&&(segment.event.flags&ST_CLIPPED));
 /* Full legal catalog is bounded; no hidden eviction and deletion shrinks it. */
 for(unsigned i=0;i<4;i++){library.labels[i]=*l;library.labels[i].next_id=7;for(unsigned j=0;j<6;j++){library.labels[i].examples[j]=segment.event;library.labels[i].examples[j].id=j+1;}}
 n=st_bank_encode(&library,0,bytes,sizeof(bytes));assert(n==ST_BANK_MAX&&n<65536);assert(st_bank_decode(&decoded,0,bytes,n));memset(&library.labels[0].examples[0],0,sizeof(st_example));assert(st_bank_encode(&library,0,bytes,sizeof(bytes))==n-64*38);
 printf("Temporal models: ambient rejection/excess/hysteresis, bounded learning, duration/attack/decay/impacts/flux, positive/negative/ambiguous matching, pitch evidence, time-warp budget, pre-onset context, explicit clipping and atomic codec passed; library RAM %zu, maxbank %u bytes\n",sizeof(library),ST_BANK_MAX);
}
