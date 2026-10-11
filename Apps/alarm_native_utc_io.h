#ifndef UTILITIES_ALARM_NATIVE_UTC_IO_H
#define UTILITIES_ALARM_NATIVE_UTC_IO_H
/* App-owned wrappers only. No native control or RTC acquisition. Every native
 * sample closes its reader grant before the adapter can render or poll. */
static bool native_retained,native_zone_valid;
static portable_realtime_client native_client;
static portable_timezone_rule native_zone;
static char native_zone_name[PORTABLE_TIMEZONE_ID_BYTES];
static const risc_key_value_v1 *native_storage,*native_preferences;
static const alarm_service_v1 *native_service;
static risc_key_value_v1 native_storage_proxy,native_preferences_proxy;
static alarm_service_v1 native_service_proxy;
static void native_retain(void) {
    if(native_retained)return;
    native_retained=true;
    portable_realtime_stop(&native_client,true);
    portable_adapter_retain();
}
static int native_guard(void *unused) {
    (void)unused;return native_retained||portable_adapter_retained()?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_SAFE;
}
static bool native_time_halted(int rc) {
    if(rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED){native_retain();return true;}
    return false;
}
static bool native_kv_status(int32_t rc) {
    if(rc==RISC_KEY_VALUE_OK||rc==RISC_KEY_VALUE_NOT_FOUND||rc==RISC_KEY_VALUE_BUFFER_SMALL||rc==RISC_KEY_VALUE_INVALID||rc==RISC_KEY_VALUE_IO)return true;
    native_retain();return false;
}
#ifdef PORTABLE_BLE_BROADCAST
bool portable_broadcast_stop(void);
static bool native_io_ready(void) {
    if(native_retained||portable_adapter_retained())return false;
    if(!portable_broadcast_stop()){native_retain();return false;}
    return true;
}
#else
#define native_io_ready() (!native_retained)
#endif
static int32_t native_get(void *context,const char *key,void *buffer,uint32_t cap,uint32_t *size) {
    if(!native_io_ready())return RISC_KEY_VALUE_CONTEXT;
    const risc_key_value_v1 *kv=context;int32_t rc=kv->get(kv->context,key,buffer,cap,size);
    return native_kv_status(rc)?rc:RISC_KEY_VALUE_CONTEXT;
}
static int32_t native_put(void *context,const char *key,const void *buffer,uint32_t size) {
    if(!native_io_ready())return RISC_KEY_VALUE_CONTEXT;
    const risc_key_value_v1 *kv=context;int32_t rc=kv->put(kv->context,key,buffer,size);
    return native_kv_status(rc)?rc:RISC_KEY_VALUE_CONTEXT;
}
static int32_t native_result(int32_t rc) {
    if(rc==ALARM_RETAINED||rc==ALARM_OUTPUT||rc<ALARM_RETAINED||rc>ALARM_PENDING)native_retain();
    return native_retained?ALARM_RETAINED:rc;
}
static int32_t native_status(void *unused,alarm_status_v1 *out) {
    (void)unused;if(!native_io_ready())return ALARM_RETAINED;
    int32_t rc=native_result(native_service->status(native_service->context,out));
    if(rc==ALARM_OK) {
        if(native_result(out->error)==ALARM_RETAINED)return ALARM_RETAINED;
        if(out->struct_size<sizeof(*out)||out->api_version!=1||out->state>ALARM_STATE_CUE)return ALARM_INVALID;
    }
    return native_retained?ALARM_RETAINED:rc;
}
static int32_t native_step(void *unused) {(void)unused;return !native_io_ready()?ALARM_RETAINED:native_result(native_service->step(native_service->context));}
static int32_t native_refresh(void *unused) {(void)unused;return !native_io_ready()?ALARM_RETAINED:native_result(native_service->refresh(native_service->context));}
static int32_t native_ack(void *unused,const alarm_token_v1 *token) {(void)unused;return !native_io_ready()?ALARM_RETAINED:native_result(native_service->acknowledge(native_service->context,token));}
static int32_t native_stop(void *unused) {(void)unused;return !native_io_ready()?ALARM_RETAINED:native_result(native_service->stop_only(native_service->context));}
static bool native_load_zone(void) {
    if(native_retained)return false;
    native_zone_valid=false;
    int result=portable_timezone_preference_load(alarm_preferences,native_zone_name);
    if(native_retained)return false;
    if(result!=PORTABLE_TIMEZONE_LOADED&&result!=PORTABLE_TIMEZONE_MISSING)return false;
    native_zone_valid=portable_timezone_resolve(native_zone_name,sizeof(native_zone_name),&native_zone)==PORTABLE_TIMEZONE_OK;
    return native_zone_valid;
}
static bool native_local(const twatch_rtc_time_v1 *raw,twatch_rtc_time_v1 *out) {
    portable_timezone_civil utc={raw->year,raw->month,raw->day,raw->hour,raw->minute,raw->second,raw->weekday},local;int64_t epoch;
    if(!native_zone_valid||portable_timezone_civil_to_epoch(&utc,&epoch)!=PORTABLE_TIMEZONE_OK||portable_timezone_utc_to_local(&native_zone,epoch,&local,NULL)!=PORTABLE_TIMEZONE_OK||local.year<1600||local.year>9999)return false;
    *out=(twatch_rtc_time_v1){(uint16_t)local.year,local.month,local.day,local.weekday,local.hour,local.minute,local.second};return true;
}
static bool read_clock(twatch_rtc_time_v1 *out,uint32_t *seconds) {
    if(native_retained)return false;
    int result=portable_realtime_open(&native_client,runtime,PORTABLE_REALTIME_READER,PORTABLE_REALTIME_TIMER_ONLY,native_guard,NULL);
    if(native_time_halted(result)||result!=PORTABLE_REALTIME_OK)return false;
    risc_realtime_snapshot_v1 sample={.struct_size=sizeof(sample)};
    result=portable_realtime_read(&native_client,&sample);
    if(native_time_halted(result))return false;
    int closed=portable_realtime_close(&native_client);
    if(native_time_halted(closed)||closed!=PORTABLE_REALTIME_OK||result!=PORTABLE_REALTIME_OK||!points_utc_from_unix(sample.epoch_seconds,seconds))return false;
    portable_timezone_civil utc;if(portable_timezone_epoch_to_civil(sample.epoch_seconds,&utc)!=PORTABLE_TIMEZONE_OK)return false;
    *out=(twatch_rtc_time_v1){(uint16_t)utc.year,utc.month,utc.day,utc.weekday,utc.hour,utc.minute,utc.second};return true;
}
/* The shared paper toolbar and QuickActions use this exact same local clock.
 * The adapter checks its custody fence immediately after this callback. */
__attribute__((visibility("hidden"))) bool portable_app_native_local_time(twatch_rtc_time_v1 *out) {
    if(!out||!runtime||!native_zone_valid||native_retained)return false;
    twatch_rtc_time_v1 utc;uint32_t seconds;
    return read_clock(&utc,&seconds)&&native_local(&utc,out);
}
static bool native_acquire(const char *name,uint32_t version,uint64_t instance) {
    risc_runtime_capability_v1 *g=&grants[acquired];*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
    bool ok=runtime->acquire(name,version,instance,g);
    /* A false result, including an empty handle, cannot prove native cleanup. */
    if(!ok){native_retain();return false;}
    if(g->struct_size!=sizeof(*g)||!g->slot||!g->generation||!g->api){native_retain();return false;}
    acquired++;return true;
}
static bool native_kv_valid(const risc_key_value_v1 *kv,bool write) {
    return kv&&kv->api_version==1&&kv->struct_size>=sizeof(*kv)&&kv->get&&(!write||kv->put);
}
static bool open_dependencies(void) {
    runtime=risc_runtime_get_api(1);acquired=0;storage=alarm_preferences=NULL;service=NULL;
    if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE||!runtime->retain_invocation||!runtime->acquire||!runtime->release||!runtime->yield_ms)return false;
    if(!native_acquire("storage.key-value",1,3))return false;
    native_storage=grants[0].api;if(!native_kv_valid(native_storage,true))return false;
    native_storage_proxy=(risc_key_value_v1){1,sizeof(native_storage_proxy),(void *)native_storage,native_get,native_put};storage=&native_storage_proxy;
    if(!native_acquire("storage.key-value",1,1))return false;
    native_preferences=grants[1].api;if(!native_kv_valid(native_preferences,false))return false;
    native_preferences_proxy=(risc_key_value_v1){1,sizeof(native_preferences_proxy),(void *)native_preferences,native_get,NULL};alarm_preferences=&native_preferences_proxy;
    if(!native_acquire(ALARM_SERVICE_CAPABILITY,ALARM_SERVICE_API_V2,0))return false;
    native_service=grants[2].api;
    const alarm_service_descriptor_v2 *descriptor=alarm_service_descriptor(native_service);
    if(!descriptor||descriptor->output_modes!=ALARM_MODE_VISUAL)return false;
    native_service_proxy=(alarm_service_v1){1,sizeof(native_service_proxy),NULL,native_status,native_step,native_refresh,native_ack,NULL,native_stop};service=&native_service_proxy;
    alarm_time_format=PORTABLE_TIME_FORMAT_12;(void)portable_time_format_load(alarm_preferences,&alarm_time_format);
    if(native_retained)return false;
    if(!native_load_zone())notice="TIME ZONE UNAVAILABLE";
    return !native_retained;
}
static void close_dependencies(void) {
    if(native_retained)return;
    while(runtime&&acquired) {
        risc_runtime_capability_v1 g=grants[acquired-1];
        if(!runtime->release(&g)||g.struct_size!=sizeof(g)||g.slot||g.generation||g.api){native_retain();return;}
        acquired--;grants[acquired]=g;
    }
    runtime=NULL;storage=alarm_preferences=NULL;service=NULL;
}
#endif
