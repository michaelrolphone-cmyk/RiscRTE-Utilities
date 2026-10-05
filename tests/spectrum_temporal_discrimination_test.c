#include <assert.h>
#include <stdio.h>
#include "../Apps/spectrum_temporal.h"
static st_example tone_event(unsigned tonebin,unsigned repeats,bool broadband,unsigned gain){
 st_segmenter segment={0};uint32_t power[128]={0},previous[128]={0};
 for(unsigned i=0;i<4;i++){st_frame f=st_frame_make(power,previous,false);st_segment_observe(&segment,&f);}
 static const unsigned envelope[8]={100,80,60,40,20,5,1,1};
 for(unsigned i=0;i<8*repeats;i++){for(unsigned b=0;b<128;b++)power[b]=broadband?envelope[i%8]*gain:0;power[tonebin]=envelope[i%8]*gain*100;st_frame f=st_frame_make(power,previous,true);st_segment_observe(&segment,&f);memcpy(previous,power,sizeof(previous));}
 for(unsigned i=0;i<4;i++){memset(power,0,sizeof(power));st_frame f=st_frame_make(power,previous,false);st_segment_observe(&segment,&f);memcpy(previous,power,sizeof(previous));}
 assert(st_example_valid(&segment.event));return segment.event;
}
static unsigned similarity(const st_example*a,const st_example*b,unsigned shift){st_dtw d;st_dtw_start(&d,a,b,shift);while(!d.done)st_dtw_step(&d,a,b,4);return 1000-d.cost;}
static st_example ringing_tail(unsigned attack_bin){
 st_segmenter s={0};uint32_t p[128]={0},previous[128]={0};st_frame f=st_frame_make(p,previous,false);for(unsigned i=0;i<4;i++)st_segment_observe(&s,&f);
 for(unsigned i=0;i<12;i++){if(!i){for(unsigned k=0;k<128;k++)p[k]=100000;p[attack_bin]+=10000;}else{memset(p,0,sizeof(p));p[15]=1000000/i;}f=st_frame_make(p,previous,true);st_segment_observe(&s,&f);memcpy(previous,p,sizeof(p));}
 for(unsigned i=0;i<4;i++){memset(p,0,sizeof(p));f=st_frame_make(p,previous,false);st_segment_observe(&s,&f);memcpy(previous,p,sizeof(p));}assert(st_example_valid(&s.event));return s.event;
}
static st_example background_event(unsigned gain){
 spectrum_background b={0};st_segmenter s={0};uint32_t p[128]={0},previous[128]={0};for(unsigned i=0;i<72;i++)spectrum_background_observe(&b,p);for(unsigned i=0;i<4;i++){st_frame f=st_frame_make(b.excess,previous,b.foreground);st_segment_observe(&s,&f);}unsigned amplitude[]={100,80,60,40,20,5,1,1};
 for(unsigned i=0;i<12;i++){p[15]=i<8?amplitude[i]*gain:0;spectrum_background_observe(&b,p);spectrum_background_observe(&b,p);st_frame f=st_frame_make(b.excess,previous,b.foreground);memcpy(previous,b.excess,sizeof(previous));st_segment_observe(&s,&f);}assert(st_example_valid(&s.event));return s.event;
}
int main(void){
 st_example a=tone_event(15,1,true,1000),different=tone_event(63,1,true,1000);assert(a.peak_hz==984&&different.peak_hz==3984);assert(st_tonal_example(&a)&&st_tonal_example(&different));assert(similarity(&a,&different,0)<ST_MATCH_MIN&&similarity(&a,&different,2)<ST_MATCH_MIN);
 st_example one=tone_event(15,1,false,1000),two=tone_event(15,2,false,1000);assert(one.impacts==1&&two.impacts==2);assert(similarity(&one,&two,1)<ST_MATCH_MIN);
 st_example far=tone_event(15,1,false,1);assert(one.peak_db-far.peak_db>=2900);assert(similarity(&one,&far,1)>=ST_MATCH_MIN);
 st_example tail_a=ringing_tail(15),tail_b=ringing_tail(63);assert(tail_a.peak_hz!=tail_b.peak_hz&&st_frequency_evidence(&tail_a)==st_frequency_evidence(&tail_b));assert(similarity(&tail_a,&tail_b,1)>=ST_MATCH_MIN);
 st_example loud=background_event(100000),quiet=background_event(100);assert(similarity(&loud,&quiet,1)>=ST_MATCH_MIN);unsigned coarse;st_choose_shift(&loud,&quiet,1,&coarse);assert(coarse==0);
 st_example stopped=one;stopped.flags=ST_CLIPPED;assert(st_example_valid(&stopped));assert(similarity(&stopped,&one,1)<ST_MATCH_MIN);stopped.flags|=ST_CONFIRMED_END;assert(st_example_valid(&stopped)&&similarity(&stopped,&one,1)>=ST_MATCH_MIN);stopped.flags=ST_CONFIRMED_END;assert(!st_example_valid(&stopped));
 st_frame invalid=one.frames[one.pre];invalid.peak_hz=0;assert(!st_frame_valid(&invalid));invalid=one.frames[one.pre];memset(invalid.shape,0,sizeof(invalid.shape));assert(!st_frame_valid(&invalid));
 uint32_t full[128],zero[128]={0};for(unsigned i=0;i<128;i++)full[i]=UINT32_MAX;st_frame f=st_frame_make(full,zero,true);assert(st_frame_valid(&f)&&f.flux==255);assert(st_shape_distance(&f,&f,0)==0);for(int shift=-2;shift<=2;shift++)assert(st_shape_distance(&f,&f,shift)<=1000);
 puts("Temporal discrimination: octave mismatch under broadband noise rejected, single/double impacts remain distinct,30dB intensity tolerance retained, tonal evidence validated and full-scale weighted arithmetic bounded");
}
