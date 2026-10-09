/* Cooperative ordinary ELF. The deployment selects its sensor capability.
 * Runtime owns no radio/data-sharing policy and receives no poll callbacks. */
#include "TelemetryBroadcastV1.h"
#include "RiscBluetoothTelemetryV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscProviderV2.h"
#include <string.h>
#define RETRY_MS 5000u
static const risc_platform_clock_api_v1 *clock_api;
static const risc_bluetooth_telemetry_v1 *radio;
static telemetry_broadcast_status_v1 view;
static uint64_t token,next_try,selection_at;
static uint32_t selected[RISC_TELEMETRY_MAX_FIELDS],selected_count;
static bool started,busy;
static bool enter(void) { if(!started||busy)return false;busy=true;return true; }
static void leave(void) { busy=false; }
static unsigned field_bytes(uint32_t metric) {
    switch(metric) {
      case RISC_TELEMETRY_BATTERY_PERCENT:case RISC_TELEMETRY_CHARGING:return 2;
      case RISC_TELEMETRY_TEMPERATURE_CENTIC:case RISC_TELEMETRY_HUMIDITY_CENTIPERCENT:case RISC_TELEMETRY_VOLTAGE_MV:return 3;
      case RISC_TELEMETRY_PRESSURE_CENTIHPA:case RISC_TELEMETRY_ILLUMINANCE_CENTILUX:return 4;
      default:return 0;
    }
}
static bool close_token(void) {
    if(token&&!radio->close(radio->context,token)) {
        view.cleanup_pending=true;view.state=TELEMETRY_BROADCAST_RETAINED;return false;
    }
    token=0;view.cleanup_pending=false;
    risc_ble_telemetry_status_v1 state={.struct_size=sizeof(state)};
    if(radio->status(radio->context,&state))view.restore_failed=state.restore_failed;
    return true;
}
/* Validate the complete bounded catalog. Prioritize one current field of each
 * supported metric (all seven fit), then additional channels within the exact
 * 31-byte legacy packet budget. Every omitted current field is reported. */
static int32_t selected_ids(uint32_t ids[RISC_TELEMETRY_MAX_FIELDS]) {
    risc_telemetry_field_v1 fields[RISC_TELEMETRY_MAX_FIELDS];
    bool current[RISC_TELEMETRY_MAX_FIELDS]={0},chosen[RISC_TELEMETRY_MAX_FIELDS]={0};
    unsigned total=0,available=0;bool ended=false;
    for(unsigned i=0;i<=RISC_TELEMETRY_MAX_FIELDS;i++) {
        risc_telemetry_field_v1 field={0};int32_t rc=radio->enumerate(radio->context,i,&field);
        if(rc==0){ended=true;break;}
        if(rc!=1||i==RISC_TELEMETRY_MAX_FIELDS||!field.id||!memchr(field.label,0,sizeof(field.label)))return -1;
        for(unsigned j=0;j<i;j++)if(fields[j].id==field.id)return -1;
        fields[total++]=field;
        if(field_bytes(field.metric)) {
            int32_t value;rc=radio->read(radio->context,field.id,&value);
            if(rc<0||rc>1)return -1;
            if(rc==1){current[i]=true;available++;}
        }
    }
    if(!ended)return -1;
    unsigned count=0,bytes=8,metrics=0;
    for(unsigned pass=0;pass<2;pass++)for(unsigned i=0;i<total;i++) {
        if(!current[i]||chosen[i])continue;
        unsigned bit=1u<<fields[i].metric,n=field_bytes(fields[i].metric);
        if(!pass&&(metrics&bit))continue;
        if(bytes+n>31)continue;
        ids[count++]=fields[i].id;chosen[i]=true;bytes+=n;metrics|=bit;
    }
    view.omitted_fields=available-count;
    return (int32_t)count;
}
static bool pause(void *context) {
    (void)context;if(!enter())return false;
    bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_PAUSED;
    next_try=0;leave();return ok;
}
static bool step(void *context,bool allow,const telemetry_broadcast_policy_v1 *policy) {
    (void)context;if(!policy||policy->struct_size<sizeof(*policy)||!enter())return false;
    if(view.cleanup_pending) {
        /* A previous cleanup failure admits no normal I/O until close works. */
        bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_PAUSED;leave();return ok;
    }
    view.enabled=policy->enabled;view.settings_valid=policy->settings_valid;
    if(!allow||!view.enabled||!view.settings_valid) {
        bool ok=close_token();
        if(ok)view.state=!view.settings_valid?TELEMETRY_BROADCAST_SETTINGS_ERROR:!view.enabled?TELEMETRY_BROADCAST_OFF:TELEMETRY_BROADCAST_PAUSED;
        leave();return ok;
    }
    uint64_t now=clock_api->monotonic_ms(clock_api->context);
    if(now==UINT64_MAX) {bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_RETRY;leave();return ok;}
    if(!policy->radios_allowed) {
        bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_BLE_OFF;
        next_try=0;leave();return ok;
    }
    if(token && (now<selection_at || now-selection_at>=RETRY_MS)) {
        uint32_t ids[RISC_TELEMETRY_MAX_FIELDS];int32_t count=selected_ids(ids);
        selection_at=now;
        if(count<=0 || (uint32_t)count!=selected_count || memcmp(ids,selected,(size_t)count*sizeof(*ids))) {
            if(!close_token()){leave();return false;}
            next_try=0;
        }
    }
    if(!token) {
        if(next_try&&now<next_try&&next_try-now<=RETRY_MS){leave();return true;}
        uint32_t ids[RISC_TELEMETRY_MAX_FIELDS];int32_t count=selected_ids(ids);
        view.fields=count>0?(uint32_t)count:0;
        if(count<=0||!radio->publish(radio->context,ids,(uint32_t)count,true,&token)||!token) {
            bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_RETRY;
            next_try=now+RETRY_MS;leave();return ok;
        }
        memcpy(selected,ids,(size_t)count*sizeof(*ids));selected_count=(uint32_t)count;selection_at=now;
        view.state=TELEMETRY_BROADCAST_STARTING;
    }
    risc_ble_telemetry_status_v1 state={.struct_size=sizeof(state)};
    if(!radio->poll(radio->context,token,4)||!radio->status(radio->context,&state)||state.state==RISC_BLE_TELEMETRY_FAULT) {
        bool ok=close_token();if(ok)view.state=TELEMETRY_BROADCAST_RETRY;
        next_try=now+RETRY_MS;leave();return ok;
    }
    view.updates=state.updates;view.restore_failed=state.restore_failed;
    view.state=state.state==RISC_BLE_TELEMETRY_PUBLISHING?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_STARTING;
    leave();return true;
}
static bool status(void *context,telemetry_broadcast_status_v1 *out) {
    (void)context;if(!out||out->struct_size<sizeof(*out)||!enter())return false;
    *out=view;out->struct_size=sizeof(*out);leave();return true;
}
static int32_t enumerate(void *context,uint32_t index,risc_telemetry_field_v1 *out) {
    (void)context;if(!out||!enter())return -1;
    int32_t rc=view.cleanup_pending?-1:radio->enumerate(radio->context,index,out);leave();return rc;
}
static int32_t read_value(void *context,uint32_t id,int32_t *out) {
    (void)context;if(!out||!enter())return -1;
    int32_t rc=view.cleanup_pending?-1:radio->read(radio->context,id,out);leave();return rc;
}
static bool start(const risc_provider_dependency_v1 *d,size_t count) {
    if(started||busy||!d||count!=2)return false;
    const risc_platform_clock_api_v1 *c=NULL;const risc_bluetooth_telemetry_v1 *r=NULL;
    for(size_t i=0;i<count;i++) {
        if(!d[i].capability_id||d[i].api_version!=1||!d[i].api)return false;
        if(!strcmp(d[i].capability_id,"platform.clock")&&!c)c=d[i].api;
        else if(!strcmp(d[i].capability_id,RISC_BLUETOOTH_TELEMETRY_CAPABILITY)&&!r)r=d[i].api;
        else return false;
    }
    if(!c||!r||c->api_version!=1||c->struct_size<sizeof(*c)||!c->monotonic_ms||r->api_version!=1||r->struct_size<sizeof(*r)||!r->enumerate||!r->read||!r->publish||!r->poll||!r->status||!r->close)return false;
    clock_api=c;radio=r;token=next_try=selection_at=0;selected_count=0;
    view=(telemetry_broadcast_status_v1){.struct_size=sizeof(view),.state=TELEMETRY_BROADCAST_OFF};started=true;return true;
}
static bool quiesce(void) {
    if(!started)return true;
    if(busy)return false;
    busy=true;
    bool ok=close_token();busy=false;return ok;
}
static void stop(void) {
    if(token||busy)return;
    started=false;clock_api=NULL;radio=NULL;
}
static const telemetry_broadcast_v1 api={1,sizeof(api),NULL,step,pause,status,enumerate,read_value};
static const risc_driver_v2 driver={2,sizeof(driver),"telemetry-broadcast",TELEMETRY_BROADCAST_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
