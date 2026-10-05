#define POINTS_IN_TIME_SERVICE
/* Production provider fixture: Points are short non-modal cues, not alarms. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Services/alarm_service/service.c"
static uint8_t blobs[7][64];static uint32_t sizes[7];
static uint64_t ms;static uint32_t rtc_base;static int opens,writes,effects,stops,silences,closes,puts;
static bool put_fail,put_persists,write_fail,effect_fail,revoked;
static const alarm_service_v1 *client;static const risc_driver_v2 *driver_api;
static int index_key(const char *key){const char *keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,POINTS_CONFIG_KEY,POINTS_OCCURRENCE_KEY};for(int i=0;i<7;i++)if(!strcmp(keys[i],key))return i;assert(0);return 0;}
static int32_t get_blob(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(!revoked);int i=index_key(k);*n=0;if(!sizes[i])return -1;if(cap<sizes[i])return -2;memcpy(b,blobs[i],sizes[i]);*n=sizes[i];return 0;}
static int32_t put_blob(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!revoked);int i=index_key(k);assert(i==3||i==4||i==6);puts++;if(!put_fail||put_persists){memcpy(blobs[i],b,n);sizes[i]=n;}return put_fail?-5:0;}
static uint64_t mono(void*c){(void)c;return ms;}
static bool read_rtc(void*c,twatch_rtc_time_v1*out){(void)c;return points_calendar(rtc_base+(uint32_t)(ms/1000),out);}
static bool h_effect(void*c,uint8_t e){(void)c;assert(e);effects++;return !effect_fail;}
static bool h_stop(void*c){(void)c;stops++;return true;}
static bool a_open(void*c,uint32_t r,uint8_t n){(void)c;assert(r==8000&&n==1);opens++;return true;}
static bool a_write(void*c,const int16_t*p,size_t n){(void)c;assert(n==256);bool audible=false;for(size_t i=0;i<n;i++)audible|=p[i]!=0;assert(audible);writes++;return !write_fail;}
static bool a_gain(void*c,uint16_t g,uint16_t m){(void)c;(void)g;(void)m;return true;}
static bool a_silence(void*c){(void)c;silences++;return true;}
static bool a_close(void*c){(void)c;closes++;return true;}
static const risc_bound_key_value_v1 bound={1,sizeof(bound),NULL,get_blob,put_blob};
static const risc_platform_clock_api_v1 clk={1,sizeof(clk),NULL,mono,NULL};
static const twatch_rtc_api_v1 rtc_api={2,sizeof(rtc_api),NULL,read_rtc,NULL,NULL,NULL};
static const twatch_haptic_api_v1 hapi={1,sizeof(hapi),NULL,h_effect,h_stop};
static const twatch_audio_out_api_v1 aapi={1,sizeof(aapi),NULL,a_open,a_write,a_gain,a_silence,a_close};
static const risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",1,&bound},{"platform.clock",1,&clk},{"rtc.clock",2,&rtc_api},{"haptic.effect",1,&hapi},{"audio.output",1,&aapi}};
static alarm_status_v1 snapshot(void){alarm_status_v1 s={.struct_size=sizeof(s)};assert(client->status(NULL,&s)==0);return s;}
static void pump(unsigned n){while(n--){(void)client->step(NULL);alarm_status_v1 s=snapshot();if(active&&selected==2){assert(s.state!=ALARM_STATE_ALERT&&s.state!=ALARM_STATE_DISMISSING);assert(!s.occurrence.generation&&!s.label[0]);}ms++;}}
static void boot(bool clear){if(driver_api)assert(driver_api->quiesce());if(clear){memset(blobs,0,sizeof(blobs));memset(sizes,0,sizeof(sizes));ms=0;}put_fail=put_persists=write_fail=effect_fail=revoked=false;opens=writes=effects=stops=silences=closes=puts=0;driver_api=t5_driver_get(2);client=driver_api->capability;assert(driver_api->start(deps,5));}
static uint32_t civil(unsigned y,unsigned m,unsigned d,unsigned h,unsigned minute){twatch_rtc_time_v1 t={(uint16_t)y,(uint8_t)m,(uint8_t)d,0,(uint8_t)h,(uint8_t)minute,0};portable_time_candidate c[2];assert(portable_time_inverse(&t,c)==1);uint32_t out;assert(alarm_calendar_seconds(c[0].rtc.year,c[0].rtc.month,c[0].rtc.day,c[0].rtc.hour,c[0].rtc.minute,0,&out));return out;}
static points_config catalog(uint32_t now,unsigned kind,unsigned duration,unsigned mode,bool end,bool warn){points_config c={.revision=1,.created=now-3600};c.points[0]=(points_item){.kind=(uint8_t)kind,.enabled=1,.mode=(uint8_t)mode,.weekdays=127,.hour=12,.minute=0,.duration_minutes=(uint16_t)duration,.notify_end=end,.warn3=warn};return c;}
static void save(points_config c){assert(points_config_valid(&c));points_config_encode(&c,blobs[5]);sizes[5]=64;}
static void settle_edge(unsigned edge,uint32_t deadline){for(unsigned i=0;i<3000;i++){pump(1);if(points_occ.state==ALARM_OCC_ACKED&&points_occ.edge==edge&&points_occ.deadline==deadline&&!active&&phase==IDLE)return;}fprintf(stderr,"edge=%u phase=%d active=%d state=%u deadline=%u\n",edge,phase,active,points_occ.state,points_occ.deadline);assert(0);}
static alarm_sleep_v1 sleep_plan(void){alarm_sleep_v1 p={.struct_size=sizeof(p)};for(unsigned i=0;i<200;i++){int r=client->prepare_sleep(NULL,&p);if(!r)return p;assert(r==ALARM_PENDING);pump(1);}assert(0);return p;}
int main(void){
 uint32_t noon=civil(2026,10,4,12,0);rtc_base=noon;
 /* Start, warning and end each produce one short non-modal tap/beep. */
 boot(true);blobs[2][0]=3;sizes[2]=1;save(catalog(noon,POINTS_LUNCH,30,3,true,true));settle_edge(POINTS_EDGE_START,noon);
 assert(opens==1&&writes==1&&effects==1&&closes>=1);assert(sleep_plan().deadline==noon+27*60);
 int o=opens,w=writes,h=effects;ms=(uint64_t)27*60*1000;settle_edge(POINTS_EDGE_WARNING,noon+27*60);assert(opens==o+1&&writes==w+1&&effects==h+1);
 assert(sleep_plan().deadline==noon+30*60);o=opens;w=writes;h=effects;ms=(uint64_t)30*60*1000;settle_edge(POINTS_EDGE_END,noon+30*60);assert(opens==o+1&&writes==w+1&&effects==h+1);
 assert(sleep_plan().deadline==noon+86400);
 /* END and warning are truly independent. */
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_BREAK,30,1,false,true));settle_edge(POINTS_EDGE_START,noon);assert(sleep_plan().deadline==noon+27*60);
 ms=(uint64_t)27*60*1000;settle_edge(POINTS_EDGE_WARNING,noon+27*60);assert(sleep_plan().deadline==noon+86400);
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_BREAK,30,1,true,false));settle_edge(POINTS_EDGE_START,noon);assert(sleep_plan().deadline==noon+30*60);
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_BREAK,30,1,false,false));settle_edge(POINTS_EDGE_START,noon);assert(sleep_plan().deadline==noon+86400);
 /* Both custom kinds schedule identically; output mode remains per point. */
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_CUSTOM_1,0,2,false,false));settle_edge(POINTS_EDGE_START,noon);assert(opens==1&&writes==1&&!effects);
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_CUSTOM_2,0,1,false,false));settle_edge(POINTS_EDGE_START,noon);assert(!opens&&!writes&&effects==1);
 /* Mode zero still resolves through Settings. */
 boot(true);rtc_base=noon;blobs[2][0]=3;sizes[2]=1;save(catalog(noon,POINTS_CUSTOM_1,0,0,false,false));settle_edge(POINTS_EDGE_START,noon);assert(opens&&writes&&effects);
 /* A failed cue remains durable pending and replays after restart, still without a modal. */
 boot(true);rtc_base=noon;save(catalog(noon,POINTS_BREAK,0,2,false,false));write_fail=true;for(unsigned i=0;i<100;i++){pump(1);if(phase==BLOCKED)break;}assert(points_occ.state==ALARM_OCC_PENDING);write_fail=false;boot(false);settle_edge(POINTS_EDGE_START,noon);
 /* Persisted cursor prevents duplicate cue after reboot. */
 int before=effects+opens;boot(false);pump(1000);assert(effects+opens==0);(void)before;
 /* Config bits round-trip and the ledger tracks three independent delivered bits. */
 points_config c=catalog(noon,POINTS_LUNCH,30,1,true,true);uint8_t b[64];points_config_encode(&c,b);points_config d;assert(points_config_decode(&d,b,64)&&d.points[0].notify_end&&d.points[0].warn3);
 points_ledger l={.revision=1,.generation=1};assert(points_ledger_mark(&l,0,100,POINTS_EDGE_START));assert(points_ledger_mark(&l,0,100,POINTS_EDGE_WARNING));assert(!points_ledger_handled(&l,0,100,POINTS_EDGE_END));
 puts("Points short non-modal start/warning/end cues, custom kinds, modes, sleep and replay passed");return 0;
}
