#include "../Apps/spectrum_store.h"
#include <assert.h>
#include <stdio.h>
static void checksum(uint8_t b[32]){spectrum_store_put16(b+30,spectrum_store_checksum(b));}
int main(void){
 spectrum_preferences p=spectrum_preferences_default(),q;uint8_t b[32],good[32];assert(spectrum_preferences_valid(&p));spectrum_preferences_encode(&p,b);assert(spectrum_preferences_decode(&q,b,32));assert(q.low_hz==20&&q.high_hz==8000&&q.fft_size==2048&&q.source==0&&q.log_amplitude&&q.gain_db==12);memcpy(good,b,32);
 for(unsigned i=0;i<32;i++){memcpy(b,good,32);b[i]^=0x80;assert(!spectrum_preferences_decode(&q,b,32));}
 for(unsigned n=0;n<32;n++)assert(!spectrum_preferences_decode(&q,good,n));
 assert(!spectrum_preferences_decode(&q,good,33));assert(!spectrum_preferences_decode(NULL,good,32));assert(!spectrum_preferences_decode(&q,NULL,32));
 const unsigned indexes[]={0,1,2,3,4,6,8,10,11,12,13,14,15,16,29};for(unsigned i=0;i<sizeof(indexes)/sizeof(*indexes);i++){memcpy(b,good,32);b[indexes[i]]=255;checksum(b);assert(!spectrum_preferences_decode(&q,b,32));}
 for(unsigned n=256;n<=8192;n*=2){p.fft_size=(uint16_t)n;for(int gain=-24;gain<=60;gain+=3){p.gain_db=(int8_t)gain;for(int t=-90;t<=-20;t+=5){p.threshold_db=(int8_t)t;spectrum_preferences_encode(&p,b);assert(spectrum_preferences_decode(&q,b,32));assert(q.fft_size==n&&q.gain_db==gain&&q.threshold_db==t);}}}
 p=spectrum_preferences_default();p.high_hz=20000;assert(!spectrum_preferences_valid(&p));p=spectrum_preferences_default();p.low_hz=p.high_hz;assert(!spectrum_preferences_valid(&p));p=spectrum_preferences_default();p.gain_db=1;assert(!spectrum_preferences_valid(&p));
 spectrum_label l=spectrum_label_default(1),m;assert(l.present&&l.frequency_hz==440&&!strcmp(l.name,"Tuning fork"));spectrum_label_encode(&l,b);memcpy(good,b,32);assert(spectrum_label_decode(&m,b,32));assert(m.present&&m.color==5&&!strcmp(m.name,l.name));
 for(unsigned i=0;i<32;i++){memcpy(b,good,32);b[i]^=1;assert(!spectrum_label_decode(&m,b,32));}for(unsigned n=0;n<32;n++)assert(!spectrum_label_decode(&m,good,n));
 for(unsigned i=0;i<8;i++){l=(spectrum_label){true,8000,(uint8_t)i,"ABCDEFGHIJKLMNOP"};spectrum_label_encode(&l,b);assert(spectrum_label_decode(&m,b,32));assert(strlen(m.name)==16&&m.color==i);}
 l=(spectrum_label){0};spectrum_label_encode(&l,b);assert(spectrum_label_decode(&m,b,32)&&!m.present);b[8]='x';checksum(b);assert(!spectrum_label_decode(&m,b,32));
 const char *bad[]={""," name","name ","bad\nname"};for(unsigned i=0;i<4;i++){l=spectrum_label_default(0);strcpy(l.name,bad[i]);assert(!spectrum_label_valid(&l));}l=spectrum_label_default(0);memset(l.name,'x',17);assert(!spectrum_label_valid(&l));l=spectrum_label_default(0);l.frequency_hz=8001;assert(!spectrum_label_valid(&l));l=spectrum_label_default(0);l.color=8;assert(!spectrum_label_valid(&l));
 memcpy(b,good,32);b[24]='x';checksum(b);assert(!spectrum_label_decode(&m,b,32));memcpy(b,good,32);b[29]=1;checksum(b);assert(!spectrum_label_decode(&m,b,32));
 for(unsigned i=0;i<8;i++){l=spectrum_label_default(i);spectrum_label_encode(&l,b);assert(spectrum_label_decode(&m,b,32));assert(m.present==(i<4));}
 puts("Spectrum storage: explicit endian records, checksum, all controls, eight labels, tombstones, validation and corruption tests passed");return 0;
}
