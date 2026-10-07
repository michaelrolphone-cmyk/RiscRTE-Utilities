#include <assert.h>
#include <stdio.h>
#include "../Apps/spectrum_neural.h"
static st_library library,original;
static sn_trainer trainer,repeat;
#include "spectrum_neural_fixture.h"
static void setup(void){
 memset(&library,0,sizeof(library));
 for(unsigned i=0;i<2;i++){
  st_label *l=&library.labels[i];l->present=true;l->next_id=6;
  snprintf(l->name,sizeof(l->name),"Door %u",i);
  for(unsigned j=0;j<3;j++)l->examples[j]=sn_fixture_example(i,j,ST_POSITIVE);
  l->examples[3]=sn_fixture_example(2,3,ST_NEGATIVE);l->examples[4]=sn_fixture_example(2,4,ST_NEGATIVE);assert(st_label_valid(l));
 }
 original=library;
}
static void finish(sn_trainer *t){
 unsigned steps=0;while(t->state==SN_PREPARING||t->state==SN_TRAINING||t->state==SN_CHECKING){
  unsigned updates=t->updates;sn_tick(t,&library);assert(t->updates-updates<=1);assert(++steps<3000);
 }
}
static st_matcher match(st_example query,bool heldout){
 st_matcher m;st_match_begin(&m,&query);if(heldout)memcpy(m.excluded_examples,trainer.held,sizeof(m.excluded_examples));
 unsigned ticks=0;while(m.running){st_match_tick(&m,&library,64);assert(++ticks<64);}return m;
}
int main(void){
 setup();sn_reset(&trainer,&library,255);assert(trainer.state==SN_PREPARING&&trainer.eligible==3&&trainer.held[0]==20&&trainer.held[1]==20);
 finish(&trainer);assert(trainer.state==SN_ACTIVE&&trainer.has_active);
 assert(trainer.checks==4&&trainer.baseline_correct==2&&trainer.candidate_correct==4&&!trainer.regressions);
 assert(trainer.updates==SN_EPOCHS*6&&trainer.epoch==SN_EPOCHS&&!memcmp(&library,&original,sizeof(library)));
 for(unsigned i=0;i<2;i++){
  st_matcher m=match(library.labels[i].examples[2],true);
  assert(m.reason==ST_RESULT_AMBIGUOUS&&m.selected<0&&m.compared==6);
  assert(sn_apply(&trainer,&m)&&m.selected==(int)i&&m.reason==ST_RESULT_MATCH);
  unsigned score=m.score;assert(!sn_apply(&trainer,&m)&&m.score==score);
 }
 st_matcher unknown=match(sn_fixture_example(2,7,ST_POSITIVE),false);
 assert(unknown.reason==ST_RESULT_AMBIGUOUS&&!sn_apply(&trainer,&unknown)&&unknown.selected<0);
 unknown=match(sn_fixture_example(3,7,ST_POSITIVE),false);
 assert(unknown.reason==ST_RESULT_UNKNOWN&&!sn_apply(&trainer,&unknown)&&unknown.selected<0);
 st_matcher blocked=match(library.labels[0].examples[2],true);blocked.negative[0]=1000;
 assert(sn_apply(&trainer,&blocked)&&blocked.selected==0);
 blocked=match(library.labels[0].examples[2],true);
 blocked.positive[4]=990;assert(!sn_apply(&trainer,&blocked)&&blocked.selected<0);
 /* Preparation and training are deterministic and interruption-safe; pausing
  * consists of doing no work, and resuming does not restart an epoch. */
 sn_reset(&repeat,&library,255);for(unsigned i=0;i<111;i++)sn_tick(&repeat,&library);
 unsigned at=repeat.updates;sn_model snapshot=repeat.candidate;assert(!memcmp(&snapshot,&repeat.candidate,sizeof(snapshot))&&repeat.updates==at);
 finish(&repeat);assert(!memcmp(&trainer,&repeat,sizeof(trainer)));
 /* A held-out confusing nonmatch resembling its own label blocks promotion.
  * No training sample or persisted label is sacrificed to fit this check. */
 setup();library.labels[0].examples[4]=sn_fixture_example(0,4,ST_NEGATIVE);
 sn_reset(&repeat,&library,255);finish(&repeat);assert(repeat.state==SN_BASELINE&&!repeat.has_active&&repeat.regressions>0);
 /* Easy unrelated negatives cannot qualify unknown-result calibration. */
 setup();library.labels[0].examples[4]=sn_fixture_example(3,4,ST_NEGATIVE);
 sn_reset(&repeat,&library,255);finish(&repeat);assert(repeat.state==SN_BASELINE&&!repeat.has_active&&repeat.calibrated!=repeat.eligible);
 /* With insufficient or unread samples, retain the temporal matcher. */
 setup();memset(&library.labels[1].examples[2],0,sizeof(st_example));sn_reset(&repeat,&library,255);assert(repeat.state==SN_NEEDS_EXAMPLES&&!repeat.has_active);
 setup();sn_reset(&repeat,&library,1);assert(repeat.state==SN_NEEDS_EXAMPLES);
 setup();library.labels[0].examples[1].id=library.labels[0].examples[0].id;sn_reset(&repeat,&library,255);assert(repeat.state==SN_NEEDS_EXAMPLES);
 /* Changing examples invalidates every old prediction before rebuilding. */
 setup();sn_reset(&trainer,&library,255);assert(!trainer.has_active&&trainer.state==SN_PREPARING);
 printf("Neural candidate: %zu-byte bounded trainer, held-out baseline 2/4 to 4/4, unknown/negative/untrained-label vetoes, no-regression promotion, interruption and unchanged examples PASS\n",sizeof(trainer));
 return 0;
}
