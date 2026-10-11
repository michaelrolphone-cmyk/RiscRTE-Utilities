#ifndef CONTEXT_FINGERPRINT_H
#define CONTEXT_FINGERPRINT_H
/* Portable, allocation-free RF/PCM feature extraction. Storage belongs to the
 * caller; keep pipelines/banks in PSRAM, not a small embedded task stack.
 * Sparse bins retain exact 1 ms coordinates. Overflow invalidates the temporal
 * component instead of folding unrelated intervals into a misleading bin. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#define CF_HIST_CAPACITY 256u
#define CF_INTERVAL_MAX_MS 10000u
#define CF_NOISE_SAMPLES 10000u
#define CF_NOISE_LEVELS 4096u
#define CF_BANDS 128u
#define CF_SOURCES 2u
#define CF_PROFILES 24u
#define CF_NAME_SIZE 17u
#define CF_PEAKS 5u
#define CF_BURSTS 64u
enum { CF_AUDIO, CF_RF };
enum { CF_ROOM=1, CF_EVENT=2, CF_NEGATIVE=3 };
enum { CF_ENVELOPE=1, CF_SPECTRAL=2, CF_TEMPORAL=4, CF_OVERFLOW=8, CF_SPARSE=16, CF_BACKGROUND=32 };
typedef struct { uint16_t ms; float mass; } cf_bin;
typedef struct {
    uint32_t flags,identity,windows;
    uint16_t bins,peak_count;
    cf_bin repetition[CF_HIST_CAPACITY];
    float envelope_mean,envelope_variance,duty_cycle,burst_rate;
    float power_mean,power_variance,peak_hz[CF_PEAKS],peak_amplitude[CF_PEAKS];
    float frequency_origin,frequency_span,coverage,resolution_ms;
    /* Unfiltered canonical band powers, normalized to full-scale squared. */
    float background[CF_BANDS];
} cf_feature;
typedef struct { uint64_t start_us; uint32_t duration_us; float peak; } cf_burst;
typedef struct {
    cf_feature feature[CF_SOURCES];
    float weight[CF_SOURCES];
    uint64_t updated_seconds[CF_SOURCES]; /* UTC seconds, zero = unavailable. */
    uint8_t present[CF_SOURCES],kind;
    char name[CF_NAME_SIZE];
} cf_profile;
typedef struct {
    cf_profile profiles[CF_PROFILES];
    uint32_t generation;
} cf_bank;
typedef struct {
    bool enabled[CF_SOURCES],have[CF_SOURCES];
    uint32_t fresh_windows[CF_SOURCES],consumed[CF_SOURCES];
    uint64_t observed_us[CF_SOURCES];
    cf_feature live[CF_SOURCES];
} cf_fusion;
typedef struct {
    uint32_t window_ms,warmup_ms;
    float threshold_ratio,threshold_offset,temporal_weight;
    float interval_scale_ms,confidence_threshold,ambiguity_margin,training_threshold;
    uint32_t ramp_windows,stale_after_seconds;
    bool temporal_only;
} cf_config;
typedef struct { int slot; float confidence,runner_up; uint32_t sources; bool ambiguous; } cf_match;
typedef struct {
    cf_config config;
    uint32_t identity,rate,phase,average_count,noise_next,noise_count;
    uint16_t noise[CF_NOISE_SAMPLES],tree[CF_NOISE_LEVELS+1];
    uint16_t noise_time[CF_NOISE_SAMPLES];
    uint64_t snapshot_at;
    float resolution_ms;
    uint64_t sampled_us;bool sparse;
    uint64_t expected_us,window_start_us,last_tick_us,burst_start_us,previous_start_us;
    bool started,contiguous,active,previous_valid,hist_overflow;
    float average_sum,burst_peak;
    uint32_t ticks,on_ticks,starts,intervals,spectral_frames;
    float sum,sum_squares;
    float power[CF_BANDS],origin,span;
    cf_bin hist[CF_HIST_CAPACITY];
    uint16_t bins,burst_next,burst_count;
    cf_burst bursts[CF_BURSTS];
} cf_pipeline;
/* Avoid libgcc u64-to-float aliases unsupported by the restricted ELF loader. */
static inline float cf_float_u64(uint64_t n){return (float)(uint32_t)(n>>32)*4294967296.f+(float)(uint32_t)n;}
static inline float cf_abs(float x){return x<0?-x:x;}
static inline float cf_min(float a,float b){return a<b?a:b;}
static inline float cf_max(float a,float b){return a>b?a:b;}
static inline bool cf_finite(float x){return x==x&&x<=3.402823466e38f&&x>=-3.402823466e38f;}
/* Newton iteration seeded from IEEE754 exponent; no libm dependency in ELF. */
static inline float cf_sqrt(float x){
    if(!(x>0)||!cf_finite(x))return 0;
    uint32_t bits;memcpy(&bits,&x,4);bits=(bits>>1)+0x1fc00000u;
    float y;memcpy(&y,&bits,4);for(unsigned i=0;i<5;i++)y=.5f*(y+x/y);return y;
}
static inline cf_config cf_defaults(unsigned source){
    cf_config c={source==CF_RF?5000u:1500u,100u,1.8f,1.f/4095.f,.7f,100.f,.75f,.08f,.95f,10u,14u*86400u,false};return c;
}
static inline bool cf_config_valid(const cf_config *c){return c&&c->window_ms>=100&&c->window_ms<=60000&&c->warmup_ms<=10000&&c->threshold_ratio>1&&c->threshold_ratio<=100&&c->threshold_offset>=0&&c->threshold_offset<=1&&c->temporal_weight>=0&&c->temporal_weight<=1&&c->interval_scale_ms>0&&c->confidence_threshold>0&&c->confidence_threshold<=1&&c->ambiguity_margin>=0&&c->ambiguity_margin<1&&c->training_threshold>=c->confidence_threshold&&c->training_threshold<=1&&c->ramp_windows&&c->stale_after_seconds;}
static inline bool cf_init(cf_pipeline *p,const cf_config *c,uint32_t identity){
    if(!p||!cf_config_valid(c))return false;
    memset(p,0,sizeof(*p));p->config=*c;p->identity=identity;return true;
}
static inline void cf_noise_add(cf_pipeline *p,unsigned value,int change){
    for(unsigned i=value+1;i<=CF_NOISE_LEVELS;i+=i&(~i+1u))p->tree[i]=(uint16_t)(p->tree[i]+change);
}
static inline float cf_noise_median(const cf_pipeline *p){
    if(!p->noise_count)return 0;
    unsigned rank=(p->noise_count+1u)/2u,index=0;
    for(unsigned bit=CF_NOISE_LEVELS;bit;bit>>=1){unsigned next=index+bit;if(next<=CF_NOISE_LEVELS&&p->tree[next]<rank){rank-=p->tree[next];index=next;}}
    return (float)index/2047.f;
}
static inline void cf_gap(cf_pipeline *p){
    p->contiguous=p->active=p->previous_valid=false;p->phase=p->average_count=0;p->average_sum=0;
    p->noise_count=p->noise_next=0;memset(p->tree,0,sizeof(p->tree));
}
static inline bool cf_hist_add(cf_bin *bins,uint16_t *count,unsigned ms,float mass){
    if(ms>CF_INTERVAL_MAX_MS||!mass)return true;
    unsigned at=0;while(at<*count&&bins[at].ms<ms)++at;
    if(at<*count&&bins[at].ms==ms){bins[at].mass+=mass;return true;}
    if(*count==CF_HIST_CAPACITY)return false;
    for(unsigned i=*count;i>at;--i)bins[i]=bins[i-1];
    bins[at].ms=(uint16_t)ms;bins[at].mass=mass;++*count;return true;
}
static inline void cf_tick(cf_pipeline *p,float value,uint64_t stamp){
    while(p->noise_count){unsigned oldest=(p->noise_next+CF_NOISE_SAMPLES-p->noise_count)%CF_NOISE_SAMPLES;
        if((uint16_t)(stamp/1000u-p->noise_time[oldest])<=10000u)break;
        cf_noise_add(p,p->noise[oldest],-1);--p->noise_count;}
    float floor=cf_noise_median(p);
    bool on=p->noise_count>=(p->sparse?10u:p->config.warmup_ms)&&value>floor*p->config.threshold_ratio+p->config.threshold_offset;
    if(on){
        ++p->on_ticks;
        if(!p->active){
            ++p->starts;p->burst_start_us=stamp;p->burst_peak=value;
            if(p->previous_valid&&stamp>=p->previous_start_us){
                uint64_t ms=(stamp-p->previous_start_us+500u)/1000u;
                if(ms<=CF_INTERVAL_MAX_MS){++p->intervals;if(!cf_hist_add(p->hist,&p->bins,(unsigned)ms,1.f))p->hist_overflow=true;}
            }
            p->previous_start_us=stamp;p->previous_valid=true;
        }
        p->burst_peak=cf_max(p->burst_peak,value);
    }else if(p->active){
        uint64_t duration=stamp-p->burst_start_us;
        p->bursts[p->burst_next]=(cf_burst){p->burst_start_us,duration>UINT32_MAX?UINT32_MAX:(uint32_t)duration,p->burst_peak};
        p->burst_next=(uint16_t)((p->burst_next+1u)%CF_BURSTS);if(p->burst_count<CF_BURSTS)++p->burst_count;
    }
    p->active=on;p->last_tick_us=stamp;++p->ticks;p->sum+=value;p->sum_squares+=value*value;
    unsigned q=(unsigned)cf_min(value*2047.f,4095.f);
    if(p->noise_count==CF_NOISE_SAMPLES)cf_noise_add(p,p->noise[p->noise_next],-1);else ++p->noise_count;
    p->noise[p->noise_next]=(uint16_t)q;p->noise_time[p->noise_next]=(uint16_t)(stamp/1000u);cf_noise_add(p,q,1);p->noise_next=(p->noise_next+1u)%CF_NOISE_SAMPLES;
}
static inline bool cf_feed_begin(cf_pipeline *p,uint32_t rate,uint64_t start_us,bool contiguous){
    if(!p||rate<1000||rate>80000000u)return false;
    if(!p->started){p->started=true;p->window_start_us=start_us;}
    uint64_t tolerance=1000000u/rate+1u;
    if(!contiguous||!p->contiguous||p->rate!=rate||start_us<p->expected_us||start_us-p->expected_us>tolerance)cf_gap(p);
    p->rate=rate;p->contiguous=true;return true;
}
static inline void cf_magnitude(cf_pipeline *p,float value,uint64_t stamp){
    p->average_sum+=value;++p->average_count;p->phase+=1000u;
    if(p->phase>=p->rate){p->phase-=p->rate;cf_tick(p,p->average_sum/p->average_count,stamp);p->average_sum=0;p->average_count=0;}
}
/* Timestamp is the first actual sample, never the poll/return time. false
 * continuity is mandatory for independently acquired SRAM/PSRAM dumps. */
static inline bool cf_iq(cf_pipeline *p,const uint32_t *pairs,size_t count,uint32_t rate,uint64_t start_us,bool contiguous){
    if(!pairs||!count||count>80000000u||!cf_feed_begin(p,rate,start_us,contiguous))return false;
    for(size_t k=0;k<count;k++){
        int32_t i=(int32_t)(pairs[k]&1023u),q=(int32_t)((pairs[k]>>10)&1023u);if(i&512)i-=1024;if(q&512)q-=1024;
        cf_magnitude(p,cf_sqrt((float)(i*i+q*q))/512.f,start_us+(uint64_t)k*1000000u/rate);
    }
    p->expected_us=start_us+(uint64_t)count*1000000u/rate;return true;
}
static inline bool cf_pcm(cf_pipeline *p,const int16_t *pcm,size_t count,uint32_t rate,uint64_t start_us,bool contiguous){
    if(!pcm||!count||count>80000000u||!cf_feed_begin(p,rate,start_us,contiguous))return false;
    for(size_t k=0;k<count;k++)cf_magnitude(p,cf_abs((float)pcm[k])/32768.f,start_us+(uint64_t)k*1000000u/rate);
    p->expected_us=start_us+(uint64_t)count*1000000u/rate;return true;
}
/* Timestamped driver envelope, independent of foreground/render cadence.
 * Burst sampling coverage stays explicit; loss invalidates the current window. */
static inline bool cf_envelope_sample(cf_pipeline*p,float value,uint64_t stamp,uint32_t sampled_us,bool gap){
 if(!p||!cf_finite(value)||value<0||value>1.5f||!sampled_us)return false;
 if(!p->started){p->started=true;p->window_start_us=stamp;}
 if(p->snapshot_at){
  if(stamp<=p->snapshot_at||stamp-p->snapshot_at>500000u||gap){cf_gap(p);p->hist_overflow=true;}
  else p->resolution_ms=cf_max(p->resolution_ms,cf_float_u64(stamp-p->snapshot_at)/1000.f);
 }
 p->sparse=true;p->snapshot_at=stamp;p->sampled_us+=sampled_us;cf_tick(p,value,stamp);return true;
}
/* Independent radio dumps also carry useful slow envelope observations.
 * Their timestamps are explicitly coarse; intervals faster than four sample
 * spacings are rejected. This path cannot identify a 100ms beacon from a
 * 100ms polling loop. It can represent multi-second appliance cycles. */
static inline bool cf_iq_snapshot(cf_pipeline*p,const uint32_t*pairs,size_t count,uint32_t rate,uint64_t stamp){
    if(!p||!pairs||!count||count>8192||rate<1000)return false;
    if(!p->started){p->started=true;p->window_start_us=stamp;}
    if(p->sparse&&p->snapshot_at){
        if(stamp<=p->snapshot_at||stamp-p->snapshot_at>500000u){cf_gap(p);p->hist_overflow=true;}
        else p->resolution_ms=cf_max(p->resolution_ms,cf_float_u64(stamp-p->snapshot_at)/1000.f);
    }
    p->sparse=true;p->snapshot_at=stamp;p->sampled_us+=(uint64_t)count*1000000u/rate;
    float sum=0;for(size_t k=0;k<count;k++){int32_t i=(int32_t)(pairs[k]&1023),q=(int32_t)((pairs[k]>>10)&1023);if(i&512)i-=1024;if(q&512)q-=1024;sum+=cf_sqrt((float)(i*i+q*q))/512.f;}
    cf_tick(p,sum/count,stamp);return true;
}
/* Canonical unfiltered FFT band powers, as produced by existing analyzers. */
static inline bool cf_spectrum(cf_pipeline *p,const uint32_t *power,size_t count,float scale,float origin,float span){
    if(!p||!power||count!=CF_BANDS||!(scale>0)||!(span>0))return false;
    if(p->spectral_frames&&(p->origin!=origin||p->span!=span))return false;
    p->origin=origin;p->span=span;
    for(unsigned i=0;i<CF_BANDS;i++)p->power[i]+=power[i]*scale;
    ++p->spectral_frames;return true;
}
static inline bool cf_take(cf_pipeline *p,uint64_t end_us,cf_feature *out){
    if(!p||!out||!p->started||end_us<p->window_start_us||end_us-p->window_start_us<(uint64_t)p->config.window_ms*1000u)return false;
    memset(out,0,sizeof(*out));out->identity=p->identity;out->windows=1;
    float seconds=cf_float_u64(end_us-p->window_start_us)/1000000.f;
    out->coverage=cf_min(p->sparse?cf_float_u64(p->sampled_us)/(seconds*1000000.f):(float)p->ticks/(seconds*1000.f),1.f);
    out->resolution_ms=p->sparse?p->resolution_ms:1.f;if(p->sparse)out->flags|=CF_SPARSE;
    if(p->ticks){
        out->flags|=CF_ENVELOPE;out->envelope_mean=(float)(p->sum/p->ticks);
        out->envelope_variance=cf_max(0,(float)(p->sum_squares/p->ticks)-out->envelope_mean*out->envelope_mean);
        out->duty_cycle=(float)p->on_ticks/p->ticks;out->burst_rate=p->starts/seconds;
    }
    if(p->hist_overflow)out->flags|=CF_OVERFLOW;
    if(p->sparse){
        unsigned kept=0;float total=0;
        for(unsigned i=0;i<p->bins;i++)if(p->hist[i].ms>=4.f*p->resolution_ms){p->hist[kept++]=p->hist[i];total+=p->hist[i].mass;}
        p->bins=(uint16_t)kept;p->intervals=(uint32_t)total;
    }
    if(p->intervals&&!p->hist_overflow&&((!p->sparse&&out->coverage>=.8f)||(p->sparse&&p->ticks>=20&&p->resolution_ms>0&&p->resolution_ms<=250.f))){
        out->flags|=CF_TEMPORAL;out->bins=p->bins;
        for(unsigned i=0;i<p->bins;i++){out->repetition[i]=p->hist[i];out->repetition[i].mass/=p->intervals;}
    }
    if(p->spectral_frames){
        out->frequency_origin=p->origin;out->frequency_span=p->span;
        float mean=0,variance=0;
        for(unsigned i=0;i<CF_BANDS;i++){p->power[i]/=p->spectral_frames;mean+=p->power[i]/CF_BANDS;}
        for(unsigned i=0;i<CF_BANDS;i++){float d=p->power[i]-mean;variance+=d*d/CF_BANDS;}
        out->power_mean=mean;out->power_variance=variance;
        memcpy(out->background,p->power,sizeof(out->background));out->flags|=CF_BACKGROUND;
        if(mean>1e-12f){
            out->flags|=CF_SPECTRAL;
            for(unsigned i=0;i<CF_BANDS;i++){
                float v=p->power[i];if((i&&v<=p->power[i-1])||(i+1<CF_BANDS&&v<p->power[i+1]))continue;
                unsigned at=0;while(at<out->peak_count&&out->peak_amplitude[at]>=v)++at;
                if(at>=CF_PEAKS)continue;
                unsigned n=out->peak_count<CF_PEAKS?out->peak_count++:CF_PEAKS-1;
                for(unsigned j=n;j>at;j--){out->peak_amplitude[j]=out->peak_amplitude[j-1];out->peak_hz[j]=out->peak_hz[j-1];}
                out->peak_amplitude[at]=v;out->peak_hz[at]=p->origin+(i+.5f)*p->span/CF_BANDS;
            }
            /* Frequency order avoids artificial distance when peaks swap rank. */
            for(unsigned i=1;i<out->peak_count;i++)for(unsigned j=i;j&&out->peak_hz[j]<out->peak_hz[j-1];j--){
                float f=out->peak_hz[j],v=out->peak_amplitude[j];out->peak_hz[j]=out->peak_hz[j-1];out->peak_amplitude[j]=out->peak_amplitude[j-1];out->peak_hz[j-1]=f;out->peak_amplitude[j-1]=v;
            }
        }
    }
    p->window_start_us=end_us;p->ticks=p->on_ticks=p->starts=p->intervals=p->spectral_frames=0;p->sum=p->sum_squares=0;p->sampled_us=0;p->bins=0;p->hist_overflow=false;
    memset(p->power,0,sizeof(p->power));return true;
}
/* Keep the rolling floor but begin a new event window at the observed onset. */
static inline void cf_window_reset(cf_pipeline*p,uint64_t now_us){
    p->window_start_us=now_us;p->ticks=p->on_ticks=p->starts=p->intervals=p->spectral_frames=0;
    p->sum=p->sum_squares=0;p->sampled_us=0;p->bins=0;p->hist_overflow=false;p->previous_valid=false;
    memset(p->power,0,sizeof(p->power));
}
/* Convenience IQ-window interface; caller supplies reusable work memory and
 * the existing FFT power spectrum. Streaming callers use iq/spectrum/take. */
static inline bool cf_extract_iq(cf_pipeline *work,const cf_config *cfg,uint32_t identity,const uint32_t *iq,size_t count,uint32_t rate,const uint32_t power[CF_BANDS],float origin,float span,cf_feature *out){
    if(!cf_init(work,cfg,identity)||!cf_iq(work,iq,count,rate,0,true)||!cf_spectrum(work,power,CF_BANDS,1.f/1073741824.f,origin,span))return false;
    return cf_take(work,(uint64_t)count*1000000u/rate,out);
}
static inline float cf_emd(const cf_feature *a,const cf_feature *b){
    unsigned i=0,j=0,prior=0;float cdf=0,area=0;
    while(i<a->bins||j<b->bins){
        unsigned next=i<a->bins?a->repetition[i].ms:CF_INTERVAL_MAX_MS+1u;
        if(j<b->bins&&b->repetition[j].ms<next)next=b->repetition[j].ms;
        area+=cf_abs(cdf)*(next-prior);prior=next;
        if(i<a->bins&&a->repetition[i].ms==next)cdf+=a->repetition[i++].mass;
        if(j<b->bins&&b->repetition[j].ms==next)cdf-=b->repetition[j++].mass;
    }
    return area;
}
static inline float cf_relative(float a,float b,float floor){return (a-b)/cf_max(cf_max(cf_abs(a),cf_abs(b)),floor);}
static inline float cf_distance(const cf_feature *a,const cf_feature *b,const cf_config *c){
    if(a->identity!=b->identity)return 1e6f;
    if((b->flags&CF_TEMPORAL)&&!(a->flags&CF_TEMPORAL))return 1e6f; /* missing learned timing is not evidence */
    bool temporal=(a->flags&b->flags&CF_TEMPORAL)!=0;
    float interval_scale=cf_max(c->interval_scale_ms,cf_max(a->resolution_ms,b->resolution_ms));
    if(c->temporal_only)return temporal?cf_emd(a,b)/interval_scale:1e6f;
    float sum=0;unsigned n=0;
#define CF_DIST(x,y,floor) do{float d=cf_relative((x),(y),(floor));sum+=d*d;++n;}while(0)
    if(a->flags&b->flags&CF_ENVELOPE){
        CF_DIST(a->envelope_mean,b->envelope_mean,.01f);CF_DIST(a->envelope_variance,b->envelope_variance,.001f);
        CF_DIST(a->duty_cycle,b->duty_cycle,.1f);CF_DIST(a->burst_rate,b->burst_rate,1.f);
    }
    if(a->flags&b->flags&CF_SPECTRAL){
        if(a->frequency_origin!=b->frequency_origin||a->frequency_span!=b->frequency_span)return 1e6f;
        CF_DIST(a->power_mean,b->power_mean,1e-8f);CF_DIST(a->power_variance,b->power_variance,1e-12f);
        for(unsigned i=0;i<CF_PEAKS&&(i<a->peak_count||i<b->peak_count);i++){
            float fa=i<a->peak_count?(a->peak_hz[i]-a->frequency_origin)/a->frequency_span:0;
            float fb=i<b->peak_count?(b->peak_hz[i]-b->frequency_origin)/b->frequency_span:0;
            float d=cf_min(cf_abs(fa-fb)*CF_BANDS*.5f,1.f);sum+=d*d;++n;
            CF_DIST(a->peak_amplitude[i],b->peak_amplitude[i],1e-8f);
        }
    }
#undef CF_DIST
    if(!n&&!temporal)return 1e6f;
    float spectral=n?cf_sqrt(sum/n):0;
    return temporal?(n?c->temporal_weight:1.f)*cf_emd(a,b)/interval_scale+(n?(1.f-c->temporal_weight)*spectral:0):spectral;
}
static inline void cf_enable(cf_fusion *f,unsigned source,bool enabled){
    if(source>=CF_SOURCES||f->enabled[source]==enabled)return;
    f->enabled[source]=enabled;f->have[source]=false;f->fresh_windows[source]=f->consumed[source]=0;
}
static inline void cf_publish(cf_fusion *f,unsigned source,const cf_feature *v,uint64_t now_us){
    if(source>=CF_SOURCES||!f->enabled[source])return;
    f->live[source]=*v;f->have[source]=!!(v->flags&(CF_SPECTRAL|CF_TEMPORAL));f->observed_us[source]=now_us;
    if(f->have[source]&&f->fresh_windows[source]<UINT32_MAX)++f->fresh_windows[source];
}
static inline float cf_weight(const cf_profile *p,const cf_fusion *f,unsigned source,uint64_t now_seconds,const cf_config *c){
    float w=p->weight[source]*cf_min((float)f->fresh_windows[source]/c->ramp_windows,1.f);
    uint64_t updated=p->updated_seconds[source];
    if(now_seconds&&updated&&now_seconds>updated+c->stale_after_seconds)w/=1.f+cf_float_u64(now_seconds-updated-c->stale_after_seconds)/c->stale_after_seconds;
    return w;
}
static inline float cf_profile_score(const cf_profile*p,const cf_fusion*f,uint64_t now_us,uint64_t now_seconds,const cf_config*c,uint32_t *used){
        float total=0,denominator=0;uint32_t sources=0;
        for(unsigned s=0;s<CF_SOURCES;s++){
            if(!f->enabled[s]||!f->have[s]||now_us<f->observed_us[s]||now_us-f->observed_us[s]>12000000u)continue;
            /* All fresh enabled sources contribute to the denominator: a
             * profile missing a contradictory source cannot win by omission. */
            float w=p->present[s]?cf_weight(p,f,s,now_seconds,c):cf_min((float)f->fresh_windows[s]/c->ramp_windows,1.f);
            denominator+=w;
            if(p->present[s]){float d=cf_distance(&f->live[s],&p->feature[s],c);total+=w/(1.f+4.f*d);sources|=1u<<s;}
        }
        *used=sources;return denominator>0?total/denominator:0;
}
static inline cf_match cf_match_profiles(const cf_bank *bank,const cf_fusion *f,unsigned kind,uint64_t now_us,uint64_t now_seconds,const cf_config *c){
    cf_match result={-1,0,0,0,false};
    for(unsigned i=0;i<CF_PROFILES;i++){
        const cf_profile *p=&bank->profiles[i];if(p->kind!=kind)continue;
        uint32_t sources=0;float score=cf_profile_score(p,f,now_us,now_seconds,c,&sources);
        if(score>result.confidence){result.runner_up=result.confidence;result.confidence=score;result.slot=(int)i;result.sources=sources;}
        else if(score>result.runner_up)result.runner_up=score;
    }
    result.ambiguous=result.confidence>=c->confidence_threshold&&result.confidence-result.runner_up<c->ambiguity_margin;
    if(kind==CF_EVENT&&result.slot>=0){
        const char*name=bank->profiles[result.slot].name;
        for(unsigned i=0;i<CF_PROFILES;i++)if(bank->profiles[i].kind==CF_NEGATIVE&&!strcmp(bank->profiles[i].name,name)){
            uint32_t used=0;float score=cf_profile_score(&bank->profiles[i],f,now_us,now_seconds,c,&used);
            if(score>=c->confidence_threshold&&score>=result.confidence-.05f){result.slot=-1;result.confidence=0;break;}
        }
    }
    if(result.confidence<c->confidence_threshold||result.ambiguous)result.slot=-1;
    return result;
}
static inline bool cf_blend(cf_feature *dst,const cf_feature *src,float alpha){
    if(dst->identity!=src->identity)return false;
    if(dst->flags&src->flags&CF_TEMPORAL){
        unsigned union_count=dst->bins;
        for(unsigned i=0;i<src->bins;i++){unsigned j=0;while(j<dst->bins&&dst->repetition[j].ms!=src->repetition[i].ms)++j;if(j==dst->bins)++union_count;}
        if(union_count>CF_HIST_CAPACITY)return false; /* preserve original */
        for(unsigned i=0;i<dst->bins;i++)dst->repetition[i].mass*=1.f-alpha;
        for(unsigned i=0;i<src->bins;i++)(void)cf_hist_add(dst->repetition,&dst->bins,src->repetition[i].ms,alpha*src->repetition[i].mass);
    }else if(src->flags&CF_TEMPORAL){dst->bins=src->bins;memcpy(dst->repetition,src->repetition,sizeof(dst->repetition));dst->flags|=CF_TEMPORAL;}
    dst->resolution_ms=cf_max(dst->resolution_ms,src->resolution_ms);dst->flags|=src->flags&CF_SPARSE;
#define CF_BLEND(field) dst->field=dst->field*(1.f-alpha)+src->field*alpha
    if(src->flags&CF_ENVELOPE){CF_BLEND(envelope_mean);CF_BLEND(envelope_variance);CF_BLEND(duty_cycle);CF_BLEND(burst_rate);dst->flags|=CF_ENVELOPE;}
    if(src->flags&CF_SPECTRAL){CF_BLEND(power_mean);CF_BLEND(power_variance);for(unsigned i=0;i<CF_PEAKS;i++){CF_BLEND(peak_hz[i]);CF_BLEND(peak_amplitude[i]);}dst->peak_count=src->peak_count;dst->frequency_origin=src->frequency_origin;dst->frequency_span=src->frequency_span;dst->flags|=CF_SPECTRAL;}
    if(src->flags&CF_BACKGROUND){for(unsigned i=0;i<CF_BANDS;i++){if(dst->flags&CF_BACKGROUND){CF_BLEND(background[i]);}else dst->background[i]=src->background[i];}dst->flags|=CF_BACKGROUND;}
#undef CF_BLEND
    if(dst->windows<UINT32_MAX)++dst->windows;
    return true;
}
static inline int cf_profile_find(const cf_bank*b,unsigned kind,const char*name){
    if(!b||!name||!name[0])return -1;
    for(unsigned i=0;i<CF_PROFILES;i++)if(b->profiles[i].kind==kind&&!strcmp(b->profiles[i].name,name))return (int)i;
    return -1;
}
static inline int cf_profile_slot(const cf_bank*b,unsigned kind,const char*name){
    int slot=cf_profile_find(b,kind,name);if(slot>=0)return slot;
    if(kind<CF_ROOM||kind>CF_NEGATIVE)return -1;
    for(unsigned i=(kind-1)*8;i<kind*8;i++)if(!b->profiles[i].kind)return (int)i;
    return -1;
}
static inline bool cf_background_power(const cf_profile*p,unsigned source,uint32_t identity,uint32_t out[CF_BANDS]){
    if(!p||source>=CF_SOURCES||p->kind!=CF_ROOM||!p->present[source])return false;
    const cf_feature*f=&p->feature[source];if(f->identity!=identity||!(f->flags&CF_BACKGROUND))return false;
    for(unsigned i=0;i<CF_BANDS;i++)out[i]=(uint32_t)cf_min(f->background[i]*1073741824.f,2147483648.f);
    return true;
}
static inline bool cf_train(cf_bank *bank,cf_fusion *f,unsigned slot,const char *name,unsigned kind,uint64_t now_us,uint64_t utc,const cf_config *c,bool confirmed){
    if(slot>=CF_PROFILES||!name||!name[0]||!memchr(name,0,CF_NAME_SIZE)||(kind!=CF_ROOM&&kind!=CF_EVENT&&kind!=CF_NEGATIVE))return false;
    cf_profile *p=&bank->profiles[slot];
    if(p->kind&&(p->kind!=kind||strcmp(p->name,name)))return false;
    if(!confirmed){cf_match m=cf_match_profiles(bank,f,kind,now_us,utc,c);if(m.slot!=(int)slot||m.confidence<c->training_threshold)return false;}
    bool changed=false;
    for(unsigned s=0;s<CF_SOURCES;s++){
        if(!f->enabled[s]||!f->have[s]||now_us<f->observed_us[s]||now_us-f->observed_us[s]>12000000u||f->consumed[s]==f->fresh_windows[s])continue;
        /* A fused answer must not self-train a disagreeing weak source. */
        if(!confirmed&&(!p->present[s]||1.f/(1.f+4.f*cf_distance(&f->live[s],&p->feature[s],c))<c->training_threshold))continue;
        if(p->present[s]){if(!cf_blend(&p->feature[s],&f->live[s],.05f))continue;}else p->feature[s]=f->live[s];
        p->present[s]=1;p->weight[s]=1.f;if(utc)p->updated_seconds[s]=utc;f->consumed[s]=f->fresh_windows[s];changed=true;
    }
    if(changed){p->kind=(uint8_t)kind;memcpy(p->name,name,strlen(name)+1u);if(++bank->generation==0)++bank->generation;}
    return changed;
}
#endif
