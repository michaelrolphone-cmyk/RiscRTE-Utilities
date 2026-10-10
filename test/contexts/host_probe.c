/* Test-only, read-only access to the real presenter's current hit map. */
#include SCENE_HOST_SOURCE
bool contexts_test_target(uint32_t action,int value,int*x,int*y){
 for(unsigned i=0;i<visible.count;i++){const hit*h=&visible.items[i];int at=node_index(&document,h->node);
  if(at<0||document.nodes[at].action!=action||(value!=-999&&h->value!=value)||h->h<12)continue;
  for(int part=1;part<=3;part++){int px=h->x+h->w*part/4,py=h->y+h->h/2;bool covered=false;
   for(unsigned j=i+1;j<visible.count;j++){const hit*b=&visible.items[j];if(px>=b->x&&py>=b->y&&px<b->x+b->w&&py<b->y+b->h)covered=true;}
   if(!covered){*x=px;*y=py;return true;}
  }
 }
 return false;
}
