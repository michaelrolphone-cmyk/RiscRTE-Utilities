#include "allocation_budget.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static struct { void *pointer; size_t bytes; } blocks[64];
static size_t live,peak;
void *canvas_test_malloc(size_t bytes) {
 const char *text=getenv("CANVAS_ALLOC_LIMIT");
 size_t limit=text?(size_t)strtoul(text,NULL,10):SIZE_MAX;
 if(bytes>limit || live>limit-bytes)return NULL;
 void *p=malloc(bytes);if(!p)return NULL;
 for(unsigned i=0;i<64;++i)if(!blocks[i].pointer){
  blocks[i].pointer=p;blocks[i].bytes=bytes;live+=bytes;if(live>peak)peak=live;return p;
 }
 assert(!"allocation tracker full");return NULL;
}
void *canvas_test_calloc(size_t count,size_t size) {
 if(size && count>SIZE_MAX/size)return NULL;
 void *p=canvas_test_malloc(count*size);if(p)memset(p,0,count*size);return p;
}
void canvas_test_free(void *p) {
 if(!p)return;
 for(unsigned i=0;i<64;++i)if(blocks[i].pointer==p){
  live-=blocks[i].bytes;blocks[i].pointer=NULL;blocks[i].bytes=0;free(p);return;
 }
 assert(!"unowned or double free");
}
size_t canvas_test_live_bytes(void){return live;}
size_t canvas_test_peak_bytes(void){return peak;}
