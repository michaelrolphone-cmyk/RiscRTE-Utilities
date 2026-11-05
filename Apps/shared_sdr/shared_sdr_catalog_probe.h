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
static bool sdr_catalog_grant_equal(const RiscBoot::Runtime::AppGrantPolicy&a,
                                    const RiscBoot::Runtime::AppGrantPolicy&b){
 return a.capability&&b.capability&&!strcmp(a.capability,b.capability)&&
  a.api==b.api&&a.instance==b.instance&&a.driver==b.driver&&a.platform==b.platform&&
  a.keyValue==b.keyValue&&a.installedFiles==b.installedFiles&&
  a.fileOpen==b.fileOpen&&a.fileOpenSource==b.fileOpenSource&&!strcmp(a.sharedFile,b.sharedFile);
}
static bool sdr_catalog_source_equal(const RiscBoot::Runtime::AppPolicy&a,
                                     const RiscBoot::Runtime::AppPolicy&b){
 return a.sourceDriver==b.sourceDriver&&a.sourceFiles==b.sourceFiles&&
  a.internalFallback==b.internalFallback&&!strcmp(a.sourcePath,b.sourcePath);
}
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
  const auto&a=authority->grants[i];size_t matches=0;
  for(size_t j=0;j<policy.count;++j)if(sdr_catalog_grant_equal(a,policy.grants[j]))++matches;
  if(matches!=1)return false;
  // Negative controls prove backend/dispatch changes cannot pass as identical
  // public capability tuples. Mutate copies only, never the live Runtime.
  auto changed=a;changed.driver=decltype(changed.driver)(a.driver+1);
  if(sdr_catalog_grant_equal(a,changed))return false;
  changed=a;changed.platform=decltype(changed.platform)(a.platform+1);
  if(sdr_catalog_grant_equal(a,changed))return false;
  changed=a;changed.keyValue=!a.keyValue;
  if(sdr_catalog_grant_equal(a,changed))return false;
  changed=a;changed.installedFiles=!a.installedFiles;
  if(sdr_catalog_grant_equal(a,changed))return false;
  changed=a;changed.fileOpen=!a.fileOpen;
  if(sdr_catalog_grant_equal(a,changed))return false;
  changed=a;changed.fileOpenSource=!a.fileOpenSource;
  if(sdr_catalog_grant_equal(a,changed))return false;
 }
 const std::string expected=std::string(relative).substr(0,std::string(relative).rfind('/')+1)+"waterfall.elf";
 if(!sdr_catalog_source_equal(policy,*authority)||policy.sourceDriver<0||
    std::string("/")+expected!=policy.sourcePath||
    std::string(r.root_)+"/"+expected!=policy.elf)return false;
 auto changedSource=policy;changedSource.sourceDriver=decltype(policy.sourceDriver)(policy.sourceDriver+1);
 if(sdr_catalog_source_equal(policy,changedSource))return false;
 changedSource=policy;changedSource.sourcePath[0]='!';
 if(sdr_catalog_source_equal(policy,changedSource))return false;
 changedSource=policy;changedSource.sourceFiles=!policy.sourceFiles;
 if(sdr_catalog_source_equal(policy,changedSource))return false;
 changedSource=policy;changedSource.internalFallback=!policy.internalFallback;
 if(sdr_catalog_source_equal(policy,changedSource))return false;
 if(expected!=launch||strcmp(policy.catalog.displayName,"Waterfall")||strcmp(policy.catalog.icon,"solid:f0ec"))return false;
 fprintf(stderr,"shared SDR catalog: real installationAdmit inherits exact reservation grants, rejects wrong directory/missing requirement, preserves foreground and presentation\n");
 return true;
}
