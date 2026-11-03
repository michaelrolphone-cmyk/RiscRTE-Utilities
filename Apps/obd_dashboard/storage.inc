/* Settings transactions and explicit trip recording use only the granted SD
 * volume. User files are never truncated; trip creation is exclusive. */
static void feedback(obd_app*,const char*);
#define DB_SETTINGS_PATH "/Apps/obd-dashboard/settings.v1"
#define DB_SETTINGS_TEMP "/Apps/obd-dashboard/settings.tmp"
typedef struct {uint32_t magic,size,crc;db_config config;} settings_record;
static bool storage_api_valid(const risc_storage_volume_api_v1_ext*v,const risc_storage_volume_api_v1_fs*f){
 return v&&f&&v->base.ready&&v->base.stat&&v->base.file_open_read&&v->base.file_read&&v->base.file_write&&v->base.file_close&&v->file_open&&v->file_sync&&v->mkdir&&v->handle_error&&f->state.observe&&f->replace_file&&f->recover_replace;
}
static bool storage_alive(obd_app*a){
 if(!authority(a))return retain_app(a);
 if(a->fs){int32_t state=a->fs->state.observe(a->volume->base.context);if(!authority(a)||state==RISC_STORAGE_STATE_RETAINED)return retain_app(a);}
 return true;
}
static void storage_message(obd_app*a,const char*s){a->storage_error=true;feedback(a,s);}
static bool volume_close(obd_app*a,risc_storage_file_t file){
 bool ok=a->volume->base.file_close(a->volume->base.context,file,true);
 if(!storage_alive(a))return false;if(!ok)return retain_app(a);return true;
}
static bool load_settings(obd_app*a){
 if(!a->fs||!a->volume)return true;
 bool recovered=a->fs->recover_replace(a->volume->base.context,DB_SETTINGS_PATH);if(!storage_alive(a))return false;
 if(!recovered){storage_message(a,"Settings recovery failed; using defaults");return true;}
 uint64_t size=0;bool directory=false,exists=a->volume->base.stat(a->volume->base.context,DB_SETTINGS_PATH,&size,&directory);if(!storage_alive(a))return false;
 if(!exists)return true;if(directory||size!=sizeof(settings_record)){storage_message(a,"Settings invalid; using defaults");return true;}
 risc_storage_file_t f=a->volume->base.file_open_read(a->volume->base.context,DB_SETTINGS_PATH,&size);if(!storage_alive(a))return false;
 if(!f){storage_message(a,"Cannot read settings");return true;}
 settings_record r;size_t n=a->volume->base.file_read(a->volume->base.context,f,&r,sizeof(r));if(!storage_alive(a))return false;uint32_t error=a->volume->handle_error(a->volume->base.context,f,false);if(!storage_alive(a)||!volume_close(a,f))return false;
 if(!error&&n==sizeof(r)&&r.magic==0x4f424431u&&r.size==sizeof(r.config)&&r.crc==db_crc(&r.config,sizeof(r.config))&&db_config_valid(&r.config))a->data.cfg=r.config;
 else storage_message(a,"Settings invalid; using defaults");return true;
}
static bool save_settings(obd_app*a){
 if(!a->save_config)return true;a->save_config=false;
 if(!a->fs||!a->volume){storage_message(a,"SD settings unavailable; session only");return true;}
 if(!db_config_valid(&a->data.cfg)){storage_message(a,"Settings validation failed");return true;}
 bool recovered=a->fs->recover_replace(a->volume->base.context,DB_SETTINGS_PATH);if(!storage_alive(a))return false;
 if(!recovered){storage_message(a,"Settings recovery failed");return true;}
 settings_record r={.magic=0x4f424431u,.size=sizeof(db_config),.config=a->data.cfg};r.crc=db_crc(&r.config,sizeof(r.config));
 risc_storage_file_t f=a->volume->file_open(a->volume->base.context,DB_SETTINGS_TEMP,RISC_STORAGE_OPEN_WRITE|RISC_STORAGE_OPEN_CREATE|RISC_STORAGE_OPEN_TRUNCATE);
 if(!storage_alive(a))return false;if(!f){storage_message(a,"Cannot save settings");return true;}
 size_t n=a->volume->base.file_write(a->volume->base.context,f,&r,sizeof(r));if(!storage_alive(a))return false;
 uint32_t error=a->volume->handle_error(a->volume->base.context,f,false);if(!storage_alive(a))return false;
 bool synced=a->volume->file_sync(a->volume->base.context,f);if(!storage_alive(a)||!volume_close(a,f))return false;
 if(error||n!=sizeof(r)||!synced){storage_message(a,"Settings write failed; old file retained");return true;}
 bool ok=a->fs->replace_file(a->volume->base.context,DB_SETTINGS_TEMP,DB_SETTINGS_PATH);if(!storage_alive(a))return false;
 if(!ok)storage_message(a,"Settings commit failed; recovery retained");return true;
}
static bool log_close(obd_app*a){
 if(!a->log_file)return true;bool synced=a->volume->file_sync(a->volume->base.context,a->log_file);
 if(!storage_alive(a)||!volume_close(a,a->log_file))return false;a->log_file=0;
 if(!synced)storage_message(a,"Trip sync failed; inspect saved CSV");return true;
}
static bool log_start(obd_app*a){
 if(!a->volume||!a->fs){storage_message(a,"Trip logging needs the SD volume");return true;}
 if(a->elm.state!=OBD_LIVE){feedback(a,"Connect to the ECU before recording");return true;}
 bool ready=a->volume->base.ready(a->volume->base.context);if(!storage_alive(a))return false;if(!ready){storage_message(a,"SD card is not ready");return true;}
 uint64_t size;bool isdir=false;bool exists=a->volume->base.stat(a->volume->base.context,"/OBD",&size,&isdir);if(!storage_alive(a))return false;
 if(exists&&!isdir){storage_message(a,"/OBD is not a directory");return true;}
 if(!exists){bool ok=a->volume->mkdir(a->volume->base.context,"/OBD");if(!storage_alive(a))return false;if(!ok){storage_message(a,"Cannot create /OBD");return true;}}
 for(unsigned attempt=0;attempt<256;attempt++){
  if(a->data.cfg.trip_sequence==UINT32_MAX){storage_message(a,"Trip sequence exhausted");return true;}
  snprintf(a->log_path,sizeof(a->log_path),"/OBD/trip-%08lu.csv",(unsigned long)++a->data.cfg.trip_sequence);
  exists=a->volume->base.stat(a->volume->base.context,a->log_path,&size,&isdir);if(!storage_alive(a))return false;if(exists)continue;
  a->log_file=a->volume->file_open(a->volume->base.context,a->log_path,RISC_STORAGE_OPEN_WRITE|RISC_STORAGE_OPEN_CREATE|RISC_STORAGE_OPEN_EXCLUSIVE);
  if(!storage_alive(a))return false;break;
 }
 if(!a->log_file){storage_message(a,"Cannot create a new trip file");return true;}
 static const char header[]="elapsed_ms,ecu,pid,value_milli,unit\n";
 size_t n=a->volume->base.file_write(a->volume->base.context,a->log_file,header,sizeof(header)-1);if(!storage_alive(a))return false;
 if(n!=sizeof(header)-1){if(!log_close(a))return false;storage_message(a,"Trip header write failed");return true;}
 a->log_bytes=(uint32_t)n;a->last_log_sync=a->now;memset(a->log_seen_once,0,sizeof(a->log_seen_once));a->save_config=true;feedback(a,"Recording CSV in /OBD");return true;
}
static bool log_samples(obd_app*a){
 if(!a->log_file)return true;
 for(unsigned i=0;i<OBD_METRICS;i++){
  const obd_sample*s=&a->elm.samples[i];if(!obd_fresh(&a->elm,i,a->now)||(a->log_seen_once[i]&&a->log_seen[i]==s->at_ms))continue;
  char line[112];int length=snprintf(line,sizeof(line),"%lu,%lX,%02X,%ld,%s\n",(unsigned long)(s->at_ms-a->data.started_ms),(unsigned long)a->elm.ecu,obd_specs[i].pid,(long)s->milli,obd_specs[i].unit);
  if(length<=0||length>=(int)sizeof(line)){storage_message(a,"Trip row overflow");return log_close(a);}
  size_t n=a->volume->base.file_write(a->volume->base.context,a->log_file,line,(size_t)length);if(!storage_alive(a))return false;
  if(n!=(size_t)length){if(!log_close(a))return false;storage_message(a,"Trip write failed; recording stopped");return true;}
  a->log_bytes+=(uint32_t)n;a->log_seen_once[i]=true;a->log_seen[i]=s->at_ms;
 }
 if((uint32_t)(a->now-a->last_log_sync)>=2000){bool ok=a->volume->file_sync(a->volume->base.context,a->log_file);if(!storage_alive(a))return false;a->last_log_sync=a->now;
  if(!ok){if(!log_close(a))return false;storage_message(a,"Trip sync failed; recording stopped");}}
 return true;
}
