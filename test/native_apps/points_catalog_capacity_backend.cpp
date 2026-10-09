#include "runtime/storage/AppDataFiles.h"
#include <cassert>
#include <chrono>
extern "C" void *capacity_backend_allocate(size_t);
extern "C" void capacity_backend_free(void*);
static unsigned long cooperations;
static RiscStorage::AppDataFiles backend({nullptr,[](void*){return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());},[](void*){++cooperations;return true;},capacity_backend_allocate,capacity_backend_free});
extern "C" void capacity_backend_start(const char *root){assert(backend.configure(root));}
extern "C" int32_t capacity_backend_stat(void*,const char*n,uint32_t*s,uint64_t*r){return backend.stat(5,n,s,r);}
extern "C" int32_t capacity_backend_read(void*,const char*n,uint64_t r,void*b,uint32_t cap,uint32_t*s,uint64_t*a){return backend.read(5,n,r,b,cap,s,a);}
extern "C" int32_t capacity_backend_replace(void*,const char*n,uint64_t r,const void*b,uint32_t size){return backend.replace(5,n,r,b,size);}
extern "C" unsigned long capacity_backend_cooperations(void){return cooperations;}
