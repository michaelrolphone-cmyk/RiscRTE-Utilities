#define main prior_audio_controller_main
#include "audio_spectrum_test.c"
#undef main
#include <math.h>
static int16_t spoken[176000];
static unsigned sample_at,pass,step,voices,divisor;
static uint32_t expected[1000];
static bool spoken_read(void*c,int16_t*p,size_t cap,size_t*got){
 assert(c==&mic&&live&&cap==256);reads++;now+=16;
 for(unsigned i=0;i<256;i++,sample_at++){
  int value=sample_at>=40960&&sample_at<216960?spoken[sample_at-40960]/(int)divisor:0;
  value+=(int)(300*sin(sample_at*6.283185307179586*80/16000.));p[i]=(int16_t)value;
 }*got=256;return true;
}
static void voice_setup(void){
 signatures[0]=(spectrum_signature){.kind=SPECTRUM_SIGNATURE_ROOM,.frames=64};strcpy(signatures[0].name,"Room");
 for(unsigned i=0;i<128;i++){signatures[0].sums[i]=6400;signature_means[0][i]=100;}
 manual_room=0;signature_mode=pass==1?2:pass==2?1:0;
 prefs.fft_size=pass==1?8192:256;prefs.gain_db=pass==1?60:-24;configure_dsp();
}
static void voice_compare(void){
 uint32_t state=((uint32_t)speech.active<<24)|((uint32_t)speech.confidence<<16)|(uint16_t)speech.amplitude_db;
 if(!pass)expected[step]=state;else if(pass<3)assert(expected[step]==state);step++;
 if(speech.active){monitor_item items[17];unsigned n=monitor_items(items),voiced=0;for(unsigned i=0;i<n;i++)voiced+=items[i].kind==MONITOR_VOICE;assert(n<=17&&voiced==1);voices++;}
}
static void voice_stopped(void){
 assert(!running&&!portable_audio_capture_active());monitor_item items[17];unsigned n=monitor_items(items);
 for(unsigned i=0;i<n;i++)assert(items[i].kind!=MONITOR_VOICE);
}
int main(int argc,char**argv){
 assert(argc>=3);FILE*f=fopen(argv[2],"rb");assert(f);for(unsigned i=0;i<176000;i++){int a=fgetc(f),b=fgetc(f);assert(a>=0&&b>=0);spoken[i]=(int16_t)((unsigned)a|((unsigned)b<<8));}assert(fgetc(f)==EOF&&!ferror(f));assert(!fclose(f));
 for(pass=0;pass<5;pass++){
  reset();sample_at=step=voices=0;divisor=pass==3?1:pass==4?8:4;mic.read=spoken_read;
  check(voice_setup);start();for(unsigned i=0;i<990;i++)check(voice_compare);add(EVENT_INPUT,T5_APP_BUTTON_CONFIRM,1);check(voice_stopped);back();run();
  assert(voices>20&&!live&&!grant_live&&!store_live&&opens==1&&closes==1);
  printf("Genuine speech pass%u gain1/%u voiced observations%u/990\n",pass,divisor,voices);
 }
 puts("Genuine speech: three intensities, identical RAW/manual/AUTO/FFT/gain state traces, one uncertain Monitor row, silence clearing and explicit Stop cleanup PASS");return 0;
}
