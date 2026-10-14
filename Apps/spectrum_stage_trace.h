#pragma once
/* Opt-in T069 diagnostics. Fixed RAM only: no allocation, storage, provider
 * polling or sleeps. Runtime copies the bounded checkpoint; no app pointer
 * survives. Sparse live markers also use the native buffered diagnostic path.
 * The caller must establish live custody before EVERY call. */
#ifdef PORTABLE_SPECTRUM_STAGE_TRACE
#include "RiscDiagnosticCheckpointV1.h"
#include <stdio.h>
#include <string.h>
static struct {
 risc_diagnostic_checkpoint_client_v1 client;
 const risc_runtime_api_v1 *runtime;
 const char *name;
 unsigned sequence,head,count,captures,frames;
 char rows[6][96],copy[576];
} spectrum_stage;
static void spectrum_stage_init(const risc_runtime_api_v1 *rt,const char *name) {
 memset(&spectrum_stage,0,sizeof(spectrum_stage));spectrum_stage.runtime=rt;spectrum_stage.name=name;
 risc_diagnostic_checkpoint_client_v1 c={.struct_size=sizeof(c)};
 if(risc_runtime_diagnostic_checkpoint_client(rt,&c)&&c.api_version==1&&c.struct_size>=sizeof(c)&&c.invocation&&c.write)spectrum_stage.client=c;
 spectrum_stage.captures=spectrum_stage.frames=2;
}
static void spectrum_stage_arm(void){spectrum_stage.captures=spectrum_stage.frames=2;}
static bool spectrum_stage_take(bool frame){unsigned *n=frame?&spectrum_stage.frames:&spectrum_stage.captures;if(!*n)return false;--*n;return true;}
static void spectrum_stage_mark(const char *stage,unsigned a,unsigned b,unsigned c) {
 unsigned at=spectrum_stage.head;char *line=spectrum_stage.rows[at];
 snprintf(line,96,"T069 %s n=%u %s a=%u b=%u c=%u",spectrum_stage.name,++spectrum_stage.sequence,stage,a,b,c);
 spectrum_stage.head=(at+1)%6;if(spectrum_stage.count<6)++spectrum_stage.count;
 unsigned used=0;
 for(unsigned i=0;i<spectrum_stage.count;i++){const char *s=spectrum_stage.rows[(spectrum_stage.head+6-spectrum_stage.count+i)%6];unsigned n=(unsigned)strlen(s);memcpy(spectrum_stage.copy+used,s,n);used+=n;spectrum_stage.copy[used++]='\n';}
 if(spectrum_stage.client.write){int rc=spectrum_stage.client.write(spectrum_stage.client.invocation,spectrum_stage.copy,used);if(rc<0){spectrum_stage.client=(risc_diagnostic_checkpoint_client_v1){0};spectrum_stage.runtime=NULL;return;}}
 if(spectrum_stage.runtime&&spectrum_stage.runtime->diagnostic)spectrum_stage.runtime->diagnostic(line);
}
#else
#define spectrum_stage_init(rt,name) ((void)0)
#define spectrum_stage_arm() ((void)0)
#define spectrum_stage_take(frame) false
#define spectrum_stage_mark(stage,a,b,c) ((void)0)
#endif
