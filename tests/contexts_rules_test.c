#include "../lib/Contexts/include/ContextsRules.h"
#include <assert.h>
#include <stdio.h>
static cr_store s,decoded;
static cr_engine e;
static unsigned char bytes[CR_FILE_BYTES];
int main(void){
 cr_init(&s);cr_engine_init(&e);s.items[0]=(cr_item){.kind=CR_PLACE,.name="Office"};
 s.items[1]=(cr_item){.kind=CR_BATTERY,.arg0=20,.name="Low battery"};
 strcpy(s.rules[0].name,"Work");s.rules[0].enabled=1;s.rules[0].item=0;s.rules[0].hold_seconds=20;s.rules[0].settings[CR_VOLUME]=3;s.rules[0].steps=2;s.rules[0].workflow[0]=(cr_step){.kind=CR_TIMER,.value=300};s.rules[0].workflow[1]=(cr_step){.kind=CR_MESSAGE,.text="Break"};
 strcpy(s.rules[1].name,"Save power");s.rules[1].enabled=1;s.rules[1].item=1;s.rules[1].priority=1;s.rules[1].settings[CR_VOLUME]=0;
 assert(cr_encode(&s,bytes,sizeof(bytes)));assert(cr_decode(&decoded,bytes,sizeof(bytes)));assert(!memcmp(&s,&decoded,sizeof(s)));
 bytes[96]^=1;assert(!cr_decode(&decoded,bytes,sizeof(bytes)));bytes[96]^=1;
 cr_observation o={.enabled=true,.room="Office",.now_ms=1000};cr_result r=cr_evaluate(&e,&s,&o);assert(r.entered==1&&r.settings[CR_VOLUME]==3&&r.changed==(1<<CR_VOLUME));
 o.now_ms=2000;r=cr_evaluate(&e,&s,&o);assert(!r.entered&&!r.changed);
 o.battery_valid=true;o.battery=10;r=cr_evaluate(&e,&s,&o);assert(r.entered==2&&r.settings[CR_VOLUME]==0&&r.winners[CR_VOLUME]==1);
 o.room=NULL;o.classification_pending=true;o.now_ms=25000;r=cr_evaluate(&e,&s,&o);assert(r.active==3&&!r.entered);
 o.classification_pending=false;r=cr_evaluate(&e,&s,&o);assert(r.left==1&&r.active==2);
 o.room="Office";o.now_ms=26000;r=cr_evaluate(&e,&s,&o);assert(r.entered==1);
 o.enabled=false;r=cr_evaluate(&e,&s,&o);assert(r.left==3&&!r.active&&!e.pending);
 o.enabled=true;o.now_ms=27000;r=cr_evaluate(&e,&s,&o);assert(r.entered==3);s.rules[0].enabled=0;r=cr_evaluate(&e,&s,&o);assert(r.left==1);
 assert(cr_valid(&s));s.rules[1].settings[CR_VOLUME]=11;assert(!cr_valid(&s));
 puts("Contexts rules: persistence CRC, edge-only actions, priority composition, hold, capture pause, unknown, disable PASS");
}
