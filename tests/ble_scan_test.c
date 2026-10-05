#include "ble_scan_core.h"
#include <assert.h>
#include <stdio.h>
static void acknowledge(ble_scan*s,uint32_t now){uint8_t cmd[16];assert(ble_command(s,cmd,now));assert(!ble_command(s,cmd,now));uint8_t ack[]={0x0e,4,1,cmd[0],cmd[1],0};ble_event(s,4,ack,sizeof(ack),now);}
int main(void){
 ble_scan s;ble_begin(&s);for(unsigned i=0;i<5;i++)acknowledge(&s,100+i);assert(s.phase==BLE_SCANNING);
 uint8_t report[]={2,1,0,1,1,2,3,4,5,6,18,4,9,'T','e','m',12,0x16,0xd2,0xfc,0x40,1,88,2,0x9c,0xff,3,0x88,0x13,(uint8_t)-56};
 assert(ble_reports(&s,report,sizeof(report),1000));assert(s.count==1);ble_device*d=&s.devices[0];
 assert(!strcmp(d->name,"Tem")&&d->rssi==-56&&d->has_battery&&d->battery==88&&d->temperature==-100&&d->humidity==5000);
 assert(ble_reports(&s,report,sizeof(report),2000));assert(s.count==1&&d->reports==2&&d->seen==2000);
 ble_scan copy=s;assert(!ble_reports(&s,report,sizeof(report)-1,3000));assert(!memcmp(&s,&copy,sizeof(s)));
 report[11]=31;assert(!ble_reports(&s,report,sizeof(report),3000));report[11]=4;
 for(unsigned i=2;i<34;i++){report[4]=(uint8_t)i;assert(ble_reports(&s,report,sizeof(report),3000));}assert(s.count==32&&s.dropped==1);
 assert(!ble_expired(&s,15000));assert(ble_expired(&s,15104)&&s.phase==BLE_COMPLETE);
 ble_begin(&s);uint8_t cmd[16];assert(ble_command(&s,cmd,UINT32_MAX-1000));assert(!ble_expired(&s,900));assert(ble_expired(&s,1000)&&s.phase==BLE_ERROR);
 ble_begin(&s);assert(ble_command(&s,cmd,0));uint8_t wrong[]={0x0e,4,1,4,12,0};ble_event(&s,4,wrong,6,0);assert(s.pending);
 wrong[3]=3;wrong[5]=1;ble_event(&s,4,wrong,6,0);assert(s.phase==BLE_ERROR);
 ble_begin(&s);assert(ble_command(&s,cmd,0));uint8_t zero[]={0x0e,4,0,3,12,0};ble_event(&s,4,zero,6,100);assert(!s.pending&&!s.credits&&!ble_command(&s,cmd,200));zero[2]=1;zero[3]=zero[4]=0;ble_event(&s,4,zero,6,200);assert(ble_command(&s,cmd,200));
 ble_device encrypted={0};uint8_t enc[]={0x41,1,99};ble_bthome(&encrypted,enc,3);assert(encrypted.encrypted&&!encrypted.has_battery);
 /* Deterministic malformed input sweep exercises every bounded parse offset. */
 uint32_t random=1;for(unsigned i=0;i<50000;i++){uint8_t bytes[260];for(unsigned j=0;j<260;j++){random=random*1664525u+1013904223u;bytes[j]=(uint8_t)(random>>24);}memset(&s,0,sizeof(s));(void)ble_reports(&s,bytes,i%260,0);ble_event(&s,4,bytes,i%260,0);}
 puts("BLE commands, timeouts, parser bounds, identity, BTHome and saturation passed");
}
