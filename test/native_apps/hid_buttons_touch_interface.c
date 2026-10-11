/* Declared RiscTouchV1 interface double. This is NOT GT911 source or a
 * physical-controller simulation. It fans ordered copied events to subscribers. */
#include "RiscTouchV1.h"
#include <assert.h>
#include <string.h>
void hid_renderer_watch_report(risc_touch_snapshot_v1 *sample);
uint64_t hid_renderer_watch_millis(void);
static risc_touch_snapshot_v1 current;
static risc_touch_event_v1 queue[RISC_TOUCH_QUEUE_LENGTH];
static struct {bool live;uint64_t seen;} cursors[RISC_TOUCH_MAX_SUBSCRIBERS+1];
static uint64_t serial;
static int find(const risc_touch_snapshot_v1*s,unsigned id){for(unsigned i=0;i<s->contact_count;i++)if(s->contacts[i].id==id)return (int)i;return -1;}
static void emit(unsigned kind,unsigned id,unsigned x,unsigned y){uint64_t seq=++serial;queue[(seq-1)%RISC_TOUCH_QUEUE_LENGTH]=(risc_touch_event_v1){.sequence=seq,.timestamp_ms=hid_renderer_watch_millis(),.kind=kind,.id=id,.x=x,.y=y};}
static uint64_t sub(void*c){(void)c;for(unsigned i=1;i<=RISC_TOUCH_MAX_SUBSCRIBERS;i++)if(!cursors[i].live){cursors[i].live=true;cursors[i].seen=serial;return i;}return 0;}
static bool unsub(void*c,uint64_t n){(void)c;if(n<1||n>RISC_TOUCH_MAX_SUBSCRIBERS||!cursors[n].live)return false;cursors[n].live=false;return true;}
static bool poll(void*c,size_t n){(void)c;assert(n==1);risc_touch_snapshot_v1 value={0};hid_renderer_watch_report(&value);assert(value.contact_count<=RISC_TOUCH_MAX_CONTACTS);
 for(unsigned i=0;i<current.contact_count;i++){const risc_touch_contact_v1*p=&current.contacts[i];if(find(&value,p->id)<0)emit(RISC_TOUCH_EVENT_UP,p->id,p->x,p->y);}
 for(unsigned i=0;i<value.contact_count;i++){const risc_touch_contact_v1*p=&value.contacts[i];int old=find(&current,p->id);if(old<0)emit(RISC_TOUCH_EVENT_DOWN,p->id,p->x,p->y);else if(current.contacts[old].x!=p->x||current.contacts[old].y!=p->y)emit(RISC_TOUCH_EVENT_MOVE,p->id,p->x,p->y);}
 for(unsigned i=0;i<32;i++)if((value.buttons^current.buttons)&(1u<<i))emit(value.buttons&(1u<<i)?RISC_TOUCH_EVENT_BUTTON_DOWN:RISC_TOUCH_EVENT_BUTTON_UP,i,0,0);
 current=value;current.sequence=serial;current.timestamp_ms=hid_renderer_watch_millis();return true;}
static int32_t next(void*c,uint64_t n,risc_touch_event_v1*out){(void)c;if(n<1||n>RISC_TOUCH_MAX_SUBSCRIBERS||!cursors[n].live)return -1;if(serial-cursors[n].seen>RISC_TOUCH_QUEUE_LENGTH){cursors[n].seen=serial;return -1;}if(cursors[n].seen==serial)return 0;*out=queue[cursors[n].seen++%RISC_TOUCH_QUEUE_LENGTH];return 1;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=current;return true;}
static const risc_touch_api_v1 api={1,sizeof(api),NULL,sub,unsub,poll,next,snapshot};
const risc_touch_api_v1 *hid_watch_touch_start(void){memset(&current,0,sizeof(current));memset(cursors,0,sizeof(cursors));serial=0;hid_renderer_watch_report(&current);return &api;}
void hid_watch_touch_stop(void){for(unsigned i=1;i<=RISC_TOUCH_MAX_SUBSCRIBERS;i++)assert(!cursors[i].live);}
