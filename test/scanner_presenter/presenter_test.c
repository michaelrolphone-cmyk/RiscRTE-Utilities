#include "scanner_presenter.h"
#include <assert.h>
#include <stdlib.h>
static unsigned cases;
static scanner_presenter_model m;
static risc_components_document_v1 d;
static void reset(void){memset(&m,0,sizeof(m));m.sensor_available=m.name_available=m.enable_available=m.radio_enabled=true;m.status.state=RISC_BLE_SENSOR_IDLE;memset(&d,0,sizeof(d));}
static bool text_has(const char *s){for(unsigned i=0;i<d.node_count;i++)if(strstr(d.nodes[i].text,s)||strstr(d.nodes[i].label,s))return true;return false;}
static unsigned action_node(unsigned action){for(unsigned i=0;i<d.node_count;i++)if(d.nodes[i].action==action)return d.nodes[i].id;return 0;}
static unsigned label_node(const char *label){for(unsigned i=0;i<d.node_count;i++)if(!strcmp(d.nodes[i].label,label))return i;return UINT_MAX;}
static void build(void){assert(scanner_presenter_build(&d,&m,7));assert(d.node_count<=96);for(unsigned i=0;i<d.node_count;i++){assert(d.nodes[i].id==i+1);assert(memchr(d.nodes[i].label,0,sizeof(d.nodes[i].label)));assert(memchr(d.nodes[i].text,0,sizeof(d.nodes[i].text)));}}
static void detail(void){reset();m.detail=true;m.status.count=1;}
int main(void){
 reset();build();assert(text_has("Tap Scan")&&text_has("No devices")&&action_node(SCANNER_SCAN));cases++;
 reset();m.active=true;m.status.state=RISC_BLE_SENSOR_SCANNING;build();assert(text_has("15 seconds")&&text_has("Stop"));cases++;
 reset();m.status.count=32;for(unsigned i=0;i<32;i++){m.devices[i].address[0]=(uint8_t)i;m.devices[i].rssi=-42;}build();for(unsigned i=0;i<32;i++)assert(action_node(SCANNER_SELECT_BASE+i));cases++;
 strcpy(m.aliases[31],"Named last");build();assert(action_node(SCANNER_SELECT_BASE+31)<action_node(SCANNER_SELECT_BASE));cases++;
 m.sensors=true;m.devices[9].bthome=true;build();assert(action_node(SCANNER_SELECT_BASE+9)&&!action_node(SCANNER_SELECT_BASE+31));cases++;
 m.devices[9].bthome=false;build();assert(text_has("Sensor filter"));cases++;
 reset();m.sensor_available=false;build();assert(text_has("service unavailable"));assert(!(d.nodes[action_node(SCANNER_SCAN)-1].flags&RISC_SCENE_DISABLED)&&text_has("Retry scan"));cases++;
 reset();m.radio_enabled=false;build();assert(action_node(SCANNER_ENABLE));cases++;
 m.enable_available=false;build();assert(d.nodes[action_node(SCANNER_ENABLE)-1].flags&RISC_SCENE_DISABLED);cases++;
 reset();m.status.state=RISC_BLE_SENSOR_ERROR;strcpy(m.status.error,"radio rejected");m.status.restore_failed=m.status.cleanup_pending=true;build();assert(text_has("radio rejected")&&text_has("Restore failed")&&text_has("custody retained"));cases++;
 reset();m.status.dropped=UINT32_MAX;m.status.malformed=UINT32_MAX;build();assert(text_has("4294967295 dropped")&&text_has("4294967295 malformed"));cases++;
 detail();m.devices[0].address_type=3;m.devices[0].address[5]=0xab;m.devices[0].rssi=127;build();assert(text_has("AB:00:00:00:00:00")&&text_has("Random address / type 3")&&text_has("Signal unavailable")&&text_has("this address only"));cases++;
 detail();m.now_ms=2000;m.copied_at=1000;m.devices[0].seen_age_ms=UINT32_MAX;m.devices[0].reports=UINT32_MAX;build();assert(text_has("4294967 s ago / 4294967295 reports"));cases++;
 detail();m.now_ms=500;m.copied_at=UINT32_MAX-499;m.devices[0].seen_age_ms=1000;build();assert(text_has("2 s ago"));cases++;
 detail();m.devices[0].service_count=8;for(unsigned i=0;i<8;i++)m.devices[0].services[i]=(uint16_t)(0x1810+i);m.devices[0].has_company=true;m.devices[0].company=0x1234;build();assert(text_has("0x1817")&&text_has("0x1234"));cases++;
 detail();m.devices[0].reading_count=12;for(unsigned i=0;i<12;i++)m.devices[0].readings[i]=(risc_ble_reading_v1){i+1,INT32_MIN};build();unsigned count=0;for(unsigned i=0;i<d.node_count;i++)if(!strcmp(d.nodes[i].label,"MEASUREMENT"))count++;assert(count==12&&text_has("-21474836.48 C")&&text_has("Metric 12: -2147483648"));cases++;
 detail();m.devices[0].payload_size=31;for(unsigned i=0;i<31;i++)m.devices[0].payload[i]=(uint8_t)i;build();assert(text_has("18 19 1A 1B 1C 1D 1E "));cases++;
 detail();m.devices[0].bthome=true;m.devices[0].bthome_version=1;build();assert(text_has("Version unsupported"));cases++;
 m.devices[0].bthome_version=2;m.devices[0].encrypted=true;build();assert(text_has("Encrypted"));cases++;
 m.devices[0].encrypted=false;m.devices[0].measurement_invalid=true;build();assert(text_has("Invalid sensor sample"));cases++;
 m.devices[0].measurement_partial=true;build();assert(text_has("Some readings unsupported")&&text_has("unverified broadcasts"));cases++;
 detail();m.name_retry=true;m.alias_state[0]=-1;build();assert(action_node(SCANNER_RETRY_NAME)&&text_has("explicit Name can repair"));cases++;
 m.name_available=false;build();assert(d.nodes[action_node(SCANNER_RETRY_NAME)-1].flags&RISC_SCENE_DISABLED);cases++;
 detail();memset(m.aliases[0],'x',25);memset(m.notice,'n',72);memset(m.devices[0].name,'y',32);build();assert(strlen(d.nodes[label_node("NAME")].text)==24);cases++;
 reset();m.status.count=33;d.revision=123;assert(!scanner_presenter_build(&d,&m,7)&&d.revision==123);cases++;
 detail();m.devices[0].reading_count=13;assert(!scanner_presenter_build(&d,&m,7));m.devices[0].reading_count=0;m.devices[0].payload_size=32;assert(!scanner_presenter_build(&d,&m,7));m.devices[0].payload_size=0;m.devices[0].service_count=9;assert(!scanner_presenter_build(&d,&m,7));m.devices[0].service_count=0;m.devices[0].address_type=4;assert(!scanner_presenter_build(&d,&m,7));cases++;
 reset();assert(!scanner_presenter_build(&d,&m,0));m.detail=true;assert(!scanner_presenter_build(&d,&m,7));cases++;
 reset();m.status.count=32;build();uint32_t a=0,index=0;risc_scene_event_v1 e={.struct_size=sizeof(e),.kind=RISC_SCENE_ACTION_EVENT,.sequence=1,.document_revision=7,.node=action_node(SCANNER_SELECT_BASE+31),.action=SCANNER_SELECT_BASE+31};assert(scanner_presenter_intent(&d,&e,&a,&index)&&index==31);cases++;
 e.document_revision=6;assert(!scanner_presenter_intent(&d,&e,&a,&index));e.document_revision=7;e.node++;assert(!scanner_presenter_intent(&d,&e,&a,&index));cases++;
 e.node=action_node(SCANNER_SCAN);e.action=SCANNER_SCAN;e.value=1;assert(!scanner_presenter_intent(&d,&e,&a,&index));e.value=0;cases++;
 e.node=0;e.action=SCANNER_BACK;assert(scanner_presenter_intent(&d,&e,&a,&index)&&a==SCANNER_BACK&&index==UINT32_MAX);e.action=SCANNER_SCAN;assert(!scanner_presenter_intent(&d,&e,&a,&index));cases++;
 e.node=action_node(SCANNER_SCAN);e.sequence=0;assert(!scanner_presenter_intent(&d,&e,&a,&index));e.sequence=1;e.kind=RISC_SCENE_VALUE_EVENT;assert(!scanner_presenter_intent(&d,&e,&a,&index));e.kind=RISC_SCENE_ACTION_EVENT;d.nodes[e.node-1].flags=RISC_SCENE_DISABLED;assert(!scanner_presenter_intent(&d,&e,&a,&index));cases++;
 reset();m.status.count=32;scanner_presenter_model before=m;build();assert(!memcmp(&m,&before,sizeof(m)));cases++;
 printf("%u presenter cases PASS\n",cases);return 0;
}
