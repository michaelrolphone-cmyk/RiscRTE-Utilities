#ifndef SPECTRUM_TEMPORAL_FILES_H
#define SPECTRUM_TEMPORAL_FILES_H
#include "spectrum_temporal_store.h"
#include "RiscAppDataV1.h"
#define ST_FILES_CORRUPT (-100)
#define ST_FILES_CONFLICT (-101)
#define ST_FILES_BUSY (-102)
/* One pending user edit at a time. Immutable full old/new snapshots are made
 * before any provider call can yield. Retry compares exact bytes, never just a
 * CRC, revision assumption or partial read. Other namespaces may invalidate all
 * revision tokens; every save begins with stat/read and passes the fresh token. */
typedef struct {
 const risc_app_data_v1 *api;
 uint8_t ready,exists;
 int pending;
 int32_t errors[2];
 bool retained,base_exists;
 uint32_t base_size,wanted_size;
 uint8_t base[ST_BANK_MAX],wanted[ST_BANK_MAX],readback[ST_BANK_MAX];
} st_files;
static inline const char *st_file_name(unsigned bank){return bank?"spectrum-events-b.sqt":"spectrum-events-a.sqt";}
static inline void st_files_init(st_files *s,const risc_app_data_v1 *api){memset(s,0,sizeof(*s));s->pending=-1;s->api=api;if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->stat||!api->read||!api->replace)s->api=NULL;}
static inline int32_t st_files_result(st_files*s,unsigned bank,int32_t result){s->errors[bank]=result;if(result==RISC_APP_DATA_RETAINED)s->retained=true;return result;}
static inline void st_bank_empty(st_library*l,unsigned bank){memset(l->labels+bank*4,0,sizeof(st_label)*4);l->generation[bank]=0;}
static inline int32_t st_files_read_current(st_files*s,unsigned bank,uint32_t *size,uint64_t *revision){
 *size=0;*revision=0;if(s->retained)return RISC_APP_DATA_RETAINED;
 if(!s->api)return RISC_APP_DATA_UNAVAILABLE;
 int32_t result=s->api->stat(s->api->context,st_file_name(bank),size,revision);
 if(result!=RISC_APP_DATA_OK)return st_files_result(s,bank,result);
 if(!*revision||*size<ST_BANK_MIN||*size>ST_BANK_MAX)return st_files_result(s,bank,ST_FILES_CORRUPT);
 uint32_t actual_size=0;uint64_t actual_revision=0;result=s->api->read(s->api->context,st_file_name(bank),*revision,s->readback,sizeof(s->readback),&actual_size,&actual_revision);
 if(result!=RISC_APP_DATA_OK)return st_files_result(s,bank,result);
 if(actual_size!=*size||!actual_revision)return st_files_result(s,bank,ST_FILES_CORRUPT);
 *revision=actual_revision;return RISC_APP_DATA_OK;
}
static inline int32_t st_files_load(st_files*s,st_library*l,unsigned bank,bool discard){
 if(bank>=2||(!discard&&s->pending==(int)bank))return ST_FILES_BUSY;
 uint32_t size;uint64_t revision;int32_t result=st_files_read_current(s,bank,&size,&revision);
 if(result==RISC_APP_DATA_NOT_FOUND){st_bank_empty(l,bank);s->exists&=(uint8_t)~(1u<<bank);}
 else if(result==RISC_APP_DATA_OK){if(!st_bank_decode(l,bank,s->readback,size)){s->ready&=(uint8_t)~(1u<<bank);return st_files_result(s,bank,ST_FILES_CORRUPT);}s->exists|=(uint8_t)(1u<<bank);}
 else{s->ready&=(uint8_t)~(1u<<bank);return result;}
 s->ready|=(uint8_t)(1u<<bank);if(discard&&s->pending==(int)bank){s->pending=-1;s->wanted_size=0;}return st_files_result(s,bank,RISC_APP_DATA_OK);
}
static inline int32_t st_files_begin(st_files*s,const st_library*l,unsigned bank){
 if(bank>=2||s->pending>=0)return ST_FILES_BUSY;
 if(s->retained)return RISC_APP_DATA_RETAINED;
 if(!(s->ready&(1u<<bank)))return s->errors[bank]?s->errors[bank]:RISC_APP_DATA_UNAVAILABLE;
 if(l->generation[bank]==UINT32_MAX)return RISC_APP_DATA_INVALID;
 s->base_exists=!!(s->exists&(1u<<bank));s->base_size=s->base_exists?(uint32_t)st_bank_encode(l,bank,s->base,sizeof(s->base)):0;if(s->base_exists&&!s->base_size)return ST_FILES_CORRUPT;s->pending=(int)bank;s->wanted_size=0;return RISC_APP_DATA_OK;
}
static inline bool st_files_freeze(st_files*s,st_library*l){
 if(s->pending<0||s->wanted_size)return false;
 unsigned bank=(unsigned)s->pending;l->generation[bank]++;s->wanted_size=(uint32_t)st_bank_encode(l,bank,s->wanted,sizeof(s->wanted));
 if(!s->wanted_size){if(s->base_exists)(void)st_bank_decode(l,bank,s->base,s->base_size);else st_bank_empty(l,bank);s->pending=-1;return false;}return true;
}
static inline int32_t st_files_save(st_files*s){
 if(s->pending<0)return RISC_APP_DATA_OK;
 unsigned bank=(unsigned)s->pending;
 if(s->retained)return RISC_APP_DATA_RETAINED;
 if(!s->wanted_size||!s->api)return st_files_result(s,bank,RISC_APP_DATA_UNAVAILABLE);
 uint32_t size=0;uint64_t revision=0;int32_t result=st_files_read_current(s,bank,&size,&revision);
 if(result==RISC_APP_DATA_OK){
  if(size==s->wanted_size&&!memcmp(s->readback,s->wanted,size)){s->pending=-1;s->ready|=(uint8_t)(1u<<bank);s->exists|=(uint8_t)(1u<<bank);return st_files_result(s,bank,RISC_APP_DATA_OK);}
  if(!s->base_exists||size!=s->base_size||memcmp(s->readback,s->base,size))return st_files_result(s,bank,ST_FILES_CONFLICT);
 }else if(result==RISC_APP_DATA_NOT_FOUND){if(s->base_exists)return st_files_result(s,bank,ST_FILES_CONFLICT);revision=0;}
 else return result;
 result=s->api->replace(s->api->context,st_file_name(bank),revision,s->wanted,s->wanted_size);
 if(result==RISC_APP_DATA_OK){s->pending=-1;s->ready|=(uint8_t)(1u<<bank);s->exists|=(uint8_t)(1u<<bank);}return st_files_result(s,bank,result);
}
#endif
