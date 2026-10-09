/* Fault only the transport/release boundary; the scanner, adapter and generic
 * sensor provider execute normally. Permanent retention exits only the test. */
#include <setjmp.h>
static jmp_buf probe_escape;
static unsigned probe_kind,probe_operation,probe_attempts,probe_waits,probe_sleeps;
static int probe_failures;
static void probe_io(void){assert(!probe_kind);}
static void probe_init(void){
 probe_operation=!strcmp(getenv("BLE_PROBE_OPERATION"),"close")?1u:2u;
 probe_failures=atoi(getenv("BLE_PROBE_FAILURES"));
}
static bool probe_refuse(unsigned kind){
 if(kind!=probe_operation){probe_io();return false;}
 assert(!probe_kind||probe_kind==kind);probe_attempts++;
 if(probe_failures<0||probe_attempts<=(unsigned)probe_failures){probe_kind=kind;return true;}
 probe_kind=0;return false;
}
static void probe_yield(uint32_t n){
 if(!probe_kind)return;
 assert(n==50);probe_waits++;
 if(probe_waits==12){
  assert(probe_failures<0&&probe_attempts>=12);
  longjmp(probe_escape,1);
 }
}
static void probe_finish(bool retained){
 assert(retained==(probe_failures<0));
 if(retained){assert(probe_kind&&grants);if(probe_operation==1)assert(radio_owned);}
 else {
  assert(!probe_kind&&probe_attempts>=(unsigned)probe_failures+1u);
  assert(probe_waits==(unsigned)probe_failures);
  if(getenv("BLE_PROBE_SLEEP"))assert(probe_sleeps==1);
 }
 printf("Scanner cleanup: attempts=%u waits=%u retained=%u sleep=%u PASS\n",
        probe_attempts,probe_waits,retained?1u:0u,probe_sleeps);
}
