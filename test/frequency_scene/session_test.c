#include "frequency_session.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static frequency_session session;
static unsigned opens,writes,closes,gains,silences,retains;
static bool open_ok=true,write_ok=true,close_ok=true,lost,retained;
static unsigned lose_at;
static int16_t last_pcm[TONE_FRAMES];
static void retain(void*c){assert(c==&session);if(!retained){retained=true;++retains;}}
/* Same exact-identity responsibility as the scene owner: callback failure
 * latches invocation retention before returning false to the pure session. */
static bool alive(void*c){assert(c==&session);if(lost){retain(c);return false;}return !retained;}
static void boundary(unsigned at){assert(!retained);if(lose_at==at)lost=true;}
static bool open_audio(void*c,uint32_t rate,uint8_t channels){assert(c==&session&&rate==16000&&channels==1);++opens;boundary(1);return open_ok;}
static bool write_audio(void*c,const int16_t*p,size_t n){assert(c==&session&&p&&n==256);++writes;memcpy(last_pcm,p,sizeof(last_pcm));boundary(2);return write_ok;}
static bool close_audio(void*c){assert(c==&session);++closes;boundary(3);return close_ok;}
static bool gain(void*c,uint16_t a,uint16_t b){(void)c;(void)a;(void)b;++gains;assert(false);return false;}
static bool silence(void*c){(void)c;++silences;assert(false);return false;}
static twatch_audio_out_api_v1 output={1,sizeof(output),&session,open_audio,write_audio,gain,silence,close_audio};
static unsigned io(void){return opens+writes+closes+gains+silences;}
static void no_more_io(void){unsigned n=io();assert(!frequency_start(&session,&output,0));assert(!frequency_step(&session,0));assert(!frequency_edit(&session,1,1));assert(!frequency_stop(&session));assert(io()==n);}
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];frequency_init(&session,alive,retain,&session);
 assert(session.hz==440&&session.level==100&&session.step==100&&!session.playing&&!session.owned);
 if(!strcmp(mode,"default-idle")){assert(frequency_step(&session,99999)&&frequency_stop(&session)&&!io());}
 else if(!strcmp(mode,"continuous")){assert(frequency_start(&session,&output,0));for(unsigned i=0;i<100;i++)assert(frequency_step(&session,i*16));assert(opens==1&&writes==100&&!closes&&session.playing);assert(frequency_stop(&session)&&closes==1);}
 else if(!strcmp(mode,"phase-edit")){assert(frequency_start(&session,&output,0)&&frequency_step(&session,1));uint32_t phase=session.tone.phase;uint16_t level=session.tone.gain;assert(frequency_edit(&session,100,-1));assert(session.tone.phase==phase&&session.tone.gain==level&&session.hz==540&&session.level==99);assert(frequency_step(&session,2)&&opens==1&&!closes);assert(frequency_stop(&session));}
 else if(!strcmp(mode,"clamps")){assert(frequency_edit(&session,INT_MAX,INT_MAX)&&session.hz==7000&&session.level==100);assert(frequency_edit(&session,INT_MIN,INT_MIN)&&session.hz==20&&!session.level);assert(frequency_start(&session,&output,0)&&frequency_step(&session,1));for(unsigned i=0;i<256;i++)assert(last_pcm[i]==0);assert(frequency_stop(&session));}
 else if(!strcmp(mode,"watchdog")||!strcmp(mode,"rollover")){uint32_t start=!strcmp(mode,"rollover")?UINT32_MAX-1000u:123u;assert(frequency_start(&session,&output,start));assert(frequency_step(&session,start+59999)&&session.playing&&writes==1);assert(frequency_step(&session,start+60000)&&!session.playing&&!session.owned&&closes==1);assert(frequency_step(&session,start+60001)&&writes==1&&opens==1);}
 else if(!strcmp(mode,"no-auto-resume")){assert(frequency_start(&session,&output,0)&&frequency_stop(&session));for(unsigned i=0;i<10;i++)assert(frequency_step(&session,i)&&frequency_stop(&session));assert(opens==1&&closes==1&&!writes);assert(frequency_start(&session,&output,100)&&session.started==100&&session.tone.phase==0);assert(frequency_stop(&session));}
 else if(!strcmp(mode,"failed-open")){open_ok=false;assert(!frequency_start(&session,&output,0));assert(opens==1&&closes==1&&!session.playing&&!session.owned&&!retained);assert(frequency_step(&session,1)&&!writes);}
 else if(!strcmp(mode,"failed-open-close")){open_ok=close_ok=false;assert(!frequency_start(&session,&output,0));assert(opens==1&&closes==1&&session.owned&&session.terminal&&retains==1);no_more_io();}
 else if(!strcmp(mode,"failed-close")){assert(frequency_start(&session,&output,0));close_ok=false;assert(!frequency_stop(&session));assert(session.owned&&session.terminal&&retains==1&&closes==1);no_more_io();}
 else if(!strcmp(mode,"failed-write")){assert(frequency_start(&session,&output,0));write_ok=false;assert(frequency_step(&session,1));assert(!session.playing&&!session.owned&&writes==1&&closes==1&&!retained);assert(frequency_step(&session,2)&&writes==1);}
 else if(!strcmp(mode,"failed-write-close")){assert(frequency_start(&session,&output,0));write_ok=close_ok=false;assert(!frequency_step(&session,1)&&session.terminal&&retains==1&&closes==1);no_more_io();}
 else if(!strcmp(mode,"duplicate-start")){assert(frequency_start(&session,&output,0));assert(!frequency_start(&session,&output,1)&&session.terminal&&retains==1&&opens==1&&!closes);no_more_io();}
 else if(!strcmp(mode,"invalid-output")){twatch_audio_out_api_v1 bad=output;assert(!frequency_start(&session,NULL,0));bad.api_version=2;assert(!frequency_start(&session,&bad,0));bad=output;bad.struct_size=sizeof(bad)-1;assert(!frequency_start(&session,&bad,0));bad=output;bad.open=NULL;assert(!frequency_start(&session,&bad,0));bad=output;bad.write=NULL;assert(!frequency_start(&session,&bad,0));bad=output;bad.close=NULL;assert(!frequency_start(&session,&bad,0));assert(!io()&&!retained);}
 else if(!strcmp(mode,"replacement-before")){lost=true;no_more_io();assert(retains==1&&!io());}
 else if(!strcmp(mode,"replacement-open")){lose_at=1;assert(!frequency_start(&session,&output,0)&&session.owned&&retains==1&&!closes);no_more_io();}
 else if(!strcmp(mode,"replacement-write")){assert(frequency_start(&session,&output,0));lose_at=2;assert(!frequency_step(&session,1)&&retains==1&&writes==1&&!closes);no_more_io();}
 else if(!strcmp(mode,"replacement-close")){assert(frequency_start(&session,&output,0));lose_at=3;assert(!frequency_stop(&session)&&retains==1&&closes==1&&session.owned);no_more_io();}
 else {assert(false);}
 assert(!gains&&!silences);printf("frequency session %s PASS\n",mode);return 0;
}
