#ifndef SPECTRUM_STORE_H
#define SPECTRUM_STORE_H
/* Independent, fixed-size, versioned records. No struct serialization, heap or
 * multi-key commit: each label has its own explicit tombstone. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define SPECTRUM_LABEL_MAX 8u
#define SPECTRUM_LABEL_NAME_MAX 16u
#define SPECTRUM_PREFERENCES_SIZE 32u
#define SPECTRUM_LABEL_RECORD_SIZE 32u
typedef struct {
 uint16_t low_hz,high_hz,fft_size;
 uint8_t source,window,palette; /* source is a reserved legacy byte; live is 0. */
 bool log_frequency,log_amplitude,show_labels;
 int8_t gain_db,threshold_db;
} spectrum_preferences;
typedef struct {bool present;uint16_t frequency_hz;uint8_t color;char name[17];} spectrum_label;
static inline bool spectrum_store_option(uint16_t n,const uint16_t *values,unsigned count){for(unsigned i=0;i<count;i++)if(values[i]==n)return true;return false;}
static inline bool spectrum_preferences_valid(const spectrum_preferences *p){
 static const uint16_t low[]={0,20,50,100,200,500,1000},high[]={1000,2000,5000,8000};
 return p && spectrum_store_option(p->low_hz,low,7) && spectrum_store_option(p->high_hz,high,4) && p->low_hz<p->high_hz && p->fft_size>=256 && p->fft_size<=8192 && !(p->fft_size&(p->fft_size-1)) && p->source<=1 && p->window<=4 && p->palette<=4 && p->gain_db>=-24 && p->gain_db<=60 && p->gain_db%3==0 && p->threshold_db>=-90 && p->threshold_db<=-20 && p->threshold_db%5==0;
}
static inline spectrum_preferences spectrum_preferences_default(void){return (spectrum_preferences){20,8000,2048,0,1,0,true,true,true,12,-60};}
static inline uint16_t spectrum_store_u16(const uint8_t *b){return (uint16_t)(b[0]|((uint16_t)b[1]<<8));}
static inline void spectrum_store_put16(uint8_t *b,uint16_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);}
static inline uint16_t spectrum_store_checksum(const uint8_t *b){uint16_t n=0x7631;for(unsigned i=0;i<30;i++)n=(uint16_t)((n^b[i])*257u+17u);return n;}
static inline bool spectrum_store_header(const uint8_t *b,size_t size,uint8_t kind){return b && size==32 && b[0]=='S' && b[1]=='P' && b[2]==1 && b[3]==kind && spectrum_store_u16(b+30)==spectrum_store_checksum(b);}
static inline void spectrum_preferences_encode(const spectrum_preferences *p,uint8_t b[32]){
 memset(b,0,32);b[0]='S';b[1]='P';b[2]=1;b[3]=1;spectrum_store_put16(b+4,p->low_hz);spectrum_store_put16(b+6,p->high_hz);spectrum_store_put16(b+8,p->fft_size);b[10]=(uint8_t)(p->gain_db+24);b[11]=(uint8_t)(p->threshold_db+90);b[12]=0;b[13]=p->window;b[14]=p->palette;b[15]=(uint8_t)(p->log_frequency|(p->log_amplitude<<1)|(p->show_labels<<2));spectrum_store_put16(b+30,spectrum_store_checksum(b));
}
static inline bool spectrum_preferences_decode(spectrum_preferences *out,const uint8_t *b,size_t n){
 if(!out || !spectrum_store_header(b,n,1) || b[10]>84 || b[11]>70 || b[15]>7)return false;
 for(unsigned i=16;i<30;i++)if(b[i])return false;
 spectrum_preferences p={spectrum_store_u16(b+4),spectrum_store_u16(b+6),spectrum_store_u16(b+8),b[12],b[13],b[14],!!(b[15]&1),!!(b[15]&2),!!(b[15]&4),(int8_t)((int)b[10]-24),(int8_t)((int)b[11]-90)};if(!spectrum_preferences_valid(&p))return false;
 /* Accept checksummed v1 records from the removed source option. Preserve all
  * other settings and leave the original record intact until an explicit edit. */
 p.source=0;*out=p;return true;
}
static inline bool spectrum_label_valid(const spectrum_label *l){if(!l)return false;if(!l->present)return true;if(l->frequency_hz>8000 || l->color>=8 || !l->name[0] || l->name[0]==' ')return false;unsigned n=0;for(;n<=16 && l->name[n];n++)if((unsigned char)l->name[n]<32 || (unsigned char)l->name[n]>126)return false;return n && n<=16 && l->name[n-1]!=' ';}
static inline void spectrum_label_encode(const spectrum_label *l,uint8_t b[32]){
 memset(b,0,32);b[0]='S';b[1]='P';b[2]=1;b[3]=2;if(l->present){b[4]=1;b[5]=l->color;spectrum_store_put16(b+6,l->frequency_hz);for(unsigned i=0;i<16 && l->name[i];i++)b[8+i]=(uint8_t)l->name[i];}spectrum_store_put16(b+30,spectrum_store_checksum(b));
}
static inline bool spectrum_label_decode(spectrum_label *out,const uint8_t *b,size_t n){
 if(!out || !spectrum_store_header(b,n,2) || b[4]>1)return false;
 for(unsigned i=25;i<30;i++)if(b[i])return false;
 spectrum_label l={0};
 if(!b[4]){for(unsigned i=5;i<25;i++)if(b[i])return false;}else{l.present=true;l.color=b[5];l.frequency_hz=spectrum_store_u16(b+6);memcpy(l.name,b+8,17);if(!spectrum_label_valid(&l))return false;unsigned z=0;while(z<17 && l.name[z])z++;for(;z<17;z++)if(b[8+z])return false;}*out=l;return true;
}
static inline spectrum_label spectrum_label_default(unsigned i){
 static const spectrum_label defaults[4]={{true,120,1,"Mains hum"},{true,440,5,"Tuning fork"},{true,1000,6,"Test tone"},{true,3200,2,"Alarm"}};return i<4?defaults[i]:(spectrum_label){0};
}
#endif
