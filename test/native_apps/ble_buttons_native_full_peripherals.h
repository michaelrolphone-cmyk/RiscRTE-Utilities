/* Full-feature host service doubles. Actual System and Watch bridge sources are
 * linked separately; this file owns only copied provider API responses. */
#include "TelemetryBroadcastV1.h"
#include "ContextsServiceV1.h"
#include "RiscPowerKeyV1.h"
#include "RiscPowerSleepV1.h"
#include "RiscDisplayPowerV1.h"
#include "PortableMotionTap.h"
#include "utility_atomic_transport.h"
static unsigned full_retains,full_sleep_entries,full_denials;
static bool full_back_sent;
static bool full_yes(void*c){(void)c;return true;}
static bool full_key_events(void*c,uint32_t*e){(void)c;*e=0;if((getenv("HID_RENDER_FULL_BACK")||getenv("HID_RENDER_ALARM_BACK"))&&polls>=(getenv("HID_RENDER_ALARM_BACK")?60u:100u)&&!full_back_sent){*e=RISC_POWER_KEY_SHORT_PRESS;full_back_sent=true;}return true;}
static const risc_power_key_api_v1 full_key={1,sizeof(full_key),NULL,full_key_events};
static bool full_tx_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;(void)allow;(void)p;return true;}
static bool full_tx_status(void*c,telemetry_broadcast_status_v1*s){(void)c;*s=(telemetry_broadcast_status_v1){.struct_size=sizeof(*s),.settings_valid=true};return true;}
static int32_t full_tx_enum(void*c,uint32_t n,risc_telemetry_field_v1*out){(void)c;(void)n;(void)out;return 0;}
static int32_t full_tx_read(void*c,uint32_t n,int32_t*out){(void)c;(void)n;(void)out;return 0;}
static const telemetry_broadcast_v1 full_tx={1,sizeof(full_tx),NULL,full_tx_step,full_yes,full_tx_status,full_tx_enum,full_tx_read};
static bool full_context_step(void*c,const contexts_policy_v1*p){(void)c;(void)p;return true;}
static bool full_context_status(void*c,contexts_status_v1*s){(void)c;*s=(contexts_status_v1){.struct_size=sizeof(*s)};return true;}
static bool full_context_export(void*c,uint32_t n){(void)c;(void)n;return true;}
static bool full_context_record(void*c,uint32_t a,uint32_t b,uint32_t d,const void*e,uint32_t f){(void)c;(void)a;(void)b;(void)d;(void)e;(void)f;return true;}
static bool full_context_finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static int32_t full_context_label(void*c,uint32_t a,uint32_t b,contexts_label_v1*d){(void)c;(void)a;(void)b;(void)d;return 0;}
static bool full_context_claim(void*c,uint32_t a,uint32_t b,const char*n,uint32_t d){(void)c;(void)a;(void)b;(void)n;(void)d;return true;}
static bool full_context_result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)c;(void)a;(void)b;(void)d;return true;}
static const contexts_service_v1 full_context={.api_version=1,.struct_size=CONTEXTS_SERVICE_V1_SIZE,.step=full_context_step,.pause=full_yes,.status=full_context_status,.request_export=full_context_export,.begin_export=full_context_export,.export_record=full_context_record,.finish_export=full_context_finish,.label=full_context_label,.claim_preset=full_context_claim,.preset_result=full_context_result,.capture_audio=full_yes};
static bool full_wake_pending(void*c,bool*p){(void)c;*p=false;return true;}
static int32_t full_light_for(void*c,uint32_t ms,risc_light_sleep_result_v1*r){(void)c;assert(!hid_owned&&!subs&&!frames&&!pending_token);full_sleep_entries++;sleep_seen=true;printf("LIGHT requested=%u at=%u\n",ms,ticks);*r=(risc_light_sleep_result_v1){.struct_size=sizeof(*r),.wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO};ticks+=3100;return RISC_LIGHT_SLEEP_OK;}
static int32_t full_deep_set(void*c,uint32_t ms){(void)c;(void)ms;assert(!"Unexpected deep sleep");return RISC_DEEP_SLEEP_UNSUPPORTED;}
static bool full_tap_info(void*c,twatch_tap_info_v1*i){(void)c;*i=(twatch_tap_info_v1){sizeof(*i),TWATCH_TAP_BMA423,0,255,4};return true;}
static bool full_tap_config(void*c,uint16_t v){(void)c;(void)v;return true;}
static bool full_tap_observe(void*c,twatch_tap_observation_v1*v){(void)c;*v=(twatch_tap_observation_v1){.struct_size=sizeof(*v)};return true;}
static const twatch_motion_api_v1 full_motion={.api_version=1,.struct_size=sizeof(full_motion),.prepare_wake=full_yes,.wake_pending=full_wake_pending,.resume_wake=full_yes,.tap_info=full_tap_info,.tap_configure=full_tap_config,.tap_observe_begin=full_tap_config,.tap_observe=full_tap_observe};
static const risc_power_sleep_api_v1 full_pmu={.api_version=1,.struct_size=sizeof(full_pmu),.prepare_sleep=full_yes,.resume=full_yes,.light_sleep_for=full_light_for,.sleep_wake_pending=full_wake_pending,.light_sleep_set=full_light_for,.deep_sleep_set=full_deep_set};
static const risc_display_power_api_v1 full_panel={.api_version=1,.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE,.prepare_sleep=full_yes,.resume=full_yes};
static int32_t full_atomic_submit(void*c,uint64_t revision,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t size,uint64_t*t){if(!strcmp(m[0].key,"hid_buttons")){save_attempts++;if(getenv("HID_RENDER_SAVE_FAIL")&&save_attempts==1){*t=0;return RISC_ATOMIC_IO;}}return uat_submit(c,revision,m,count,intent,size,t);}
static int32_t full_atomic_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){int32_t rc=uat_collect(c,t,r);if(!rc&&!r->status){for(unsigned i=0;i<uat.count;i++){unsigned slot=0;while(slot<32&&cells[slot].key[0]&&strcmp(cells[slot].key,uat.records[i].key))slot++;assert(slot<32);strcpy(cells[slot].key,uat.records[i].key);cells[slot].size=uat.records[i].size;memcpy(cells[slot].bytes,uat.records[i].value,uat.records[i].size);}}return rc;}
static const risc_key_value_atomic_v1 full_atomic={1,sizeof(full_atomic),NULL,uat_state,uat_get,full_atomic_submit,full_atomic_collect,uat_cancel};
static int32_t full_alarm_resume(void*c,const alarm_sleep_v1*s){(void)c;(void)s;return ALARM_OK;}
static const alarm_service_sleep_v1 full_alarm={.base={1,sizeof(full_alarm),NULL,fake_alarm_status,fake_alarm_step,fake_alarm_step,fake_alarm_ack,fake_alarm_prepare,fake_alarm_step},.resume_sleep=full_alarm_resume};
static bool full_retain(void){full_retains++;fprintf(stderr,"UNEXPECTED retained at=%u polls=%u\n",ticks,polls);return true;}
static bool full_exit(void){return polls>=stop_poll&&!pending_token;}
