#include "test/scene/telemetry_probe.c"
/* Bounds from the actual current logical scene, including in-flight raster. */
int telemetry_test_logical_target(unsigned a,int value,int*x,int*y){
 if(!active||!view_current())return 0;
 for(unsigned i=0;i<visible.count;i++){
  const hit*h=&visible.items[i];int at=node_index(&document,h->node);
  if(at<0||document.nodes[at].action!=a||(h->kind==HIT_VALUE&&h->value!=value))continue;
  if(h->w<12||h->h<12)continue;*x=h->x+h->w/2;*y=h->y+h->h/2;return 1;
 }return 0;
}
int telemetry_test_scroll(void){return logical_model.scroll;}
