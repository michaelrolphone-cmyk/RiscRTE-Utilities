// Test-only in-memory disk and native owner/worker guard, derived from Runtime native replica regression fixtures.
#include "experimental/native_replica/NativeReplicaCore.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
using C=RiscExperimental::NativeReplicaCore;
using Bytes=std::vector<uint8_t>;
struct Disk {
 struct PowerCut {};
 enum Mode{Before,After,Torn};
 Bytes root;std::map<uint64_t,Bytes> objects;uint64_t next=1;
 unsigned calls=0,cut=0;Mode mode=Before;bool crash=false;size_t capacity=16*1024*1024;
 bool faultAfterPublication=false,failNextRead=false;
 bool begin(){++calls;if(cut==calls&&mode==Before&&crash)throw PowerCut{};return cut==calls&&mode==Before;}
 bool end()const{if(cut==calls&&mode!=Before&&crash)throw PowerCut{};return cut==calls&&mode!=Before;}
 void fault(unsigned n,Mode m,bool power=false){calls=0;cut=n;mode=m;crash=power;}
 C::Backend backend(){return {this,
  [](void*v,void*p,uint32_t n){auto&d=*static_cast<Disk*>(v);if(d.failNextRead){d.failNextRead=false;return int(C::Io);}if(d.begin())return int(C::Io);if(d.root.empty())return int(C::Missing);if(d.root.size()!=n)return int(C::Io);memcpy(p,d.root.data(),n);return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,uint32_t n,uint64_t*id){auto&d=*static_cast<Disk*>(v);if(d.begin())return int(C::Io);size_t used=0;for(auto&e:d.objects)used+=e.second.size();if(used>d.capacity||n>d.capacity-used)return int(C::NoSpace);*id=d.next++;d.objects[*id]=Bytes(n,0xa5);return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,uint64_t id,uint32_t at,const void*p,uint32_t n){auto&d=*static_cast<Disk*>(v);if(d.begin())return int(C::Io);auto it=d.objects.find(id);if(it==d.objects.end()||at>it->second.size()||n>it->second.size()-at)return int(C::Io);const uint32_t written=d.cut==d.calls&&d.mode==Torn?n/2:n;memcpy(it->second.data()+at,p,written);return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,uint64_t id,uint32_t at,void*p,uint32_t n){auto&d=*static_cast<Disk*>(v);if(d.begin())return int(C::Io);auto it=d.objects.find(id);if(it==d.objects.end()||at>it->second.size()||n>it->second.size()-at)return int(C::Io);memcpy(p,it->second.data()+at,n);if(d.faultAfterPublication){d.faultAfterPublication=false;d.failNextRead=true;}return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,uint64_t id){auto&d=*static_cast<Disk*>(v);if(d.begin())return int(C::Io);if(!d.objects.count(id))return int(C::Io);return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,const void*p,uint32_t n){auto&d=*static_cast<Disk*>(v);if(d.begin())return int(C::Io); // Root is atomic even when immutable payload writes can tear.
   if(!(d.cut==d.calls&&d.mode==Torn))d.root.assign(static_cast<const uint8_t*>(p),static_cast<const uint8_t*>(p)+n);
   if(d.faultAfterPublication){d.faultAfterPublication=false;d.failNextRead=true;}return d.end()?int(C::Io):int(C::Ok);},
  [](void*v,uint64_t id){auto&d=*static_cast<Disk*>(v);if(d.begin())return;if(d.root.size()==C::RootBytes){uint64_t active=0;for(unsigned i=0;i<8;++i)active|=uint64_t(d.root[24+i])<<(8*i);assert(active!=id);}d.objects.erase(id);}
 };}
};
struct Alloc {
 unsigned calls=0,fail=0,watchWipe=0,wiped=0;void*watched=nullptr;std::map<void*,size_t> live;size_t bytes=0,peak=0;
 C::Allocator api(){return {this,[](void*v,size_t n)->void*{auto&a=*static_cast<Alloc*>(v);if(++a.calls==a.fail)return nullptr;void*p=malloc(n);if(p){if(a.calls==a.watchWipe)a.watched=p;a.live[p]=n;a.bytes+=n;if(a.bytes>a.peak)a.peak=a.bytes;}return p;},[](void*v,void*p){auto&a=*static_cast<Alloc*>(v);auto i=a.live.find(p);assert(i!=a.live.end());if(p==a.watched){for(size_t k=0;k<i->second;++k)assert(!static_cast<uint8_t*>(p)[k]);++a.wiped;a.watched=nullptr;}a.bytes-=i->second;a.live.erase(i);free(p);}};}
 void discardVolatileAfterPowerCut(){for(auto&v:live)free(v.first);live.clear();bytes=0;}
 ~Alloc(){assert(live.empty()&&bytes==0);}
};
C::Limits limits(){return {4*1024*1024,4096,1024};}
C::Identity base(uint64_t gen,uint8_t lineage=1){C::Identity i{};i.known=true;i.generation=gen;i.lineage[0]=lineage;i.digest[0]=uint8_t(gen);return i;}
std::string get(C&c,const char*k){char v[2048];uint32_t n=0;assert(c.get(k,v,sizeof v,&n)==C::Ok);return std::string(v,n);}
void missing(C&c,const char*k){char v[4];uint32_t n=99;assert(c.get(k,v,sizeof v,&n)==C::Missing&&n==0);}
int put(C&c,const char*k,const char*v,const char*intent="explicit-save"){C::Mutation m{k,C::Put,v,uint32_t(strlen(v))};return c.apply(c.revision(),&m,1,intent,uint32_t(strlen(intent)));}
std::vector<uint64_t> pending(C&c){std::vector<uint64_t> out;uint64_t after=0,id=0;char intent[1024];uint32_t n;for(;;){int rc=c.pending(after,&id,intent,sizeof intent,&n);if(rc==C::Missing)return out;assert(rc==C::Ok&&n);out.push_back(id);after=id;}}
#include "experimental/native_replica/NativeAtomicKeyValue.h"
#include <thread>
#include <mutex>
#include <condition_variable>
using A=RiscExperimental::NativeAtomicKeyValue;
struct Gate {
 std::thread::id ownerThread=std::this_thread::get_id(),workerThread{};
 bool held=false,deny=false;int finishResult=0;
 A::Guard api(){return {this,
 [](void*p){return std::this_thread::get_id()==static_cast<Gate*>(p)->ownerThread;},
 [](void*p){return std::this_thread::get_id()==static_cast<Gate*>(p)->workerThread;},
 [](void*p){auto&g=*static_cast<Gate*>(p);if(g.held||g.deny)return false;g.held=true;return true;},
 [](void*p){auto&g=*static_cast<Gate*>(p);assert(g.held);g.held=false;return true;},[](void*p){return static_cast<Gate*>(p)->finishResult;}};}
 void run(A&a){workerThread=ownerThread;assert(a.workerStep());workerThread={};}
};

static A* atomic_endpoint;static Gate* atomic_gate;static unsigned atomic_steps,atomic_waits;

static C* atomic_core;static bool uncertain_case;
