#pragma once
/* Host-only metadata probe. The owning fixture exposes private Runtime state.
 * Its test-only currentRuntime hook selects an inert foreground context.
 * Production installationAdmit, authority inheritance and catalog parsing run
 * unchanged. No Xtensa entry point, native provider or storage operation runs. */
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
extern "C" RiscBoot::Runtime* sdr_test_select_runtime(RiscBoot::Runtime*);
static bool sdr_catalog_probe(RiscBoot::Runtime&r,const char*relative,const char*file){
 std::ifstream input(file);std::string text(std::istreambuf_iterator<char>(input),{});
 if(text.empty()||!r.installation_)return false;
 JsonDocument manifest;if(deserializeJson(manifest,text))return false;
 if(strcmp(manifest["id"]|"","waterfall"))return false;
 auto* invocation=r.invocation_;const bool active=invocation->active_;
 const auto safe=r.port_.providerStorageSafe;
 auto* previous=sdr_test_select_runtime(&r);
 struct Restore {
  RiscBoot::Runtime&r;RiscBoot::Runtime*previous;bool active;bool(*safe)();
  ~Restore(){r.invocation_->active_=active;r.port_.providerStorageSafe=safe;sdr_test_select_runtime(previous);}
 } restore{r,previous,active,safe};
 invocation->active_=true;r.port_.providerStorageSafe=[](){return true;};
 char launch[193]{};
 if(r.installationAdmit(&r,"Apps/not-waterfall/manifest.json",text.c_str(),launch,sizeof(launch))!=RISC_INSTALL_DENIED)return false;
 JsonDocument missing;missing.set(manifest);missing["requires"][0]["capability"]="test.missing-required";
 std::string missingText;serializeJson(missing,missingText);
 if(r.installationAdmit(&r,relative,missingText.c_str(),launch,sizeof(launch))!=RISC_INSTALL_DENIED)return false;
 // Demonstrate the formerly omitted field actually selects legacy behavior.
 JsonDocument legacy;legacy.set(manifest);legacy.remove("runtime_lifecycle");
 std::string legacyText;serializeJson(legacy,legacyText);
 if(r.installationAdmit(&r,relative,legacyText.c_str(),launch,sizeof(launch))!=RISC_INSTALL_OK)return false;
 size_t index=r.policyCount_;
 for(size_t i=0;i<r.policyCount_;++i)if(!r.policies_[i].catalogAuthority&&!strcmp(r.policies_[i].id,"waterfall"))index=i;
 if(index==r.policyCount_||!r.residentLegacy_[index]||r.residentForeground_[index])return false;
 if(r.installationAdmit(&r,relative,text.c_str(),launch,sizeof(launch))!=RISC_INSTALL_OK)return false;
 const auto&policy=r.policies_[index];const RiscBoot::Runtime::AppPolicy*authority=nullptr;
 for(size_t i=0;i<r.bootPolicyCount_;++i)if(r.policies_[i].catalogAuthority&&!strcmp(r.policies_[i].id,"waterfall"))authority=&r.policies_[i];
 if(!authority||policy.count!=authority->count||!policy.enabled||!r.residentForeground_[index]||r.residentLegacy_[index])return false;
 for(size_t i=0;i<authority->count;++i){
  const auto&a=authority->grants[i];bool found=false;
  for(size_t j=0;j<policy.count;++j){const auto&g=policy.grants[j];
   if(!strcmp(a.capability,g.capability)&&a.api==g.api&&a.instance==g.instance&&
      !strcmp(a.sharedFile,g.sharedFile))found=true;}
  if(!found)return false;
 }
 const std::string expected=std::string(relative).substr(0,std::string(relative).rfind('/')+1)+"waterfall.elf";
 if(expected!=launch||strcmp(policy.catalog.displayName,"Waterfall")||strcmp(policy.catalog.icon,"solid:f0ec"))return false;
 fprintf(stderr,"shared SDR catalog: real installationAdmit inherits exact reservation grants, rejects wrong directory/missing requirement, preserves foreground and presentation\n");
 return true;
}
