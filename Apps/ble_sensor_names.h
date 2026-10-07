#pragma once
#include "RiscKeyValueV1.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define BLE_ALIAS_MAX 24u
/* One bounded, versioned record per explicitly named observed identity. No
 * automatic writes, address resolution, tracking, or peer-key storage. */
static void ble_alias_key(uint8_t type,const uint8_t address[6],char key[16]){
 static const char hex[]="0123456789abcdef";key[0]='b';key[1]='s';key[2]=(char)('0'+type);
 for(unsigned i=0;i<6;i++){key[3+i*2]=hex[address[i]>>4];key[4+i*2]=hex[address[i]&15];}key[15]=0;
}
static uint32_t ble_alias_crc(const uint8_t*p,unsigned n){uint32_t c=0xffffffff;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^(0xedb88320u&((uint32_t)0-(c&1u)));}return ~c;}
static bool ble_alias_encode(uint8_t type,const uint8_t address[6],const char*name,uint8_t out[40]){
 if(type>3||!address||!name)return false;
 size_t n=0;while(n<=BLE_ALIAS_MAX&&name[n]){if((unsigned char)name[n]<32||(unsigned char)name[n]>126)return false;n++;}if(n>BLE_ALIAS_MAX)return false;
 memset(out,0,40);out[0]='B';out[1]='S';out[2]=1;out[3]=type;memcpy(out+4,address,6);out[10]=(uint8_t)n;memcpy(out+11,name,n);
 uint32_t crc=ble_alias_crc(out,36);for(unsigned i=0;i<4;i++)out[36+i]=(uint8_t)(crc>>(i*8));return true;
}
static bool ble_alias_decode(uint8_t type,const uint8_t address[6],const uint8_t*p,uint32_t n,char out[BLE_ALIAS_MAX+1]){
 if(!p||n!=40||p[0]!='B'||p[1]!='S'||p[2]!=1||p[3]!=type||memcmp(p+4,address,6)||p[10]>BLE_ALIAS_MAX)return false;
 uint32_t crc=0;for(unsigned i=0;i<4;i++)crc|=(uint32_t)p[36+i]<<(i*8);if(crc!=ble_alias_crc(p,36))return false;
 for(unsigned i=0;i<p[10];i++)if(p[11+i]<32||p[11+i]>126)return false;
 for(unsigned i=11+p[10];i<36;i++)if(p[i])return false;
 memcpy(out,p+11,p[10]);out[p[10]]=0;return true;
}
/* 1 valid alias/tombstone, 0 missing, -1 unreadable. Never overwrite a corrupt
 * or uncertain record automatically. Saving is an explicit user action. */
static int ble_alias_load(const risc_key_value_v1*kv,uint8_t type,const uint8_t address[6],char out[BLE_ALIAS_MAX+1]){
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get||type>3)return -1;
 char key[16];uint8_t b[40];uint32_t n=0;ble_alias_key(type,address,key);int32_t rc=kv->get(kv->context,key,b,sizeof(b),&n);
 if(rc==RISC_KEY_VALUE_NOT_FOUND){out[0]=0;return 0;}
 return rc==RISC_KEY_VALUE_OK&&ble_alias_decode(type,address,b,n,out)?1:-1;
}
static bool ble_alias_save(const risc_key_value_v1*kv,uint8_t type,const uint8_t address[6],const char*name){
 if(!kv||kv->api_version!=1||kv->struct_size<sizeof(*kv)||!kv->get||!kv->put)return false;
 char key[16],check[BLE_ALIAS_MAX+1];uint8_t b[40];if(!ble_alias_encode(type,address,name,b))return false;
 ble_alias_key(type,address,key);int32_t rc=kv->put(kv->context,key,b,sizeof(b));
 return (rc==RISC_KEY_VALUE_OK||rc==RISC_KEY_VALUE_IO)&&ble_alias_load(kv,type,address,check)==1&&!strcmp(name,check);
}
