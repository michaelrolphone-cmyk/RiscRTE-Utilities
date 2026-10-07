/* Link the production Runtime logger. Arduino's USB transport is the Runtime's
 * existing host shim; no physical serial connection is represented here. */
#include "ports/esp32s3/SleepDiagnostics.h"
#include <Arduino.h>
#include <cassert>
#include <cstdlib>
#include <cstdio>
extern "C" void hid_serial_start(void) {
    Serial.connected=std::getenv("HID_RENDER_SERIAL_ABSENT")==nullptr;
    RiscDiagnostics::start();
}
extern "C" void hid_serial_line(const char *text) { RiscDiagnostics::line(text); }
extern "C" void hid_serial_finish(void) {
    const bool pairing=std::getenv("HID_RENDER_PAIR")!=nullptr;
    if(std::getenv("HID_RENDER_SERIAL_ABSENT"))assert(Serial.output.empty() && Serial.writes==0);
    else if(pairing){
        assert(Serial.output.find("HID pairing contact target=")!=std::string::npos);
        assert(Serial.output.find("HID pairing response action=")!=std::string::npos);
        assert(Serial.output.find("result=ok")!=std::string::npos);
    }
    assert(Serial.output.find("000042")==std::string::npos);
    /* The app has returned and released its grants. The runtime's owner and
     * journal remain live, and reconnect can replay recent app diagnostics. */
    Serial.connected=true;Serial.output.clear();Serial.input="diag\n";
    for(unsigned i=0;i<200;++i)RiscDiagnostics::poll();
    assert(Serial.output.find("RTE_DIAG end\n")!=std::string::npos);
    assert(Serial.output.find("000042")==std::string::npos);
    if(std::getenv("HID_RENDER_TRANSPORT_RECONNECT")){
        assert(Serial.output.find("HID stop reason=Bluetooth error - reconnect")!=std::string::npos);
        assert(Serial.output.find("HID suspend status=1 state=6 flags=16 error=12")!=std::string::npos);
        assert(Serial.output.find("HID closed status=1 state=0 flags=16 error=12")!=std::string::npos);
    }
    if(pairing){
        assert(Serial.output.find("HID pairing contact target=")!=std::string::npos);
        assert(Serial.output.find("HID pairing response action=")!=std::string::npos);
        assert(Serial.output.find("result=ok")!=std::string::npos);
    }
    std::puts("HID Runtime logger: USB-shim output and post-return diagnostic replay passed");
}
