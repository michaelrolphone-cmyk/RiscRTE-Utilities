#ifndef CONTEXT_FINGERPRINT_STORE_H
#define CONTEXT_FINGERPRINT_STORE_H
#include "ContextFingerprint.h"
#define CF_STORE_MAX 72000u
#define CF_STORE_SCHEMA 2u
/* Explicit little-endian wire format, per source, independent of C padding. */
typedef struct { uint8_t *data; uint32_t at,size; bool ok; } cf_writer;
typedef struct { const uint8_t *data; uint32_t at,size; bool ok; } cf_reader;
static inline void cf_w32(cf_writer*w,uint32_t v){if(w->at>w->size||w->size-w->at<4){w->ok=false;return;}for(unsigned i=0;i<4;i++)w->data[w->at++]=(uint8_t)(v>>(i*8));}
static inline uint32_t cf_r32(cf_reader*r){if(r->at>r->size||r->size-r->at<4){r->ok=false;return 0;}uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)r->data[r->at++]<<(i*8);return v;}
static inline void cf_wf(cf_writer*w,float v){uint32_t bits;memcpy(&bits,&v,4);cf_w32(w,bits);}
static inline float cf_rf(cf_reader*r){uint32_t bits=cf_r32(r);float v;memcpy(&v,&bits,4);if(!cf_finite(v)||v<0)r->ok=false;return v;}
static inline uint32_t cf_crc(const uint8_t*p,uint32_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1u));}return ~c;}
static inline bool cf_feature_valid(const cf_feature *v){
    if(v->flags&~63u||v->bins>CF_HIST_CAPACITY||v->peak_count>CF_PEAKS||v->coverage<0||v->coverage>1||v->duty_cycle<0||v->duty_cycle>1)return false;
    if((v->flags&CF_TEMPORAL)&&(!v->bins||(v->flags&CF_OVERFLOW)))return false;
    if(!(v->flags&CF_TEMPORAL)&&v->bins)return false;
    for(unsigned i=0;i<CF_BANDS;i++)if(!cf_finite(v->background[i])||v->background[i]<0||v->background[i]>2.f)return false;
    float sum=0;
    for(unsigned i=0;i<v->bins;i++){if(v->repetition[i].ms>10000||(i&&v->repetition[i].ms<=v->repetition[i-1].ms)||!cf_finite(v->repetition[i].mass)||v->repetition[i].mass<=0)return false;sum+=v->repetition[i].mass;}
    if(v->bins&&cf_abs(sum-1.f)>.002f)return false;
    if((v->flags&CF_SPECTRAL)&&(!v->frequency_span||!v->power_mean))return false;
    return true;
}
static inline void cf_write_feature(cf_writer*w,const cf_feature*v){
    cf_w32(w,v->flags);cf_w32(w,v->identity);cf_w32(w,v->windows);cf_w32(w,v->bins);cf_w32(w,v->peak_count);
    const float scalars[]={v->envelope_mean,v->envelope_variance,v->duty_cycle,v->burst_rate,v->power_mean,v->power_variance,v->frequency_origin,v->frequency_span,v->coverage,v->resolution_ms};
    for(unsigned i=0;i<10;i++)cf_wf(w,scalars[i]);
    for(unsigned i=0;i<CF_PEAKS;i++){cf_wf(w,v->peak_hz[i]);cf_wf(w,v->peak_amplitude[i]);}
    for(unsigned i=0;i<CF_BANDS;i++)cf_wf(w,v->background[i]);
    for(unsigned i=0;i<v->bins;i++){cf_w32(w,v->repetition[i].ms);cf_wf(w,v->repetition[i].mass);}
}
static inline void cf_read_feature(cf_reader*r,cf_feature*v,unsigned schema){
    memset(v,0,sizeof(*v));v->flags=cf_r32(r);v->identity=cf_r32(r);v->windows=cf_r32(r);
    uint32_t bins=cf_r32(r),peaks=cf_r32(r);if(bins>CF_HIST_CAPACITY||peaks>CF_PEAKS){r->ok=false;return;}
    v->bins=(uint16_t)bins;v->peak_count=(uint16_t)peaks;
    float *scalars[]={&v->envelope_mean,&v->envelope_variance,&v->duty_cycle,&v->burst_rate,&v->power_mean,&v->power_variance,&v->frequency_origin,&v->frequency_span,&v->coverage,&v->resolution_ms};
    for(unsigned i=0;i<10;i++)*scalars[i]=cf_rf(r);
    for(unsigned i=0;i<CF_PEAKS;i++){v->peak_hz[i]=cf_rf(r);v->peak_amplitude[i]=cf_rf(r);}
    if(schema>=2)for(unsigned i=0;i<CF_BANDS;i++)v->background[i]=cf_rf(r);
    else if(v->flags&CF_BACKGROUND)r->ok=false;
    for(unsigned i=0;i<bins;i++){uint32_t ms=cf_r32(r);if(ms>10000)r->ok=false;v->repetition[i].ms=(uint16_t)ms;v->repetition[i].mass=cf_rf(r);}
    if(!cf_feature_valid(v))r->ok=false;
}
static inline uint32_t cf_encode_source(const cf_bank*b,unsigned source,uint8_t*data,uint32_t capacity){
    if(!b||source>=CF_SOURCES||!data)return 0;
    cf_writer w={data,0,capacity,true};cf_w32(&w,0x31504643u);cf_w32(&w,CF_STORE_SCHEMA);cf_w32(&w,source);cf_w32(&w,b->generation);
    for(unsigned i=0;i<CF_PROFILES;i++){
        const cf_profile*p=&b->profiles[i];bool present=p->kind&&p->present[source];cf_w32(&w,present?p->kind:0);
        if(!present)continue;
        if(!memchr(p->name,0,CF_NAME_SIZE)||!p->name[0]||!cf_feature_valid(&p->feature[source]))return 0;
        for(unsigned j=0;j<CF_NAME_SIZE;j++)cf_w32(&w,(uint8_t)p->name[j]);
        cf_wf(&w,p->weight[source]);cf_w32(&w,(uint32_t)p->updated_seconds[source]);cf_w32(&w,(uint32_t)(p->updated_seconds[source]>>32));
        cf_write_feature(&w,&p->feature[source]);
    }
    if(!w.ok||capacity-w.at<4)return 0;
    uint32_t crc=cf_crc(data,w.at);cf_w32(&w,crc);return w.ok?w.at:0;
}
/* Validate into caller-provided scratch before replacing the live bank. */
static inline bool cf_decode_source(cf_bank*out,unsigned source,const uint8_t*data,uint32_t size){
    if(!out||source>=CF_SOURCES||!data||size<116||size>CF_STORE_MAX)return false;
    cf_reader tail={data,size-4,size,true};if(cf_r32(&tail)!=cf_crc(data,size-4))return false;
    cf_reader r={data,0,size-4,true};
    if(cf_r32(&r)!=0x31504643u)return false;
    unsigned schema=cf_r32(&r);if((schema!=1&&schema!=CF_STORE_SCHEMA)||cf_r32(&r)!=source)return false;
    memset(out,0,sizeof(*out));out->generation=cf_r32(&r);
    for(unsigned i=0;i<CF_PROFILES&&r.ok;i++){
        cf_profile*p=&out->profiles[i];uint32_t kind=cf_r32(&r);if(!kind)continue;
        if(kind>CF_NEGATIVE){r.ok=false;break;}p->kind=(uint8_t)kind;
        for(unsigned j=0;j<CF_NAME_SIZE;j++){uint32_t c=cf_r32(&r);if(c>126||(c&&c<32))r.ok=false;p->name[j]=(char)c;}
        if(!p->name[0]||p->name[CF_NAME_SIZE-1])r.ok=false;
        p->weight[source]=cf_rf(&r);if(p->weight[source]<=0||p->weight[source]>1)r.ok=false;
        uint64_t low=cf_r32(&r),high=cf_r32(&r);p->updated_seconds[source]=low|(high<<32);
        cf_read_feature(&r,&p->feature[source],schema);p->present[source]=1;
    }
    return r.ok&&r.at==r.size;
}
#endif
