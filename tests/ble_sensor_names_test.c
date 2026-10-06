#include "ble_sensor_names.h"
#include <assert.h>
#include <stdio.h>
static uint8_t record[40];static bool present,write_failure,uncertain;static char stored_key[16];
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){(void)c;assert(cap==40);if(!present||strcmp(k,stored_key))return RISC_KEY_VALUE_NOT_FOUND;memcpy(out,record,40);*n=40;return 0;}
static int32_t put(void*c,const char*k,const void*p,uint32_t n){(void)c;assert(n==40&&strlen(k)==15);if(write_failure)return -5;strcpy(stored_key,k);memcpy(record,p,40);present=true;return uncertain?-5:0;}
int main(void){
 risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};uint8_t address[]={1,2,3,4,5,6},other[]={1,2,3,4,5,7};char name[25],key[16];
 ble_alias_key(1,address,key);assert(!strcmp(key,"bs1010203040506"));assert(ble_alias_load(&kv,1,address,name)==0&&!name[0]);
 assert(ble_alias_save(&kv,1,address,"Kitchen"));assert(ble_alias_load(&kv,1,address,name)==1&&!strcmp(name,"Kitchen"));
 assert(ble_alias_load(&kv,0,address,name)==0&&ble_alias_load(&kv,1,other,name)==0);
 assert(!ble_alias_save(&kv,1,address,"Too long to be a valid name for this sensor"));assert(!ble_alias_save(&kv,1,address,"Line\nbreak"));
 write_failure=true;assert(!ble_alias_save(&kv,1,address,"Bedroom"));write_failure=false;
 uncertain=true;assert(ble_alias_save(&kv,1,address,"Bedroom"));uncertain=false;
 record[15]^=1;assert(ble_alias_load(&kv,1,address,name)==-1);record[15]^=1;
 assert(ble_alias_save(&kv,1,address,""));assert(ble_alias_load(&kv,1,address,name)==1&&!name[0]);
 uint8_t b[40];assert(ble_alias_encode(1,address,"123456789012345678901234",b));assert(ble_alias_decode(1,address,b,40,name)&&strlen(name)==24);
 assert(!ble_alias_decode(0,address,b,40,name)&&!ble_alias_decode(1,address,b,39,name));
 puts("BLE names: persistent address+type identity, CRC, length, tombstone, explicit writes and readback passed");
}
