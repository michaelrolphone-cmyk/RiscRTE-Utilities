#define ALARM_VOLUME_CONTROL
#define main legacy_alarm_main
#include "alarm_service_test.c"
#undef main
static void volume(unsigned v){blobs[5][0]=(uint8_t)v;sizes[5]=1;}
static void active_alarm(unsigned v,unsigned output){boot(true);schedule(1,1,101);mode(output);if(v<=255)volume(v);rtc_base=101;until(ALARM_STATE_ALERT);pump(12);}
static void before_gain(void){boot(true);schedule(1,1,101);mode(3);volume(40);rtc_base=101;for(unsigned i=0;i<100&&phase!=SET_AUDIO_GAIN;i++)pump(1);assert(phase==SET_AUDIO_GAIN&&opens==1&&!gains&&!writes&&!effects&&audio_uncertain);}
int main(void){
 /* Missing defaults to 50 with no preference write and full-scale source PCM. */
 active_alarm(256,3);assert(last_gain==100&&gains==1&&last_peak==16383&&effects>0&&!sizes[5]);
 for(unsigned v=10;v<=100;v+=10){active_alarm(v,3);assert(last_gain==100&&gains==1&&last_peak==32767*v/100&&effects>0);}
 active_alarm(0,3);assert(!opens&&!writes&&!gains&&effects>0);
 active_alarm(0,2);assert(!opens&&!writes&&!gains&&!effects);
 active_alarm(80,1);assert(!opens&&!gains&&effects>0);
 boot(true);volume(101);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&!opens&&!effects);volume(50);client->refresh(NULL);until(ALARM_STATE_READY);
 boot(true);volume(50);sizes[5]=2;until(ALARM_STATE_BLOCKED);assert(!opens&&!effects);
 boot(true);get_fail=true;until(ALARM_STATE_BLOCKED);assert(!opens&&!effects);
 boot(true);schedule(1,1,101);mode(3);volume(70);rtc_base=101;gain_fail=true;until(ALARM_STATE_ALERT);pump(12);until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_OUTPUT&&closes&&!writes&&!effects&&!snapshot().output_uncertain);
 /* Read failure at the exact preference phase never chooses default. */
 boot(true);volume_read_fail=true;until(ALARM_STATE_BLOCKED);assert(snapshot().error==ALARM_STORAGE&&gets==4&&!opens&&!effects);volume_read_fail=false;client->refresh(NULL);until(ALARM_STATE_READY);
 before_gain();alarm_token_v1 t=snapshot().occurrence;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(!gains&&!writes&&!effects&&closes);
 before_gain();ms+=ALARM_INVOCATION_MS;until(ALARM_STATE_READY);assert(!gains&&!writes&&!effects&&closes);
 before_gain();alarm_sleep_v1 plan={.struct_size=sizeof(plan)};assert(client->prepare_sleep(NULL,&plan)==ALARM_PENDING&&phase==SET_AUDIO_GAIN&&!gains);
 int32_t stopped=ALARM_PENDING;for(unsigned i=0;i<3&&stopped==ALARM_PENDING;i++)stopped=client->stop_only(NULL);assert(stopped==ALARM_OK&&!gains&&!writes&&!effects&&!audio_uncertain);int calls=gets+put_count+reads;pump(5);assert(calls==gets+put_count+reads);
 before_gain();t=snapshot().occurrence;close_fail=true;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_BLOCKED);assert(audio_uncertain&&!gains&&!writes&&!effects);close_fail=false;assert(client->acknowledge(NULL,&t)==ALARM_PENDING);until(ALARM_STATE_READY);assert(!audio_uncertain&&!gains&&!writes&&!effects);
 /* Reboot reads the explicitly selected value; it never resets it to default. */
 active_alarm(30,2);boot(false);until(ALARM_STATE_ALERT);pump(12);assert(last_gain==100&&blobs[5][0]==30);
 /* Countdown uses the same sound preference while keeping vibration independent. */
 boot(true);schedule(2,1,101);mode(3);volume(90);rtc_base=101;until(ALARM_STATE_ALERT);pump(12);assert(last_gain==100&&last_peak==29490&&effects);
 assert(driver_api->quiesce());twatch_audio_out_api_v1 no_gain=aapi;no_gain.set_gain=NULL;risc_provider_dependency_v1 invalid[5];memcpy(invalid,deps,sizeof(invalid));invalid[4].api=&no_gain;assert(!driver_api->start(invalid,5));
 puts("Alarm volume default50,0-100,full-scalePCM,persisted selection,mute,vibration,failures and gain admission passed");
 return 0;
}
