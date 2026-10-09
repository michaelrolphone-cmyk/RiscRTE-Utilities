#define main preserved_alarm_fixture_main
#include "native_utc_alarm_adapter_test.c"
#undef main
int main(void){
 const unsigned hours[]={0,1,9,10,12,23,99};
 for(unsigned i=0;i<sizeof(hours)/sizeof(*hours);i++){
  unsigned value[]={hours[i],3,5};char got[24],expected[24];
#if DAILY_ALARM_KIND == 1
  if(hours[i]>23)continue;
  alarm_time_format=PORTABLE_TIME_FORMAT_24;
  snprintf(expected,sizeof(expected),"%u:03",hours[i]);alarm_value_text(got,sizeof(got),value);assert(!strcmp(got,expected));
  alarm_time_format=PORTABLE_TIME_FORMAT_12;
  snprintf(expected,sizeof(expected),"%u:03 %s",hours[i]%12?hours[i]%12:12,hours[i]<12?"AM":"PM");alarm_value_text(got,sizeof(got),value);assert(!strcmp(got,expected));
#else
  snprintf(expected,sizeof(expected),"%u:03:05",hours[i]);alarm_value_text(got,sizeof(got),value);assert(!strcmp(got,expected));
#endif
 }
 puts("Native alarm/countdown formatter: unpadded hours, preserved minute/second padding PASS");return 0;
}
