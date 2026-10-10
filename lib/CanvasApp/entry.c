#include "CanvasApp.h"
void canvas_original_main(void);
__attribute__((visibility("default"))) void app_main(void) {
 if(canvas_open(CANVAS_KIND)){canvas_original_main();canvas_close();}
}
