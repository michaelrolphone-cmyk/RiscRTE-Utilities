#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
static risc_storage_volume_api_v1_fs fs_api;
static FILE* sf_handles[8];static char sf_root[512];
static unsigned sf_opens,sf_writes,sf_syncs,sf_replacements,sf_reads;
static bool sf_short,sf_close_fail,sf_retain,sf_missing,sf_replace_fail,sf_retain_on_write;
static void sf_path(const char*path,char*out){assert(path[0]=='/'&&!strstr(path,".."));snprintf(out,1024,"%s%s",sf_root,path);}
static bool sf_ready(void*c){(void)c;assert(native_alive&&!sf_retain);return !sf_missing;}
static int32_t sf_observe(void*c){(void)c;assert(native_alive);return sf_retain?RISC_STORAGE_STATE_RETAINED:sf_missing?RISC_STORAGE_STATE_UNAVAILABLE:RISC_STORAGE_STATE_READY;}
static bool sf_stat(void*c,const char*p,uint64_t*z,bool*d){(void)c;assert(native_alive&&!sf_retain);char path[1024];sf_path(p,path);struct stat st;if(sf_missing||stat(path,&st))return false;*z=(uint64_t)st.st_size;*d=S_ISDIR(st.st_mode);return true;}
static uint32_t sf_open(void*c,const char*p,uint32_t flags){(void)c;assert(native_alive&&!sf_retain);if(sf_missing)return 0;char path[1024];sf_path(p,path);unsigned i=1;while(i<8&&sf_handles[i])i++;assert(i<8);
 const char*m=flags&RISC_STORAGE_OPEN_WRITE?(flags&RISC_STORAGE_OPEN_EXCLUSIVE?"wbx":flags&RISC_STORAGE_OPEN_TRUNCATE?"wb":"r+b"):"rb";
 sf_handles[i]=fopen(path,m);if(!sf_handles[i])return 0;sf_opens++;return i;}
static uint32_t sf_open_read(void*c,const char*p,uint64_t*z){bool d;if(!sf_stat(c,p,z,&d)||d)return 0;return sf_open(c,p,RISC_STORAGE_OPEN_READ);}
static size_t sf_read(void*c,uint32_t f,void*b,size_t n){(void)c;assert(native_alive&&!sf_retain&&f<8&&sf_handles[f]);sf_reads++;return fread(b,1,n,sf_handles[f]);}
static size_t sf_write(void*c,uint32_t f,const void*b,size_t n){(void)c;assert(native_alive&&!sf_retain&&f<8&&sf_handles[f]);sf_writes++;if(sf_retain_on_write){sf_retain=true;return 0;}if(sf_short)n/=2;return fwrite(b,1,n,sf_handles[f]);}
static bool sf_sync(void*c,uint32_t f){(void)c;assert(native_alive&&!sf_retain&&f<8&&sf_handles[f]);sf_syncs++;return !fflush(sf_handles[f]);}
static bool sf_close(void*c,uint32_t f,bool commit){(void)c;assert(native_alive&&!sf_retain&&f<8&&sf_handles[f]&&commit);if(sf_close_fail)return false;bool ok=!fclose(sf_handles[f]);sf_handles[f]=NULL;return ok;}
static bool sf_mkdir(void*c,const char*p){(void)c;assert(native_alive&&!sf_retain);char path[1024];sf_path(p,path);return !mkdir(path,0700);}
static bool sf_recover(void*c,const char*p){(void)c;(void)p;assert(native_alive&&!sf_retain);return !sf_missing;}
static bool sf_replace(void*c,const char*p,const char*q){(void)c;assert(native_alive&&!sf_retain);if(sf_replace_fail)return false;char from[1024],to[1024];sf_path(p,from);sf_path(q,to);sf_replacements++;return !rename(from,to);}
static uint32_t sf_error(void*c,uint32_t f,bool d){(void)c;(void)f;(void)d;return 0;}
static bool sf_tell(void*c,uint32_t d,uint64_t*p){(void)c;(void)d;(void)p;assert(0);return false;}
static bool sf_seek(void*c,uint32_t d,uint64_t p){(void)c;(void)d;(void)p;assert(0);return false;}
static int32_t sf_metadata(void*c,const char*p,risc_storage_metadata_v1*m){(void)c;(void)p;(void)m;assert(0);return -1;}
static bool sf_truncate(void*c,uint32_t f,uint64_t n){(void)c;(void)f;(void)n;assert(0);return false;}
static void sf_init(void){
 snprintf(sf_root,sizeof(sf_root),"%s/sd",frames);mkdir(sf_root,0700);char p[1024];sf_path("/Apps",p);mkdir(p,0700);sf_path("/Apps/obd-dashboard",p);mkdir(p,0700);
 risc_storage_volume_api_v1_ext*e=(void*)&fs_api;*e=(risc_storage_volume_api_v1_ext){.base={.api_version=1,.struct_size=sizeof(fs_api),.ready=sf_ready,.stat=sf_stat,.file_open_read=sf_open_read,.file_read=sf_read,.file_write=sf_write,.file_close=sf_close},.file_open=sf_open,.file_sync=sf_sync,.mkdir=sf_mkdir,.handle_error=sf_error};
 fs_api.state.state_tag=RISC_STORAGE_STATE_TAG;fs_api.state.state_version=1;fs_api.state.observe=sf_observe;
 fs_api.fs_tag=RISC_STORAGE_FS_TAG;fs_api.fs_version=1;fs_api.dir_tell=sf_tell;fs_api.dir_seek=sf_seek;fs_api.metadata=sf_metadata;fs_api.file_truncate=sf_truncate;fs_api.replace_file=sf_replace;fs_api.recover_replace=sf_recover;
}
