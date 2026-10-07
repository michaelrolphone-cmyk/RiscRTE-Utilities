#ifndef NOVA_RF_IDENTITY_H
#define NOVA_RF_IDENTITY_H
#include "rf_math.h"
/* Complete receiver compatibility identity. Display settings and coherent burst
 * length are deliberately absent: every signature uses the first256 pairs of
 * one burst. settings_id distinguishes known provider/calibration revisions. */
#define RF_IDENTITY_SIZE 64u
#define RF_IQ_FORMAT_S10_I0_Q10 1u
#define RF_RAW_AUTO UINT32_MAX
typedef struct {
 uint32_t lo_hz,sample_rate_hz,width_hz,raw_gain,rf_gain,bb_gain,filter,dc[4],iq_correction,lo_mode,settings_id,settings_flags;
 uint16_t algorithm_version,iq_format;
} rf_capture_identity;
_Static_assert(sizeof(rf_capture_identity)==RF_IDENTITY_SIZE,"fixed RF identity");
static inline uint32_t rf_signature_u32(const uint8_t *b){return b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static inline void rf_signature_put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(n>>(i*8));}
static inline uint16_t rf_store_u16(const uint8_t *b){return (uint16_t)(b[0]|((uint16_t)b[1]<<8));}
static inline void rf_store_put16(uint8_t *b,uint16_t n){b[0]=(uint8_t)n;b[1]=(uint8_t)(n>>8);}
static inline uint32_t rf_signature_crc(const uint8_t *b,size_t n){uint32_t crc=UINT32_MAX;for(size_t i=0;i<n;i++){crc^=b[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));}return ~crc;}
static inline rf_capture_identity rf_identity_default(void){return (rf_capture_identity){2440000000u,80000000u,40000000u,24u,RF_RAW_AUTO,RF_RAW_AUTO,0,{RF_RAW_AUTO,RF_RAW_AUTO,RF_RAW_AUTO,RF_RAW_AUTO},RF_RAW_AUTO,0,0,7,1,RF_IQ_FORMAT_S10_I0_Q10};}
static inline bool rf_identity_valid(const rf_capture_identity *i){
 if(!i||i->algorithm_version!=1||i->iq_format!=RF_IQ_FORMAT_S10_I0_Q10||(i->sample_rate_hz!=16000000u&&i->sample_rate_hz!=80000000u)||(i->width_hz!=20000000u&&i->width_hz!=40000000u)||i->lo_hz<=i->sample_rate_hz/2u||(uint64_t)i->lo_hz+i->sample_rate_hz/2u>UINT32_MAX||i->raw_gain>127|| (i->rf_gain!=RF_RAW_AUTO&&i->rf_gain>511)||(i->bb_gain!=RF_RAW_AUTO&&i->bb_gain>127)||(i->filter&~0x3f3fu)||(i->iq_correction!=RF_RAW_AUTO&&(i->iq_correction&~0x3f1fu))||(i->settings_flags&~7u))return false;
 for(unsigned k=0;k<4;k++)if(i->dc[k]!=RF_RAW_AUTO&&i->dc[k]>511)return false;
 return true;
}
static inline bool rf_identity_encode(const rf_capture_identity *i,uint8_t out[RF_IDENTITY_SIZE]){
 if(!out||!rf_identity_valid(i))return false;
 const uint32_t fields[15]={i->lo_hz,i->sample_rate_hz,i->width_hz,i->raw_gain,i->rf_gain,i->bb_gain,i->filter,i->dc[0],i->dc[1],i->dc[2],i->dc[3],i->iq_correction,i->lo_mode,i->settings_id,i->settings_flags};
 for(unsigned k=0;k<15;k++)rf_signature_put32(out+k*4,fields[k]);
 rf_store_put16(out+60,i->algorithm_version);rf_store_put16(out+62,i->iq_format);return true;
}
static inline bool rf_identity_decode(rf_capture_identity *out,const uint8_t *b,size_t n){
 if(!out||!b||n!=RF_IDENTITY_SIZE)return false;
 rf_capture_identity i={0};i.lo_hz=rf_signature_u32(b);i.sample_rate_hz=rf_signature_u32(b+4);i.width_hz=rf_signature_u32(b+8);i.raw_gain=rf_signature_u32(b+12);i.rf_gain=rf_signature_u32(b+16);i.bb_gain=rf_signature_u32(b+20);i.filter=rf_signature_u32(b+24);for(unsigned k=0;k<4;k++)i.dc[k]=rf_signature_u32(b+28+k*4);i.iq_correction=rf_signature_u32(b+44);i.lo_mode=rf_signature_u32(b+48);i.settings_id=rf_signature_u32(b+52);i.settings_flags=rf_signature_u32(b+56);i.algorithm_version=rf_store_u16(b+60);i.iq_format=rf_store_u16(b+62);
 if(!rf_identity_valid(&i))return false;
 *out=i;return true;
}
static inline bool rf_identity_equal(const rf_capture_identity *a,const rf_capture_identity *b){uint8_t aa[64],bb[64];return rf_identity_encode(a,aa)&&rf_identity_encode(b,bb)&&!memcmp(aa,bb,64);}
static inline uint32_t rf_identity_low_hz(const rf_capture_identity *i){return i->lo_hz-i->sample_rate_hz/2u;}
static inline uint32_t rf_identity_high_hz(const rf_capture_identity *i){return i->lo_hz+i->sample_rate_hz/2u;}
/*1..65535 is normalized full-passband position;0 always means no evidence. */
static inline uint32_t rf_coord_hz(const rf_capture_identity *i,uint16_t coordinate){if(!i||!coordinate)return 0;return rf_identity_low_hz(i)+(uint32_t)rf_dsp_div_u64_u32((uint64_t)(coordinate-1u)*i->sample_rate_hz,65534u);}
static inline uint16_t rf_hz_coord(const rf_capture_identity *i,uint32_t hz){if(!i||hz<rf_identity_low_hz(i)||hz>rf_identity_high_hz(i))return 0;return (uint16_t)(1u+rf_dsp_div_u64_u32((uint64_t)(hz-rf_identity_low_hz(i))*65534u+i->sample_rate_hz/2u,i->sample_rate_hz));}
#endif
