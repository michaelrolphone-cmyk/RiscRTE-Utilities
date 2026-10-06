#ifndef SPECTRUM_TEMPORAL_H
#define SPECTRUM_TEMPORAL_H
#include "spectrum_background.h"
#define ST_LABELS 8u
#define ST_EXAMPLES 6u
#define ST_FRAMES 64u
#define ST_PRE 4u
#define ST_FRAME_MS 64u
#define ST_BANDS 64u
#define ST_POSITIVE 1u
#define ST_NEGATIVE 2u
#define ST_CLIPPED 1u
#define ST_CONFIRMED_END 2u
#define ST_ACTIVE 1u
#define ST_TONAL 2u
#define ST_MAX_SHIFT 2
#define ST_MATCH_MIN 800u
#define ST_MATCH_MARGIN 80u
#define ST_DTW_RADIUS 4u
/* 64 log-frequency bands, eight per octave over 31.25..8000 Hz. Each nibble
 * retains relative band power at approximately 3 dB steps. Absolute residual
 * level and dominant frequency remain separate evidence, never discarded. */
typedef struct {uint8_t shape[32];int16_t level_db;uint16_t peak_hz;uint8_t flux,flags;} st_frame;
typedef struct {
 uint32_t id;
 uint8_t count,pre,kind,flags,onset,end,impacts,impact_at[8];
 uint16_t duration_ms,attack_ms,decay_ms,peak_hz;
 int16_t peak_db;
 st_frame frames[ST_FRAMES];
} st_example;
typedef struct {bool present;uint8_t shift_limit;char name[17];uint32_t next_id;st_example examples[ST_EXAMPLES];} st_label;
typedef struct {st_label labels[ST_LABELS];uint32_t generation[2];} st_library;
_Static_assert(sizeof(st_frame)==38,"fixed temporal column budget");
_Static_assert(sizeof(st_library)<120000,"bounded example RAM");
static inline unsigned st_nibble(const st_frame *f,int band){if(band<0||band>=64)return 0;return (f->shape[(unsigned)band/2u]>>((band&1)*4))&15u;}
static inline void st_set_nibble(st_frame *f,unsigned band,unsigned value){unsigned shift=(band&1)*4;f->shape[band/2]=(uint8_t)((f->shape[band/2]&~(15u<<shift))|(value<<shift));}
static inline unsigned st_log_band(unsigned linear){int32_t log=spectrum_dsp_log2_q16(1500u+2000u*linear)-spectrum_dsp_log2_q16(1000u);unsigned index=(unsigned)((log*8+32768)/65536);return index<64?index:63;}
static inline st_frame st_frame_make(const uint32_t excess[128],const uint32_t previous[128],bool active){
 st_frame f={0};uint64_t bands[64]={0},total=0,positive=0,peak=0;unsigned peak_linear=0;
 for(unsigned i=0;i<128;i++){bands[st_log_band(i)]+=excess[i];total+=excess[i];if(excess[i]>previous[i])positive+=excess[i]-previous[i];if(excess[i]>excess[peak_linear])peak_linear=i;}
 for(unsigned i=0;i<64;i++)if(bands[i]>peak)peak=bands[i];
 f.level_db=spectrum_background_db(total);f.peak_hz=peak?(uint16_t)((1500u+2000u*peak_linear+16u)/32u):0;f.flags=active?ST_ACTIVE:0;
 uint64_t flux_total=total,flux_positive=positive>total?total:positive;while(flux_total>UINT32_MAX){flux_total>>=1;flux_positive>>=1;}
 uint64_t localized=0,other=0;for(unsigned i=peak_linear?peak_linear-1:0;i<128&&i<=peak_linear+1;i++)localized+=excess[i];
 for(unsigned i=0;i<128;i++)if(i+2u<peak_linear||i>peak_linear+2u){uint64_t sum=0;for(unsigned j=i?i-1u:0;j<128&&j<=i+1u;j++)sum+=excess[j];if(sum>other)other=sum;}
 if(total&&localized*3u>=total&&(uint64_t)excess[peak_linear]*8u>=total&&localized>=other*2u)f.flags|=ST_TONAL;
 f.flux=flux_total?(uint8_t)spectrum_dsp_div_u64_u32(flux_positive*255u,(uint32_t)flux_total):0;
 unsigned scale=0;while(peak>UINT32_MAX){peak>>=1;++scale;}
 if(peak)for(unsigned i=0;i<64;i++){uint32_t p=(uint32_t)(bands[i]>>scale);if(!p)continue;int level=15-(spectrum_dsp_log2_q16((uint32_t)peak)-spectrum_dsp_log2_q16(p)+65535)/65536;if(level<0)level=0;if(level>15)level=15;st_set_nibble(&f,i,(unsigned)level);}
 return f;
}
static inline bool st_frame_valid(const st_frame *f){
 if(!f||f->level_db< -12000||f->level_db>1000||f->peak_hz>8000||(f->flags&~(ST_ACTIVE|ST_TONAL)))return false;
 unsigned peak=0;for(unsigned i=0;i<64;i++){unsigned n=st_nibble(f,(int)i);if(n>peak)peak=n;}
 if(!f->peak_hz)return !peak&&f->level_db==-12000&&!f->flux&&!f->flags;
 return peak==15&&f->level_db> -12000;
}
static inline void st_summarize(st_example *e){
 e->peak_db=-12000;e->peak_hz=0;e->onset=e->pre;e->end=e->count?e->count-1:0;e->impacts=0;memset(e->impact_at,0,sizeof(e->impact_at));unsigned peak_index=e->pre,last_impact=0;bool have_impact=false;
 for(unsigned i=e->pre;i<e->count;i++){if(e->frames[i].level_db>e->peak_db){e->peak_db=e->frames[i].level_db;e->peak_hz=e->frames[i].peak_hz;peak_index=i;}if((e->frames[i].flags&ST_ACTIVE)&&e->frames[i].flux>=180&&(!have_impact||i>=last_impact+2u)){last_impact=i;have_impact=true;if(e->impacts<8)e->impact_at[e->impacts]=(uint8_t)i;if(e->impacts<255)++e->impacts;}}
 while(e->onset<e->count&&!(e->frames[e->onset].flags&ST_ACTIVE))++e->onset;
 if(e->onset>=e->count)e->onset=e->pre;
 while(e->end>e->onset&&!(e->frames[e->end].flags&ST_ACTIVE))--e->end;
 unsigned lo=e->onset,hi=e->onset,last_hi=peak_index,last_lo=peak_index;
 for(unsigned i=e->onset;i<=peak_index;i++){if(e->frames[i].level_db<e->peak_db-700)lo=i;if(e->frames[i].level_db<e->peak_db-100)hi=i;}
 for(unsigned i=peak_index;i<=e->end;i++){if(e->frames[i].level_db>=e->peak_db-100)last_hi=i;if(e->frames[i].level_db>=e->peak_db-700)last_lo=i;}
 e->duration_ms=(uint16_t)((e->end-e->onset+1u)*ST_FRAME_MS);e->attack_ms=(uint16_t)((hi>=lo?hi-lo:0)*ST_FRAME_MS);e->decay_ms=(uint16_t)((last_lo>=last_hi?last_lo-last_hi:0)*ST_FRAME_MS);
}
static inline bool st_example_valid(const st_example *e){
 if(!e||!e->id||e->kind<1||e->kind>2||e->count<2||e->count>ST_FRAMES||e->pre>ST_PRE||e->pre>=e->count||(e->flags&~(ST_CLIPPED|ST_CONFIRMED_END))||((e->flags&ST_CONFIRMED_END)&&!(e->flags&ST_CLIPPED)))return false;
 bool active=false;for(unsigned i=0;i<e->count;i++){if(!st_frame_valid(&e->frames[i]))return false;active|=!!(e->frames[i].flags&ST_ACTIVE);}if(!active)return false;
 st_example copy=*e;st_summarize(&copy);
 return copy.onset==e->onset&&copy.end==e->end&&copy.impacts==e->impacts&&copy.duration_ms==e->duration_ms&&copy.attack_ms==e->attack_ms&&copy.decay_ms==e->decay_ms&&copy.peak_hz==e->peak_hz&&copy.peak_db==e->peak_db&&!memcmp(copy.impact_at,e->impact_at,8);
}
static inline unsigned st_example_count(const st_label *l,unsigned kind){unsigned n=0;for(unsigned i=0;i<ST_EXAMPLES;i++)if(l->examples[i].id&&(!kind||l->examples[i].kind==kind))++n;return n;}
static inline bool st_label_valid(const st_label *l){
 if(!l||l->shift_limit>ST_MAX_SHIFT)return false;
 if(!l->present){if(l->name[0]||l->next_id||l->shift_limit)return false;for(unsigned i=0;i<ST_EXAMPLES;i++)if(l->examples[i].id)return false;return true;}
 if(!spectrum_signature_name_valid(l->name)||!l->next_id)return false;
 for(unsigned i=0;i<ST_EXAMPLES;i++)if(l->examples[i].id){if(!st_example_valid(&l->examples[i])||l->examples[i].id>=l->next_id)return false;for(unsigned j=0;j<i;j++)if(l->examples[i].id==l->examples[j].id)return false;}
 return true;
}
/* Segmentation is bounded and used for both learning and recognition. A limit
 * produces an explicitly clipped review window; no saved sample is evicted. */
typedef struct {st_frame pre[ST_PRE];unsigned next,used,quiet;bool collecting,ready;st_example event;} st_segmenter;
static inline void st_segment_reset(st_segmenter *s){memset(s,0,sizeof(*s));}
static inline void st_segment_observe(st_segmenter *s,const st_frame *f){
 if(s->ready)return;
 if(!s->collecting&&(f->flags&ST_ACTIVE)){
  memset(&s->event,0,sizeof(s->event));s->event.kind=ST_POSITIVE;s->event.id=1;s->event.pre=(uint8_t)s->used;
  for(unsigned i=0;i<s->used;i++)s->event.frames[s->event.count++]=s->pre[(s->next+ST_PRE-s->used+i)%ST_PRE];
  s->collecting=true;s->quiet=0;
 }
 if(s->collecting){
  if(s->event.count<ST_FRAMES)s->event.frames[s->event.count++]=*f;
  s->quiet=f->flags&ST_ACTIVE?0:s->quiet+1;
  if(s->quiet>=4||s->event.count==ST_FRAMES){if(s->event.count==ST_FRAMES&&s->quiet<4)s->event.flags|=ST_CLIPPED;st_summarize(&s->event);s->ready=true;s->collecting=false;}
 }
 s->pre[s->next]=*f;s->next=(s->next+1u)%ST_PRE;if(s->used<ST_PRE)++s->used;
}
static inline void st_segment_rearm(st_segmenter *s){s->ready=s->collecting=false;s->quiet=0;memset(&s->event,0,sizeof(s->event));}
static inline void st_segment_stop(st_segmenter *s){if(s->collecting){if(s->event.count<2||s->event.count<=s->event.pre)return;s->event.flags|=ST_CLIPPED;st_summarize(&s->event);s->ready=true;s->collecting=false;}}
static inline unsigned st_abs(int v){return (unsigned)(v<0?-v:v);}
static inline unsigned st_shape_distance(const st_frame *a,const st_frame *b,int shift){unsigned difference=0,total=0;for(int k=-ST_MAX_SHIFT;k<64+ST_MAX_SHIFT;k++){unsigned na=st_nibble(a,k),nb=st_nibble(b,k-shift);unsigned aa=na?(1u<<na):0,bb=nb?(1u<<nb):0;difference+=st_abs((int)aa-(int)bb);total+=aa+bb;}return total?difference*1000u/total:0;}
static inline unsigned st_frame_distance(const st_frame *a,const st_frame *b,int shift,int a_peak,int b_peak){
 unsigned shape=st_shape_distance(a,b,shift);unsigned envelope=st_abs((a->level_db-a_peak)-(b->level_db-b_peak))/6u;if(envelope>1000)envelope=1000;unsigned flux=st_abs((int)a->flux-b->flux)*1000u/255u;
 return (shape*6u+envelope*3u+flux)/10u;
}
static inline st_frame st_average_shape(const st_example *e){
 st_frame f={0};uint32_t sums[64]={0},peak=0;
 for(unsigned band=0;band<64;band++){for(unsigned i=e->pre;i<e->count;i++)if(e->frames[i].flags&ST_ACTIVE){unsigned n=st_nibble(&e->frames[i],(int)band);if(n)sums[band]+=1u<<n;}if(sums[band]>peak)peak=sums[band];}
 if(peak)for(unsigned band=0;band<64;band++)if(sums[band]){int n=15-(spectrum_dsp_log2_q16(peak)-spectrum_dsp_log2_q16(sums[band])+65535)/65536;if(n>0)st_set_nibble(&f,band,(unsigned)n);}
 return f;
}
static inline int st_choose_shift(const st_example *observed,const st_example *saved,unsigned limit,unsigned *distance){
 st_frame a=st_average_shape(observed),b=st_average_shape(saved);int best=0;*distance=st_shape_distance(&a,&b,0);
 for(int magnitude=1;magnitude<=(int)limit;magnitude++)for(int sign=-1;sign<=1;sign+=2){int shift=magnitude*sign;unsigned d=st_shape_distance(&a,&b,shift)+(unsigned)magnitude*15u;if(d<*distance){best=shift;*distance=d;}}
 return best;
}
static inline unsigned st_morphology_cost(const st_example *a,const st_example *b){
 unsigned duration_a=a->duration_ms,duration_b=b->duration_ms;if(!duration_a||!duration_b)return 1000;
 unsigned attack=st_abs((int)(a->attack_ms*1000u/duration_a)-(int)(b->attack_ms*1000u/duration_b));unsigned decay=st_abs((int)(a->decay_ms*1000u/duration_a)-(int)(b->decay_ms*1000u/duration_b));unsigned impacts=st_abs((int)a->impacts-b->impacts)*160u;if(impacts>1000)impacts=1000;
 unsigned spacing=0,n=a->impacts<b->impacts?a->impacts:b->impacts;if(n>8)n=8;
 for(unsigned i=1;i<n;i++){unsigned aa=(a->impact_at[i]-a->impact_at[i-1])*64000u/duration_a,bb=(b->impact_at[i]-b->impact_at[i-1])*64000u/duration_b;spacing+=st_abs((int)aa-(int)bb);}if(n>1)spacing/=n-1;if(spacing>1000)spacing=1000;
 return (attack+decay+impacts+spacing)/4u;
}
static inline unsigned st_peak_phase(const st_example *e){unsigned peak=e->onset;for(unsigned i=e->onset;i<=e->end;i++)if(e->frames[i].level_db>e->frames[peak].level_db)peak=i;return (peak-e->onset)*1000u/(e->end-e->onset+1u);}
static inline bool st_tonal_example(const st_example *e){unsigned count=0,active=0;for(unsigned i=e->pre;i<e->count;i++)if(e->frames[i].flags&ST_ACTIVE){active++;if(e->frames[i].flags&ST_TONAL)count++;}return active&&count*3u>=active;}
static inline uint16_t st_frequency_evidence(const st_example *e){
 uint16_t values[ST_FRAMES];unsigned count=0;for(unsigned i=e->pre;i<e->count;i++)if((e->frames[i].flags&(ST_ACTIVE|ST_TONAL))==(ST_ACTIVE|ST_TONAL)){unsigned at=count++;while(at&&values[at-1]>e->frames[i].peak_hz){values[at]=values[at-1];at--;}values[at]=e->frames[i].peak_hz;}
 return count?values[count/2u]:e->peak_hz;
}
static inline bool st_absolute_frequency_compatible(const st_example *a,const st_example *b,unsigned limit){
 bool ta=st_tonal_example(a),tb=st_tonal_example(b);if(!ta||!tb)return true;
 unsigned aa=st_frequency_evidence(a),bb=st_frequency_evidence(b);unsigned low=aa<bb?aa:bb,high=aa>bb?aa:bb;
 uint32_t ratio=spectrum_dsp_exp2_q16((int32_t)limit*65536/8);
 /* Canonical two-bin power bands have 62.5Hz spacing. Retain that honest
  * measurement allowance even with shift disabled; never allow octave jumps. */
 return low&&high<=((uint64_t)low*ratio>>16)+63u;
}
static inline bool st_partial(const st_example *e){return (e->flags&ST_CLIPPED)&&!(e->flags&ST_CONFIRMED_END);}
/* Incremental constrained DTW: only (1,1),(1,2),(2,1) steps, +/-4-column
 * corridor around proportional time. At most2x speed change, no unrestricted
 * path stretching. Caller budgets rows and keeps audio/UI polling between them. */
typedef struct {uint32_t row[3][ST_FRAMES+1];unsigned i,rows,cols;int shift;bool done;uint32_t cells;unsigned cost;} st_dtw;
#define ST_INF 100000000u
static inline bool st_dtw_start(st_dtw *d,const st_example *a,const st_example *b,unsigned shift_limit){
 memset(d,0,sizeof(*d));d->rows=(unsigned)a->end-a->onset+1u;d->cols=(unsigned)b->end-b->onset+1u;
 if(a->count>64||b->count>64||a->pre>=a->count||b->pre>=b->count||a->onset>a->end||b->onset>b->end||a->end>=a->count||b->end>=b->count||shift_limit>2||(!st_partial(a)&&!st_partial(b)&&a->impacts&&b->impacts&&a->impacts!=b->impacts)||!st_absolute_frequency_compatible(a,b,shift_limit)||st_abs((int)st_peak_phase(a)-(int)st_peak_phase(b))>400u||!d->rows||!d->cols||d->rows>d->cols*2u||d->cols>d->rows*2u||(st_partial(a)!=st_partial(b))){d->done=true;d->cost=1000;return false;}
 unsigned coarse;d->shift=st_choose_shift(a,b,shift_limit,&coarse);if(coarse>500){d->done=true;d->cost=1000;return false;}
 for(unsigned r=0;r<3;r++)for(unsigned j=0;j<=ST_FRAMES;j++)d->row[r][j]=ST_INF;
 d->row[0][0]=0;d->i=1;return true;
}
static inline void st_dtw_step(st_dtw *d,const st_example *a,const st_example *b,unsigned budget){
 while(!d->done&&budget--){unsigned i=d->i,r=i%3;for(unsigned j=0;j<=d->cols;j++)d->row[r][j]=ST_INF;unsigned center=(i*d->cols+d->rows/2u)/d->rows,low=center>ST_DTW_RADIUS?center-ST_DTW_RADIUS:1u,high=center+ST_DTW_RADIUS;if(low<1)low=1;if(high>d->cols)high=d->cols;
  for(unsigned j=low;j<=high;j++){
   unsigned cost=st_frame_distance(&a->frames[a->onset+i-1u],&b->frames[b->onset+j-1u],d->shift,a->peak_db,b->peak_db);
   uint32_t best=d->row[(i+2u)%3][j-1u];best=best>=ST_INF?ST_INF:best+cost*2u;
   if(j>=2){unsigned previous=st_frame_distance(&a->frames[a->onset+i-1u],&b->frames[b->onset+j-2u],d->shift,a->peak_db,b->peak_db);uint32_t candidate=d->row[(i+2u)%3][j-2u];if(candidate<ST_INF){candidate+=(cost+previous)*3u/2u;if(candidate<best)best=candidate;}}
   if(i>=2){unsigned previous=st_frame_distance(&a->frames[a->onset+i-2u],&b->frames[b->onset+j-1u],d->shift,a->peak_db,b->peak_db);uint32_t candidate=d->row[(i+1u)%3][j-1u];if(candidate<ST_INF){candidate+=(cost+previous)*3u/2u;if(candidate<best)best=candidate;}}
   d->row[r][j]=best;d->cells++;
  }
  if(i==d->rows){uint32_t sum=d->row[r][d->cols];unsigned divisor=d->rows+d->cols;unsigned average=sum>=ST_INF?1000:sum/divisor;d->cost=(average*8u+st_morphology_cost(a,b)*2u)/10u+st_abs(d->shift)*15u+(st_tonal_example(a)!=st_tonal_example(b)?100u:0u);if(d->cost>1000)d->cost=1000;d->done=true;}else ++d->i;
 }
}

enum {ST_RESULT_UNKNOWN,ST_RESULT_MATCH,ST_RESULT_AMBIGUOUS,ST_RESULT_NEGATIVE};
typedef struct {
 st_example query;st_dtw work;
 unsigned label,example,positive[8],negative[8],compared,cells,work_units;
 int shift[8];uint16_t reference_hz[8],observed_hz;uint32_t example_id[8];
 bool running,complete,work_started;uint8_t allowed_labels;int selected;unsigned reason,score;
} st_matcher;
static inline void st_match_begin(st_matcher *m,const st_example *query){memset(m,0,sizeof(*m));m->selected=-1;if(!query||!st_example_valid(query)){m->complete=true;return;}m->query=*query;m->observed_hz=st_frequency_evidence(query);m->running=true;m->allowed_labels=255;}
static inline void st_match_finish(st_matcher *m){
 unsigned best=0,second=0;int selected=-1;
 for(unsigned i=0;i<8;i++){if(m->positive[i]>best){second=best;best=m->positive[i];selected=(int)i;}else if(m->positive[i]>second)second=m->positive[i];}
 m->score=best;m->reason=ST_RESULT_UNKNOWN;
 if(best>=ST_MATCH_MIN){if(best-second<ST_MATCH_MARGIN)m->reason=ST_RESULT_AMBIGUOUS;else if(m->negative[selected]+60u>=best)m->reason=ST_RESULT_NEGATIVE;else{m->reason=ST_RESULT_MATCH;m->selected=selected;}}
 m->running=false;m->complete=true;
}
/* A tick must offer at least8 work units; coarse preparation is atomic and
 * charged8, while each DTW row costs1. The app offers64. Unused tail budget
 * is returned before starting another template, never hidden as setup debt. */
static inline void st_match_tick(st_matcher *m,const st_library *library,unsigned budget){
 if(!m||!library||!m->running||budget<8u)return;
 while(m->label<ST_LABELS&&budget){const st_label *label=&library->labels[m->label];
  if(!(m->allowed_labels&(1u<<m->label))||!label->present||m->example==ST_EXAMPLES){m->label++;m->example=0;m->work_started=false;continue;}
  const st_example *example=&label->examples[m->example];if(!example->id){m->example++;continue;}
  if(!m->work_started){if(budget<8u)return;(void)st_dtw_start(&m->work,&m->query,example,label->shift_limit);m->work_started=true;budget-=8u;m->work_units+=8u;if(!budget&&!m->work.done)return;}
  unsigned before=m->work.i;st_dtw_step(&m->work,&m->query,example,budget);unsigned used=m->work.i-before+(m->work.done?1u:0u);if(!used)used=1;unsigned charged=used>budget?budget:used;m->work_units+=charged;budget=used>=budget?0:budget-used;
  if(!m->work.done)return;
  unsigned score=1000u-m->work.cost;m->cells+=m->work.cells;m->compared++;
  if(example->kind==ST_POSITIVE&&score>m->positive[m->label]){m->positive[m->label]=score;m->shift[m->label]=m->work.shift;m->reference_hz[m->label]=st_frequency_evidence(example);m->example_id[m->label]=example->id;}
  else if(example->kind==ST_NEGATIVE&&score>m->negative[m->label])m->negative[m->label]=score;
  m->example++;m->work_started=false;
 }
 if(m->label==ST_LABELS)st_match_finish(m);
}
#endif
