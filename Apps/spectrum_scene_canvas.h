#ifndef SPECTRUM_SCENE_CANVAS_H
#define SPECTRUM_SCENE_CANVAS_H
/* Responsive logical chart content over published ui.canvas. No physical APIs.
 * Glyphs derive unchanged from public bf111 Apps/daily_draw.h. */
#include "RiscCanvasV1.h"
#include "spectrum_dsp.h"
#include "spectrum_store.h"
#include <limits.h>
#include <stdio.h>
static inline const uint8_t *spectrum_canvas_glyph(char c) {
    static const uint8_t digits[10][5] = {
        {62,81,73,69,62}, {0,66,127,64,0}, {98,81,73,73,70},
        {34,65,73,73,54}, {24,20,18,127,16}, {39,69,69,69,57},
        {60,74,73,73,48}, {1,113,9,5,3}, {54,73,73,73,54}, {6,73,73,41,30}
    };
    static const uint8_t letters[26][5] = {
        {126,9,9,9,126}, {127,73,73,73,54}, {62,65,65,65,34},
        {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1},
        {62,65,73,73,58}, {127,8,8,8,127}, {0,65,127,65,0},
        {32,64,65,63,1}, {127,8,20,34,65}, {127,64,64,64,64},
        {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62},
        {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70},
        {38,73,73,73,50}, {1,1,127,1,1}, {63,64,64,64,63},
        {31,32,64,32,31}, {63,64,56,64,63}, {99,20,8,20,99},
        {7,8,112,8,7}, {97,81,73,69,67}
    };
    static const uint8_t blank[5] = {0}, unknown[5] = {2,1,81,9,6};
    static const uint8_t dot[5] = {0,96,96,0,0}, colon[5] = {0,54,54,0,0};
    static const uint8_t plus[5] = {8,8,62,8,8}, minus[5] = {8,8,8,8,8};
    static const uint8_t slash[5] = {64,32,16,8,4}, equals[5] = {20,20,20,20,20};
    if (c >= '0' && c <= '9') return digits[(unsigned)(c - '0')];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return letters[(unsigned)(c - 'A')];
    switch (c) {
        case ' ': return blank;
        case '.': return dot;
        case ':': return colon;
        case '+': return plus;
        case '-': return minus;
        case '/': return slash;
        case '=': return equals;
        case '*': return letters['X' - 'A'];
        default: return unknown;
    }
}

typedef struct {
 const spectrum_dsp_config *config;
 const spectrum_preferences *preferences;
 const uint16_t *levels,*peaks;
 const int16_t *db,*waterfall_db;
 uint32_t level_count;
 const uint8_t *history;
 uint32_t history_frames,history_bands,history_next,history_count;
 const uint32_t *palette,*label_colors;
 const spectrum_label *labels;
 const bool *label_active;
 unsigned view;
 bool running,frozen,cursor_visible;
 uint16_t cursor_hz;
 const char *status,*room,*event;
} spectrum_canvas_model;
typedef struct {uint32_t width,height,revision,scale;int x,y,w,h;uint16_t low_hz,high_hz;bool log_frequency;unsigned view;spectrum_dsp_config config;bool cursor_visible;uint16_t cursor_hz;} spectrum_canvas_layout;
enum {SC_NONE,SC_VIEW,SC_TOGGLE,SC_FREEZE,SC_CONTROLS,SC_BACK,SC_LABEL,SC_CURSOR,SC_HIDE_CURSOR,SC_SHARED_CONTROLS};
typedef struct {unsigned action;int32_t value;} spectrum_canvas_command;
typedef struct {uint64_t contact,sequence;uint32_t revision;int down_x,down_y;unsigned pressed;bool down,plot,moved,blocked,defer_cursor;} spectrum_canvas_input;
static inline unsigned sc_ratio(unsigned a,unsigned b,unsigned d){return d?(unsigned)spectrum_dsp_div_u64_u32((uint64_t)a*b,d):0;}
static inline uint32_t sc_fade(uint32_t c,unsigned n){return ((((c>>16)&255u)*n/255u)<<16)|((((c>>8)&255u)*n/255u)<<8)|((c&255u)*n/255u);}
static inline void sc_pixel(risc_canvas_frame_v1*f,int x,int y,uint32_t c){
 if(x<0||y<0||(unsigned)x>=f->width||(unsigned)y>=f->height)return;
 uint8_t*p=(uint8_t*)f->pixels+(size_t)y*f->stride;
 if(f->encoding==RISC_CANVAS_RGBA8){p+=(size_t)x*4;p[0]=(uint8_t)(c>>16);p[1]=(uint8_t)(c>>8);p[2]=(uint8_t)c;p[3]=255;}
 else {static const uint8_t d[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};unsigned lum=(((c>>16)&255u)*77u+((c>>8)&255u)*150u+(c&255u)*29u)>>8;uint8_t bit=(uint8_t)(128u>>(x&7));if(lum>(unsigned)d[((unsigned)y&3u)*4u+((unsigned)x&3u)]*16u)p[(unsigned)x/8]|=bit;else p[(unsigned)x/8]&=(uint8_t)~bit;}
}
static inline void sc_fill(risc_canvas_frame_v1*f,int x,int y,int w,int h,uint32_t c){for(int j=0;j<h;j++)for(int i=0;i<w;i++)sc_pixel(f,x+i,y+j,c);}
static inline void sc_text(risc_canvas_frame_v1*f,int x,int y,int max,const char*s,unsigned scale,uint32_t c){if(!s)return;for(unsigned k=0;s[k]&&max>=(int)(5u*scale);k++,x+=(int)(6u*scale),max-=(int)(6u*scale)){const uint8_t*g=spectrum_canvas_glyph(s[k]);for(unsigned i=0;i<5;i++)for(unsigned j=0;j<7;j++)if(g[i]&(1u<<j))sc_fill(f,x+(int)(i*scale),y+(int)(j*scale),(int)scale,(int)scale,c);}}
static inline void sc_line(risc_canvas_frame_v1*f,int x,int y,int xx,int yy,uint32_t c){int dx=xx>x?xx-x:x-xx,sx=x<xx?1:-1,dy=yy>y?y-yy:yy-y,sy=y<yy?1:-1,e=dx+dy;for(;;){sc_pixel(f,x,y,c);if(x==xx&&y==yy)break;int z=2*e;if(z>=dy){e+=dy;x+=sx;}if(z<=dx){e+=dx;y+=sy;}}}
static inline bool spectrum_canvas_render(risc_canvas_frame_v1*f,const spectrum_canvas_model*m,spectrum_canvas_layout*l){
 if(!f||!m||!l||!m->config||!spectrum_dsp_config_valid(m->config)||!m->preferences||!f->pixels||f->struct_size!=sizeof(*f)||!f->geometry_revision||f->width>INT_MAX/4u||f->height>INT_MAX/4u||f->width<48||f->height<80)return false;
 if(f->encoding!=RISC_CANVAS_RGBA8&&f->encoding!=RISC_CANVAS_MONO1)return false;
 size_t row=f->encoding==RISC_CANVAS_RGBA8?(size_t)f->width*4:(f->width+7u)/8u;
 if(f->stride<row||f->height>SIZE_MAX/f->stride||f->byte_count<(size_t)f->height*f->stride)return false;
 unsigned scale=(f->width<f->height?f->width:f->height)/180u;if(!scale)scale=1;if(scale>4)scale=4;
 *l=(spectrum_canvas_layout){f->width,f->height,f->geometry_revision,scale,(int)(3*scale),(int)(25*scale),(int)f->width-(int)(6*scale),(int)f->height-(int)(58*scale),m->config->low_hz,m->config->high_hz,m->config->log_frequency,m->view,*m->config,m->cursor_visible,m->cursor_hz};
 if(l->h<8){l->scale=scale=1;l->x=2;l->y=20;l->w=(int)f->width-4;l->h=(int)f->height-48;if(l->h<8)return false;}
 memset(f->pixels,0,(size_t)f->height*f->stride);sc_fill(f,0,0,(int)f->width,(int)f->height,0);
 const char*tabs[]={"SPECTRUM","WATERFALL","MONITOR","CONTROLS"};
 for(unsigned i=0;i<4;i++){int x=(int)sc_ratio(i,f->width,4);sc_text(f,x+2,2,(int)(f->width/4)-4,tabs[i],scale,i==m->view?0x19e3ff:0xcfe9ee);}
 sc_text(f,3,(int)(11*scale),(int)f->width-6,m->status,scale,0xcfe9ee);
 if(m->view==1&&m->history&&m->palette&&m->history_frames&&m->history_bands&&m->history_count<=m->history_frames&&m->history_next<m->history_frames){
  for(int x=0;x<l->w;x++){unsigned at=sc_ratio((unsigned)x,m->history_frames,(unsigned)l->w);if(at<m->history_frames-m->history_count)continue;unsigned source=(m->history_next+at)%m->history_frames;for(int y=0;y<l->h;y++){unsigned band=sc_ratio((unsigned)y,m->history_bands,(unsigned)l->h);sc_pixel(f,l->x+x,l->y+y,m->palette[m->history[(size_t)source*m->history_bands+band]]);}}
 }else if(m->levels&&m->peaks&&m->level_count){int prev=l->y+l->h-1,peak=prev;for(int x=0;x<l->w;x++){unsigned k=sc_ratio((unsigned)x,m->level_count,(unsigned)l->w);unsigned a=m->levels[k]>32767?32767:m->levels[k],b=m->peaks[k]>32767?32767:m->peaks[k];int y=l->y+l->h-1-(int)sc_ratio(a,(unsigned)(l->h-1),32767),py=l->y+l->h-1-(int)sc_ratio(b,(unsigned)(l->h-1),32767);sc_fill(f,l->x+x,y+1,1,l->y+l->h-y-1,0x083340);if(x){sc_line(f,l->x+x-1,prev,l->x+x,y,0x19e3ff);sc_line(f,l->x+x-1,peak,l->x+x,py,0xffd24a);}prev=y;peak=py;}}
 /* Domain scale and labels are mapped to current logical extent. */
 unsigned dim=m->view==1?(unsigned)l->h:(unsigned)l->w;
 for(unsigned i=0;i<5;i++){unsigned at=sc_ratio(i,dim-1,4);uint16_t hz=spectrum_dsp_frequency_at(m->config,at,dim);char b[24];snprintf(b,sizeof(b),"%uHz",(unsigned)hz);if(m->view==1)sc_text(f,l->x+1,l->y+(int)(at>(unsigned)(l->h-(int)(7*scale))?(unsigned)(l->h-(int)(7*scale)):at),l->w,b,scale,0xcfe9ee);else sc_text(f,l->x+(int)at,l->y+l->h-(int)(8*scale),l->w-(int)at,b,scale,0xcfe9ee);}
 if(m->view!=1)for(unsigned tick=0;tick<(m->preferences->log_amplitude?5u:4u);tick++){char b[16];if(m->preferences->log_amplitude)snprintf(b,sizeof(b),"-%udB",tick*20u);else snprintf(b,sizeof(b),"%u%%",100u-tick*25u);sc_text(f,l->x+1,l->y+(int)sc_ratio(tick*(m->preferences->log_amplitude?20u:25u),(unsigned)(l->h-16*scale),m->preferences->log_amplitude?90u:100u),l->w,b,scale,0xcfe9ee);}
 if(m->preferences->show_labels&&m->labels&&m->label_colors)for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++){const spectrum_label*q=&m->labels[i];if(!q->present||q->color>=8||q->frequency_hz<m->config->low_hz||q->frequency_hz>m->config->high_hz)continue;unsigned at=spectrum_dsp_column_at(m->config,q->frequency_hz,dim);uint32_t c=m->label_colors[q->color];if(!m->label_active||!m->label_active[i])c=sc_fade(c,120);if(m->view==1){for(int x=0;x<l->w;x+=6)sc_fill(f,l->x+x,l->y+(int)at,3,1,c);sc_text(f,l->x+4,l->y+(int)at,l->w-4,q->name,scale,c);}else{for(int y=0;y<l->h;y+=6)sc_fill(f,l->x+(int)at,l->y+y,1,3,c);sc_text(f,l->x+(int)at,l->y+(int)((i%3)*8*scale),l->w-(int)at,q->name,scale,c);}}
 if(m->cursor_visible){unsigned at=spectrum_dsp_column_at(m->config,m->cursor_hz,dim);if(m->view==1)sc_line(f,l->x,l->y+(int)at,l->x+l->w-1,l->y+(int)at,0xffffff);else sc_line(f,l->x+(int)at,l->y,l->x+(int)at,l->y+l->h-1,0xffffff);char b[48];int db=0;if(m->view==1&&m->waterfall_db&&m->history_bands)db=m->waterfall_db[spectrum_dsp_column_at(m->config,m->cursor_hz,m->history_bands)]/100;else if(m->db&&m->level_count)db=m->db[spectrum_dsp_column_at(m->config,m->cursor_hz,m->level_count)]/100;snprintf(b,sizeof(b),"%u Hz %d dB / LABEL",(unsigned)m->cursor_hz,db);sc_text(f,l->x+2,l->y+2,l->w-4,b,scale,0xffffff);}
 int bottom=l->y+l->h+2;sc_text(f,3,bottom,(int)f->width-6,m->room,scale,0xcfe9ee);sc_text(f,3,bottom+(int)(8*scale),(int)f->width-6,m->event,scale,0x3dff9a);
 const char*buttons[]={m->running?"STOP":"START",m->frozen?"RESUME":"FREEZE","LABEL","BACK"};for(unsigned i=0;i<4;i++)sc_text(f,(int)sc_ratio(i,f->width,4)+2,(int)f->height-(int)(8*scale),(int)(f->width/4)-4,buttons[i],scale,0xffffff);
 return true;
}
static inline unsigned sc_hit(const spectrum_canvas_layout*l,int x,int y){if(x<0||y<0||(unsigned)x>=l->width||(unsigned)y>=l->height)return 0;unsigned col=sc_ratio((unsigned)x,4,l->width);if(y<(int)(10*l->scale))return 100+col;if(y>=(int)l->height-(int)(10*l->scale))return 200+col;return 0;}
static inline spectrum_canvas_command sc_button(unsigned n){if(n>=100&&n<103)return(spectrum_canvas_command){SC_VIEW,(int32_t)(n-100)};if(n==103)return(spectrum_canvas_command){SC_CONTROLS,0};if(n>=200&&n<204){static const unsigned a[]={SC_TOGGLE,SC_FREEZE,SC_LABEL,SC_BACK};return(spectrum_canvas_command){a[n-200],0};}return(spectrum_canvas_command){0};}
static inline spectrum_canvas_command spectrum_canvas_event(const risc_canvas_event_v1*e,const spectrum_canvas_layout*l,spectrum_canvas_input*s){
 spectrum_canvas_command none={0};if(!e||!l||!s||e->struct_size!=sizeof(*e)||!e->sequence||e->sequence<=s->sequence)return none;s->sequence=e->sequence;
 if(e->kind==RISC_CANVAS_GEOMETRY_CHANGED||e->kind==RISC_CANVAS_INPUT_GAP||e->geometry_revision!=l->revision){s->down=false;s->blocked=true;return none;}
 if(e->kind==RISC_CANVAS_CONTROLS_REQUEST){s->down=false;return(spectrum_canvas_command){SC_SHARED_CONTROLS,0};}
 if(e->kind==RISC_CANVAS_NAVIGATION){if(s->blocked)return none;uint32_t p=e->navigation.pressed;if(p&RISC_NAV_HOME)return(spectrum_canvas_command){SC_BACK,0};if(p&RISC_NAV_BACK)return(spectrum_canvas_command){SC_CONTROLS,0};if(p&RISC_NAV_DOWN)return(spectrum_canvas_command){SC_HIDE_CURSOR,0};if(p&RISC_NAV_CONFIRM)return(spectrum_canvas_command){SC_TOGGLE,0};if(p&RISC_NAV_RIGHT)return(spectrum_canvas_command){SC_VIEW,(int32_t)((l->view+1)%3)};if(p&RISC_NAV_LEFT)return(spectrum_canvas_command){SC_VIEW,(int32_t)((l->view+2)%3)};return none;}
 if(e->kind!=RISC_CANVAS_CONTACT)return none;
 if(e->phase==RISC_CANVAS_CANCEL){s->down=false;s->blocked=true;return none;}
 if(e->phase==RISC_CANVAS_DOWN){if(s->down){s->down=false;s->blocked=true;return none;}if(s->blocked)return none;s->down=true;s->contact=e->contact;s->revision=e->geometry_revision;s->down_x=e->x;s->down_y=e->y;s->pressed=sc_hit(l,e->x,e->y);s->moved=false;s->defer_cursor=false;s->plot=!s->pressed&&e->x>=l->x&&e->x<l->x+l->w&&e->y>=l->y&&e->y<l->y+l->h;if(s->plot&&l->cursor_visible){int at=l->view==1?e->y-l->y:e->x-l->x;int cursor=(int)spectrum_dsp_column_at(&l->config,l->cursor_hz,(unsigned)(l->view==1?l->h:l->w));s->defer_cursor=at-cursor>(int)(14*l->scale)||cursor-at>(int)(14*l->scale);}}
 if(!s->down||s->contact!=e->contact)return none;
 if(e->phase==RISC_CANVAS_MOVE||e->phase==RISC_CANVAS_UP){int64_t dx=(int64_t)e->x-s->down_x,dy=(int64_t)e->y-s->down_y;if(dx>6||dx< -6||dy>6||dy< -6)s->moved=true;}
 if(s->plot){if(s->defer_cursor&&!s->moved){if(e->phase==RISC_CANVAS_UP){s->down=false;return(spectrum_canvas_command){SC_HIDE_CURSOR,0};}return none;}int64_t at=l->view==1?(int64_t)e->y-l->y:(int64_t)e->x-l->x;int dim=l->view==1?l->h:l->w;if(at<0)at=0;if(at>=dim)at=dim-1;spectrum_dsp_config c=l->config;uint16_t hz=spectrum_dsp_frequency_at(&c,(unsigned)at,(unsigned)dim);if(e->phase==RISC_CANVAS_UP)s->down=false;return(spectrum_canvas_command){SC_CURSOR,hz};}
 if(e->phase==RISC_CANVAS_UP){s->down=false;if(!s->moved&&s->pressed==sc_hit(l,e->x,e->y))return sc_button(s->pressed);}return none;
}
/* Root calls only after a successful provider contacts snapshot proves neutral. */
static inline void spectrum_canvas_neutral(spectrum_canvas_input*s){s->down=false;s->blocked=false;}
#endif
