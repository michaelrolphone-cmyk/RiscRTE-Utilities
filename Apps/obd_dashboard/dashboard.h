#ifndef RISC_OBD_DASHBOARD_H
#define RISC_OBD_DASHBOARD_H
#include "elm.h"
#define DB_HISTORY 601u
#define DB_NO_VALUE INT32_MIN
#define DB_ALERTS 32u
/* All limits and histories use canonical milli-units, unaffected by units UI. */
typedef struct {int32_t low,high;uint32_t enabled,has_low,has_high;} db_limit;
typedef struct {
 uint32_t version,imperial,update_ms,alerts,protocol,dash_count,dash[8],graph_count,graph[3],trip_sequence;
 db_limit limits[OBD_METRICS];
} db_config;
typedef struct {unsigned metric;int32_t value,limit;uint32_t at_ms;bool high;} db_alert;
typedef struct {const char*short_name;int32_t minimum,maximum,step;bool selectable;} db_spec;
extern const db_spec db_specs[OBD_METRICS];
typedef struct {
 db_config cfg;
 int32_t history[OBD_METRICS][DB_HISTORY],frozen[3][DB_HISTORY];
 uint32_t stamps[DB_HISTORY],frozen_stamps[DB_HISTORY],seen[OBD_METRICS];
 uint32_t outside_since[OBD_METRICS],mute_until[OBD_METRICS],started_ms,frozen_at;
 bool seen_once[OBD_METRICS],outside[OBD_METRICS],latched[OBD_METRICS],muted[OBD_METRICS],paused;
 db_alert alert,alert_log[DB_ALERTS];unsigned alert_count;bool alert_visible;
} db_model;
void db_init(db_model*,uint32_t);
bool db_config_valid(const db_config*);
void db_tick(db_model*,const obd_client*,uint32_t);
bool db_last(const db_model*,unsigned,uint32_t,bool,int32_t*);
void db_freeze(db_model*,uint32_t);
void db_ack(db_model*,uint32_t,bool mute);
bool db_stats(const db_model*,unsigned metric,uint32_t now,uint32_t span,bool frozen,int32_t*mn,int32_t*mx,int32_t*avg);
void db_series(const db_model*,unsigned metric,uint32_t now,uint32_t span,bool frozen,int32_t lo,int32_t hi,uint16_t*out,unsigned count);
uint32_t db_poll_mask(const db_model*,int detail);
bool db_warning(const db_model*,const obd_client*,unsigned,uint32_t);
uint32_t db_crc(const void*,size_t);
#endif
