#pragma once
/* Bounded, allocation-free passive LE advertising host. No connections,
 * pairing, bonding, ATT/GATT writes or advertised-data execution. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define BLE_MAX_DEVICES 32u
#define BLE_SCAN_MS 15000u
#define BLE_COMMAND_MS 2000u
typedef struct {
 uint8_t address[6],address_type,event_type,payload[31],payload_size;
 int8_t rssi;char name[32];uint16_t services[8],company;
 uint8_t service_count,battery,bthome_version;bool has_company,bthome,encrypted,has_temperature,has_humidity,has_battery;
 int16_t temperature;uint16_t humidity;uint32_t seen,reports,measurement_seen;
} ble_device;
typedef enum {BLE_IDLE,BLE_STARTING,BLE_SCANNING,BLE_COMPLETE,BLE_ERROR} ble_phase;
typedef struct {
 ble_device devices[BLE_MAX_DEVICES];unsigned count,dropped,malformed;
 ble_phase phase;unsigned command;uint16_t waiting;uint32_t command_at,scan_at;
 bool pending;uint8_t credits;const char *error;
} ble_scan;
static uint16_t ble_u16(const uint8_t*p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static void ble_service(ble_device*d,uint16_t value){
 for(unsigned i=0;i<d->service_count;i++)if(d->services[i]==value)return;
 if(d->service_count<8)d->services[d->service_count++]=value;
}
static void ble_bthome(ble_device*d,const uint8_t*p,size_t n){
 if(n<1)return;
 d->bthome=true;d->bthome_version=p[0]>>5;d->measurement_seen=d->seen;d->encrypted=(p[0]&1)!=0;
 d->has_battery=d->has_temperature=d->has_humidity=false;
 if((p[0]>>5)!=2 || d->encrypted)return;
 for(size_t i=1;i<n;){
  unsigned id=p[i++],size=(id==0||id==1)?1:(id==2||id==3)?2:0;
  if(!size || size>n-i)return;
  if(id==1 && p[i]<=100){d->battery=p[i];d->has_battery=true;}
  if(id==2){d->temperature=(int16_t)ble_u16(p+i);d->has_temperature=true;}
  if(id==3 && ble_u16(p+i)<=10000){d->humidity=ble_u16(p+i);d->has_humidity=true;}
  i+=size;
 }
}
static bool ble_ad_valid(const uint8_t*p,size_t n){
 for(size_t i=0;i<n;){unsigned size=p[i++];if(!size)return true;if(size>n-i)return false;
  unsigned type=p[i];if((type==2||type==3) && ((size-1)&1))return false;
  if((type==0x16||type==0xff)&&size<3)return false;
  i+=size;}
 return true;
}
static void ble_ad(ble_device*d,const uint8_t*p,size_t n){
 for(size_t i=0;i<n;){unsigned size=p[i++];if(!size)break;unsigned type=p[i++];size--;const uint8_t*v=p+i;i+=size;
  if(type==8||type==9){size_t count=size<31?size:31;for(size_t j=0;j<count;j++)d->name[j]=(v[j]>=32&&v[j]<=126)?(char)v[j]:'?';d->name[count]=0;}
  if(type==2||type==3)for(unsigned j=0;j<size;j+=2)ble_service(d,ble_u16(v+j));
  if(type==0xff){d->company=ble_u16(v);d->has_company=true;}
  if(type==0x16){uint16_t uuid=ble_u16(v);ble_service(d,uuid);if(uuid==0xfcd2)ble_bthome(d,v+2,size-2);}
 }
}
/* Validate every report before mutating the table, including multi-report
 * truncation. Identity is address + address type; selected indexes stay stable. */
static bool ble_reports(ble_scan*s,const uint8_t*p,size_t n,uint32_t now){
 if(n<2||p[0]!=2||!p[1])return false;
 size_t cursor=2;
 for(unsigned r=0;r<p[1];r++){
  if(n-cursor<10)return false;
  unsigned len=p[cursor+8];if(len>31 || len+10>n-cursor || p[cursor]>4 || p[cursor+1]>3)return false;
  if(!ble_ad_valid(p+cursor+9,len))return false;
  cursor+=len+10;
 }
 if(cursor!=n)return false;
 cursor=2;
 for(unsigned r=0;r<p[1];r++){
  const uint8_t*q=p+cursor;unsigned len=q[8],index=0;
  for(;index<s->count;index++)if(s->devices[index].address_type==q[1]&&!memcmp(s->devices[index].address,q+2,6))break;
  if(index==s->count && s->count==BLE_MAX_DEVICES){if(s->dropped!=UINT32_MAX)s->dropped++;cursor+=len+10;continue;}
  ble_device*d=&s->devices[index];if(index==s->count){memset(d,0,sizeof(*d));memcpy(d->address,q+2,6);d->address_type=q[1];s->count++;}
  d->event_type=q[0];d->rssi=(int8_t)q[len+9];d->seen=now;if(d->reports!=UINT32_MAX)d->reports++;
  d->payload_size=(uint8_t)len;memcpy(d->payload,q+9,len);ble_ad(d,q+9,len);cursor+=len+10;
 }
 return true;
}
static void ble_fail(ble_scan*s,const char*why){s->phase=BLE_ERROR;s->pending=false;s->error=why;}
static void ble_begin(ble_scan*s){memset(s,0,sizeof(*s));s->phase=BLE_STARTING;s->credits=1;}
/* At most one outstanding command; ack must match opcode and status. */
static size_t ble_command(ble_scan*s,uint8_t out[16],uint32_t now){
 static const uint16_t opcodes[]={0x0c03,0x0c01,0x2001,0x200b,0x200c};
 static const uint8_t sizes[]={0,8,8,7,2};
 static const uint8_t parameters[][8]={{0},{0x10,0x80,0,0,0,0,0,0x20},{2,0,0,0,0,0,0,0},{0,0x60,0,0x30,0,0,0},{1,0}};
 if(s->phase!=BLE_STARTING||s->pending||!s->credits||s->command>=5)return 0;
 uint16_t opcode=opcodes[s->command];out[0]=(uint8_t)opcode;out[1]=(uint8_t)(opcode>>8);out[2]=sizes[s->command];memcpy(out+3,parameters[s->command],out[2]);
 s->pending=true;s->credits=0;s->waiting=opcode;s->command_at=now;return 3u+out[2];
}
static void ble_event(ble_scan*s,uint8_t type,const uint8_t*p,size_t n,uint32_t now){
 if(type!=4)return;
 if(n<2||n!=2u+p[1]){s->malformed++;return;}
 if(p[0]==0x0e && n>=5)s->credits=p[2];
 if(p[0]==0x0f && n==6)s->credits=p[3];
 if(p[0]==0x10){ble_fail(s,"Controller error");return;}
 if(p[0]==0x0e && n>=6 && s->pending && ble_u16(p+3)==s->waiting){
  if(p[5]){ble_fail(s,"Command rejected");return;}
  s->pending=false;s->command++;s->command_at=now;
  if(s->command==5){s->phase=BLE_SCANNING;s->scan_at=now;}
 }else if(p[0]==0x0f && n==6 && s->pending && ble_u16(p+4)==s->waiting && p[2])ble_fail(s,"Command failed");
 else if(p[0]==0x3e && s->phase==BLE_SCANNING && n>=3 && p[2]==2 && !ble_reports(s,p+2,n-2,now))s->malformed++;
}
static bool ble_expired(ble_scan*s,uint32_t now){
 if(s->phase==BLE_STARTING&&(s->pending||!s->credits)&&(uint32_t)(now-s->command_at)>=BLE_COMMAND_MS){ble_fail(s,"Controller timeout");return true;}
 if(s->phase==BLE_SCANNING&&(uint32_t)(now-s->scan_at)>=BLE_SCAN_MS){s->phase=BLE_COMPLETE;return true;}
 return s->phase==BLE_ERROR;
}
