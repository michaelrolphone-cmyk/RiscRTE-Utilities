#ifndef RISC_OBD_ELM_H
#define RISC_OBD_ELM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OBD_METRICS 23
#define OBD_DTC_MAX 32
#define OBD_LOG_LINES 30
#define OBD_LINE_BYTES 160
/* All decoded engineering values use milli-units, never undocumented guesses. */
enum obd_metric { OBD_RPM,OBD_SPEED,OBD_COOLANT,OBD_LOAD,OBD_THROTTLE,OBD_VOLTS,OBD_FUEL,
 OBD_INTAKE,OBD_MAP,OBD_MAF,OBD_SHORT_TRIM,OBD_LONG_TRIM,OBD_OIL,OBD_RUNTIME,OBD_MIL,OBD_RATE,OBD_ADVANCE,OBD_O2,OBD_BARO,OBD_STANDARD,OBD_FUEL_TYPE,OBD_DISTANCE_CLEAR,OBD_DISTANCE_MIL };
enum obd_state {OBD_OFF,OBD_INITIALIZING,OBD_SEARCHING,OBD_DISCOVERING,OBD_LIVE,OBD_FAULT};
typedef struct {uint8_t pid,bytes,scale;uint32_t interval_ms;const char *name,*unit;} obd_spec;
typedef struct {int32_t milli;uint32_t at_ms,due_ms;bool supported,valid;} obd_sample;
enum { OBD_DIAG_IDLE,OBD_DIAG_LOADING,OBD_DIAG_VALID,OBD_DIAG_UNAVAILABLE,OBD_DIAG_ERROR };
enum {OBD_JOB_NONE=-1,OBD_JOB_VIN,OBD_JOB_STORED,OBD_JOB_PENDING,OBD_JOB_PERMANENT,OBD_JOB_FREEZE_CODE,OBD_JOB_FREEZE_DATA,OBD_JOB_CLEAR,OBD_JOB_VOLTAGE};
typedef struct {
 uint16_t codes[OBD_DTC_MAX];unsigned count,status;bool truncated;
} obd_codes;
typedef struct {
 uint32_t jobs,poll_mask,requested_protocol,clear_deadline,clear_ecu;
 int job;unsigned ff_cursor,assembly_size,assembly_needed,assembly_seq,legacy_vin_seq;
 uint8_t assembly[128],candidate_raw[4],readiness[4];uint32_t readiness_at;
 bool readiness_valid,assembly_bad,clear_armed;
 obd_codes codes[3],pending_codes;
 char vin[18],pending_vin[21];unsigned vin_status;
 uint16_t freeze_code,freeze_requested;unsigned freeze_status;obd_sample freeze[OBD_METRICS];
 int32_t adapter_mv;unsigned voltage_status,clear_status;
 char logs[OBD_LOG_LINES][96];unsigned log_head,log_count;uint32_t log_revision;
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
void obd_request_faults(obd_client*);
void obd_request_info(obd_client*);
void obd_request_freeze(obd_client*,uint16_t code);
bool obd_confirm_clear(obd_client*,uint32_t now);
void obd_code_text(uint16_t code,char out[6]);
const char *obd_protocol_name(unsigned protocol);
int32_t obd_convert(unsigned metric,int32_t milli,bool imperial);
void obd_value(unsigned metric,int32_t milli,bool imperial,char*,size_t);
const char *obd_unit(unsigned metric,bool imperial);
bool obd_fresh(const obd_client *c,unsigned metric,uint32_t now);
void obd_format(const obd_client *c,unsigned metric,uint32_t now,bool imperial,char *value,size_t nv,char *unit,size_t nu);
#endif
