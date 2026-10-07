/* Real app + pinned real adapter. Only provider hardware is simulated. */
#include "paper_utility_peripherals.h"
#include NOVA_APP_SOURCE
static void touch(unsigned at,int px,int py,int wx,int wy) {
 assert(action_count<256);actions[action_count].at=at;actions[action_count].x=paper_profile?px:wx;actions[action_count++].y=paper_profile?py:wy;
}
#if NOVA_APP_ID == 1
static void key(unsigned at,unsigned k){touch(at,80+(int)(k%4)*106,281+(int)(k/4)*106,40+(int)(k%4)*53,74+(int)(k/4)*46);}
static void tools(unsigned at){touch(at,340,735,210,26);}
static void back(unsigned at){touch(at,110,735,30,26);}
#elif NOVA_APP_ID == 2
static void toggle_at(unsigned at){touch(at,90,628,48,204);}
static void reset_at(unsigned at){touch(at,240,628,120,204);}
#else
static void edit(unsigned at){touch(at,200,370,110,82);}
static void second(unsigned at){touch(at,200,436,120,180);}
static void add(unsigned at){touch(at,330,452,200,118);}
static void done(unsigned at){touch(at,200,625,120,206);}
static void back(unsigned at){touch(at,110,735,30,26);}
static void start(unsigned at){touch(at,110,628,50,206);}
static void cancel(unsigned at){touch(at,340,628,174,206);}
#endif
int main(int argc,char **argv) {
 assert(argc==4);directory=argv[1];unsigned scenario=(unsigned)atoi(argv[2]);paper_profile=atoi(argv[3])!=0;
 memset(pixels,0xa5,sizeof(pixels));stop_poll=240;
#if NOVA_APP_ID == 1
 if(scenario!=4&&scenario!=6&&scenario!=8&&scenario!=10)key(20,0);
 switch(scenario){
 case 0:key(40,15);key(60,9);key(80,14);key(100,14);break;
 case 1:tools(40);back(60);key(80,1);back(100);break;
 case 2:tools(40);home_at=60;break;
 case 3:key(40,3);key(60,12);key(80,14);key(100,10);break;
 case 4:cancel_contact=true;key(20,0);break;
 case 5:refuse_launch=true;back(40);key(60,1);back(80);break;
 case 6:key(20,0);stop_poll=20;break;
 case 7:key(40,1);tools(60);touch(80,200,484,120,204);tools(100);touch(120,200,380,120,152);tools(140);touch(160,200,276,120,100);key(180,2);break;
 case 8:case 10:break;
 case 9:tools(40);raw_home_at=60;break;
 default:assert(0);}
#elif NOVA_APP_ID == 2
 switch(scenario){
 case 0:toggle_at(20);toggle_at(100);break;
 case 1:toggle_at(20);toggle_at(100);reset_at(120);reset_at(140);break;
 case 2:toggle_at(20);home_at=100;break;
 case 3:touch(40,200,735,30,26);break;
 case 4:cancel_contact=true;toggle_at(20);break;
 case 5:fail_put_once=true;toggle_at(20);toggle_at(60);toggle_at(100);break;
 case 6:toggle_at(20);stop_poll=20;break;
 case 7:toggle_at(20);toggle_at(60);toggle_at(100);toggle_at(140);break;
 case 8:case 10:break;
 case 9:toggle_at(20);raw_home_at=100;break;
 case 11:deny_private_store=true;touch(40,200,735,30,26);break;
 default:assert(0);}
#else
 switch(scenario){
 case 0:edit(20);second(40);add(60);done(80);back(100);start(120);cancel(160);break;
 case 1:edit(20);second(40);back(60);back(80);back(100);break;
 case 2:edit(20);second(40);home_at=60;break;
 case 3:start(20);start(60);cancel(100);start(140);break;
 case 4:cancel_contact=true;start(20);break;
 case 5:fail_put_once=true;start(20);cancel(40);start(60);break;
 case 6:start(20);stop_poll=20;break;
 case 7:edit(20);second(40);touch(60,110,452,40,118);add(80);done(100);back(120);start(140);break;
 case 8:case 10:break;
 case 9:edit(20);second(40);raw_home_at=60;break;
 case 11:deny_private_store=true;back(40);break;
 default:assert(0);}
#endif
 if(scenario==10){
#if NOVA_APP_ID == 1
 key(18,0);
#elif NOVA_APP_ID == 2
 toggle_at(18);
#else
 start(18);
#endif
 }
 if(scenario==8||scenario==10){assert(paper_profile);touch(20,200,20,0,0);touch(21,200,100,0,0);touch(40,130,345,0,0);touch(60,140,450,0,0);touch(80,340,553,0,0);touch(120,240,620,0,0);touch(121,240,560,0,0);home_at=160;}
 assert(app_module_init()==0);app_main();
 if(scenario==10){assert(launches==1&&!strcmp(destination,"default.elf"));
#if NOVA_APP_ID == 1
 assert(!strcmp(calculator_state.text,"7")&&!puts_count);
#elif NOVA_APP_ID == 2
 assert(clock_state.running&&puts_count==1);
#else
 assert(writer.saved.enabled&&writer.saved.revision==1&&puts_count==1);
#endif
 }
 if(scenario==11)assert(!puts_count&&launches==1&&!strcmp(destination,"springboard.elf"));
 if(scenario==8)assert(!puts_count&&launches==1&&!strcmp(destination,"default.elf")&&presents>=3);
#if NOVA_APP_ID == 1
 switch(scenario){
 case 0:assert(!strcmp(calculator_state.text,"11"));break;
 case 1:case 5:assert(!strcmp(calculator_state.text,"78"));assert(launches==(scenario==5?2u:1u)&&!strcmp(destination,"springboard.elf"));break;
 case 2:case 9:assert(calculator_menu&&launches==1&&!strcmp(destination,"default.elf"));break;
 case 3:assert(!calculator_state.error&&!strcmp(calculator_state.text,"3"));break;
 case 4:case 6:assert(!strcmp(calculator_state.text,"0"));break;
 case 7:assert(!strcmp(calculator_state.text,"9")&&!calculator_menu);break;
 }
#elif NOVA_APP_ID == 2
 switch(scenario){
 case 0:assert(!clock_state.running&&clock_state.elapsed_ms>1000&&clock_state.saved.elapsed_ms==clock_state.elapsed_ms);break;
 case 1:assert(!clock_state.running&&!clock_state.elapsed_ms);break;
 case 2:case 9:assert(clock_state.running&&clock_state.saved.running&&launches==1&&!strcmp(destination,"default.elf"));break;
 case 3:assert(launches==1&&!strcmp(destination,"springboard.elf"));break;
 case 4:case 6:assert(!clock_state.running&&!puts_count);break;
 case 5:assert(!clock_state.running&&clock_state.elapsed_ms>0&&puts_count==3);break;
 case 7:assert(!clock_state.running&&puts_count==4&&clock_state.elapsed_ms==clock_state.saved.elapsed_ms);break;
 }
 if(paper_profile&&scenario==0)assert(presents<12); /* Watch remains 50ms. */
#else
 switch(scenario){
 case 0:assert(!writer.uncertain&&!writer.saved.enabled&&writer.saved.revision==2&&values[2]==1);break;
 case 1:assert(!puts_count&&alarm_page==0&&launches==1&&!strcmp(destination,"springboard.elf"));break;
 case 2:case 9:assert(!puts_count&&alarm_page==2&&launches==1&&!strcmp(destination,"default.elf"));break;
 case 3:assert(writer.saved.enabled&&writer.saved.revision==4);break;
 case 4:case 6:assert(!puts_count);break;
 case 5:assert(writer.saved.enabled&&!writer.uncertain&&writer.saved.revision==1&&puts_count==2);break;
 case 7:assert(writer.saved.enabled&&writer.saved.duration==300&&values[2]==0);break;
 }
#endif
 app_module_fini();assert(!fixture_grants&&!frames&&!subs);
 printf("app=%d profile=%s scenario=%u frames=%u ticks=%u: model, navigation, grants and cleanup passed\n",NOVA_APP_ID,paper_profile?"paper":"Watch",scenario,presents,ticks);
}
