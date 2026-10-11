#ifndef RISC_OBD_ELM_H
#define RISC_OBD_ELM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OBD_METRICS 16
#define OBD_LINE_BYTES 160
/* All decoded engineering values use milli-units, never undocumented guesses. */
enum obd_metric { OBD_RPM,OBD_SPEED,OBD_COOLANT,OBD_LOAD,OBD_THROTTLE,OBD_VOLTS,OBD_FUEL,
 OBD_INTAKE,OBD_MAP,OBD_MAF,OBD_SHORT_TRIM,OBD_LONG_TRIM,OBD_OIL,OBD_RUNTIME,OBD_MIL,OBD_RATE };
enum obd_state {OBD_OFF,OBD_INITIALIZING,OBD_SEARCHING,OBD_DISCOVERING,OBD_LIVE,OBD_FAULT};
typedef struct {uint8_t pid,bytes,scale;uint32_t interval_ms;const char *name,*unit;} obd_spec;
typedef struct {int32_t milli;uint32_t at_ms,due_ms;bool supported,valid;} obd_sample;
typedef struct {
 uint32_t state,stage,ecu,ecu_kind,protocol,mask[3],revision,requests,replies,bad_lines,failed_requests;
 uint32_t sent_ms,last_sent_ms,deadline_ms,rtt_ms,last_good_ms,received;
 uint32_t candidate_ecu,candidate_kind,candidate_value;
 unsigned cursor,line_size,fail_streak;
 int metric;
 bool waiting,prepared,found,at_ok,banner,negative,explicit_error,have_ecu,paused;
 char command[16],line[OBD_LINE_BYTES],message[80],identity[48];
 obd_sample samples[OBD_METRICS];
} obd_client;
extern const obd_spec obd_specs[OBD_METRICS];
void obd_init(obd_client *c,uint32_t now);
void obd_stop(obd_client *c,const char *reason);
/* next returns the same bounded command until sent() follows an accepted write. */
size_t obd_next(obd_client *c,uint32_t now,const char **command);
void obd_sent(obd_client *c,uint32_t now);
void obd_receive(obd_client *c,const uint8_t *bytes,size_t count,uint32_t now);
void obd_tick(obd_client *c,uint32_t now);
bool obd_fresh(const obd_client *c,unsigned metric,uint32_t now);
void obd_format(const obd_client *c,unsigned metric,uint32_t now,bool imperial,char *value,size_t nv,char *unit,size_t nu);
#endif
