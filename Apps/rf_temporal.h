#ifndef RF_TEMPORAL_H
#define RF_TEMPORAL_H
#include "rf_background.h"
#define RT_LABELS 8u
#define RT_EXAMPLES 6u
#define RT_FRAMES 64u
#define RT_PRE 4u
#define RT_FRAME_MS 100u
#define RT_DELTA_MAX_MS 65535u
#define RT_GAP_BEFORE 4u
#define RT_BANDS 64u
#define RT_POSITIVE 1u
#define RT_NEGATIVE 2u
#define RT_CLIPPED 1u
#define RT_CONFIRMED_END 2u
#define RT_ACTIVE 1u
#define RT_TONAL 2u
#define RT_MAX_SHIFT 2
#define RT_MATCH_MIN 800u
#define RT_MATCH_MARGIN 80u
#define RT_DTW_RADIUS 4u
/* Full-passband, FFT-shifted linear bands. Coordinates map the 128 canonical
 * bin centers into 1..65535; zero means no signal. Each observation records
 * its actual millisecond timestamp and explicitly flags unobserved intervals.
 * A sequence duration is elapsed observation span, never occupied RF time. */
typedef struct {uint8_t shape[32];int16_t level_db;uint16_t peak_coord;uint8_t flux,flags;uint32_t timestamp_ms;} rt_frame;
typedef struct {
 uint32_t id;
 uint8_t count,pre,kind,flags,onset,end,impacts,impact_at[8];
 uint32_t duration_ms,attack_ms,decay_ms;
 uint16_t peak_coord;
 int16_t peak_db;
 rt_frame frames[RT_FRAMES];
} rt_example;
typedef struct {bool present;uint8_t shift_limit;char name[17];uint32_t next_id;rt_example examples[RT_EXAMPLES];} rt_label;
typedef struct {rt_label labels[RT_LABELS];uint32_t generation[2];rf_capture_identity identity;} rt_library;
_Static_assert(sizeof(rt_frame)<=44,"bounded timestamped observation RAM");
_Static_assert(sizeof(rt_library)<140000,"bounded example RAM");
static inline unsigned rt_nibble(const rt_frame *f,int band){if(band<0||band>=64)return 0;return (f->shape[(unsigned)band/2u]>>((band&1)*4))&15u;}
static inline void rt_set_nibble(rt_frame *f,unsigned band,unsigned value){unsigned shift=(band&1)*4;f->shape[band/2]=(uint8_t)((f->shape[band/2]&~(15u<<shift))|(value<<shift));}
static inline unsigned rt_linear_band(unsigned linear){return linear/2u;}
static inline uint16_t rt_bin_coord(unsigned bin){return bin<128u?(uint16_t)(1u+((4u*bin+1u)*65534u+256u)/512u):0;}
static inline rt_frame rt_frame_make(const uint32_t excess[128],const uint32_t previous[128],bool active,uint32_t timestamp_ms,bool missing_before){
 rt_frame f={0};f.timestamp_ms=timestamp_ms;uint64_t bands[64]={0},total=0,positive=0,peak=0;unsigned peak_linear=0;
 for(unsigned i=0;i<128;i++){bands[rt_linear_band(i)]+=excess[i];total+=excess[i];if(excess[i]>previous[i])positive+=excess[i]-previous[i];if(excess[i]>excess[peak_linear])peak_linear=i;}
 for(unsigned i=0;i<64;i++)if(bands[i]>peak)peak=bands[i];
 f.level_db=rf_background_db(total);if(total){int value=((int)f.level_db+12050)/100;if(value<1)value=1;if(value>130)value=130;f.level_db=(int16_t)(value*100-12000);}f.peak_coord=peak?rt_bin_coord(peak_linear):0;f.flags=(peak&&active?RT_ACTIVE:0)|(missing_before?RT_GAP_BEFORE:0);
 uint64_t flux_total=total,flux_positive=positive>total?total:positive;while(flux_total>UINT32_MAX){flux_total>>=1;flux_positive>>=1;}
 uint64_t localized=0,other=0;for(unsigned i=peak_linear?peak_linear-1:0;i<128&&i<=peak_linear+1;i++)localized+=excess[i];
 for(unsigned i=0;i<128;i++)if(i+2u<peak_linear||i>peak_linear+2u){uint64_t sum=0;for(unsigned j=i?i-1u:0;j<128&&j<=i+1u;j++)sum+=excess[j];if(sum>other)other=sum;}
 if(total&&localized*3u>=total&&(uint64_t)excess[peak_linear]*8u>=total&&localized>=other*2u)f.flags|=RT_TONAL;
 f.flux=flux_total?(uint8_t)rf_dsp_div_u64_u32(flux_positive*255u,(uint32_t)flux_total):0;
 unsigned scale=0;while(peak>UINT32_MAX){peak>>=1;++scale;}
 if(peak)for(unsigned i=0;i<64;i++){uint32_t p=(uint32_t)(bands[i]>>scale);if(!p)continue;int level=15-(rf_dsp_log2_q16((uint32_t)peak)-rf_dsp_log2_q16(p)+65535)/65536;if(level<0)level=0;if(level>15)level=15;rt_set_nibble(&f,i,(unsigned)level);}
 return f;
}
static inline bool rt_frame_valid(const rt_frame *f){
 if(!f||f->level_db< -12000||f->level_db>1000||f->level_db%100||(f->flags&~(RT_ACTIVE|RT_TONAL|RT_GAP_BEFORE)))return false;
 unsigned peak=0;for(unsigned i=0;i<64;i++){unsigned n=rt_nibble(f,(int)i);if(n>peak)peak=n;}
 if(!f->peak_coord)return !peak&&f->level_db==-12000&&!f->flux&&!(f->flags&~RT_GAP_BEFORE);
 return peak==15&&f->level_db> -12000;
}
static inline unsigned rt_elapsed(const rt_example *e,unsigned first,unsigned last){return e->frames[last].timestamp_ms-e->frames[first].timestamp_ms;}
/* Only derived metadata is needed while validating an immutable example.
 * Keep the full frame sequence in place instead of copying it onto the stack. */
typedef struct {
 uint8_t onset,end,impacts,impact_at[8];
 uint32_t duration_ms,attack_ms,decay_ms;
 uint16_t peak_coord;int16_t peak_db;
} rt_summary;
_Static_assert(sizeof(rt_summary)<=32,"bounded temporal summary scratch");
static inline void rt_summary_make(const rt_example *e,rt_summary *summary){
 summary->peak_db=-12000;summary->peak_coord=0;summary->onset=e->pre;summary->end=e->count?e->count-1:0;summary->impacts=0;memset(summary->impact_at,0,sizeof(summary->impact_at));unsigned peak_index=e->pre,last_impact=0;bool have_impact=false;
 for(unsigned i=e->pre;i<e->count;i++){if(e->frames[i].level_db>summary->peak_db){summary->peak_db=e->frames[i].level_db;summary->peak_coord=e->frames[i].peak_coord;peak_index=i;}if((e->frames[i].flags&RT_ACTIVE)&&e->frames[i].flux>=180&&(!have_impact||rt_elapsed(e,last_impact,i)>=RT_FRAME_MS)){last_impact=i;have_impact=true;if(summary->impacts<8)summary->impact_at[summary->impacts]=(uint8_t)i;if(summary->impacts<255)++summary->impacts;}}
 while(summary->onset<e->count&&!(e->frames[summary->onset].flags&RT_ACTIVE))++summary->onset;
 if(summary->onset>=e->count)summary->onset=e->pre;
 while(summary->end>summary->onset&&!(e->frames[summary->end].flags&RT_ACTIVE))--summary->end;
 if(peak_index<summary->onset)peak_index=summary->onset;
 unsigned lo=summary->onset,hi=summary->onset,last_hi=peak_index,last_lo=peak_index;
 for(unsigned i=summary->onset;i<=peak_index;i++){if(e->frames[i].level_db<summary->peak_db-700)lo=i;if(e->frames[i].level_db<summary->peak_db-100)hi=i;}
 for(unsigned i=peak_index;i<=summary->end;i++){if(e->frames[i].level_db>=summary->peak_db-100)last_hi=i;if(e->frames[i].level_db>=summary->peak_db-700)last_lo=i;}
 summary->duration_ms=(uint32_t)(rt_elapsed(e,summary->onset,summary->end)+1u);summary->attack_ms=(uint32_t)(hi>=lo?rt_elapsed(e,lo,hi):0);summary->decay_ms=(uint32_t)(last_lo>=last_hi?rt_elapsed(e,last_hi,last_lo):0);
}
static inline void rt_summarize(rt_example *e){
 rt_summary summary;rt_summary_make(e,&summary);
 e->peak_db=summary.peak_db;
 e->peak_coord=summary.peak_coord;
 e->onset=summary.onset;
 e->end=summary.end;
 e->impacts=summary.impacts;
 memcpy(e->impact_at,summary.impact_at,sizeof(e->impact_at));
 e->duration_ms=summary.duration_ms;
 e->attack_ms=summary.attack_ms;
 e->decay_ms=summary.decay_ms;
}
static inline bool rt_example_valid(const rt_example *e){
 if(!e||!e->id||e->kind<1||e->kind>2||e->count<2||e->count>RT_FRAMES||e->pre>RT_PRE||e->pre>=e->count||(e->flags&~(RT_CLIPPED|RT_CONFIRMED_END))||((e->flags&RT_CONFIRMED_END)&&!(e->flags&RT_CLIPPED)))return false;
 bool active=false;for(unsigned i=0;i<e->count;i++){if(!rt_frame_valid(&e->frames[i]))return false;if(i&&(e->frames[i].timestamp_ms<=e->frames[i-1].timestamp_ms||e->frames[i].timestamp_ms-e->frames[i-1].timestamp_ms>RT_DELTA_MAX_MS))return false;if(i>=e->pre)active|=!!(e->frames[i].flags&RT_ACTIVE);}if(!active)return false;
 rt_summary summary;rt_summary_make(e,&summary);
 return summary.onset==e->onset&&summary.end==e->end&&summary.impacts==e->impacts&&summary.duration_ms==e->duration_ms&&summary.attack_ms==e->attack_ms&&summary.decay_ms==e->decay_ms&&summary.peak_coord==e->peak_coord&&summary.peak_db==e->peak_db&&!memcmp(summary.impact_at,e->impact_at,8);
}
static inline unsigned rt_example_count(const rt_label *l,unsigned kind){unsigned n=0;for(unsigned i=0;i<RT_EXAMPLES;i++)if(l->examples[i].id&&(!kind||l->examples[i].kind==kind))++n;return n;}
static inline bool rt_label_valid(const rt_label *l){
 if(!l||l->shift_limit>RT_MAX_SHIFT)return false;
 if(!l->present){if(l->name[0]||l->next_id||l->shift_limit)return false;for(unsigned i=0;i<RT_EXAMPLES;i++)if(l->examples[i].id)return false;return true;}
 if(!rf_signature_name_valid(l->name)||!l->next_id)return false;
 for(unsigned i=0;i<RT_EXAMPLES;i++)if(l->examples[i].id){if(!rt_example_valid(&l->examples[i])||l->examples[i].id>=l->next_id)return false;for(unsigned j=0;j<i;j++)if(l->examples[i].id==l->examples[j].id)return false;}
 return true;
}
/* Segmentation is bounded and used for both learning and recognition. A limit
 * produces an explicitly clipped review window; no saved sample is evicted. */
typedef struct {rt_frame pre[RT_PRE];unsigned next,used,quiet;bool collecting,ready;rt_example event;} rt_segmenter;
static inline void rt_segment_reset(rt_segmenter *s){memset(s,0,sizeof(*s));}
static inline void rt_segment_observe(rt_segmenter *s,const rt_frame *f){
 if(!s||!f||s->ready||!rt_frame_valid(f))return;
 if(s->used){const rt_frame *last=&s->pre[(s->next+RT_PRE-1u)%RT_PRE];
  if(f->timestamp_ms<=last->timestamp_ms||f->timestamp_ms-last->timestamp_ms>RT_DELTA_MAX_MS){
   s->next=s->used=0;
   if(s->collecting){s->event.flags|=RT_CLIPPED;rt_summarize(&s->event);s->ready=true;s->collecting=false;return;}
  }
 }
 if(!s->collecting&&(f->flags&RT_ACTIVE)){
  memset(&s->event,0,sizeof(s->event));s->event.kind=RT_POSITIVE;s->event.id=1;s->event.pre=(uint8_t)s->used;
  for(unsigned i=0;i<s->used;i++)s->event.frames[s->event.count++]=s->pre[(s->next+RT_PRE-s->used+i)%RT_PRE];
  s->collecting=true;s->quiet=0;
 }
 if(s->collecting){
  if(s->event.count<RT_FRAMES)s->event.frames[s->event.count++]=*f;
  s->quiet=f->flags&RT_ACTIVE?0:s->quiet+1;
  if(s->quiet>=4||s->event.count==RT_FRAMES){if(s->event.count==RT_FRAMES&&s->quiet<4)s->event.flags|=RT_CLIPPED;rt_summarize(&s->event);s->ready=true;s->collecting=false;}
 }
 s->pre[s->next]=*f;s->next=(s->next+1u)%RT_PRE;if(s->used<RT_PRE)++s->used;
}
static inline void rt_segment_rearm(rt_segmenter *s){s->ready=s->collecting=false;s->quiet=0;memset(&s->event,0,sizeof(s->event));}
static inline void rt_segment_stop(rt_segmenter *s){if(s->collecting){if(s->event.count<2||s->event.count<=s->event.pre)return;s->event.flags|=RT_CLIPPED;rt_summarize(&s->event);s->ready=true;s->collecting=false;}}
static inline unsigned rt_abs(int v){return (unsigned)(v<0?-v:v);}
static inline unsigned rt_shape_distance(const rt_frame *a,const rt_frame *b,int shift){unsigned difference=0,total=0;for(int k=-RT_MAX_SHIFT;k<64+RT_MAX_SHIFT;k++){unsigned na=rt_nibble(a,k),nb=rt_nibble(b,k-shift);unsigned aa=na?(1u<<na):0,bb=nb?(1u<<nb):0;difference+=rt_abs((int)aa-(int)bb);total+=aa+bb;}return total?difference*1000u/total:0;}
static inline unsigned rt_frame_distance(const rt_frame *a,const rt_frame *b,int shift,int a_peak,int b_peak){
 unsigned shape=rt_shape_distance(a,b,shift);unsigned envelope=rt_abs((a->level_db-a_peak)-(b->level_db-b_peak))/6u;if(envelope>1000)envelope=1000;unsigned flux=rt_abs((int)a->flux-b->flux)*1000u/255u;
 unsigned gap=((a->flags^b->flags)&RT_GAP_BEFORE)?1000u:0u;return (shape*6u+envelope*2u+flux+gap)/10u;
}
static inline rt_frame rt_average_shape(const rt_example *e){
 rt_frame f={0};uint32_t sums[64]={0},peak=0;
 for(unsigned band=0;band<64;band++){for(unsigned i=e->pre;i<e->count;i++)if(e->frames[i].flags&RT_ACTIVE){unsigned n=rt_nibble(&e->frames[i],(int)band);if(n)sums[band]+=1u<<n;}if(sums[band]>peak)peak=sums[band];}
 if(peak)for(unsigned band=0;band<64;band++)if(sums[band]){int n=15-(rf_dsp_log2_q16(peak)-rf_dsp_log2_q16(sums[band])+65535)/65536;if(n>0)rt_set_nibble(&f,band,(unsigned)n);}
 return f;
}
static inline int rt_choose_shift(const rt_example *observed,const rt_example *saved,unsigned limit,unsigned *distance){
 rt_frame a=rt_average_shape(observed),b=rt_average_shape(saved);int best=0;*distance=rt_shape_distance(&a,&b,0);
 for(int magnitude=1;magnitude<=(int)limit;magnitude++)for(int sign=-1;sign<=1;sign+=2){int shift=magnitude*sign;unsigned d=rt_shape_distance(&a,&b,shift)+(unsigned)magnitude*15u;if(d<*distance){best=shift;*distance=d;}}
 return best;
}
static inline unsigned rt_morphology_cost(const rt_example *a,const rt_example *b){
 unsigned duration_a=a->duration_ms,duration_b=b->duration_ms;if(!duration_a||!duration_b)return 1000;
 unsigned attack=rt_abs((int)(a->attack_ms*1000u/duration_a)-(int)(b->attack_ms*1000u/duration_b));unsigned decay=rt_abs((int)(a->decay_ms*1000u/duration_a)-(int)(b->decay_ms*1000u/duration_b));unsigned impacts=rt_abs((int)a->impacts-b->impacts)*160u;if(impacts>1000)impacts=1000;
 unsigned spacing=0,n=a->impacts<b->impacts?a->impacts:b->impacts;if(n>8)n=8;
 for(unsigned i=1;i<n;i++){unsigned aa=rt_elapsed(a,a->impact_at[i-1],a->impact_at[i])*1000u/duration_a,bb=rt_elapsed(b,b->impact_at[i-1],b->impact_at[i])*1000u/duration_b;spacing+=rt_abs((int)aa-(int)bb);}if(n>1)spacing/=n-1;if(spacing>1000)spacing=1000;
 return (attack+decay+impacts+spacing)/4u;
}
static inline unsigned rt_peak_phase(const rt_example *e){unsigned peak=e->onset;for(unsigned i=e->onset;i<=e->end;i++)if(e->frames[i].level_db>e->frames[peak].level_db)peak=i;return rt_elapsed(e,e->onset,peak)*1000u/e->duration_ms;}
static inline bool rt_tonal_example(const rt_example *e){unsigned count=0,active=0;for(unsigned i=e->pre;i<e->count;i++)if(e->frames[i].flags&RT_ACTIVE){active++;if(e->frames[i].flags&RT_TONAL)count++;}return active&&count*3u>=active;}
static inline uint16_t rt_frequency_evidence(const rt_example *e){
 uint16_t values[RT_FRAMES];unsigned count=0;for(unsigned i=e->pre;i<e->count;i++)if((e->frames[i].flags&(RT_ACTIVE|RT_TONAL))==(RT_ACTIVE|RT_TONAL)){unsigned at=count++;while(at&&values[at-1]>e->frames[i].peak_coord){values[at]=values[at-1];at--;}values[at]=e->frames[i].peak_coord;}
 return count?values[count/2u]:e->peak_coord;
}
static inline bool rt_absolute_frequency_compatible(const rt_example *a,const rt_example *b,unsigned limit){
 if(!rt_tonal_example(a)||!rt_tonal_example(b))return true;
 unsigned aa=rt_frequency_evidence(a),bb=rt_frequency_evidence(b);
 /* A shift is a whole linear passband band (1024 coordinates), with one
  * canonical two-FFT-bin measurement allowance. No relative-Hz tolerance. */
 return aa&&bb&&(aa>bb?aa-bb:bb-aa)<=limit*1024u+512u;
}
static inline bool rt_partial(const rt_example *e){return (e->flags&RT_CLIPPED)&&!(e->flags&RT_CONFIRMED_END);}
/* Incremental constrained DTW: only (1,1),(1,2),(2,1) steps, +/-4-column
 * corridor around proportional time. At most2x speed change, no unrestricted
 * path stretching. Caller budgets rows and keeps RF/UI polling between them. */
typedef struct {uint32_t row[3][RT_FRAMES+1];unsigned i,rows,cols;int shift;bool done;uint32_t cells;unsigned cost;} rt_dtw;
#define RT_INF 100000000u
static inline bool rt_step_time_valid(const rt_example *a,const rt_example *b,unsigned i,unsigned j,unsigned da,unsigned db){
 if(i<=da||j<=db)return i==da&&j==db;
 unsigned at=a->onset+i-1u,bt=b->onset+j-1u;
 unsigned ad=rt_elapsed(a,at-da,at),bd=rt_elapsed(b,bt-db,bt);
 return ad&&bd&&ad<=2u*bd&&bd<=2u*ad;
}
static inline bool rt_dtw_start(rt_dtw *d,const rt_example *a,const rt_example *b,unsigned shift_limit){
 memset(d,0,sizeof(*d));if(!a||!b||!rt_example_valid(a)||!rt_example_valid(b)){d->done=true;d->cost=1000;return false;}d->rows=(unsigned)a->end-a->onset+1u;d->cols=(unsigned)b->end-b->onset+1u;
 if(a->count>64||b->count>64||a->pre>=a->count||b->pre>=b->count||a->onset>a->end||b->onset>b->end||a->end>=a->count||b->end>=b->count||shift_limit>2||(!rt_partial(a)&&!rt_partial(b)&&a->impacts&&b->impacts&&a->impacts!=b->impacts)||!rt_absolute_frequency_compatible(a,b,shift_limit)||rt_abs((int)rt_peak_phase(a)-(int)rt_peak_phase(b))>400u||!a->duration_ms||!b->duration_ms||a->duration_ms>2u*b->duration_ms||b->duration_ms>2u*a->duration_ms||!d->rows||!d->cols||d->rows>d->cols*2u||d->cols>d->rows*2u||(rt_partial(a)!=rt_partial(b))){d->done=true;d->cost=1000;return false;}
 unsigned coarse;d->shift=rt_choose_shift(a,b,shift_limit,&coarse);if(coarse>500){d->done=true;d->cost=1000;return false;}
 for(unsigned r=0;r<3;r++)for(unsigned j=0;j<=RT_FRAMES;j++)d->row[r][j]=RT_INF;
 d->row[0][0]=0;d->i=1;return true;
}
static inline void rt_dtw_step(rt_dtw *d,const rt_example *a,const rt_example *b,unsigned budget){
 while(!d->done&&budget--){unsigned i=d->i,r=i%3;for(unsigned j=0;j<=d->cols;j++)d->row[r][j]=RT_INF;unsigned center=(i*d->cols+d->rows/2u)/d->rows,low=center>RT_DTW_RADIUS?center-RT_DTW_RADIUS:1u,high=center+RT_DTW_RADIUS;if(low<1)low=1;if(high>d->cols)high=d->cols;
  for(unsigned j=low;j<=high;j++){
   unsigned cost=rt_frame_distance(&a->frames[a->onset+i-1u],&b->frames[b->onset+j-1u],d->shift,a->peak_db,b->peak_db);
   uint32_t best=rt_step_time_valid(a,b,i,j,1u,1u)?d->row[(i+2u)%3][j-1u]:RT_INF;best=best>=RT_INF?RT_INF:best+cost*2u;
   if(j>=2&&rt_step_time_valid(a,b,i,j,1u,2u)){unsigned previous=rt_frame_distance(&a->frames[a->onset+i-1u],&b->frames[b->onset+j-2u],d->shift,a->peak_db,b->peak_db);uint32_t candidate=d->row[(i+2u)%3][j-2u];if(candidate<RT_INF){candidate+=(cost+previous)*3u/2u;if(candidate<best)best=candidate;}}
   if(i>=2&&rt_step_time_valid(a,b,i,j,2u,1u)){unsigned previous=rt_frame_distance(&a->frames[a->onset+i-2u],&b->frames[b->onset+j-1u],d->shift,a->peak_db,b->peak_db);uint32_t candidate=d->row[(i+1u)%3][j-1u];if(candidate<RT_INF){candidate+=(cost+previous)*3u/2u;if(candidate<best)best=candidate;}}
   d->row[r][j]=best;d->cells++;
  }
  if(i==d->rows){uint32_t sum=d->row[r][d->cols];unsigned divisor=d->rows+d->cols;unsigned average=sum>=RT_INF?1000:sum/divisor;d->cost=(average*8u+rt_morphology_cost(a,b)*2u)/10u+rt_abs(d->shift)*15u+(rt_tonal_example(a)!=rt_tonal_example(b)?100u:0u);if(d->cost>1000)d->cost=1000;d->done=true;}else ++d->i;
 }
}

enum {RT_RESULT_UNKNOWN,RT_RESULT_MATCH,RT_RESULT_AMBIGUOUS,RT_RESULT_NEGATIVE};
typedef struct {
 rt_example query;rt_dtw work;
 unsigned label,example,positive[8],negative[8],compared,cells,work_units;
 int shift[8];uint16_t reference_coord[8],observed_coord;uint32_t example_id[8];
 rf_capture_identity identity;uint32_t generation[2];bool bound;
 bool running,complete,work_started;uint8_t allowed_labels,excluded_examples[RT_LABELS];int selected;unsigned reason,score;
} rt_matcher;
static inline void rt_match_begin(rt_matcher *m,const rt_example *query){memset(m,0,sizeof(*m));m->selected=-1;if(!query||!rt_example_valid(query)){m->complete=true;return;}m->query=*query;m->observed_coord=rt_frequency_evidence(query);m->running=true;m->allowed_labels=255;}
static inline void rt_match_finish(rt_matcher *m){
 unsigned best=0,second=0;int selected=-1;
 for(unsigned i=0;i<8;i++){if(m->positive[i]>best){second=best;best=m->positive[i];selected=(int)i;}else if(m->positive[i]>second)second=m->positive[i];}
 m->score=best;m->reason=RT_RESULT_UNKNOWN;
 if(best>=RT_MATCH_MIN){if(best-second<RT_MATCH_MARGIN)m->reason=RT_RESULT_AMBIGUOUS;else if(m->negative[selected]+60u>=best)m->reason=RT_RESULT_NEGATIVE;else{m->reason=RT_RESULT_MATCH;m->selected=selected;}}
 m->running=false;m->complete=true;
}
/* A tick must offer at least8 work units; coarse preparation is atomic and
 * charged8, while each DTW row costs1. The app offers64. Unused tail budget
 * is returned before starting another template, never hidden as setup debt. */
static inline void rt_match_tick(rt_matcher *m,const rt_library *library,unsigned budget){
 if(!m||!library||!m->running||budget<8u)return;
 if(!rf_identity_valid(&library->identity)||(m->bound&&(!rf_identity_equal(&m->identity,&library->identity)||memcmp(m->generation,library->generation,sizeof(m->generation))))){m->selected=-1;m->reason=RT_RESULT_UNKNOWN;m->score=0;m->running=false;m->complete=true;return;}
 if(!m->bound){m->identity=library->identity;memcpy(m->generation,library->generation,sizeof(m->generation));m->bound=true;}
 while(m->label<RT_LABELS&&budget){const rt_label *label=&library->labels[m->label];
  if(!(m->allowed_labels&(1u<<m->label))||!label->present||m->example==RT_EXAMPLES){m->label++;m->example=0;m->work_started=false;continue;}
  const rt_example *example=&label->examples[m->example];if(!example->id||(m->excluded_examples[m->label]&(1u<<m->example))){m->example++;continue;}
  if(!m->work_started){if(budget<8u)return;(void)rt_dtw_start(&m->work,&m->query,example,label->shift_limit);m->work_started=true;budget-=8u;m->work_units+=8u;if(!budget&&!m->work.done)return;}
  unsigned before=m->work.i;rt_dtw_step(&m->work,&m->query,example,budget);unsigned used=m->work.i-before+(m->work.done?1u:0u);if(!used)used=1;unsigned charged=used>budget?budget:used;m->work_units+=charged;budget=used>=budget?0:budget-used;
  if(!m->work.done)return;
  unsigned score=1000u-m->work.cost;m->cells+=m->work.cells;m->compared++;
  if(example->kind==RT_POSITIVE&&score>m->positive[m->label]){m->positive[m->label]=score;m->shift[m->label]=m->work.shift;m->reference_coord[m->label]=rt_frequency_evidence(example);m->example_id[m->label]=example->id;}
  else if(example->kind==RT_NEGATIVE&&score>m->negative[m->label])m->negative[m->label]=score;
  m->example++;m->work_started=false;
 }
 if(m->label==RT_LABELS)rt_match_finish(m);
}
#endif
