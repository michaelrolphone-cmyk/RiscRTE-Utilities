/* Regression: production adapter starts without an acquired surface. The
 * real Calculator/header must acquire and submit, not paint into a fixture's
 * preinstalled framebuffer. Providers only are modeled. */
#define main inherited_capture_main
#include "nova/peripherals.h"
#undef main
int main(int argc,char**argv){
 int result=inherited_capture_main(argc,argv);
 assert(!result&&presents>0&&!grants&&!frames&&!subs);
 unsigned lit=0;for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)if(pixels[y*244+x])lit++;
 assert(lit>100);printf("PASS real adapter cold native Calculator: %u submitted frames, %u lit pixels, clean teardown\n",presents,lit);return 0;
}
