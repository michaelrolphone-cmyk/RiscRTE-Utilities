#define main original_peripheral_main
#define grants fixture_grants
#include "nova/peripherals.h"
#undef grants
#undef main
#include "../../Apps/alarms.c"
static unsigned volume_puts;
static bool volume_put_error,volume_read_error,volume_commit_error;
static int32_t volume_test_get(void *c,const char *key,void *data,uint32_t cap,uint32_t *size) {
    if(!strcmp(key,ALARM_VOLUME_KEY) && volume_read_error && volume_puts){*size=0;return RISC_KEY_VALUE_IO;}
    return fake_get(c,key,data,cap,size);
}
static int32_t volume_test_put(void *c,const char *key,const void *data,uint32_t size) {
    assert(!strcmp(key,ALARM_VOLUME_KEY) && size==1);volume_puts++;
    if(!volume_put_error || volume_commit_error)(void)fake_put(c,key,data,size);
    return volume_put_error?RISC_KEY_VALUE_IO:0;
}
static const risc_key_value_v1 volume_test_api={1,sizeof(volume_test_api),NULL,volume_test_get,volume_test_put};
static void direct_start(void) {
    assert(app_module_init()==0);app=t5_app_get_api(1);assert(open_dependencies());alarm_preferences=&volume_test_api;
    (void)alarm_writer_load(&writer,storage,1);refresh_status();volume_uncertain=false;volume_load();alarm_page=0;draw();
}
int main(int argc,char **argv) {
    assert(argc==3);directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);memset(pixels,0xa5,sizeof(pixels));
    if(scenario==1){uint8_t v=80;fake_put(NULL,ALARM_VOLUME_KEY,&v,1);}
    if(scenario==4){uint8_t v=101;fake_put(NULL,ALARM_VOLUME_KEY,&v,1);}
    direct_start();
    assert(volume_value==(scenario==1?80:ALARM_VOLUME_DEFAULT));assert(volume_valid==(scenario!=4));assert(!volume_puts);
    assert(alarm_tap(120,140) && alarm_page==3);draw();
    if(scenario==0 || scenario==1){assert(alarm_tap(200,118));assert(alarm_tap(174,206));assert(alarm_page==0 && volume_value==(scenario==1?90:60) && volume_puts==1);}
    else if(scenario==2){assert(alarm_tap(200,118));assert(alarm_tap(50,206));assert(alarm_page==0 && volume_value==50 && !volume_puts);}
    else if(scenario==3){for(unsigned i=0;i<5;i++)assert(alarm_tap(40,118));assert(!volume_choice);assert(alarm_tap(174,206));assert(!volume_value && volume_valid);}
    else if(scenario==4){assert(alarm_tap(40,118));assert(volume_valid && volume_choice==40);assert(alarm_tap(174,206));assert(volume_value==40);}
    else if(scenario==5 || scenario==6) {
        assert(alarm_tap(200,118));volume_put_error=true;volume_commit_error=scenario==6;volume_read_error=scenario==6;
        assert(alarm_tap(174,206));assert(volume_uncertain && volume_choice==60 && volume_value==50);draw();
        assert(!alarm_back());assert(!alarm_tap(200,118));assert(volume_choice==60 && volume_puts==1);
        volume_put_error=volume_read_error=false;assert(alarm_tap(174,206));assert(!volume_uncertain && volume_value==60 && volume_puts==2);
    } else if(scenario==7){for(unsigned i=0;i<20;i++)assert(alarm_tap(200,118));assert(volume_choice==100);for(unsigned i=0;i<20;i++)assert(alarm_tap(40,118));assert(volume_choice==0);assert(alarm_back() == false && alarm_page==0 && !volume_puts);}
    else if(scenario==8) {
        for(unsigned format=0;format<2;format++)for(unsigned hour=0;hour<24;hour++) {
            alarm_time_format=format;values[0]=hour;values[1]=59;alarm_page=2;alarm_field=0;draw();
            unsigned expected=format==PORTABLE_TIME_FORMAT_24?(hour+1)%24:(hour/12)*12+(hour%12+1)%12;
            assert(alarm_tap(200,118)&&values[0]==expected);
            assert(alarm_tap(40,118)&&values[0]==hour);
            if(format==PORTABLE_TIME_FORMAT_12) {
                assert(alarm_tap(60,206)&&values[0]==(hour+12)%24);
                assert(alarm_tap(60,206)&&values[0]==hour);
            }
            char actual[40],expected_text[40];alarm_value_text(actual,sizeof(actual),values);
            if(format==PORTABLE_TIME_FORMAT_24)snprintf(expected_text,sizeof(expected_text),"%02u:59",hour);
            else snprintf(expected_text,sizeof(expected_text),"%u:59 %s",hour%12?hour%12:12,hour<12?"AM":"PM");
            assert(!strcmp(actual,expected_text));assert(!writer.saved.revision&&!volume_puts);
        }
        alarm_time_format=PORTABLE_TIME_FORMAT_12;values[0]=12;alarm_page=2;alarm_field=0;
    }
    else assert(!"Unknown volume scenario");
    draw();assert(!writer.saved.enabled && !writer.saved.revision);uint8_t mode;uint32_t size;assert(fake_get(NULL,"alert_mode",&mode,1,&size)==RISC_KEY_VALUE_NOT_FOUND);
    close_dependencies();app_module_fini();assert(!fixture_grants && !subs && !frames);
    printf("Alarm volume scenario %u:50%% default/persistence/uncertainty/mute and independent vibration passed\n",scenario);return 0;
}
