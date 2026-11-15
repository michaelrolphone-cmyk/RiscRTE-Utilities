#ifndef RISC_TEST_IMAGE_OBSERVER_H
#define RISC_TEST_IMAGE_OBSERVER_H
#include <cassert>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace ImageObserver {
static std::string root;
static void set_root(const std::string& value){root=value;}
struct Image { std::string name,path; void* handle; const void* address; unsigned attempts=0,closed=0; };
static std::vector<Image> images;
struct DeclaredImage {std::string name,entry;};
static std::vector<DeclaredImage> declared_images;
static void declare_image(const std::string& name,const char* entry){
    for(const auto& prior:declared_images)if(prior.name==name){assert(prior.entry==entry);return;}
    declared_images.push_back({name,entry});
}
extern "C" void* __real_dlopen(const char*,int);
extern "C" int __real_dlclose(void*);
static std::string bytes(const std::string& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
extern "C" void* __wrap_dlopen(const char* path,int flags){
    std::string name,entry;
    if(path&&!(flags&RTLD_NOLOAD)){
        const auto input=bytes(path);assert(!input.empty());
        for(const auto& candidate:declared_images)
            if(input==bytes(root+"/"+candidate.name)){assert(name.empty());name=candidate.name;entry=candidate.entry;}
        assert(!name.empty()&&"Unexpected fixture image");
    }
    void* handle=__real_dlopen(path,flags);
    if(handle&&!name.empty()){
        // Exact known fixture exports exist. Successful dlsym/dladdr do not
        // consume dlerror; never clear or replace the loader's error state.
        const void* address=dlsym(handle,entry.c_str());
        Dl_info info{};assert(address&&dladdr(address,&info)&&info.dli_fname);
        images.push_back({name,info.dli_fname,handle,address});
    }
    return handle;
}
extern "C" int __wrap_dlclose(void* handle){
    Image* selected=nullptr;
    for(auto& image:images)if(image.handle==handle&&!image.closed){assert(!selected);selected=&image;}
    assert(selected);++selected->attempts;
    const int result=__real_dlclose(handle);
    if(!result)++selected->closed;
    return result;
}
static bool retained_image(const char* name){
    unsigned count=0;
    for(const auto& image:images)if(image.name==name){
        Dl_info info{};assert(!image.attempts&&!image.closed);
        assert(dladdr(image.address,&info)&&info.dli_fname&&image.path==info.dli_fname);++count;
    }
    assert(count==1);return true;
}
static void all_images_closed(){
    assert(images.size()>=declared_images.size());
    for(const auto& image:images){
        assert(image.attempts==1&&image.closed==1);
        // Query loader metadata using the numeric address; never dereference
        // unmapped code. A successful close alone would not prove unloading.
        Dl_info info{};
        if(dladdr(image.address,&info))assert(!info.dli_fname||image.path!=info.dli_fname);
    }
}


}
#endif
