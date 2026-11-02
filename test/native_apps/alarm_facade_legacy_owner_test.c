/* Real unchanged scheduler, not a second simulated scheduler. */
#define ALARM_CLOCK_CHANGE_CONTROL
#define main original_alarm_fixture_main
#include "alarm_service_test.c"
#undef main
#include "AlarmServiceFacadeV2.h"
extern const risc_driver_v2 *alarm_facade_driver_get(uint32_t);
int main(void){
 assert(original_alarm_fixture_main()==0);
 boot(true);until(ALARM_STATE_READY);
 const risc_driver_v2*facade_driver=alarm_facade_driver_get(2);
 risc_provider_dependency_v1 dependency={ALARM_SERVICE_CAPABILITY,1,client};
 int old_puts=put_count,old_reads=reads,old_opens=opens,old_effects=effects;
 assert(facade_driver->start(&dependency,1));
 const alarm_service_v1*facade=facade_driver->capability;
 const alarm_service_descriptor_v2*descriptor=alarm_service_descriptor(facade);
 const alarm_clock_change_v1*editor=alarm_clock_change_control(client);
 assert(descriptor&&editor&&facade->context==client->context);
 assert(put_count==old_puts&&reads==old_reads&&opens==old_opens&&effects==old_effects);
 rtc_base=3700;assert(editor->clock_changed(client->context,rtc_base)==ALARM_PENDING);
 until(ALARM_STATE_READY);alarm_status_v1 observed={.struct_size=sizeof(observed)};
 assert(facade->status(facade->context,&observed)==ALARM_OK&&observed.rtc_seconds==rtc_base);
 alarm_sleep_v1 ticket={.struct_size=sizeof(ticket)};
 assert(facade->prepare_sleep(facade->context,&ticket)==ALARM_PENDING);pump(8);
 assert(facade->prepare_sleep(facade->context,&ticket)==ALARM_OK);
 rtc_base+=60;assert(descriptor->resume_sleep(facade->context,&ticket)==ALARM_OK);
 assert(editor->sleep.resume_sleep(client->context,&ticket)==ALARM_STALE);
 until(ALARM_STATE_READY);
 old_reads=reads;old_puts=put_count;old_opens=opens;old_effects=effects;
 assert(facade_driver->quiesce());facade_driver->stop();
 assert(reads==old_reads&&put_count==old_puts&&opens==old_opens&&effects==old_effects);
 assert(client->status(client->context,&observed)==ALARM_OK);
 puts("Real legacy owner: original consumers, verified RTC edit, shared ticket single-use and no facade I/O PASS");
 return 0;
}
