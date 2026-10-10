/* Actual production draw() passes complete H:MM:SS and separate centiseconds. */
#define PORTABLE_NOVA_UI
#define main original_stopwatch_fixture_main
#include "stopwatch_test.c"
#undef main
static char whole_label[16],fraction_label[16];
void portable_nova_begin(void){}
void portable_nova_header(const char *title){assert(!strcmp(title,"STOPWATCH"));}
void portable_nova_button(int x,int y,int w,int h,const char *text,bool selected){(void)x;(void)y;(void)w;(void)h;(void)text;(void)selected;}
bool portable_nova_hit(int x,int y,int left,int top,int w,int h){return x>=left&&y>=top&&x<left+w&&y<top+h;}
void portable_nova_center(unsigned face,int x,int y,int w,const char *text,uint32_t color){
 (void)face;(void)x;(void)w;(void)color;
 if(y==68)snprintf(whole_label,sizeof(whole_label),"%s",text);
 if(y==111)snprintf(fraction_label,sizeof(fraction_label),"%s",text);
}
int main(void){const unsigned hours[]={0,1,9,10,12,23,99};
 for(unsigned i=0;i<sizeof(hours)/sizeof(hours[0]);i++){
  setup(true);app=&fake_app;loaded=true;clock_state=(sw_clock){.elapsed_ms=hours[i]*3600000u+123450u};
  sw_clock saved_clock=clock_state;char expected[16];
#ifdef PORTABLE_UNPADDED_HOURS
  snprintf(expected,sizeof(expected),"%u:02:03",hours[i]);
#else
  snprintf(expected,sizeof(expected),"%02u:02:03",hours[i]);
#endif
  draw();assert(!strcmp(whole_label,expected)&&!strcmp(fraction_label,".45"));
  assert(!memcmp(&saved_clock,&clock_state,sizeof(clock_state))&&!writes&&!reads&&!storage_gets&&frames==1);
 }
 puts("Actual Stopwatch draw:0/1/9/10/12/23/99 hours, padded minutes/seconds/centiseconds and unchanged state PASS");
}
