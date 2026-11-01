/* Read-only test observation; all scene operations use production code. */
#include SCENE_SOURCE
__attribute__((visibility("default"))) void lora_scene_probe(risc_components_document_v1*out,uint32_t*focus){*out=logical_model.doc;*focus=logical_model.navigation_path.depth?logical_model.navigation_path.focus[logical_model.navigation_path.depth-1]:0;}
extern void lora_probe_register(void(*)(risc_components_document_v1*,uint32_t*));
__attribute__((constructor)) static void probe_register(void){lora_probe_register(lora_scene_probe);}
extern void lora_scene_trace(unsigned);
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void*f,void*c){(void)c;if(f==(void*)scene_configure)lora_scene_trace(1);if(f==(void*)update_document)lora_scene_trace(2);}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*f,void*c){(void)c;if(f==(void*)scene_snapshot)lora_scene_trace(3);}

/* Read-only hit geometry; input still traverses the physical touch provider. */
static int lora_hit_point(unsigned kind,unsigned action,int*x,int*y){
 if(!active||pending||!view_current())return 0;
 for(unsigned i=0;i<visible.count;i++){const hit*h=&visible.items[i];bool match=kind?h->kind==kind:false;
  if(!kind&&h->node){int n=node_index(&document,h->node);match=n>=0&&document.nodes[n].action==action;}
  if(match){*x=h->x+h->w/2;*y=h->y+h->h/2;return 1;}}
 if(!kind)for(unsigned i=0;i<visible.count;i++){const hit*h=&visible.items[i];if(h->kind==HIT_NEXT){*x=h->x+h->w/2;*y=h->y+h->h/2;return 2;}}
 if(!kind)for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==action){int x0,y0,w0,h0;nc_layout(i,&x0,&y0,&w0,&h0);return y0-model->scroll<0?4:3;}
 return 0;
}
extern void lora_hit_register(int(*)(unsigned,unsigned,int*,int*));
__attribute__((constructor)) static void hit_register(void){lora_hit_register(lora_hit_point);}
