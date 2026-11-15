#include "ClockEditParticipants.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static bool valid(const uint8_t*c,const uint8_t*h,uint32_t floor,uint32_t zone,uint32_t provenance,uint32_t requested,bool exact_floor){(void)requested;(void)exact_floor;return c[0]==17&&h[0]==23&&floor<=100&&zone<419&&provenance==1;}
static const clock_participant_type types[]={{1,1,32,32,valid},{2,1,32,32,valid},{3,1,64,64,valid}};
static uint8_t config[64]={17},history[64]={23};
static clock_participant_record fixture(void){
 clock_participant_record r={.phase=1,.generation=4,.requested=80,.previous=100,.prior_phase=2,.prior_requested=90,.prior_previous=110,.count=6};
 for(unsigned i=0;i<6;i++)r.rows[i]=(clock_participant_row){i/3,i%3+1,1,70,1,1,config,history};
 return r;
}
static void rejected(const uint8_t*b,size_t n){clock_participant_record r;memset(&r,0x55,sizeof(r));clock_participant_record before=r;assert(!clock_participant_decode(&r,b,n,types,3,200));assert(!memcmp(&r,&before,sizeof(r)));}
static void mutate(uint8_t*b,size_t n,size_t offset,uint32_t value){clock_participant_write(b+offset,value);clock_participant_write(b+n-4,clock_participant_checksum(b,n-4));rejected(b,n);}
int main(void){
 uint8_t b[2048],bad[2048];size_t n=0;clock_participant_record r=fixture(),out;
 assert(clock_participant_encode(b,sizeof(b),&n,&r,types,3,200));assert(n==752);
 assert(clock_participant_decode(&out,b,n,types,3,200));assert(out.count==6&&out.generation==4);
 assert(!memcmp(out.rows[5].config,config,64));assert(!memcmp(out.rows[5].history,history,64));
 /* Decode/encode views may alias the destination without corrupting identities. */
 memcpy(bad,b,n);size_t n2=0;assert(clock_participant_encode(b,sizeof(b),&n2,&out,types,3,200));assert(n2==n&&!memcmp(b,bad,n));
 uint8_t scratch[2048];assert(clock_participant_encode_scratch(b,sizeof(b),&n2,&out,types,3,200,scratch,sizeof(scratch)));assert(!memcmp(b,bad,n));
 assert(!clock_participant_encode_scratch(b,sizeof(b),(size_t *)(void *)b,&out,types,3,200,scratch,sizeof(scratch)));assert(!memcmp(b,bad,n));
 assert(!clock_participant_encode_scratch(b,sizeof(b),&n2,&out,types,3,200,b,sizeof(b)));assert(!memcmp(b,bad,n));
 assert(!clock_participant_encode_scratch(b,sizeof(b),&n2,&out,types,3,200,scratch,n-1));assert(!memcmp(b,bad,n));
 /* Every actual allocation prefix is rejected, without reading its absent tail. */
 for(size_t i=0;i<n;i++){uint8_t*p=malloc(i?i:1);assert(p);memcpy(p,b,i);rejected(p,i);free(p);}
 for(size_t i=0;i<n;i++){memcpy(bad,b,n);bad[i]^=1;rejected(bad,n);}
 const struct{size_t offset;uint32_t value;} mutations[]={
 {4,2},{8,2048},{12,3},{16,0},{20,201},{24,201},{28,3},{28,0},{32,201},{36,201},{40,17},
 {44,2},{48,9},{52,2},{56,31},{60,31},{64,101},{68,419},{72,2},
 /* duplicate first family in second current row */ {44+96+4,1}};
 for(size_t i=0;i<sizeof(mutations)/sizeof(*mutations);i++){memcpy(bad,b,n);mutate(bad,n,mutations[i].offset,mutations[i].value);}
 /* Rechecks participant-owned encoded semantics after a valid envelope checksum. */
 memcpy(bad,b,n);bad[76]=0;clock_participant_write(bad+n-4,clock_participant_checksum(bad,n-4));rejected(bad,n);
 /* Trailing data cannot masquerade as part of a valid record. */
 memcpy(bad,b,n);bad[n]=0;clock_participant_write(bad+8,(uint32_t)n+1);clock_participant_write(bad+n-3,clock_participant_checksum(bad,n-3));rejected(bad,n+1);
 /* Output and written count are unchanged on every encoding refusal. */
 memset(bad,0x55,sizeof(bad));uint8_t before[2048];memcpy(before,bad,sizeof(bad));size_t written=77;
 assert(!clock_participant_encode(bad,n-1,&written,&r,types,3,200));assert(written==77&&!memcmp(before,bad,sizeof(bad)));
 clock_participant_type invalid[3];memcpy(invalid,types,sizeof(types));invalid[2].kind=1;
 assert(!clock_participant_encode(bad,sizeof(bad),&written,&r,invalid,3,200));
 memcpy(invalid,types,sizeof(types));invalid[0].validate=NULL;assert(!clock_participant_encode(bad,sizeof(bad),&written,&r,invalid,3,200));
 memcpy(invalid,types,sizeof(types));invalid[0].config_bytes=2048;assert(!clock_participant_encode(bad,sizeof(bad),&written,&r,invalid,3,200));
 r=fixture();r.phase=2;r.prior_phase=r.prior_requested=r.prior_previous=0;r.count=3;
 assert(clock_participant_encode(b,sizeof(b),&n,&r,types,3,200));assert(clock_participant_decode(&out,b,n,types,3,200));
 r=(clock_participant_record){.generation=5};assert(clock_participant_encode(b,sizeof(b),&n,&r,types,3,200));assert(n==48);assert(clock_participant_decode(&out,b,n,types,3,200));
 puts("Mixed clock participant codec passed");return 0;
}
