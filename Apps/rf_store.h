#ifndef NOVA_RF_STORE_H
#define NOVA_RF_STORE_H
#include "rf_dsp.h"
#define RF_LABEL_MAX 8u
#define RF_LABEL_NAME_MAX 16u
#define RF_PREFERENCES_SIZE 128u
#define RF_LABEL_RECORD_SIZE 48u
enum {RF_BACKGROUND_RAW,RF_BACKGROUND_AUTO,RF_BACKGROUND_MANUAL};
typedef struct {
 rf_capture_identity identity;
 uint32_t low_hz,high_hz;
 uint16_t fft_size;
 uint8_t source,window,palette,background_mode;
 int8_t manual_room;
 bool log_frequency,log_amplitude,show_labels;
 int8_t gain_db,threshold_db;
} rf_preferences;
typedef struct {bool present;uint32_t frequency_hz,tolerance_hz;uint16_t tolerance_bins;uint8_t color;char name[17];} rf_label;
static inline rf_preferences rf_preferences_default(void){rf_dsp_config c=rf_dsp_defaults();return (rf_preferences){c.identity,c.low_hz,c.high_hz,c.fft_size,0,(uint8_t)c.window,0,RF_BACKGROUND_RAW,-1,c.log_frequency,c.log_amplitude,true,c.gain_db,c.threshold_db};}
static inline bool rf_preferences_valid(const rf_preferences *p){if(!p||p->source||p->palette>4||p->background_mode>RF_BACKGROUND_MANUAL||p->manual_room< -1||p->manual_room>=8||p->gain_db%3||p->threshold_db%5)return false;rf_dsp_config c={p->identity,p->low_hz,p->high_hz,p->fft_size,(rf_dsp_window)p->window,p->log_frequency,p->log_amplitude,p->gain_db,p->threshold_db};return rf_dsp_config_valid(&c);}
static inline bool rf_preferences_encode(const rf_preferences *p,uint8_t b[RF_PREFERENCES_SIZE]){if(!b||!rf_preferences_valid(p))return false;memset(b,0,RF_PREFERENCES_SIZE);memcpy(b,"RFP1",4);b[4]=1;(void)rf_identity_encode(&p->identity,b+8);rf_signature_put32(b+72,p->low_hz);rf_signature_put32(b+76,p->high_hz);rf_store_put16(b+80,p->fft_size);b[82]=p->window;b[83]=p->palette;b[84]=(uint8_t)(p->gain_db+24);b[85]=(uint8_t)(p->threshold_db+90);b[86]=(uint8_t)(p->log_frequency|(p->log_amplitude<<1)|(p->show_labels<<2));b[87]=p->background_mode;b[88]=(uint8_t)(p->manual_room+1);rf_signature_put32(b+124,rf_signature_crc(b,124));return true;}
static inline bool rf_preferences_decode(rf_preferences *out,const uint8_t *b,size_t n){
 if(!out||!b||n!=RF_PREFERENCES_SIZE||memcmp(b,"RFP1",4)||b[4]!=1||b[5]||b[6]||b[7]||b[84]>84||b[85]>70||b[86]>7||b[88]>8||rf_signature_u32(b+124)!=rf_signature_crc(b,124))return false;
 for(unsigned i=89;i<124;i++)if(b[i])return false;
 rf_preferences p={0};if(!rf_identity_decode(&p.identity,b+8,64))return false;p.low_hz=rf_signature_u32(b+72);p.high_hz=rf_signature_u32(b+76);p.fft_size=rf_store_u16(b+80);p.window=b[82];p.palette=b[83];p.gain_db=(int8_t)((int)b[84]-24);p.threshold_db=(int8_t)((int)b[85]-90);p.log_frequency=!!(b[86]&1);p.log_amplitude=!!(b[86]&2);p.show_labels=!!(b[86]&4);p.background_mode=b[87];p.manual_room=(int8_t)((int)b[88]-1);if(!rf_preferences_valid(&p))return false;*out=p;return true;
}
static inline bool rf_label_valid(const rf_label *l){if(!l)return false;if(!l->present)return true;if(!l->frequency_hz||l->color>=8||l->tolerance_hz>80000000u||l->tolerance_bins>8192u||(!l->tolerance_hz&&!l->tolerance_bins)||!l->name[0]||l->name[0]==' ')return false;unsigned n=0;for(;n<=16&&l->name[n];n++)if((unsigned char)l->name[n]<32||(unsigned char)l->name[n]>126)return false;return n&&n<=16&&l->name[n-1]!=' ';}
static inline bool rf_label_encode(const rf_label *l,uint8_t b[RF_LABEL_RECORD_SIZE]){if(!b||!rf_label_valid(l))return false;memset(b,0,RF_LABEL_RECORD_SIZE);memcpy(b,"RFL1",4);b[4]=1;if(l->present){b[5]=1;b[6]=l->color;rf_signature_put32(b+8,l->frequency_hz);rf_signature_put32(b+12,l->tolerance_hz);rf_store_put16(b+16,l->tolerance_bins);memcpy(b+20,l->name,strlen(l->name));}rf_signature_put32(b+44,rf_signature_crc(b,44));return true;}
static inline bool rf_label_decode(rf_label *out,const uint8_t *b,size_t n){
 if(!out||!b||n!=RF_LABEL_RECORD_SIZE||memcmp(b,"RFL1",4)||b[4]!=1||b[5]>1||b[7]||b[18]||b[19]||rf_signature_u32(b+44)!=rf_signature_crc(b,44))return false;
 for(unsigned i=37;i<44;i++)if(b[i])return false;
 rf_label l={0};if(!b[5]){for(unsigned i=6;i<44;i++)if(b[i])return false;}else{l.present=true;l.color=b[6];l.frequency_hz=rf_signature_u32(b+8);l.tolerance_hz=rf_signature_u32(b+12);l.tolerance_bins=rf_store_u16(b+16);memcpy(l.name,b+20,17);if(!rf_label_valid(&l))return false;unsigned z=0;while(z<17&&l.name[z])++z;for(;z<17;z++)if(b[20+z])return false;}*out=l;return true;
}
static inline rf_label rf_label_default(unsigned i){(void)i;return (rf_label){0};}
#endif
