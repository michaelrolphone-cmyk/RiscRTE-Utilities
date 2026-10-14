#include "dashboard.h"
#include <string.h>
const db_spec db_specs[OBD_METRICS]={
 {"RPM",0,8000000,250000,false},{"SPEED",0,240000,5000,false},{"COOLANT",-40000,130000,5000,true},
 {"ENGINE LOAD",0,100000,5000,true},{"THROTTLE",0,100000,5000,true},{"BATTERY",8000,16000,200,true},
 {"FUEL LEVEL",0,100000,5000,true},{"INTAKE AIR",-40000,100000,5000,true},{"MAP",0,255000,5000,true},
 {"AIR FLOW",0,250000,5000,true},{"FUEL TRIM ST",-25000,25000,1000,true},{"FUEL TRIM LT",-25000,25000,1000,true},
 {"OIL TEMP",-40000,150000,5000,true},{"RUN TIME",0,65535000,60000,false},{"CHECK ENGINE",0,255000,1000,false},
 {"FUEL RATE",0,60000,1000,true},{"TIMING",-10000,50000,1000,true},{"O2 B1 S2",0,1275,50,true},
 {"BAROMETER",60000,110000,1000,true},{"OBD STANDARD",0,255000,1000,false},{"FUEL TYPE",0,255000,1000,false},
 {"SINCE CLEAR",0,65535000,100000,false},{"MIL DISTANCE",0,65535000,100000,false}
};
void db_init(db_model*m,uint32_t now){
 memset(m,0,sizeof(*m));m->started_ms=now;m->cfg.version=1;m->cfg.imperial=1;m->cfg.update_ms=1000;m->cfg.alerts=1;m->cfg.dash_count=8;
 const unsigned defaults[8]={OBD_COOLANT,OBD_INTAKE,OBD_LOAD,OBD_THROTTLE,OBD_MAP,OBD_MAF,OBD_FUEL,OBD_VOLTS};
 memcpy(m->cfg.dash,defaults,sizeof(defaults));m->cfg.graph_count=3;m->cfg.graph[0]=OBD_RPM;m->cfg.graph[1]=OBD_COOLANT;m->cfg.graph[2]=OBD_LOAD;
 m->cfg.limits[OBD_COOLANT]=(db_limit){0,105000,1,0,1};m->cfg.limits[OBD_RPM]=(db_limit){0,6500000,1,0,1};
 m->cfg.limits[OBD_VOLTS]=(db_limit){11800,15200,1,1,1};m->cfg.limits[OBD_FUEL]=(db_limit){10000,0,1,1,0};
 m->cfg.limits[OBD_INTAKE]=(db_limit){0,70000,0,0,1};m->cfg.limits[OBD_LOAD]=(db_limit){0,95000,0,0,1};
 for(unsigned i=0;i<OBD_METRICS;i++)for(unsigned j=0;j<DB_HISTORY;j++)m->history[i][j]=DB_NO_VALUE;
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<DB_HISTORY;j++)m->motion[i][j]=DB_NO_VALUE;
}
bool db_config_valid(const db_config*c){
 if(!c||c->version!=1||c->imperial>1||c->alerts>1||c->dash_count>8||c->graph_count<1||c->graph_count>3||c->protocol>9||(c->update_ms!=1000&&c->update_ms!=2000&&c->update_ms!=5000))return false;
 for(unsigned i=0;i<c->dash_count;i++){if(c->dash[i]>=OBD_METRICS||!db_specs[c->dash[i]].selectable)return false;for(unsigned j=0;j<i;j++)if(c->dash[i]==c->dash[j])return false;}
 for(unsigned i=0;i<c->graph_count;i++){if(c->graph[i]>=OBD_METRICS||(!db_specs[c->graph[i]].selectable&&c->graph[i]!=OBD_RPM&&c->graph[i]!=OBD_SPEED))return false;for(unsigned j=0;j<i;j++)if(c->graph[i]==c->graph[j])return false;}
 for(unsigned i=0;i<OBD_METRICS;i++){const db_limit*t=&c->limits[i];if(t->enabled>1||t->has_low>1||t->has_high>1)return false;
  if(t->has_low&&(t->low<db_specs[i].minimum||t->low>db_specs[i].maximum))return false;
  if(t->has_high&&(t->high<db_specs[i].minimum||t->high>db_specs[i].maximum))return false;
  if(t->has_low&&t->has_high&&t->low>=t->high)return false;}
 return true;
}
bool db_warning(const db_model*m,const obd_client*c,unsigned i,uint32_t now){
 if(i>=OBD_METRICS||!m->cfg.alerts||!m->cfg.limits[i].enabled||!obd_fresh(c,i,now))return false;
 const db_limit*t=&m->cfg.limits[i];int32_t value=c->samples[i].milli;
 return (t->has_low&&value<t->low)||(t->has_high&&value>t->high);
}
static void motion_tick(db_model*m,const obd_client*c,uint32_t now){
 const obd_sample*s=&c->samples[OBD_SPEED];
 if(c->state!=OBD_LIVE||!obd_fresh(c,OBD_SPEED,now)){m->motion_samples=0;return;}
 if(m->motion_samples&&s->at_ms==m->motion_at)return;
 uint32_t dt=s->at_ms-m->motion_at;
 if(dt>obd_specs[OBD_SPEED].interval_ms*3+1500||!dt)m->motion_samples=0;
 unsigned bin=(s->at_ms/1000)%DB_HISTORY;
 m->motion_stamps[bin]=s->at_ms;
 int32_t speed=(int32_t)((int64_t)s->milli*5/18); /* milli-km/h -> mm/s */
 m->motion[0][bin]=speed;m->motion[1][bin]=m->motion[2][bin]=DB_NO_VALUE;
 if(m->motion_samples){
  int64_t accel=((int64_t)speed-m->motion_speed)*1000/dt;
  if(accel>INT32_MAX||accel<=INT32_MIN){m->motion_samples=0;}
  else {
   m->motion[1][bin]=(int32_t)accel;
   if(m->motion_samples>1){
    /* Acceleration estimates live at interval midpoints: uneven sampling matters. */
    int64_t jerk=(accel-m->motion_accel)*2000/(dt+m->motion_dt);
    if(jerk>INT32_MIN&&jerk<=INT32_MAX)m->motion[2][bin]=(int32_t)jerk;
   }
   m->motion_accel=(int32_t)accel;
  }
 }
 m->motion_speed=speed;m->motion_at=s->at_ms;m->motion_dt=dt;
 if(m->motion_samples<2)m->motion_samples++;
}
void db_motion_series(const db_model*m,unsigned k,uint32_t now,uint32_t span,uint16_t*out,unsigned count,int32_t*lo,int32_t*hi){
 *lo=INT32_MAX;*hi=INT32_MIN;
 for(unsigned i=0;i<count;i++)out[i]=UINT16_MAX;
 if(k>=3||!span||count<2)return;
 for(unsigned j=0;j<DB_HISTORY;j++)if(m->motion[k][j]!=DB_NO_VALUE&&(uint32_t)(now-m->motion_stamps[j])<=span*1000){
  int32_t v=m->motion[k][j];if(v<*lo)*lo=v;if(v>*hi)*hi=v;
 }
 if(*lo==INT32_MAX){*lo=0;*hi=1000;return;}
 int64_t pad=((int64_t)*hi-*lo)/10;if(pad<100)pad=100;
 *lo=(int32_t)(((int64_t)*lo-pad<INT32_MIN)?INT32_MIN:(int64_t)*lo-pad);
 *hi=(int32_t)(((int64_t)*hi+pad>INT32_MAX)?INT32_MAX:(int64_t)*hi+pad);
 for(unsigned i=0;i<count;i++){
  uint32_t age=span*1000*(count-1-i)/(count-1),best=UINT32_MAX;int32_t v=DB_NO_VALUE;
  for(unsigned j=0;j<DB_HISTORY;j++){
   uint32_t sample_age=now-m->motion_stamps[j];
   if(m->motion[k][j]==DB_NO_VALUE||sample_age>span*1000||sample_age<age)continue;
   uint32_t delta=sample_age-age;
   if(delta<best&&delta<=obd_specs[OBD_SPEED].interval_ms*3+1500){best=delta;v=m->motion[k][j];}
  }
  if(v!=DB_NO_VALUE)out[i]=(uint16_t)(((int64_t)v-*lo)*10000/((int64_t)*hi-*lo));
 }
}
void db_tick(db_model*m,const obd_client*c,uint32_t now){
 motion_tick(m,c,now);
 unsigned bin=(now/1000)%DB_HISTORY;uint32_t stamp=now-now%1000;
 if(m->stamps[bin]!=stamp){for(unsigned i=0;i<OBD_METRICS;i++)m->history[i][bin]=DB_NO_VALUE;m->stamps[bin]=stamp;}
 for(unsigned i=0;i<OBD_METRICS;i++){
  if(m->muted[i]&&(int32_t)(now-m->mute_until[i])>=0)m->muted[i]=false;
  if(!obd_fresh(c,i,now)||!m->cfg.alerts||!m->cfg.limits[i].enabled)m->outside[i]=false;
  const obd_sample*s=&c->samples[i];if(!obd_fresh(c,i,now)||(m->seen_once[i]&&m->seen[i]==s->at_ms))continue;
  m->seen_once[i]=true;m->seen[i]=s->at_ms;m->history[i][bin]=s->milli;
  if(!db_warning(m,c,i,now)){m->outside[i]=m->latched[i]=false;continue;}
  /* Except supply voltage, thresholds are evaluated only with a fresh running
   * engine indication; stale/off status cannot manufacture a warning. */
  if(i!=OBD_VOLTS&&(!obd_fresh(c,OBD_RPM,now)||c->samples[OBD_RPM].milli==0)){m->outside[i]=false;continue;}
  if(!m->outside[i]){m->outside[i]=true;m->outside_since[i]=s->at_ms;}
  if((uint32_t)(s->at_ms-m->outside_since[i])<1500||m->latched[i]||m->muted[i]||m->alert_visible)continue;
  const db_limit*t=&m->cfg.limits[i];bool hi=t->has_high&&s->milli>t->high;
  m->alert=(db_alert){i,s->milli,hi?t->high:t->low,now,hi};m->alert_visible=true;m->latched[i]=true;
  if(m->alert_count<DB_ALERTS)m->alert_count++;
  for(unsigned j=m->alert_count-1;j>0;j--)m->alert_log[j]=m->alert_log[j-1];m->alert_log[0]=m->alert;
 }
}
void db_ack(db_model*m,uint32_t now,bool mute){if(!m->alert_visible)return;if(mute){m->muted[m->alert.metric]=true;m->mute_until[m->alert.metric]=now+300000;}m->alert_visible=false;}
void db_freeze(db_model*m,uint32_t now){
 m->paused=!m->paused;if(!m->paused)return;m->frozen_at=now;memcpy(m->frozen_stamps,m->stamps,sizeof(m->stamps));
 for(unsigned i=0;i<m->cfg.graph_count;i++)memcpy(m->frozen[i],m->history[m->cfg.graph[i]],sizeof(m->frozen[i]));
}
static const int32_t*history(const db_model*m,unsigned metric,bool frozen){
 if(metric>=OBD_METRICS)return NULL;if(!frozen)return m->history[metric];for(unsigned i=0;i<m->cfg.graph_count;i++)if(m->cfg.graph[i]==metric)return m->frozen[i];return NULL;
}
bool db_last(const db_model*m,unsigned metric,uint32_t now,bool frozen,int32_t*out){
 const int32_t*h=history(m,metric,frozen);if(!h)return false;const uint32_t*t=frozen?m->frozen_stamps:m->stamps;if(frozen)now=m->frozen_at;
 uint32_t best=UINT32_MAX;bool found=false;for(unsigned j=0;j<DB_HISTORY;j++)if(h[j]!=DB_NO_VALUE&&(uint32_t)(now-t[j])<best){best=now-t[j];*out=h[j];found=true;}
 return found&&best<=obd_specs[metric].interval_ms*3+1500;
}
bool db_stats(const db_model*m,unsigned metric,uint32_t now,uint32_t span,bool frozen,int32_t*mn,int32_t*mx,int32_t*av){
 const int32_t*h=history(m,metric,frozen);if(!h)return false;const uint32_t*t=frozen?m->frozen_stamps:m->stamps;if(frozen)now=m->frozen_at;
 int64_t sum=0;unsigned count=0;*mn=INT32_MAX;*mx=INT32_MIN;
 for(unsigned i=0;i<DB_HISTORY;i++)if(h[i]!=DB_NO_VALUE&&(uint32_t)(now-t[i])<=span*1000){if(h[i]<*mn)*mn=h[i];if(h[i]>*mx)*mx=h[i];sum+=h[i];count++;}
 if(count)*av=(int32_t)(sum/count);return count!=0;
}
void db_series(const db_model*m,unsigned metric,uint32_t now,uint32_t span,bool frozen,int32_t lo,int32_t hi,uint16_t*out,unsigned count){
 if(!out||count<2)return;for(unsigned i=0;i<count;i++)out[i]=UINT16_MAX;
 const int32_t*h=history(m,metric,frozen);if(!h||hi<=lo||!span)return;
 const uint32_t*t=frozen?m->frozen_stamps:m->stamps;if(frozen)now=m->frozen_at;
 /* Carry a reading only over its freshness interval; missing data makes real
  * graph gaps. Fixed x coordinates always represent elapsed monotonic time. */
 for(unsigned i=0;i<count;i++){
  uint32_t age=(span*1000)*(count-1-i)/(count-1),best=UINT32_MAX;int32_t v=DB_NO_VALUE;
  for(unsigned j=0;j<DB_HISTORY;j++){
   uint32_t sample_age=now-t[j];if(h[j]==DB_NO_VALUE||sample_age>span*1000||sample_age<age)continue;
   uint32_t delta=sample_age-age;if(delta<best&&delta<=obd_specs[metric].interval_ms*3+1500){best=delta;v=h[j];}
  }
  if(v!=DB_NO_VALUE){int64_t y=((int64_t)v-lo)*10000/(hi-lo);if(y<0)y=0;if(y>10000)y=10000;out[i]=(uint16_t)y;}
 }
}
uint32_t db_poll_mask(const db_model*m,int detail){
 uint32_t mask=(1u<<OBD_RPM)|(1u<<OBD_SPEED)|(1u<<OBD_MIL)|(1u<<OBD_FUEL_TYPE);
 for(unsigned i=0;i<m->cfg.dash_count;i++)mask|=1u<<m->cfg.dash[i];for(unsigned i=0;i<m->cfg.graph_count;i++)mask|=1u<<m->cfg.graph[i];
 for(unsigned i=0;i<OBD_METRICS;i++)if(m->cfg.alerts&&m->cfg.limits[i].enabled)mask|=1u<<i;
 if(detail>=0&&detail<OBD_METRICS)mask|=1u<<detail;return mask;
}
uint32_t db_crc(const void*bytes,size_t n){const uint8_t*p=bytes;uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
