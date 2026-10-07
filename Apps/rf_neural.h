#ifndef RF_NEURAL_H
#define RF_NEURAL_H
#include "rf_temporal.h"

/* A bounded candidate, not an unconditional replacement classifier. It can
 * resolve only temporal ambiguities between sufficiently taught labels. The
 * existing shape/time/frequency gates remain mandatory. A confusing-negative
 * template can be distinguished only after explicit negative training and
 * both held-out and full-collection rejection checks. Insufficient evidence
 * always leaves the temporal answer intact.
 *
 * 64 mean linear-passband shape levels + 8 envelope samples + 8 temporal/frequency values.
 * The small hidden layer is deliberately evaluated against the simpler matcher
 * on examples excluded from BOTH training and reference matching. No IQ,
 * labels, examples or on-disk records are changed by training. */
#define RN_INPUTS 80u
#define RN_HIDDEN 16u
#define RN_EPOCHS 256u
#define RN_MIN_POSITIVES 3u
#define RN_MIN_NEGATIVES 2u
#define RN_THRESHOLD 0.75f
#define RN_MARGIN 0.20f
typedef struct {
 float input[RN_HIDDEN][RN_INPUTS],hidden_bias[RN_HIDDEN];
 float output[RT_LABELS][RN_HIDDEN],output_bias[RT_LABELS];
 float center[RN_INPUTS],scale[RN_INPUTS];
} rn_model;
enum {RN_NEEDS_EXAMPLES,RN_PREPARING,RN_TRAINING,RN_CHECKING,RN_BASELINE,RN_ACTIVE};
typedef struct {
 rn_model candidate,active;
 rt_matcher validation;
 float low[RN_INPUTS],high[RN_INPUTS];
 uint8_t held[RT_LABELS],eligible,calibrated;
 unsigned state,epoch,label,example,validation_label,validation_example;
 unsigned checks,baseline_correct,candidate_correct,regressions,updates,ticks;
 unsigned validation_pass,full_checks,full_baseline_correct,full_candidate_correct;
 bool has_active,validation_started;
 const rt_library *source;uint32_t source_generation[2];rf_capture_identity identity;
} rn_trainer;
_Static_assert(sizeof(rn_trainer)<19000,"bounded neural training and active model RAM");

static inline float rn_abs(float x){return x<0?-x:x;}
static inline float rn_limit(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static inline float rn_activation(float x){return x/(1.0f+rn_abs(x));}
static inline float rn_derivative(float value){float a=1.0f-rn_abs(value);return a*a;}
static inline void rn_features(const rt_example *e,float out[RN_INPUTS]){
 memset(out,0,sizeof(float)*RN_INPUTS);unsigned active=0;
 for(unsigned i=e->onset;i<=e->end;i++)if(e->frames[i].flags&RT_ACTIVE){
  ++active;for(unsigned b=0;b<RT_BANDS;b++)out[b]+=(float)rt_nibble(&e->frames[i],(int)b)/15.0f;
 }
 if(active)for(unsigned b=0;b<RT_BANDS;b++)out[b]/=(float)active;
 /* Empty elapsed-time buckets remain empty. Sparse observations are never
  * held across missing intervals or expanded into continuous RF occupancy. */
 unsigned counts[8]={0};
 for(unsigned j=e->onset;j<=e->end;j++){
  unsigned bucket=rt_elapsed(e,e->onset,j)*8u/e->duration_ms;if(bucket>7)bucket=7;
  out[64+bucket]+=rn_limit(1.0f+(float)(e->frames[j].level_db-e->peak_db)/4800.0f,0.0f,1.0f);++counts[bucket];
 }
 for(unsigned i=0;i<8;i++)if(counts[i])out[64+i]/=(float)counts[i];
 out[72]=(float)rf_dsp_log2_q16(e->duration_ms+1u)/(22.0f*65536.0f);
 out[73]=(float)e->attack_ms/(float)e->duration_ms;
 out[74]=(float)e->decay_ms/(float)e->duration_ms;
 out[75]=(float)(e->impacts>8?8:e->impacts)/8.0f;
 out[76]=(float)rt_peak_phase(e)/1000.0f;
 out[77]=(float)rt_frequency_evidence(e)/65535.0f;
 out[78]=rt_tonal_example(e)?1.0f:0.0f;
 out[79]=rt_partial(e)?1.0f:0.0f;
}
static inline void rn_predict(const rn_model *m,const float x[RN_INPUTS],float h[RN_HIDDEN],float y[RT_LABELS]){
 for(unsigned j=0;j<RN_HIDDEN;j++){float sum=m->hidden_bias[j];for(unsigned k=0;k<RN_INPUTS;k++)sum+=m->input[j][k]*rn_limit((x[k]-m->center[k])*m->scale[k],-2.0f,2.0f);h[j]=rn_activation(sum);}
 for(unsigned i=0;i<RT_LABELS;i++){float sum=m->output_bias[i];for(unsigned j=0;j<RN_HIDDEN;j++)sum+=m->output[i][j]*h[j];y[i]=0.5f+0.5f*rn_activation(sum);}
}
static inline void rn_seed(rn_model *m){
 memset(m,0,sizeof(*m));uint32_t seed=0x524e3031u;
 for(unsigned j=0;j<RN_HIDDEN;j++)for(unsigned k=0;k<RN_INPUTS;k++){seed=seed*1664525u+1013904223u;m->input[j][k]=((float)((seed>>16)&1023u)-511.5f)/4096.0f;}
 for(unsigned i=0;i<RT_LABELS;i++)for(unsigned j=0;j<RN_HIDDEN;j++){seed=seed*1664525u+1013904223u;m->output[i][j]=((float)((seed>>16)&1023u)-511.5f)/2048.0f;}
}
static inline void rn_train_one(rn_model *m,const rt_example *e,unsigned label,uint8_t eligible){
 float x[RN_INPUTS],h[RN_HIDDEN],y[RT_LABELS],delta[RT_LABELS]={0},hidden[RN_HIDDEN]={0};
 rn_features(e,x);rn_predict(m,x,h,y);
 for(unsigned k=0;k<RN_INPUTS;k++)x[k]=rn_limit((x[k]-m->center[k])*m->scale[k],-2.0f,2.0f);
 for(unsigned i=0;i<RT_LABELS;i++)if(eligible&(1u<<i)){
  /* A confusing example excludes its own label only. It is never silently
   * assigned to another label or treated as a global negative. */
  if(e->kind==RT_NEGATIVE&&i!=label)continue;
  float target=e->kind==RT_POSITIVE&&i==label?1.0f:0.0f;
  delta[i]=(target-y[i])*0.5f*rn_derivative(2.0f*y[i]-1.0f);
  for(unsigned j=0;j<RN_HIDDEN;j++)hidden[j]+=delta[i]*m->output[i][j];
 }
 const float rate=0.35f;
 for(unsigned j=0;j<RN_HIDDEN;j++){
  float update=rate*hidden[j]*rn_derivative(h[j]);
  m->hidden_bias[j]=rn_limit(m->hidden_bias[j]+update,-8.0f,8.0f);
  for(unsigned k=0;k<RN_INPUTS;k++)m->input[j][k]=rn_limit(m->input[j][k]+update*x[k],-8.0f,8.0f);
 }
 for(unsigned i=0;i<RT_LABELS;i++){
  m->output_bias[i]=rn_limit(m->output_bias[i]+rate*delta[i],-8.0f,8.0f);
  for(unsigned j=0;j<RN_HIDDEN;j++)m->output[i][j]=rn_limit(m->output[i][j]+rate*delta[i]*h[j],-8.0f,8.0f);
 }
}
static inline int rn_resolve(const rn_model *model,uint8_t eligible,const rt_matcher *m){
 if(!m->complete||(m->reason!=RT_RESULT_AMBIGUOUS&&m->reason!=RT_RESULT_NEGATIVE))return m->selected;
 if(!rt_example_valid(&m->query))return -1;
 unsigned candidates=0;for(unsigned i=0;i<RT_LABELS;i++)if(m->positive[i]>=RT_MATCH_MIN){if(!(eligible&(1u<<i)))return -1;++candidates;}
 if(candidates<(m->reason==RT_RESULT_AMBIGUOUS?2u:1u))return -1;
 float x[RN_INPUTS],h[RN_HIDDEN],y[RT_LABELS];rn_features(&m->query,x);rn_predict(model,x,h,y);
 float best=0,second=0;int selected=-1;
 for(unsigned i=0;i<RT_LABELS;i++)if(eligible&(1u<<i)){
  if(y[i]>best){second=best;best=y[i];selected=(int)i;}else if(y[i]>second)second=y[i];
 }
 if(selected<0||best<RN_THRESHOLD||best-second<RN_MARGIN||m->positive[selected]<RT_MATCH_MIN)return -1;
 return selected;
}
static inline void rn_reset(rn_trainer *t,const rt_library *library,uint8_t available){
 memset(t,0,sizeof(*t));t->source=library;if(!library||!rf_identity_valid(&library->identity)){t->state=RN_NEEDS_EXAMPLES;return;}t->identity=library->identity;memcpy(t->source_generation,library->generation,sizeof(t->source_generation));
 unsigned labels=0;
 for(unsigned i=0;i<RT_LABELS;i++){
  const rt_label *l=&library->labels[i];
  if(!(available&(1u<<i))||!l->present||!rt_label_valid(l)||rt_example_count(l,RT_POSITIVE)<RN_MIN_POSITIVES||rt_example_count(l,RT_NEGATIVE)<RN_MIN_NEGATIVES)continue;
  uint32_t positive=0,negative=0;unsigned p=0,n=0;
  for(unsigned j=0;j<RT_EXAMPLES;j++){const rt_example *e=&l->examples[j];if(e->kind==RT_POSITIVE&&e->id>positive){positive=e->id;p=j;}else if(e->kind==RT_NEGATIVE&&e->id>negative){negative=e->id;n=j;}}
  t->held[i]=(uint8_t)((1u<<p)|(1u<<n));t->eligible|=(uint8_t)(1u<<i);labels++;
 }
 if(labels<2){t->state=RN_NEEDS_EXAMPLES;return;}
 rn_seed(&t->candidate);for(unsigned i=0;i<RN_INPUTS;i++){t->low[i]=1.0f;t->high[i]=0.0f;}t->state=RN_PREPARING;
}
/* Any tuning or committed collection edit invalidates both candidate and
 * active checkpoint before another tick or application can use it. */
static inline bool rn_source_current(const rn_trainer *t,const rt_library *library){return library&&t->source==library&&rf_identity_equal(&t->identity,&library->identity)&&!memcmp(t->source_generation,library->generation,sizeof(t->source_generation));}
/* Exactly one training example, or one budgeted baseline-validation tick, per
 * call. There is no internal epoch loop, timer, I/O, allocation or sample write.
 * The caller simply stops calling while radio/UI/alarm work is busy. */
static inline void rn_tick(rn_trainer *t,const rt_library *library){
 if(!rn_source_current(t,library)){t->has_active=false;t->state=RN_NEEDS_EXAMPLES;return;}
 if(t->state!=RN_PREPARING&&t->state!=RN_TRAINING&&t->state!=RN_CHECKING)return;
 ++t->ticks;
 if(t->state==RN_PREPARING||t->state==RN_TRAINING){
  while(t->label<RT_LABELS){const rt_label *l=&library->labels[t->label];
   if(!(t->eligible&(1u<<t->label))||t->example==RT_EXAMPLES){t->label++;t->example=0;continue;}
   unsigned j=t->example++;if(!l->examples[j].id||(t->held[t->label]&(1u<<j)))continue;
   if(t->state==RN_PREPARING){float x[RN_INPUTS];rn_features(&l->examples[j],x);for(unsigned k=0;k<RN_INPUTS;k++){if(x[k]<t->low[k])t->low[k]=x[k];if(x[k]>t->high[k])t->high[k]=x[k];}}
   else{rn_train_one(&t->candidate,&l->examples[j],t->label,t->eligible);++t->updates;}return;
  }
  t->label=t->example=0;
  if(t->state==RN_PREPARING){for(unsigned k=0;k<RN_INPUTS;k++){float range=t->high[k]-t->low[k];t->candidate.center[k]=(t->high[k]+t->low[k])*0.5f;t->candidate.scale[k]=range>0.000001f?rn_limit(2.0f/range,0.0f,32.0f):0.0f;}t->state=RN_TRAINING;}
  else if(++t->epoch>=RN_EPOCHS)t->state=RN_CHECKING;
  return;
 }
 while(t->validation_label<RT_LABELS){unsigned label=t->validation_label,index=t->validation_example;
  if(!(t->eligible&(1u<<label))||index==RT_EXAMPLES){t->validation_label++;t->validation_example=0;continue;}
  if((!t->validation_pass&&!(t->held[label]&(1u<<index)))||!library->labels[label].examples[index].id){t->validation_example++;continue;}
  const rt_example *e=&library->labels[label].examples[index];
  if(!t->validation_started){rt_match_begin(&t->validation,e);t->validation.allowed_labels=t->eligible;
   if(!t->validation_pass)memcpy(t->validation.excluded_examples,t->held,sizeof(t->held));else t->validation.excluded_examples[label]=(uint8_t)(1u<<index);
   t->validation_started=true;}
  rt_match_tick(&t->validation,library,64);
  if(!t->validation.complete)return;
  int baseline=t->validation.selected,neural=rn_resolve(&t->candidate,t->eligible,&t->validation);
  /* A far-away negative already rejected by the temporal gates does not test
   * a neural ambiguity resolver. Each label needs a held-out confusing RF event
   * inside its plausible temporal region before this candidate can activate. */
  if(!t->validation_pass&&e->kind==RT_NEGATIVE&&(t->validation.reason==RT_RESULT_AMBIGUOUS||t->validation.reason==RT_RESULT_NEGATIVE)&&t->validation.positive[label]>=RT_MATCH_MIN)t->calibrated|=(uint8_t)(1u<<label);
  bool base_ok=e->kind==RT_POSITIVE?baseline==(int)label:baseline!=(int)label;
  bool candidate_ok=e->kind==RT_POSITIVE?neural==(int)label:neural!=(int)label;
  bool new_wrong_label=e->kind==RT_POSITIVE&&neural>=0&&neural!=(int)label&&neural!=baseline;
  if(!t->validation_pass){t->checks++;t->baseline_correct+=base_ok;t->candidate_correct+=candidate_ok;}
  else{t->full_checks++;t->full_baseline_correct+=base_ok;t->full_candidate_correct+=candidate_ok;}
  t->regressions+=(base_ok&&!candidate_ok)||new_wrong_label;
  t->validation_started=false;t->validation_example++;return;
 }
 if(!t->validation_pass){t->validation_pass=1;t->validation_label=t->validation_example=0;return;}
 if(t->calibrated==t->eligible&&!t->regressions&&t->candidate_correct>t->baseline_correct&&t->full_candidate_correct>t->full_baseline_correct){t->active=t->candidate;t->has_active=true;t->state=RN_ACTIVE;}else t->state=RN_BASELINE;
}
static inline bool rn_apply(const rn_trainer *t,rt_matcher *m){
 if(!t->has_active||!rn_source_current(t,t->source))return false;
 int selected=rn_resolve(&t->active,t->eligible,m);if(selected<0||selected==m->selected)return false;
 m->selected=selected;m->reason=RT_RESULT_MATCH;m->score=m->positive[selected];return true;
}
#endif
