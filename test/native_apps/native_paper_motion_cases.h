/* Production controllers, shared adapter, and deterministic touch sequences. */
#ifdef PORTABLE_PAPER_TRANSITIONS
static void motion_drag(unsigned at,int from,int to) {
 touch(at,200,from,0,0);touch(at+1,200,to,0,0);
}
static void motion_case(unsigned scenario) {
 assert(paper_profile);stop_poll=400;home_at=360;
 switch(scenario) {
 case 30:motion_drag(20,20,400);motion_drag(100,670,400);motion_drag(180,20,400);motion_drag(260,670,400);break;
 case 31:motion_drag(20,20,150);raw_home_at=22;home_at=0;break;
 case 32:motion_drag(20,20,150);touch(22,200,160,0,0);motion_multi_at=22;motion_drag(140,20,400);motion_drag(240,670,400);break;
 case 33:motion_drag(20,20,150);touch(22,200,160,0,0);motion_replace_at=22;motion_drag(140,20,400);motion_drag(240,670,400);break;
 case 34:motion_drag(20,20,400);home_at=22;raw_home_at=340;break;
 default:assert(0);
 }
}
static void motion_verify(unsigned scenario) {
 assert(motion_opened>=1&&motion_opened==motion_closed&&!puts_count);
 assert(motion_restored==motion_closed);
 if(scenario==30)assert(motion_opened==2&&motion_frames>=8);
 if(scenario==32||scenario==33)assert(motion_opened==2&&motion_frames>=5);
 assert(launches==1&&!strcmp(destination,"default.elf"));
}
#endif
