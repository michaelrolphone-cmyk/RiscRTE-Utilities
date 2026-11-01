#include <RiscProvisionRequestV1.h>
#include <RiscInternalPackageVolumeV1.h>
#include "experimental/native_replica/NativeAtomicKeyValue.h"
// Admit the complete supplied production store through the real Runtime and
// CpuPort. Nothing rewrites its board, driver manifests, app manifests or policy.
// The lowest hardware boundary is inert; prepare must not invoke native I/O.
#define private public
#include "bootstrap/Runtime.h"
#include "ports/esp32s3/CpuPort.h"
#undef private
#include "ports/esp32s3/NativeSerialStorageBinding.h"
#include "test/RuntimeFileFixture.h"
#include <cstdio>
#if defined(STORE_ADMISSION_COHORT) && !defined(STORE_ADMISSION_COHORT_POLICY)
#include "cohort_elf_admission.h"
#endif
#ifdef STORE_ADMISSION_COHORT_POLICY
// The RF profile test reuses already verified accepted target bytes. This
// callback checks inventory only; it does not qualify future rebuilt ELFs.
namespace CohortElf {
static bool file(void* context,const char* path,bool) {
  FILE* input=fopen(path,"rb");if(!input)return false;
  const bool present=fgetc(input)!=EOF;const bool closed=fclose(input)==0;
  if(present && closed)++*static_cast<unsigned*>(context);
  return present && closed;
}
}
#ifdef STORE_ADMISSION_RUNTIME_FEATURES
static_assert(RiscBoot::Runtime::MaxAppPolicyGrants==16,"Runtime feature grant bound changed");
#else
static_assert(RiscBoot::Runtime::MaxAppPolicyGrants==12,"RF grant bound changed");
#endif
static_assert(RISC_APP_DATA_NAMESPACE_MAX==131072,"RF namespace quota changed");
static_assert(RISC_APP_DATA_FILE_MAX==65536 && RISC_APP_DATA_FILES_MAX>=3,"RF file bounds changed");
#endif
#ifdef STORE_ADMISSION_APP_DATA
#include "app_data_admission_backend.h"
#endif
#ifdef STORE_ADMISSION_UPDATE_PLATFORMS
#include <RiscHttpClientV1.h>
#include <RiscBankStoreV1.h>
#endif

#ifdef STORE_ADMISSION_DIAGNOSTIC_SOURCE
#include <RiscDiagnosticSourceV1.h>
#endif

namespace {
unsigned hardwareCalls = 0, storageCalls = 0;
#ifdef STORE_ADMISSION_COHORT
unsigned cooperativeYields = 0;
#endif
RiscCpu::Port* cpu = nullptr;
bool owner() { return true; }
#ifdef STORE_ADMISSION_UPDATE_PLATFORMS
risc_http_client_v1 httpApi{
  1,sizeof(httpApi),nullptr,
  [](void*,const risc_http_request_v1*,uint64_t*)->int32_t{return RISC_HTTP_CLOSED;},
  [](void*,uint64_t,void*,uint32_t,uint32_t*)->int32_t{return RISC_HTTP_CLOSED;},
  [](void*,uint64_t,risc_http_response_v1*)->int32_t{return RISC_HTTP_CLOSED;},
  [](void*,uint64_t)->int32_t{return RISC_HTTP_CLOSED;}
};
risc_bank_store_v1 bankApi{
  1,sizeof(bankApi),nullptr,
  [](void*,risc_bank_status_v1*){return false;},
  [](void*,const risc_bank_image_v1*,uint64_t*)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,const char*,const void*,uint32_t,const risc_bank_image_v1*,uint64_t*)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t,risc_bank_status_v1*)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t,const void*,uint32_t)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t)->int32_t{return RISC_BANK_UNAVAILABLE;},
  [](void*,uint64_t){return false;},
  [](void*,uint32_t,void*,uint32_t,uint32_t*)->int32_t{return RISC_BANK_UNAVAILABLE;}
};
#endif
static size_t boundPlatformCount=0;
using RC=RiscExperimental::NativeReplicaCore;
using AE=RiscExperimental::NativeAtomicKeyValue;
static RC core1(1,{}, {}, {}),core6(6,{}, {}, {});
static AE endpoint1(core1,{nullptr,[](void*){return true;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return -1;}});
static AE endpoint6(core6,{nullptr,[](void*){return true;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return false;},[](void*){++storageCalls;return -1;}});
bool bind(RiscBoot::Runtime& runtime) {
 struct Writer {const char* app;uint32_t ns;};
 static const Writer writers[]={RISC_NATIVE_SETTINGS_WRITERS};
 for(const auto& writer:writers)if(!runtime.registerAtomicKeyValue(writer.app,writer.ns,writer.ns==6?endpoint6:endpoint1))return false;
 for(uint32_t ns:{1u,6u})if(!runtime.registerAtomicKeyValueReadThrough(ns,ns==6?endpoint6:endpoint1)||!runtime.registerKeyValueAvailability(ns,{nullptr,[](void*,uint32_t,risc_key_value_availability_state_v1*)->int32_t{++storageCalls;return RISC_KEY_VALUE_IO;}}))return false;

  if (!cpu->bind(runtime)) return false;
 static const risc_provision_request_api_v1 request{1,sizeof(request),nullptr,
  [](void*,risc_provision_request_status_v1*)->int32_t{++storageCalls;return RISC_PROVISION_REQUEST_UNAVAILABLE;},
  [](void*,const risc_provision_request_v1*,uint8_t*)->int32_t{++storageCalls;return RISC_PROVISION_REQUEST_UNAVAILABLE;}};
 if(!runtime.registerPlatform(RISC_PROVISION_REQUEST_CAPABILITY,1,RiscBoot::Runtime::Scope::Global,0,&request))return false;
 static risc_storage_volume_api_v1_download download{};
 auto& v=*reinterpret_cast<risc_storage_volume_api_v1*>(&download);
 v.api_version=1;v.struct_size=sizeof(download);
 auto& state=download.base.base.fs.state;
 state.state_tag=RISC_STORAGE_STATE_TAG;state.state_version=1;
 state.observe=[](void*)->int32_t{++storageCalls;return RISC_STORAGE_STATE_UNAVAILABLE;};
 auto& fs=download.base.base.fs;
 fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
 fs.dir_tell=[](void*,risc_storage_dir_t,uint64_t*){++storageCalls;return false;};
 fs.dir_seek=[](void*,risc_storage_dir_t,uint64_t){++storageCalls;return false;};
 fs.metadata=[](void*,const char*,risc_storage_metadata_v1*)->int32_t{++storageCalls;return RISC_STORAGE_FS_IO;};
 fs.file_truncate=[](void*,risc_storage_file_t,uint64_t){++storageCalls;return false;};
 fs.replace_file=[](void*,const char*,const char*){++storageCalls;return false;};
 fs.recover_replace=[](void*,const char*){++storageCalls;return false;};
 auto& async=download.base.base;
 async.async_tag=RISC_STORAGE_ASYNC_TAG;async.async_version=RISC_STORAGE_ASYNC_VERSION;
 async.begin=[](void*,uint64_t*)->int32_t{++storageCalls;return RISC_STORAGE_ASYNC_IO;};
 async.submit=[](void*,uint64_t,const risc_storage_async_request_v1*,uint64_t*)->int32_t{++storageCalls;return RISC_STORAGE_ASYNC_IO;};
 async.collect=[](void*,uint64_t,uint64_t,risc_storage_async_result_v1*)->int32_t{++storageCalls;return RISC_STORAGE_ASYNC_IO;};
 async.end=[](void*,uint64_t)->int32_t{++storageCalls;return RISC_STORAGE_ASYNC_IO;};
 download.base.package_tag=RISC_STORAGE_PACKAGE_TAG;download.base.package_version=1;
 download.base.submit_package=[](void*,uint64_t,const risc_storage_package_request_v1*,uint64_t*)->int32_t{++storageCalls;return RISC_STORAGE_ASYNC_IO;};
 download.download_tag=RISC_STORAGE_DOWNLOAD_TAG;download.download_version=2;
 static const risc_internal_package_volume_v1 package{1,sizeof(package),nullptr,&v,[](void*){++storageCalls;return false;}};
 if(!runtime.registerPlatform(RISC_INTERNAL_PACKAGE_VOLUME_CAPABILITY,1,RiscBoot::Runtime::Scope::Global,0,&package))return false;

#ifdef STORE_ADMISSION_DIAGNOSTIC_SOURCE
  static const risc_diagnostic_source_api_v1 source{1,sizeof(source),nullptr,
    [](void*,uint32_t,char*,uint32_t,uint32_t*,uint64_t*,uint32_t*)->int32_t{
      ++hardwareCalls;return RISC_DIAGNOSTIC_SOURCE_INVALID;
    }};
  if(!runtime.registerPlatform(RISC_DIAGNOSTIC_SOURCE_CAPABILITY,1,
       RiscBoot::Runtime::Scope::Global,0,&source))return false;
#endif

#ifdef STORE_ADMISSION_UPDATE_PLATFORMS
  if (!runtime.registerPlatform(RISC_HTTP_CLIENT_CAPABILITY,RISC_HTTP_CLIENT_API_V1,
                                RiscBoot::Runtime::Scope::Global,0,&httpApi)) return false;
  if (!runtime.registerPlatform(RISC_BANK_STORE_CAPABILITY,RISC_BANK_STORE_API_V1,
                                RiscBoot::Runtime::Scope::Global,0,&bankApi)) return false;
#endif
  boundPlatformCount=runtime.platformCount_;
  return true;
}
RiscCpu::Hardware hardware() {
  RiscCpu::Hardware h{};
 static RiscStorage::Worker mailbox;
 static RiscCpu::StorageWorkBackend worker{&mailbox,[](){return true;},[](){return false;},[](uint32_t){++hardwareCalls;}};h.storageWork=&worker;
 h.sdmmcOpen=[](uint8_t,uint8_t,uint8_t,uint32_t,risc_sdmmc_card_info_v1*){++hardwareCalls;return false;};
 h.sdmmcRead=[](uint64_t,uint32_t,void*){++hardwareCalls;return false;};h.sdmmcWrite=[](uint64_t,uint32_t,const void*){++hardwareCalls;return false;};h.sdmmcSync=[](){++hardwareCalls;return false;};h.sdmmcClose=[](){++hardwareCalls;return false;};

#ifdef STORE_ADMISSION_CROWN_PROFILE
  // Present only for the source/ELF-witnessed sharing composition. Admission
  // may inspect these interfaces but must never perform native I/O.
  static RiscBoot::EntropyBackend entropy{nullptr,
    [](void*,uint64_t,void*,uint32_t)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t){return true;},[](void*){return true;}};
  static RiscBoot::TcpListenerBackend tcp{nullptr,
    [](void*,uint64_t,const risc_tcp_listen_v1*,uint64_t*)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t,uint64_t,uint64_t*)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t,uint64_t,void*,uint32_t,uint32_t*)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t,uint64_t,const void*,uint32_t,uint32_t*)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t,uint64_t)->int32_t{++hardwareCalls;return -1;},
    [](void*,uint64_t){return true;},[](void*){return true;}};
  h.entropy=&entropy;h.tcpListener=&tcp;
#endif
#ifdef STORE_ADMISSION_USB_PHY
  h.usbPhyIdle=[](){++hardwareCalls;return false;};
  h.usbPhySuspend=[](){++hardwareCalls;return false;};
  h.usbPhyResume=[](){++hardwareCalls;return false;};
#endif
#ifdef STORE_ADMISSION_HCI
  h.hciOpen=[](){++hardwareCalls;return false;};
  h.hciSend=[](uint8_t,const uint8_t*,size_t,uint32_t){++hardwareCalls;return false;};
  h.hciReceive=[](uint8_t*,uint8_t*,size_t,size_t*,uint32_t){++hardwareCalls;return false;};
  h.hciClose=[](){++hardwareCalls;return false;};
  h.hciIdle=[](){++hardwareCalls;return false;};h.hciSafe=[](){++hardwareCalls;return false;};
#endif
#ifdef STORE_ADMISSION_RUNTIME_FEATURES
  h.realtimeRead=[](risc_realtime_snapshot_v1*)->int32_t{++hardwareCalls;return RISC_REALTIME_IO;};
  h.realtimeSeed=[](int64_t,uint32_t)->int32_t{++hardwareCalls;return RISC_REALTIME_IO;};
#endif
  h.owner = owner;
#ifdef STORE_ADMISSION_RADIO_IQ
  h.radioIqReady=[](){++hardwareCalls;return false;};
#ifdef STORE_ADMISSION_IQ_LIFECYCLE
  h.radioIqPrepare=[](){++hardwareCalls;return false;};
  h.radioIqCleanup=[](){++hardwareCalls;return false;};
#endif
#endif
  h.now = []() -> uint64_t { ++hardwareCalls; return 0; };
  h.sleep = [](uint32_t) { ++hardwareCalls; };
  h.gpioOpen = [](uint8_t, bool, bool, bool) { ++hardwareCalls; return false; };
  h.gpioWrite = [](uint8_t, bool) { ++hardwareCalls; return false; };
  h.gpioRead = [](uint8_t, bool*) { ++hardwareCalls; return false; };
  h.gpioPwm = [](uint8_t, uint32_t, uint16_t, uint16_t) { ++hardwareCalls; return false; };
  h.gpioClose = [](uint8_t) { ++hardwareCalls; return false; };
  h.i2cOpen = [](uint8_t, uint8_t, uint8_t, uint32_t) { ++hardwareCalls; return false; };
  h.i2cTransfer = [](uint8_t, uint8_t, const uint8_t*, size_t, uint8_t*, size_t, uint32_t) { ++hardwareCalls; return false; };
  h.i2cClose = [](uint8_t) { ++hardwareCalls; return false; };
  h.spiOpen = [](uint8_t, int16_t, int16_t, int16_t) { ++hardwareCalls; return false; };
  h.spiBegin = [](uint8_t, uint8_t, uint32_t, uint8_t, uint32_t) { ++hardwareCalls; return false; };
  h.spiTransfer = [](uint8_t, const uint8_t*, uint8_t*, size_t, uint32_t) { ++hardwareCalls; return false; };
  h.spiEnd = [](uint8_t, uint8_t, uint32_t) { ++hardwareCalls; return false; };
  h.spiClose = [](uint8_t) { ++hardwareCalls; return false; };
  h.i2sOpen = [](uint8_t, uint8_t, uint8_t, uint8_t, uint32_t) { ++hardwareCalls; return false; };
  h.i2sWrite = [](uint8_t, const int16_t*, size_t, size_t*, uint32_t) { ++hardwareCalls; return false; };
#ifdef STORE_ADMISSION_I2S_RX
  h.i2sOpenRx = [](uint8_t, uint8_t, uint8_t, uint32_t) { ++hardwareCalls; return false; };
  h.i2sRead = [](uint8_t, int16_t*, size_t, size_t*, uint32_t) { ++hardwareCalls; return false; };
#endif
  h.i2sClose = [](uint8_t) { ++hardwareCalls; return false; };
#ifdef STORE_ADMISSION_RADIO
  h.radioJoin = [](const char*, const char*) { ++hardwareCalls; return false; };
  h.radioState = [](uint8_t*, int8_t*) { ++hardwareCalls; return false; };
  h.radioLeave = []() { ++hardwareCalls; return false; };
  h.radioAddresses = [](uint8_t*, uint8_t*) { ++hardwareCalls; return false; };
  h.radioScanStart = []() { ++hardwareCalls; return false; };
  h.radioScanPoll = [](garden_radio_scan_result_v1*) { ++hardwareCalls; return false; };
  h.radioScanCancel = []() { ++hardwareCalls; return false; };
  h.radioIdle = []() { ++hardwareCalls; return false; };
#endif
  return h;
}
}

extern "C" int64_t esp_timer_get_time(){++hardwareCalls;return 0;}
extern "C" bool xPortInIsrContext(){++hardwareCalls;return false;}
static void printPlatformReceipt(const RiscBoot::Runtime& runtime) {
  printf("{\"platform_count\":%zu,\"bound_platform_count\":%zu,\"runtime_owned_platforms\":[",runtime.platformCount_,boundPlatformCount);
  for(size_t i=boundPlatformCount;i<runtime.platformCount_;++i){
    const auto& p=runtime.platforms_[i];
    printf("%s{\"capability\":\"%s\",\"api\":%u,\"scope\":%u,\"instance\":%llu}",i==boundPlatformCount?"":",",p.capability,p.api,unsigned(p.scope),(unsigned long long)p.id);
  }
  printf("],");
}
static bool bindRetainedSd(RiscBoot::Runtime& runtime) {
  namespace S=RiscCpu::NativeSerialStorage;
  if(!S::prepare(owner,owner) || !S::bindSd(runtime,owner,owner))return false;
  const auto* admission=S::admission();
  return admission && admission->count==1 && admission->ready() &&
    admission->namespaces[0].id==12 && admission->namespaces[0].writable &&
    admission->namespaces[0].files==&S::sdFiles() &&
    !S::sdFiles().bound() && !S::sdFiles().retained();
}
int main(int argc, char** argv) {
#ifdef STORE_ADMISSION_COHORT
  if (argc != 3) return 2;
#else
  if (argc != 2) return 2;
#endif
  RiscBoot::KeyValueBackend kv{
    nullptr,
    [](void*, uint32_t, const char*, void*, uint32_t, uint32_t*) -> int32_t {
      ++storageCalls; return RISC_KEY_VALUE_IO;
    },
    [](void*, uint32_t, const char*, const void*, uint32_t) -> int32_t {
      ++storageCalls; return RISC_KEY_VALUE_IO;
    }
#ifdef STORE_ADMISSION_KV_V2
    ,RISC_KEY_VALUE_V2_BLOB_MAX
#endif
  };
  // Released X4 installation.settings_sd requires the existing journal API.
  // Presence only: every I/O callback is counted and must remain uncalled.
  kv.journalRead=[](void*,uint32_t,void*,uint32_t,uint32_t*)->int32_t{++storageCalls;return RISC_KEY_VALUE_IO;};
  kv.journalWrite=[](void*,uint32_t,const void*,uint32_t)->int32_t{++storageCalls;return RISC_KEY_VALUE_IO;};
  kv.legacyNamespace=[](void*,uint32_t,bool*)->int32_t{++storageCalls;return RISC_KEY_VALUE_IO;};
  RiscCpu::Port port(hardware());
  cpu = &port;
#ifdef STORE_ADMISSION_APP_DATA
  const auto appData=admissionAppData(&storageCalls);
#endif
  RiscBoot::Port runtimePort{owner, [](risc_runtime_health_v1*) { return true; },
#ifdef STORE_ADMISSION_COHORT
                            [](uint32_t) { ++cooperativeYields; },
#else
                            [](uint32_t) { ++hardwareCalls; },
#endif
                            [](const char*) { return true; }, bind, &kv
#ifdef STORE_ADMISSION_APP_DATA
                            ,nullptr,nullptr,nullptr,&appData
#endif
                            };
#ifdef STORE_ADMISSION_RUNTIME_FEATURES
  RiscRetainedWake::Image rtcImage{};
  RiscRetainedWake::Store wake(rtcImage);runtimePort.retainedWake=&wake;
#endif
#ifdef STORE_ADMISSION_COLD_BOOT
  // prepare/admission must never ask for a boot cause or activate providers.
  runtimePort.coldBoot=[](){++hardwareCalls;return false;};
#endif
#ifdef STORE_ADMISSION_CROWN_PROFILE
  runtimePort.memoryHeap=[](risc_memory_heap_snapshot_v1*)->int32_t{++hardwareCalls;return -1;};
  runtimePort.randomBytes=[](void*,uint32_t)->int32_t{++hardwareCalls;return -1;};
#endif
#ifdef STORE_ADMISSION_FILE_SOURCE
  // Admission models authority availability only. Every source operation is
  // forbidden here and counted, just like native hardware/storage callbacks.
  const RiscBoot::FileSourceBackend fileSource{&storageCalls,
    [](void* c,uint64_t)->int32_t{++*static_cast<unsigned*>(c);return RISC_FILE_SOURCE_UNAVAILABLE;},
    [](void* c,const void*,uint32_t,uint32_t*)->int32_t{++*static_cast<unsigned*>(c);return RISC_FILE_SOURCE_UNAVAILABLE;},
    [](void* c)->int32_t{++*static_cast<unsigned*>(c);return RISC_FILE_SOURCE_UNAVAILABLE;},
    [](void* c,uint64_t,void*,uint32_t,uint32_t*)->int32_t{++*static_cast<unsigned*>(c);return RISC_FILE_SOURCE_UNAVAILABLE;},
    [](void* c)->int32_t{++*static_cast<unsigned*>(c);return RISC_FILE_SOURCE_UNAVAILABLE;},
    [](void*){return true;}};
  runtimePort.fileSource=&fileSource;
#endif
#ifdef STORE_ADMISSION_COHORT
  RiscTest::FileSources metadataFiles({std::string(argv[1]),std::string(argv[2])},owner,owner);
#else
  RiscTest::FileSources metadataFiles({std::string(argv[1])},owner,owner);
#endif
  runtimePort.files=metadataFiles.sources();
  RiscBoot::Runtime runtime(runtimePort);
  const bool prepared = runtime.prepare(argv[1]);
  if (!prepared) {
    fprintf(stderr, "ADMISSION platforms=%zu drivers=%zu\n", runtime.platformCount_, runtime.driverCount_);
    for (size_t i=0;i<runtime.platformCount_;++i) {
      const auto& p=runtime.platforms_[i];
      fprintf(stderr, "PLAT %zu cap=%s api=%u scope=%u id=%llu\n", i,p.capability,p.api,
              unsigned(p.scope),(unsigned long long)p.id);
    }
    for (size_t i=0;i<runtime.driverCount_;++i) {
      const auto& d=runtime.drivers_[i];
      fprintf(stderr, "DRV %zu id=%s inst=%llu reqs=%zu\n", i,d.id,(unsigned long long)d.instance,d.count);
      for (size_t r=0;r<d.count;++r) {
        const auto& q=d.requirements[r];
        fprintf(stderr, "  REQ cap=%s api=%u trusted=%p providerInst=%llu\n", q.capability,q.api,q.trustedApi,
                (unsigned long long)q.providerInstance);
      }
    }
  }
#ifdef STORE_ADMISSION_COHORT
  RiscBoot::Runtime candidate({});unsigned admitted=0;
  const bool validated=prepared && runtime.validateCohort(candidate,argv[2],CohortElf::file,&admitted);
  const bool retainedSdGraphBound=validated && bindRetainedSd(candidate);
  if(validated && !retainedSdGraphBound)return 4;
  printPlatformReceipt(validated?candidate:runtime);
  printf("\"retained_sd_serial_graph_bound\":%s,\"prepared\":%s,\"cohort_validated\":%s,\"error\":\"%s\",\"hardware_calls\":%u,\"storage_calls\":%u,\"elf_count\":%u,\"cooperative_yields\":%u}\n",
         retainedSdGraphBound?"true":"false",prepared?"true":"false",validated?"true":"false",prepared?candidate.error():runtime.error(),hardwareCalls,storageCalls,admitted,cooperativeYields);
#else
  const bool retainedSdGraphBound=prepared && bindRetainedSd(runtime);
  if(prepared && !retainedSdGraphBound)return 4;
  printPlatformReceipt(runtime);
  // Runtime errors are constant diagnostics, with no input text or credentials.
  printf("\"retained_sd_serial_graph_bound\":%s,\"prepared\":%s,\"error\":\"%s\",\"hardware_calls\":%u,\"storage_calls\":%u,\"i2s_tables\":%zu}\n",
         retainedSdGraphBound?"true":"false",prepared ? "true" : "false", runtime.error(), hardwareCalls, storageCalls, port.i2sCount_);
#endif
  return hardwareCalls || storageCalls ? 3 : 0;
}
