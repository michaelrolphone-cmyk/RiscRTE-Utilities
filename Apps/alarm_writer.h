#ifndef UTILITIES_ALARM_WRITER_H
#define UTILITIES_ALARM_WRITER_H
/* Shared writer state. App authority is namespace 3 only; no occurrence access. */
#include "AlarmRecords.h"
#include "RiscKeyValueV1.h"
typedef struct {
    alarm_config saved, pending;
    bool loaded, uncertain;
    int32_t error;
} alarm_writer;
static inline int32_t alarm_writer_load(alarm_writer *w,const risc_key_value_v1 *kv,unsigned kind) {
    uint8_t b[ALARM_RECORD_SIZE];uint32_t n=0;
    int32_t r=kv->get(kv->context,kind==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY,b,sizeof(b),&n);
    if(r==RISC_KEY_VALUE_NOT_FOUND){w->saved=(alarm_config){.kind=(uint8_t)kind};w->loaded=true;w->error=0;return 0;}
    if(r!=RISC_KEY_VALUE_OK||!alarm_config_decode(&w->saved,b,n,(uint8_t)kind)){w->loaded=false;w->error=ALARM_STORAGE;return ALARM_STORAGE;}
    w->loaded=true;w->error=0;return 0;
}
static inline int32_t alarm_writer_commit(alarm_writer *w,const risc_key_value_v1 *kv,const alarm_config *desired) {
    if(!w->loaded||!alarm_config_valid(desired)||desired->kind!=w->saved.kind)return ALARM_INVALID;
    if(!w->uncertain) {
        if(w->saved.revision==UINT32_MAX||desired->revision!=w->saved.revision+1)return ALARM_EXHAUSTED;
        w->pending=*desired;
    }
    uint8_t expected[ALARM_RECORD_SIZE],actual[ALARM_RECORD_SIZE];uint32_t n=0;
    alarm_config_encode(&w->pending,expected);w->uncertain=true;
    const char *key=w->pending.kind==1?ALARM_CONFIG_KEY:ALARM_TIMER_KEY;
    (void)kv->put(kv->context,key,expected,sizeof(expected));
    int32_t r=kv->get(kv->context,key,actual,sizeof(actual),&n);
    if(r!=RISC_KEY_VALUE_OK||n!=sizeof(actual)||memcmp(actual,expected,sizeof(actual))){w->error=ALARM_STORAGE;return ALARM_STORAGE;}
    w->saved=w->pending;w->uncertain=false;w->error=0;return 0;
}
#endif
