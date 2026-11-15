/* Presentation only: the shared controller still owns every UTC/KV transition. */
static const char *stopwatch_native_status(const char *phase) {
 return status && strcmp(status,phase) ? status : "";
}
static void stopwatch_native_draw(void) {
 int w=app->screen_width(),h=app->screen_height();char value[12];
 un_header(app,"Stopwatch");sw_format(clock_state.elapsed_ms,value);
 int body=h-100;const char *phase=clock_state.running?"RUNNING":loaded?"PAUSED":"STORAGE ERROR";
 un_text((utility_native_rect){8,40,w-16,body/2},value,true,NOVA_TEXT);
 un_text((utility_native_rect){8,40+body/2,w-16,body/4},phase,false,NOVA_CYAN);
 un_text((utility_native_rect){8,40+3*body/4,w-16,body-body*3/4},stopwatch_native_status(phase),false,NOVA_TEXT);
 un_button(un_footer(w,h,0,3),pending_pause?"Save pause":clock_state.running?"Pause":"Start",false);
 un_button(un_footer(w,h,1,3),reset_armed?"Confirm":"Reset",reset_armed);
 un_button(un_footer(w,h,2,3),"Retry",false);app->present(false);
}
