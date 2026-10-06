/* Continuous content-space settings list. Header and footer stay fixed, and
 * the same pixel offset is used to draw, clip and hit-test every row. */
#define CONTROLS_TOP 48
#define CONTROLS_BOTTOM 194
#define CONTROLS_ROW_HEIGHT 48
#define CONTROLS_COUNT 13
#define CONTROLS_MAX_SCROLL (CONTROLS_COUNT*CONTROLS_ROW_HEIGHT-(CONTROLS_BOTTOM-CONTROLS_TOP))
static void controls_move(int offset){int next=clamp_int(offset,0,CONTROLS_MAX_SCROLL);if(next!=controls_scroll){controls_scroll=next;dirty=true;}}
static bool controls_segment(unsigned setting){return setting==3||setting==4||setting==5;}
static void draw_setting(unsigned setting,int y){
 static const char *captions[]={"","LOW","HIGH","X SCALE","Y SCALE","LABELS","COLORS","GAIN","WINDOW","FFT SIZE","FLOOR","STORAGE","SAMPLES","EVENTS"};
 char value[24]={0};fill(12,y+47,216,1,NOVA_LINE);text(1,14,y+14,75,captions[setting],NOVA_TEXT);
 if(controls_segment(setting)){const char *a=setting==3?"LOG":setting==4?"DB":"ON",*b=setting==5?"OFF":"LIN";bool first=setting==3?prefs.log_frequency:setting==4?prefs.log_amplitude:prefs.show_labels;pill(110,y+4,55,40,a,first);pill(171,y+4,61,40,b,!first);return;}
 switch(setting){case 13:case 12:pill(110,y+4,122,40,"OPEN",false);return;case 1:snprintf(value,sizeof(value),"%u Hz",prefs.low_hz);break;case 2:snprintf(value,sizeof(value),"%u kHz",dsp_config.high_hz/1000);break;case 6:snprintf(value,sizeof(value),"%s",palette_names[prefs.palette]);break;case 7:snprintf(value,sizeof(value),"%+d dB",prefs.gain_db);break;case 8:snprintf(value,sizeof(value),"%s",window_names[prefs.window]);break;case 9:snprintf(value,sizeof(value),"%u",prefs.fft_size);break;case 10:snprintf(value,sizeof(value),"%d dB",prefs.threshold_db);break;default:pill(110,y+4,122,40,(pending_save||signature_pending||event_files.pending>=0)?"RETRY SAVE":(store_message||signature_errors||event_files.ready!=3)?"RETRY":storage?"SAVED":"RETRY",false);return;}
 pill(90,y+4,32,40,"-",false);center(1,125,y+14,72,value,NOVA_WHITE);pill(200,y+4,32,40,"+",false);
 if(setting==6)for(unsigned x=0;x<66;x++)fill(128+(int)x,y+34,1,3,palette[x*255/65]);
}
static void draw_controls(void){
 for(unsigned row=0;row<CONTROLS_COUNT;row++){int y=CONTROLS_TOP+(int)row*CONTROLS_ROW_HEIGHT-controls_scroll;if(y+CONTROLS_ROW_HEIGHT>CONTROLS_TOP&&y<CONTROLS_BOTTOM)draw_setting(row+1,y);}
 /* Paint chrome last to clip partially visible rows and text without resizing
  * glyphs. Intersecting rows always remain inside the 240px raster surface. */
 fill(0,0,240,CONTROLS_TOP,0);fill(0,CONTROLS_BOTTOM,240,240-CONTROLS_BOTTOM,0);
 text(0,12,7,216,"CONTROLS",NOVA_CYAN);char name[48];snprintf(name,sizeof(name),"MIC / %u kHz MONO / %u kHz MAX",sample_rate/1000,sample_rate/2000);text(1,12,27,220,name,NOVA_CAP);
 fill(12,CONTROLS_TOP-1,216,1,NOVA_DIM);fill(12,CONTROLS_BOTTOM,216,1,NOVA_DIM);
 int track=CONTROLS_BOTTOM-CONTROLS_TOP,thumb=track*track/(CONTROLS_COUNT*CONTROLS_ROW_HEIGHT);fill(236,CONTROLS_TOP,2,track,NOVA_LINE);fill(236,CONTROLS_TOP+controls_scroll*(track-thumb)/CONTROLS_MAX_SCROLL,2,thumb,NOVA_CYAN);
 pill(79,201,82,32,"BACK",false);
}
