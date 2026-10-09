/* Production Runtime journal over its USB host shim. */
#include "ports/esp32s3/SleepDiagnostics.h"
#include <Arduino.h>
#include <cassert>
#include <cstdio>
extern "C" void rf_serial_start(void){Serial.connected=true;RiscDiagnostics::start();}
extern "C" void rf_serial_line(const char *text){RiscDiagnostics::line(text);}
extern "C" void rf_serial_finish(int returned,int has_app_logs,int appdata_retained){
 Serial.connected=true;Serial.output.clear();Serial.input="diag\n";
 for(unsigned i=0;i<200;i++)RiscDiagnostics::poll();
 assert(Serial.output.find("RTE_DIAG end\n")!=std::string::npos);
 if(has_app_logs){
  if(appdata_retained){assert(!returned);assert(Serial.output.find("SDR cleanup-unconfirmed")==std::string::npos);}
  else assert(Serial.output.find(returned?"SDR app exit":"SDR cleanup-unconfirmed")!=std::string::npos);
 }
 for(const char *secret:{"SECRET","private-label","password","pairs=["})assert(Serial.output.find(secret)==std::string::npos);
 std::puts(returned?"RF Runtime serial journal survives application return; no sample/name secrets":"RF Runtime serial journal available during retained cleanup; no sample/name secrets");
}
