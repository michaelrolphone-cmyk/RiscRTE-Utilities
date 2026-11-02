/* Actual Scanner publish + actual scene provider; physical transport and app
 * services are doubled. This is not a full Runtime or hardware claim. */
#define main scanner_unused_root_fixture_main
#include "root_test.c"
#undef main
extern const risc_scene_api_v1 *scanner_provider_start(const char *);
extern void scanner_provider_settle(uint64_t);
extern void scanner_provider_key(void);
extern void scanner_provider_stop(void);
static const risc_scene_components_api_v1 *real_components;
static const risc_scene_lifecycle_api_v1 *real_lifecycle;
static unsigned configure_count;
static bool inject_update;
static int checked_configure(void*c,uint64_t t,uint32_t f){configure_count++;return real_lifecycle->configure(c,t,f);}
static int injected_update(void*c,uint64_t t,const risc_components_document_v1*d){
 int rc=real_components->update(c,t,d);
 if(rc==RISC_SCENE_OK&&inject_update)scanner_provider_key();
 return rc;
}
int main(int argc,char **argv){
 assert(argc==2);reset();ui_token=0;revision=0;resident.enabled=true;
 scene=scanner_provider_start(argv[1]);real_components=risc_scene_components_get_v1(scene);real_lifecycle=risc_scene_lifecycle_get_v1(scene);assert(real_components&&real_lifecycle);
 risc_scene_components_api_v1 wrapper=*real_components;wrapper.update=injected_update;
 risc_scene_lifecycle_api_v1 life=*real_lifecycle;life.configure=checked_configure;
 components=&wrapper;lifecycle=&life;
 assert(publish()&&configure_count==1);scanner_provider_settle(ui_token);
 assert(publish()&&configure_count==1);
 inject_update=true;notice("Changed scan receipt");assert(publish()&&revision==2);
 risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=scene->next(scene->context,ui_token,&e);
 fprintf(stderr,"Observed after update: configure=%u rc=%d kind=%u revision=%u\n",configure_count,rc,e.kind,e.document_revision);
 assert(configure_count==1);
 assert(rc==RISC_SCENE_OK&&e.kind==RISC_SCENE_SUSPEND_EVENT&&e.document_revision==2);
 printf("Actual Scanner publish/provider: configure=%u event_rc=%d kind=%u revision=%u sequence=%llu\n",configure_count,rc,e.kind,e.document_revision,(unsigned long long)e.sequence);
 scanner_provider_settle(ui_token);inject_update=false;
 while(!close_ui()){assert(!terminal);}
 dirty=true;assert(publish()&&configure_count==2);scanner_provider_settle(ui_token);
 while(!close_ui()){assert(!terminal);}
 scanner_provider_stop();assert(scanner_background_close());
 return 0;
}
