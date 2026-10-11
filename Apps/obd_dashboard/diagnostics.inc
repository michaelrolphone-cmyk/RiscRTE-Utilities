/* ELM327DSJ sections: trouble codes, reset codes, multiline responses and CAN
 * message types. Headers stay ON: no mixing independent ECU payloads. */
static void log_line(obd_client*c,const char*text,bool tx){
 unsigned at=c->log_head;snprintf(c->logs[at],sizeof(c->logs[at]),"%c %.92s",tx?'>':'<',text);
 for(char*p=c->logs[at];*p;p++)if(*p=='\r'||*p=='\n')*p=' ';
 c->log_head=(at+1)%OBD_LOG_LINES;if(c->log_count<OBD_LOG_LINES)c->log_count++;c->log_revision++;
}
void obd_code_text(uint16_t code,char out[6]){static const char families[]="PCBU";snprintf(out,6,"%c%X%03X",families[code>>14],(code>>12)&3,code&4095);}
void obd_request_faults(obd_client*c){if(!c||c->state!=OBD_LIVE)return;for(unsigned i=0;i<3;i++){c->jobs|=1u<<(OBD_JOB_STORED+i);c->codes[i].status=OBD_DIAG_LOADING;}}
void obd_request_info(obd_client*c){if(!c||c->state!=OBD_LIVE)return;c->jobs|=(1u<<OBD_JOB_VIN)|(1u<<OBD_JOB_VOLTAGE);c->vin_status=c->voltage_status=OBD_DIAG_LOADING;}
void obd_request_freeze(obd_client*c,uint16_t code){
 if(!c||c->state!=OBD_LIVE||!code||c->job==OBD_JOB_FREEZE_CODE||c->job==OBD_JOB_FREEZE_DATA)return;
 c->freeze_requested=code;c->freeze_code=0;c->freeze_status=OBD_DIAG_LOADING;c->ff_cursor=0;
 memset(c->freeze,0,sizeof(c->freeze));c->jobs&=~(1u<<OBD_JOB_FREEZE_DATA);c->jobs|=1u<<OBD_JOB_FREEZE_CODE;
}
static bool clear_conditions(const obd_client*c,uint32_t now){
 return c->state==OBD_LIVE&&!c->paused&&c->have_ecu&&obd_fresh(c,OBD_RPM,now)&&obd_fresh(c,OBD_SPEED,now)&&
 c->samples[OBD_RPM].milli==0&&c->samples[OBD_SPEED].milli==0&&
 (uint32_t)(now-c->samples[OBD_RPM].at_ms)<=1500&&(uint32_t)(now-c->samples[OBD_SPEED].at_ms)<=1500;
}
bool obd_confirm_clear(obd_client*c,uint32_t now){
 if(!c||!clear_conditions(c,now)||c->job==OBD_JOB_CLEAR||c->clear_armed)return false;
 /* User confirmation is a short-lived, ECU-bound single-use request. */
 c->clear_armed=true;c->clear_deadline=now+1500;c->clear_ecu=c->ecu;c->clear_status=OBD_DIAG_LOADING;return true;
}
static const unsigned ff_metrics[]={OBD_SPEED,OBD_RPM,OBD_COOLANT,OBD_LOAD,OBD_MAP,OBD_SHORT_TRIM,OBD_LONG_TRIM,OBD_INTAKE};
static bool diag_next(obd_client*c,uint32_t now){
 if(c->clear_armed){
  c->clear_armed=false;
  if(due(now,c->clear_deadline)||c->ecu!=c->clear_ecu||!clear_conditions(c,now)){c->clear_status=OBD_DIAG_ERROR;copy(c->message,sizeof(c->message),"Clear cancelled: stationary engine-off readings expired");return false;}
  c->job=OBD_JOB_CLEAR;copy(c->command,sizeof(c->command),"04\r");return true;
 }
 if(!c->jobs)return false;
 for(int j=0;j<=OBD_JOB_VOLTAGE;j++)if(c->jobs&(1u<<j)){c->job=j;c->jobs&=~(1u<<j);break;}
 switch(c->job){
 case OBD_JOB_VIN:copy(c->command,sizeof(c->command),"0902\r");break;
 case OBD_JOB_STORED:copy(c->command,sizeof(c->command),"03\r");break;
 case OBD_JOB_PENDING:copy(c->command,sizeof(c->command),"07\r");break;
 case OBD_JOB_PERMANENT:copy(c->command,sizeof(c->command),"0A\r");break;
 case OBD_JOB_FREEZE_CODE:copy(c->command,sizeof(c->command),"020200\r");break;
 case OBD_JOB_FREEZE_DATA:snprintf(c->command,sizeof(c->command),"02%02X00\r",obd_specs[ff_metrics[c->ff_cursor]].pid);break;
 case OBD_JOB_VOLTAGE:copy(c->command,sizeof(c->command),"ATRV\r");break;
 default:return false;
 }return true;
}
static void dtc_append(obd_codes*codes,uint16_t code){
 if(!code)return;for(unsigned i=0;i<codes->count;i++)if(codes->codes[i]==code)return;
 if(codes->count==OBD_DTC_MAX){codes->truncated=true;return;}codes->codes[codes->count++]=code;
}
static bool vin_char(uint8_t ch){return (ch>='0'&&ch<='9')||(ch>='A'&&ch<='Z'&&ch!='I'&&ch!='O'&&ch!='Q');}
static void diag_payload(obd_client*c,const uint8_t*b,unsigned size,bool can){
 static const uint8_t mode[]={9,3,7,10,2,2,4,0};
 if(size>=3&&b[0]==0x7f&&b[1]==mode[c->job]){if(b[2]!=0x78)c->negative=true;return;}
 if(!size||b[0]!=(uint8_t)(0x40+mode[c->job]))return;
 if(c->job>=OBD_JOB_STORED&&c->job<=OBD_JOB_PERMANENT){
  unsigned off=1,count;
  if(can){if(size<2){c->assembly_bad=true;return;}count=b[1];off=2;if(count>(size-2)/2){c->assembly_bad=true;return;}
   for(unsigned i=off+count*2;i<size;i++)if(b[i]){c->assembly_bad=true;return;}}
  else{if((size-1)%2){c->assembly_bad=true;return;}count=(size-1)/2;}
  for(unsigned i=0;i<count;i++)dtc_append(&c->pending_codes,(uint16_t)((unsigned)b[off+i*2]*256+b[off+i*2+1]));
  c->found=true;return;
 }
 if(c->job==OBD_JOB_VIN){
  if(size<3||b[1]!=2)return;
  if(can){if(size!=20||b[2]!=1){c->assembly_bad=true;return;}for(unsigned i=0;i<17;i++){if(!vin_char(b[i+3])){c->assembly_bad=true;return;}c->pending_vin[i]=(char)b[i+3];}c->found=true;}
  else{if(size!=7||b[2]!=c->legacy_vin_seq+1||b[2]>5){c->assembly_bad=true;return;}
   memcpy(c->pending_vin+c->legacy_vin_seq*4,b+3,4);c->legacy_vin_seq++;
   if(c->legacy_vin_seq==5){if(c->pending_vin[0]||c->pending_vin[1]||c->pending_vin[2]){c->assembly_bad=true;return;}
    for(unsigned i=0;i<17;i++)if(!vin_char((uint8_t)c->pending_vin[i+3])){c->assembly_bad=true;return;}
    for(unsigned i=0;i<17;i++)c->pending_vin[i]=c->pending_vin[i+3];c->pending_vin[17]=0;c->found=true;}}
  return;
 }
 if(c->job==OBD_JOB_FREEZE_CODE){if(size==5&&b[1]==2&&b[2]==0){c->candidate_value=(uint32_t)b[3]*256+b[4];c->found=true;}return;}
 if(c->job==OBD_JOB_FREEZE_DATA){unsigned i=ff_metrics[c->ff_cursor];if(size==3u+obd_specs[i].bytes&&b[1]==obd_specs[i].pid&&b[2]==0){uint8_t bytes[4]={0};memcpy(bytes,b+3,obd_specs[i].bytes);c->candidate_value=(uint32_t)decode(obd_specs[i].scale,bytes);c->found=true;}return;}
 if(c->job==OBD_JOB_CLEAR&&size==1)c->found=true;
}
static bool diag_line(obd_client*c,const char*s){
 char echo[16];copy(echo,sizeof(echo),c->command);echo[strcspn(echo,"\r")]=0;if(!strcmp(echo,s))return true;
 if(c->job==OBD_JOB_VOLTAGE){
  unsigned whole=0,frac=0,digits=0;const char*p=s;while(*p>='0'&&*p<='9'&&digits<2){whole=whole*10+(unsigned)(*p++-'0');digits++;}
  if(!digits||*p++!='.'||*p<'0'||*p>'9')return false;frac=(unsigned)(*p++-'0')*100;
  if(*p>='0'&&*p<='9')frac+=(unsigned)(*p++-'0')*10;if(*p=='V')p++;
  if(!*p&&whole<=30){c->candidate_value=whole*1000+frac;c->found=true;return true;}return false;
 }
 char hexes[OBD_LINE_BYTES];unsigned len=0;
 for(const char*p=s;*p;p++){char v=*p;if(v==' '||v=='\t')continue;if(v>='a'&&v<='f')v-=32;if(hex(v)<0)return false;hexes[len++]=v;}hexes[len]=0;
 unsigned head=c->ecu_kind==11?3:c->ecu_kind==29?8:6;
 if(len<head+2||(len-head)%2){c->bad_lines++;return true;}
 uint32_t ecu=c->ecu_kind==3?number(hexes+4,2):number(hexes,head);if(ecu!=c->ecu)return true;
 unsigned n=(len-head)/2;if(n>64){c->assembly_bad=true;return true;}uint8_t b[64];for(unsigned i=0;i<n;i++)b[i]=(uint8_t)number(hexes+head+i*2,2);
 if(c->ecu_kind==3){
  /* ATH1 on a legacy protocol includes a checksum byte. Some clones hide it;
   * infer only lengths prescribed by each requested service, not random tails. */
  unsigned expected=c->job==OBD_JOB_CLEAR?1:c->job==OBD_JOB_FREEZE_CODE?5:c->job==OBD_JOB_FREEZE_DATA?3+obd_specs[ff_metrics[c->ff_cursor]].bytes:7;
  if(n==expected+1)n--;if(n!=expected){c->assembly_bad=true;return true;}diag_payload(c,b,n,false);return true;
 }
 if(n>8||!n){c->assembly_bad=true;return true;}
 unsigned type=b[0]>>4;
 if(type==0){unsigned total=b[0]&15;if(!total||total>7||total+1>n){c->assembly_bad=true;return true;}diag_payload(c,b+1,total,true);}
 else if(type==1){
  if(n!=8||c->assembly_needed){c->assembly_bad=true;return true;}unsigned total=((b[0]&15u)<<8)|b[1];
  if(total<=7||total>sizeof(c->assembly)){c->assembly_bad=true;return true;}
  c->assembly_needed=total;c->assembly_size=6;c->assembly_seq=1;memcpy(c->assembly,b+2,6);
 }else if(type==2){
  if(n<2||!c->assembly_needed||c->assembly_size>=c->assembly_needed||(b[0]&15u)!=c->assembly_seq){c->assembly_bad=true;return true;}
  unsigned need=c->assembly_needed-c->assembly_size,copy_n=n-1;if(copy_n>need)copy_n=need;
  if(need>copy_n&&n!=8){c->assembly_bad=true;return true;}
  memcpy(c->assembly+c->assembly_size,b+1,copy_n);c->assembly_size+=copy_n;c->assembly_seq=(c->assembly_seq+1)&15u;
  if(c->assembly_size==c->assembly_needed)diag_payload(c,c->assembly,c->assembly_size,true);
 }else if(type!=3)c->assembly_bad=true;
 return true;
}
static void diag_finish(obd_client*c,bool ok){
 if(c->assembly_needed&&c->assembly_size!=c->assembly_needed)ok=false;
 if(c->negative)ok=false;
 unsigned status=ok?OBD_DIAG_VALID:c->explicit_error?OBD_DIAG_UNAVAILABLE:OBD_DIAG_ERROR;
 if(c->job==OBD_JOB_VIN){c->vin_status=status;if(ok){memcpy(c->vin,c->pending_vin,17);c->vin[17]=0;}}
 else if(c->job>=OBD_JOB_STORED&&c->job<=OBD_JOB_PERMANENT){unsigned i=(unsigned)c->job-OBD_JOB_STORED;if(ok)c->codes[i]=c->pending_codes;c->codes[i].status=status;}
 else if(c->job==OBD_JOB_FREEZE_CODE){
  c->freeze_code=ok?(uint16_t)c->candidate_value:0;
  if(ok&&c->freeze_code&&c->freeze_code==c->freeze_requested){c->ff_cursor=0;c->jobs|=1u<<OBD_JOB_FREEZE_DATA;}
  else c->freeze_status=ok?OBD_DIAG_UNAVAILABLE:status;
 }else if(c->job==OBD_JOB_FREEZE_DATA){
  unsigned i=ff_metrics[c->ff_cursor];if(ok){c->freeze[i].valid=true;c->freeze[i].milli=(int32_t)c->candidate_value;}
  if(++c->ff_cursor<sizeof(ff_metrics)/sizeof(ff_metrics[0]))c->jobs|=1u<<OBD_JOB_FREEZE_DATA;else c->freeze_status=OBD_DIAG_VALID;
 }else if(c->job==OBD_JOB_CLEAR){
  c->clear_status=status;if(ok){c->readiness_valid=false;c->freeze_status=OBD_DIAG_IDLE;memset(c->freeze,0,sizeof(c->freeze));obd_request_faults(c);c->samples[OBD_MIL].valid=false;c->samples[OBD_MIL].due_ms=0;}
  copy(c->message,sizeof(c->message),ok?"ECU acknowledged clear; re-reading codes and readiness":"Clear not acknowledged. No success assumed.");
 }else if(c->job==OBD_JOB_VOLTAGE){c->voltage_status=status;if(ok)c->adapter_mv=(int32_t)c->candidate_value;}
 if(!ok)c->failed_requests++;c->job=OBD_JOB_NONE;
}
