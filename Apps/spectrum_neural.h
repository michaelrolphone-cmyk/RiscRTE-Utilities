#ifndef SPECTRUM_NEURAL_H
#define SPECTRUM_NEURAL_H
#include "spectrum_temporal.h"

/* A bounded candidate, not an unconditional replacement classifier. It can
 * resolve only temporal ambiguities between sufficiently taught labels. The
 * existing shape/time/frequency gates remain mandatory. A confusing-negative
 * template can be distinguished only after explicit negative training and
 * both held-out and full-collection rejection checks. Insufficient evidence
 * always leaves the temporal answer intact.
 *
 * 64 mean log-band powers + 8 envelope samples + 8 temporal/frequency values.
 * The small hidden layer is deliberately evaluated against the simpler matcher
 * on examples excluded from BOTH training and reference matching. No PCM,
 * labels, examples or on-disk records are changed by training. */
#define SN_INPUTS 80u
#define SN_HIDDEN 16u
#define SN_EPOCHS 256u
#define SN_MIN_POSITIVES 3u
#define SN_MIN_NEGATIVES 2u
#define SN_THRESHOLD 0.75f
#define SN_MARGIN 0.20f
typedef struct {
 float input[SN_HIDDEN][SN_INPUTS],hidden_bias[SN_HIDDEN];
 float output[ST_LABELS][SN_HIDDEN],output_bias[ST_LABELS];
 float center[SN_INPUTS],scale[SN_INPUTS];
} sn_model;
enum {SN_NEEDS_EXAMPLES,SN_PREPARING,SN_TRAINING,SN_CHECKING,SN_BASELINE,SN_ACTIVE};
typedef struct {
 sn_model candidate,active;
 st_matcher validation;
 float low[SN_INPUTS],high[SN_INPUTS];
 uint8_t held[ST_LABELS],eligible,calibrated;
 unsigned state,epoch,label,example,validation_label,validation_example;
 unsigned checks,baseline_correct,candidate_correct,regressions,updates,ticks;
 unsigned validation_pass,full_checks,full_baseline_correct,full_candidate_correct;
 bool has_active,validation_started;
} sn_trainer;
_Static_assert(sizeof(sn_trainer)<17000,"bounded neural training and active model RAM");

static inline float sn_abs(float x){return x<0?-x:x;}
static inline float sn_limit(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static inline float sn_activation(float x){return x/(1.0f+sn_abs(x));}
static inline float sn_derivative(float value){float a=1.0f-sn_abs(value);return a*a;}
static inline void sn_features(const st_example *e,float out[SN_INPUTS]){
 memset(out,0,sizeof(float)*SN_INPUTS);unsigned active=0;
 for(unsigned i=e->onset;i<=e->end;i++)if(e->frames[i].flags&ST_ACTIVE){
  ++active;for(unsigned b=0;b<ST_BANDS;b++)out[b]+=(float)st_nibble(&e->frames[i],(int)b)/15.0f;
 }
 if(active)for(unsigned b=0;b<ST_BANDS;b++)out[b]/=(float)active;
 unsigned length=e->end-e->onset+1u;
 for(unsigned i=0;i<8;i++){
  unsigned lo=e->onset+i*length/8u,hi=e->onset+(i+1u)*length/8u;if(hi<=lo)hi=lo+1;
  for(unsigned j=lo;j<hi;j++)out[64+i]+=sn_limit(1.0f+(float)(e->frames[j].level_db-e->peak_db)/4800.0f,0.0f,1.0f);
  out[64+i]/=(float)(hi-lo);
 }
 out[72]=(float)e->duration_ms/(float)(ST_FRAMES*ST_FRAME_MS);
 out[73]=(float)e->attack_ms/(float)e->duration_ms;
 out[74]=(float)e->decay_ms/(float)e->duration_ms;
 out[75]=(float)(e->impacts>8?8:e->impacts)/8.0f;
 out[76]=(float)st_peak_phase(e)/1000.0f;
 out[77]=(float)st_frequency_evidence(e)/8000.0f;
 out[78]=st_tonal_example(e)?1.0f:0.0f;
 out[79]=st_partial(e)?1.0f:0.0f;
}
static inline void sn_predict(const sn_model *m,const float x[SN_INPUTS],float h[SN_HIDDEN],float y[ST_LABELS]){
 for(unsigned j=0;j<SN_HIDDEN;j++){float sum=m->hidden_bias[j];for(unsigned k=0;k<SN_INPUTS;k++)sum+=m->input[j][k]*sn_limit((x[k]-m->center[k])*m->scale[k],-2.0f,2.0f);h[j]=sn_activation(sum);}
 for(unsigned i=0;i<ST_LABELS;i++){float sum=m->output_bias[i];for(unsigned j=0;j<SN_HIDDEN;j++)sum+=m->output[i][j]*h[j];y[i]=0.5f+0.5f*sn_activation(sum);}
}
static inline void sn_seed(sn_model *m){
 memset(m,0,sizeof(*m));uint32_t seed=0x534e3031u;
 for(unsigned j=0;j<SN_HIDDEN;j++)for(unsigned k=0;k<SN_INPUTS;k++){seed=seed*1664525u+1013904223u;m->input[j][k]=((float)((seed>>16)&1023u)-511.5f)/4096.0f;}
 for(unsigned i=0;i<ST_LABELS;i++)for(unsigned j=0;j<SN_HIDDEN;j++){seed=seed*1664525u+1013904223u;m->output[i][j]=((float)((seed>>16)&1023u)-511.5f)/2048.0f;}
}
static inline void sn_train_one(sn_model *m,const st_example *e,unsigned label,uint8_t eligible){
 float x[SN_INPUTS],h[SN_HIDDEN],y[ST_LABELS],delta[ST_LABELS]={0},hidden[SN_HIDDEN]={0};
 sn_features(e,x);sn_predict(m,x,h,y);
 for(unsigned k=0;k<SN_INPUTS;k++)x[k]=sn_limit((x[k]-m->center[k])*m->scale[k],-2.0f,2.0f);
 for(unsigned i=0;i<ST_LABELS;i++)if(eligible&(1u<<i)){
  /* A confusing example excludes its own label only. It is never silently
   * assigned to another label or treated as a global negative. */
  if(e->kind==ST_NEGATIVE&&i!=label)continue;
  float target=e->kind==ST_POSITIVE&&i==label?1.0f:0.0f;
  delta[i]=(target-y[i])*0.5f*sn_derivative(2.0f*y[i]-1.0f);
  for(unsigned j=0;j<SN_HIDDEN;j++)hidden[j]+=delta[i]*m->output[i][j];
 }
 const float rate=0.35f;
 for(unsigned j=0;j<SN_HIDDEN;j++){
  float update=rate*hidden[j]*sn_derivative(h[j]);
  m->hidden_bias[j]=sn_limit(m->hidden_bias[j]+update,-8.0f,8.0f);
  for(unsigned k=0;k<SN_INPUTS;k++)m->input[j][k]=sn_limit(m->input[j][k]+update*x[k],-8.0f,8.0f);
 }
 for(unsigned i=0;i<ST_LABELS;i++){
  m->output_bias[i]=sn_limit(m->output_bias[i]+rate*delta[i],-8.0f,8.0f);
  for(unsigned j=0;j<SN_HIDDEN;j++)m->output[i][j]=sn_limit(m->output[i][j]+rate*delta[i]*h[j],-8.0f,8.0f);
 }
}
static inline int sn_resolve(const sn_model *model,uint8_t eligible,const st_matcher *m){
 if(!m->complete||(m->reason!=ST_RESULT_AMBIGUOUS&&m->reason!=ST_RESULT_NEGATIVE))return m->selected;
 if(!st_example_valid(&m->query))return -1;
 unsigned candidates=0;for(unsigned i=0;i<ST_LABELS;i++)if(m->positive[i]>=ST_MATCH_MIN){if(!(eligible&(1u<<i)))return -1;++candidates;}
 if(candidates<(m->reason==ST_RESULT_AMBIGUOUS?2u:1u))return -1;
 float x[SN_INPUTS],h[SN_HIDDEN],y[ST_LABELS];sn_features(&m->query,x);sn_predict(model,x,h,y);
 float best=0,second=0;int selected=-1;
 for(unsigned i=0;i<ST_LABELS;i++)if(eligible&(1u<<i)){
  if(y[i]>best){second=best;best=y[i];selected=(int)i;}else if(y[i]>second)second=y[i];
 }
 if(selected<0||best<SN_THRESHOLD||best-second<SN_MARGIN||m->positive[selected]<ST_MATCH_MIN)return -1;
 return selected;
}
static inline void sn_reset(sn_trainer *t,const st_library *library,uint8_t available){
 memset(t,0,sizeof(*t));
 unsigned labels=0;
 for(unsigned i=0;i<ST_LABELS;i++){
  const st_label *l=&library->labels[i];
  if(!(available&(1u<<i))||!l->present||!st_label_valid(l)||st_example_count(l,ST_POSITIVE)<SN_MIN_POSITIVES||st_example_count(l,ST_NEGATIVE)<SN_MIN_NEGATIVES)continue;
  uint32_t positive=0,negative=0;unsigned p=0,n=0;
  for(unsigned j=0;j<ST_EXAMPLES;j++){const st_example *e=&l->examples[j];if(e->kind==ST_POSITIVE&&e->id>positive){positive=e->id;p=j;}else if(e->kind==ST_NEGATIVE&&e->id>negative){negative=e->id;n=j;}}
  t->held[i]=(uint8_t)((1u<<p)|(1u<<n));t->eligible|=(uint8_t)(1u<<i);labels++;
 }
 if(labels<2){t->state=SN_NEEDS_EXAMPLES;return;}
 sn_seed(&t->candidate);for(unsigned i=0;i<SN_INPUTS;i++){t->low[i]=1.0f;t->high[i]=0.0f;}t->state=SN_PREPARING;
}
/* Exactly one training example, or one budgeted baseline-validation tick, per
 * call. There is no internal epoch loop, timer, I/O, allocation or sample write.
 * The caller simply stops calling while microphone/UI/alarm work is busy. */
static inline void sn_tick(sn_trainer *t,const st_library *library){
 if(t->state!=SN_PREPARING&&t->state!=SN_TRAINING&&t->state!=SN_CHECKING)return;
 ++t->ticks;
 if(t->state==SN_PREPARING||t->state==SN_TRAINING){
  while(t->label<ST_LABELS){const st_label *l=&library->labels[t->label];
   if(!(t->eligible&(1u<<t->label))||t->example==ST_EXAMPLES){t->label++;t->example=0;continue;}
   unsigned j=t->example++;if(!l->examples[j].id||(t->held[t->label]&(1u<<j)))continue;
   if(t->state==SN_PREPARING){float x[SN_INPUTS];sn_features(&l->examples[j],x);for(unsigned k=0;k<SN_INPUTS;k++){if(x[k]<t->low[k])t->low[k]=x[k];if(x[k]>t->high[k])t->high[k]=x[k];}}
   else{sn_train_one(&t->candidate,&l->examples[j],t->label,t->eligible);++t->updates;}return;
  }
  t->label=t->example=0;
  if(t->state==SN_PREPARING){for(unsigned k=0;k<SN_INPUTS;k++){float range=t->high[k]-t->low[k];t->candidate.center[k]=(t->high[k]+t->low[k])*0.5f;t->candidate.scale[k]=range>0.000001f?sn_limit(2.0f/range,0.0f,32.0f):0.0f;}t->state=SN_TRAINING;}
  else if(++t->epoch>=SN_EPOCHS)t->state=SN_CHECKING;
  return;
 }
 while(t->validation_label<ST_LABELS){unsigned label=t->validation_label,index=t->validation_example;
  if(!(t->eligible&(1u<<label))||index==ST_EXAMPLES){t->validation_label++;t->validation_example=0;continue;}
  if((!t->validation_pass&&!(t->held[label]&(1u<<index)))||!library->labels[label].examples[index].id){t->validation_example++;continue;}
  const st_example *e=&library->labels[label].examples[index];
  if(!t->validation_started){st_match_begin(&t->validation,e);t->validation.allowed_labels=t->eligible;
   if(!t->validation_pass)memcpy(t->validation.excluded_examples,t->held,sizeof(t->held));else t->validation.excluded_examples[label]=(uint8_t)(1u<<index);
   t->validation_started=true;}
  st_match_tick(&t->validation,library,64);
  if(!t->validation.complete)return;
  int baseline=t->validation.selected,neural=sn_resolve(&t->candidate,t->eligible,&t->validation);
  /* A far-away negative already rejected by the temporal gates does not test
   * a neural ambiguity resolver. Each label needs a held-out confusing sound
   * inside its plausible temporal region before this candidate can activate. */
  if(!t->validation_pass&&e->kind==ST_NEGATIVE&&(t->validation.reason==ST_RESULT_AMBIGUOUS||t->validation.reason==ST_RESULT_NEGATIVE)&&t->validation.positive[label]>=ST_MATCH_MIN)t->calibrated|=(uint8_t)(1u<<label);
  bool base_ok=e->kind==ST_POSITIVE?baseline==(int)label:baseline!=(int)label;
  bool candidate_ok=e->kind==ST_POSITIVE?neural==(int)label:neural!=(int)label;
  bool new_wrong_label=e->kind==ST_POSITIVE&&neural>=0&&neural!=(int)label&&neural!=baseline;
  if(!t->validation_pass){t->checks++;t->baseline_correct+=base_ok;t->candidate_correct+=candidate_ok;}
  else{t->full_checks++;t->full_baseline_correct+=base_ok;t->full_candidate_correct+=candidate_ok;}
  t->regressions+=(base_ok&&!candidate_ok)||new_wrong_label;
  t->validation_started=false;t->validation_example++;return;
 }
 if(!t->validation_pass){t->validation_pass=1;t->validation_label=t->validation_example=0;return;}
 if(t->calibrated==t->eligible&&!t->regressions&&t->candidate_correct>t->baseline_correct&&t->full_candidate_correct>t->full_baseline_correct){t->active=t->candidate;t->has_active=true;t->state=SN_ACTIVE;}else t->state=SN_BASELINE;
}
static inline bool sn_apply(const sn_trainer *t,st_matcher *m){
 if(!t->has_active)return false;
 int selected=sn_resolve(&t->active,t->eligible,m);if(selected<0||selected==m->selected)return false;
 m->selected=selected;m->reason=ST_RESULT_MATCH;m->score=m->positive[selected];return true;
}
#endif
