/* Read-only production observer: all actions enter through native navigation. */
#include "../../Apps/frequency_scene.c"
#include "RiscInputNavigationV1.h"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
extern void scene_test_observer(unsigned(*)(void));
extern void scene_test_result(unsigned,unsigned,bool,bool);
extern unsigned frequency_audio_writes(void);
static unsigned initialized,stage,start_writes;
static unsigned observe(void){
 static unsigned previous_stage=99,previous_focus=99;
 if(stage!=previous_stage||navigation.focus[0]!=previous_focus){fprintf(stderr,"Observer stage=%u focus=%u hz=%u level=%u playing=%d\n",stage,navigation.focus[0],synth.hz,synth.level,synth.playing);previous_stage=stage;previous_focus=navigation.focus[0];}
 if(!session||terminal||pending_checkpoint||(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY)))return 0;
 unsigned target=0;
 if(stage==0){if(!synth.playing)assert(synth.hz==440&&synth.level==100);if(synth.playing){stage=1;start_writes=frequency_audio_writes();return 0;}target=FG_TOGGLE;}
 else if(stage==1){if(frequency_audio_writes()-start_writes<5)return 0;if(synth.hz==540){stage=2;return 0;}target=FG_MORE;}
 else if(stage==2){if(synth.level==99){stage=3;return 0;}target=FG_QUIETER;}
 else if(stage==3){if(!synth.playing){stage=4;return 0;}target=FG_TOGGLE;}
 else return RISC_NAV_HOME;
 for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==target)return navigation.focus[0]==document.nodes[i].id?RISC_NAV_CONFIRM:RISC_NAV_DOWN;
 return 0;
}
__attribute__((visibility("default"))) int app_module_init(void){assert(!initialized++);scene_test_lifecycle(1);scene_test_observer(observe);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){scene_test_observer(NULL);assert(!terminal);scene_test_result(synth.hz,synth.level,synth.playing,synth.owned);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){if(initialized)scene_test_lifecycle(3);}
