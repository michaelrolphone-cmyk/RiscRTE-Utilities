#include "rf_app_data_scope.h"
#include <assert.h>
#include <stdio.h>
static bool live,refuse,dirty,release_fail,malformed;
static unsigned acquisitions,releases,calls,writes;
static int32_t failure;
static uint8_t saved[32]={1,3,5,7};static uint32_t saved_size=4;static uint64_t current=17;
static int32_t stat_file(void*c,const char*n,uint32_t*s,uint64_t*r){(void)c;assert(live&&!strcmp(n,"context-fingerprints.cfp"));calls++;if(failure)return failure;*s=saved_size;*r=current;return 0;}
static int32_t read_file(void*c,const char*n,uint64_t expected,void*b,uint32_t z,uint32_t*s,uint64_t*r){(void)c;assert(live&&!strcmp(n,"context-fingerprints.cfp"));calls++;if(failure)return failure;if(expected!=current)return RISC_APP_DATA_STALE;if(z<saved_size){*s=saved_size;return RISC_APP_DATA_BUFFER_SMALL;}memcpy(b,saved,saved_size);*s=saved_size;*r=current;return 0;}
static int32_t replace_file(void*c,const char*n,uint64_t expected,const void*b,uint32_t z){(void)c;assert(live&&!strcmp(n,"context-fingerprints.cfp"));calls++;if(failure)return failure;if(expected!=current)return RISC_APP_DATA_STALE;assert(z<=sizeof(saved));memcpy(saved,b,z);saved_size=z;current++;writes++;return 0;}
static const risc_app_data_v1 files={1,sizeof(files),0,stat_file,read_file,replace_file};
static const risc_app_data_v1 bad={99,sizeof(bad),0,stat_file,read_file,replace_file};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){assert(!live&&!strcmp(n,RISC_APP_DATA_CAPABILITY)&&v==1&&i==3);acquisitions++;if(refuse){if(dirty){g->api=&files;g->slot=1;}return false;}live=true;g->api=malformed?&bad:&files;g->slot=1;g->generation=acquisitions;return true;}
static bool release(risc_runtime_capability_v1*g){assert(live&&g->api);releases++;if(release_fail)return false;live=false;memset(g,0,sizeof(*g));return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static void fresh(rf_app_data_scope*s){live=refuse=dirty=release_fail=malformed=false;failure=0;rf_data_bind(s,&runtime,3);}
int main(void){
 rf_app_data_scope scope;fresh(&scope);const risc_app_data_v1*p=&scope.api;const char*n="context-fingerprints.cfp";uint32_t z;uint64_t revision;uint8_t bytes[32]={0},original[32];memcpy(original,saved,sizeof(saved));
 assert(!p->stat(p->context,n,&z,&revision)&&z==4&&revision==17&&!live);
 /* The revision survives a checked release/reacquire, with exact saved bytes. */
 assert(!p->read(p->context,n,revision,bytes,sizeof(bytes),&z,&revision)&&!memcmp(bytes,original,z)&&!live&&!writes);
 assert(p->replace(p->context,n,16,"changed",7)==RISC_APP_DATA_STALE&&!memcmp(saved,original,sizeof(saved))&&!live&&!writes);
 assert(!p->replace(p->context,n,revision,"changed",7)&&!live&&writes==1);
 assert(!p->stat(p->context,n,&z,&revision)&&z==7&&revision==18&&!live);
 assert(!p->read(p->context,n,revision,bytes,sizeof(bytes),&z,&revision)&&!memcmp(bytes,"changed",7)&&!live);
 assert(acquisitions==releases);
 failure=RISC_APP_DATA_NO_SPACE;assert(p->replace(p->context,n,revision,"discard",7)==failure&&!live&&writes==1);
 failure=RISC_APP_DATA_COMMIT_UNKNOWN;assert(p->replace(p->context,n,revision,"discard",7)==failure&&!live&&writes==1);
 fresh(&scope);refuse=true;assert(p->stat(p->context,n,&z,&revision)==RISC_APP_DATA_UNAVAILABLE&&!scope.retained&&!live&&z==0&&revision==0);
 fresh(&scope);malformed=true;assert(p->stat(p->context,n,&z,&revision)==RISC_APP_DATA_INVALID&&!scope.retained&&!live);
 for(unsigned mode=0;mode<3;mode++){
  fresh(&scope);if(mode==0)failure=RISC_APP_DATA_RETAINED;else if(mode==1)release_fail=true;else{refuse=dirty=true;}
  assert(p->stat(p->context,n,&z,&revision)==RISC_APP_DATA_RETAINED&&scope.retained&&z==0&&revision==0);
  unsigned a=acquisitions,r=releases,c=calls;
  assert(p->read(p->context,n,17,bytes,sizeof(bytes),&z,&revision)==RISC_APP_DATA_RETAINED);
  assert(p->replace(p->context,n,17,bytes,4)==RISC_APP_DATA_RETAINED);
  assert(a==acquisitions&&r==releases&&c==calls);
 }
 puts("RF app-data scopes: exact data/revisions, failure results, clean refusal, uncertain acquisition/release and terminal no-I/O PASS");
}
