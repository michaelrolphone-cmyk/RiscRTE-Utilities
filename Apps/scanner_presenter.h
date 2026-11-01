#pragma once
/* Copied semantic content only. Providers own geometry, scrolling and input.
 * The caller publishes a candidate, then uses that committed document for
 * intent validation. This helper owns no grants, handles or domain mutations. */
#include "RiscSceneComponentsV1.h"
#include "RiscBluetoothSensorsV1.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum { SCANNER_BACK=1, SCANNER_SCAN, SCANNER_FILTER, SCANNER_ENABLE,
       SCANNER_NAME, SCANNER_RETRY_NAME, SCANNER_SELECT_BASE=256 };
typedef struct {
    risc_ble_sensor_status_v1 status;
    risc_ble_sensor_device_v1 devices[RISC_BLE_MAX_DEVICES];
    char aliases[RISC_BLE_MAX_DEVICES][25];
    int8_t alias_state[RISC_BLE_MAX_DEVICES];
    uint32_t now_ms,copied_at,detail_index;
    bool detail,sensors,active,radio_enabled,sensor_available,enable_available;
    bool name_available,name_retry;
    char notice[72];
} scanner_presenter_model;
static inline risc_scene_node_v1 *scanner_presenter_node(
    risc_components_document_v1 *d,uint32_t kind,uint32_t action,
    const char *label,const char *value) {
    if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return NULL;
    uint32_t i=d->node_count++;
    risc_scene_node_v1 *n=&d->nodes[i];
    *n=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action};
    snprintf(n->label,sizeof(n->label),"%s",label?label:"");
    snprintf(n->text,sizeof(n->text),"%s",value?value:"");
    return n;
}
static inline uint32_t scanner_presenter_age(uint32_t age,uint32_t elapsed) {
    return (elapsed>UINT32_MAX-age?UINT32_MAX:age+elapsed)/1000u;
}
static inline void scanner_presenter_decimal(char *out,size_t capacity,
                                            const char *label,int32_t value,
                                            const char *unit) {
    /* Unsigned magnitude handles INT32_MIN without overflow or target 64-bit
     * division support. Preserve all signed measurements, including negatives. */
    uint32_t n=value<0?0u-(uint32_t)value:(uint32_t)value;
    snprintf(out,capacity,"%s %s%lu.%02lu %s",label,value<0?"-":"",
             (unsigned long)(n/100u),(unsigned long)(n%100u),unit);
}
static inline void scanner_presenter_reading(char out[72],const risc_ble_reading_v1 *r) {
    switch(r->metric) {
    case RISC_TELEMETRY_BATTERY_PERCENT:snprintf(out,72,"Battery %ld %%",(long)r->value);break;
    case RISC_TELEMETRY_TEMPERATURE_CENTIC:scanner_presenter_decimal(out,72,"Temperature",r->value,"C");break;
    case RISC_TELEMETRY_HUMIDITY_CENTIPERCENT:scanner_presenter_decimal(out,72,"Humidity",r->value,"%");break;
    case RISC_TELEMETRY_PRESSURE_CENTIHPA:scanner_presenter_decimal(out,72,"Pressure",r->value,"hPa");break;
    case RISC_TELEMETRY_ILLUMINANCE_CENTILUX:scanner_presenter_decimal(out,72,"Light",r->value,"lx");break;
    case RISC_TELEMETRY_VOLTAGE_MV:snprintf(out,72,"Voltage %ld mV",(long)r->value);break;
    case RISC_TELEMETRY_CHARGING:snprintf(out,72,"Charging: %s",r->value?"Yes":"No");break;
    default:snprintf(out,72,"Metric %lu: %ld (unknown unit)",(unsigned long)r->metric,(long)r->value);break;
    }
}
static inline bool scanner_presenter_build(risc_components_document_v1 *out,
                                           const scanner_presenter_model *m,
                                           uint32_t revision) {
    if(!out||!m||!revision||m->status.count>RISC_BLE_MAX_DEVICES||
       m->status.state>RISC_BLE_SENSOR_ERROR||
       (m->detail&&m->detail_index>=m->status.count))return false;
    for(unsigned i=0;i<m->status.count;i++) {
        const risc_ble_sensor_device_v1 *v=&m->devices[i];
        if(v->address_type>3||v->service_count>8||v->payload_size>31||
           v->reading_count>RISC_BLE_MAX_READINGS)return false;
    }
    *out=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*out),
        .revision=revision,.root=1,.route_count=1,.screen_key=m->detail?2u+m->detail_index:1u};
    out->routes[0]=(risc_scene_route_v1){.id=1,.back_action=SCANNER_BACK};
    snprintf(out->routes[0].title,sizeof(out->routes[0].title),"%s",m->detail?"SENSOR DETAILS":"BLE SCANNER");
    scanner_presenter_node(out,RISC_COMPONENT_HEADER_ACTION,SCANNER_BACK,m->detail?"Results":"Back","");
    out->details[0].symbol=RISC_SYMBOL_BACK;
    char text[72],label[40];
    if(m->notice[0]){snprintf(text,sizeof(text),"%.71s",m->notice);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"STATUS",text);}
    if(!m->detail) {
        const char *state=m->status.state==RISC_BLE_SENSOR_STARTING?"Starting scan":
            m->status.state==RISC_BLE_SENSOR_SCANNING?"Passive scan - 15 seconds":
            m->status.state==RISC_BLE_SENSOR_COMPLETE?"Scan complete":
            m->status.state==RISC_BLE_SENSOR_ERROR?"Scan failed":"Tap Scan to discover";
        scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"SCAN",state);
        if(m->status.state==RISC_BLE_SENSOR_ERROR&&m->status.error[0]){
            snprintf(text,sizeof(text),"%.47s",m->status.error);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"ERROR",text);
        }
        if(m->status.restore_failed)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"BLUETOOTH","Restore failed");
        if(m->status.cleanup_pending)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"CLEANUP","Pending; radio custody retained");
        snprintf(text,sizeof(text),"%lu saved / %lu dropped / %lu malformed",(unsigned long)m->status.count,(unsigned long)m->status.dropped,(unsigned long)m->status.malformed);
        scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"RESULTS",text);
        scanner_presenter_node(out,RISC_SCENE_ACTION,SCANNER_SCAN,m->active?"Stop":m->sensor_available?"Scan":"Retry scan","");
        risc_scene_node_v1 *n;
        scanner_presenter_node(out,RISC_SCENE_ACTION,SCANNER_FILTER,m->sensors?"Show all devices":"Show sensors","");
        if(!m->radio_enabled){n=scanner_presenter_node(out,RISC_SCENE_ACTION,SCANNER_ENABLE,"Enable Bluetooth","");if(!m->enable_available)n->flags|=RISC_SCENE_DISABLED;}
        if(!m->sensor_available)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"CAPABILITY","Bluetooth sensor service unavailable");
        unsigned visible=0;
        for(unsigned named=1;;named--) {
            for(unsigned i=0;i<m->status.count;i++) {
                const risc_ble_sensor_device_v1 *v=&m->devices[i];
                if((m->sensors&&!v->bthome)||((m->aliases[i][0]!=0)!=(named!=0)))continue;
                if(m->aliases[i][0])snprintf(label,sizeof(label),"%.24s",m->aliases[i]);
                else if(v->name[0])snprintf(label,sizeof(label),"%.31s",v->name);
                else snprintf(label,sizeof(label),"%02X:%02X:%02X:%02X:%02X:%02X",v->address[5],v->address[4],v->address[3],v->address[2],v->address[1],v->address[0]);
                if(v->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");
                else snprintf(text,sizeof(text),"%s / %d dBm",v->bthome?"BTHome":v->service_count?"Services":"BLE",v->rssi);
                scanner_presenter_node(out,RISC_COMPONENT_ROW,SCANNER_SELECT_BASE+i,label,text);visible++;
            }
            if(!named)break;
        }
        if(!visible)scanner_presenter_node(out,RISC_COMPONENT_EMPTY,0,m->active?"Listening...":"No devices in results",m->sensors?"Sensor filter is active":"Explicit passive scan; no pairing or connection");
        return true;
    }
    const risc_ble_sensor_device_v1 *v=&m->devices[m->detail_index];
    risc_scene_node_v1 *n=scanner_presenter_node(out,RISC_COMPONENT_HEADER_ACTION,m->name_retry?SCANNER_RETRY_NAME:SCANNER_NAME,m->name_retry?"Retry name":"Name","");
    if(!m->name_available)n->flags|=RISC_SCENE_DISABLED;
    snprintf(text,sizeof(text),"%.24s",m->aliases[m->detail_index]);
    if(!text[0])snprintf(text,sizeof(text),"%.31s",v->name[0]?v->name:"Unnamed device");
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"NAME",text);
    if(m->alias_state[m->detail_index]<0)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"SAVED NAME","Unavailable; explicit Name can repair");
    snprintf(text,sizeof(text),"%02X:%02X:%02X:%02X:%02X:%02X",v->address[5],v->address[4],v->address[3],v->address[2],v->address[1],v->address[0]);
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"ADDRESS",text);
    snprintf(text,sizeof(text),"%s address / type %u",(v->address_type&1)?"Random":"Public",v->address_type);
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"IDENTITY",text);
    if(v->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");else snprintf(text,sizeof(text),"%d dBm",v->rssi);
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"SIGNAL",text);
    uint32_t elapsed=m->now_ms-m->copied_at;
    snprintf(text,sizeof(text),"%lu s ago / %lu reports",(unsigned long)scanner_presenter_age(v->seen_age_ms,elapsed),(unsigned long)v->reports);
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"LAST SEEN",text);
    for(unsigned i=0;i<v->service_count;i++){snprintf(text,sizeof(text),"0x%04X",v->services[i]);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"SERVICE",text);}
    if(v->has_company){snprintf(text,sizeof(text),"0x%04X",v->company);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"MANUFACTURER",text);}
    if(v->bthome){
        snprintf(text,sizeof(text),"%lu s ago",(unsigned long)scanner_presenter_age(v->measurement_age_ms,elapsed));scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"SENSOR SAMPLE",text);
        scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"BTHOME",v->bthome_version!=2?"Version unsupported":v->encrypted?"Encrypted":v->measurement_invalid?"Invalid sensor sample":"v2 open readings");
    }
    if(v->measurement_partial)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"READINGS","Some readings unsupported");
    for(unsigned i=0;i<v->reading_count;i++){scanner_presenter_reading(text,&v->readings[i]);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"MEASUREMENT",text);}
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"TRUST","Readings are unverified broadcasts");
    if(v->address_type&1)scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"NAME SCOPE","Name follows this address only");
    scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"RAW ADVERTISING BYTES",v->payload_size?"":"None");
    for(unsigned i=0;i<v->payload_size;i+=8){unsigned at=0;for(unsigned j=i;j<v->payload_size&&j<i+8;j++)at+=(unsigned)snprintf(text+at,sizeof(text)-at,"%02X ",v->payload[j]);scanner_presenter_node(out,RISC_SCENE_TEXT_NODE,0,"BYTES",text);}
    return true;
}
/* Identity and sequence custody stay with the controller. Node zero is only
 * the exact copied route Back action, never an arbitrary UI command. */
static inline bool scanner_presenter_intent(const risc_components_document_v1 *d,
                                            const risc_scene_event_v1 *e,
                                            uint32_t *action,uint32_t *index) {
    if(!d||!e||!action||!index||e->struct_size!=sizeof(*e)||!e->sequence||
       e->kind!=RISC_SCENE_ACTION_EVENT||e->value||e->document_revision!=d->revision||
       d->node_count>RISC_COMPONENTS_MAX_NODES||d->route_count!=1)return false;
    *index=UINT32_MAX;
    if(!e->node){if(e->action!=SCANNER_BACK||d->routes[0].back_action!=SCANNER_BACK)return false;*action=SCANNER_BACK;return true;}
    for(unsigned i=0;i<d->node_count;i++) {
        const risc_scene_node_v1 *n=&d->nodes[i];
        if(n->id!=e->node)continue;
        if(n->route!=1||n->action!=e->action||!n->action||
           (n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))||
           (n->kind!=RISC_SCENE_ACTION&&n->kind!=RISC_COMPONENT_ROW&&n->kind!=RISC_COMPONENT_HEADER_ACTION))return false;
        if(e->action>=SCANNER_SELECT_BASE&&e->action<SCANNER_SELECT_BASE+RISC_BLE_MAX_DEVICES)*index=e->action-SCANNER_SELECT_BASE;
        else if(e->action<SCANNER_BACK||e->action>SCANNER_RETRY_NAME)return false;
        *action=e->action;return true;
    }
    return false;
}
