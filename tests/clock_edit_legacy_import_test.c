#include "ClockEditLegacyImport.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static void seal(uint8_t*b){alarm_write32(b+312,clock_participant_checksum(b,312));}
static void refused(const uint8_t*b,size_t n){clock_participant_record r;memset(&r,0x55,sizeof(r));clock_participant_record before=r;assert(!clock_participant_import_ace2(&r,b,n));assert(!memcmp(&r,&before,sizeof(r)));}
int main(void){
 uint8_t b[316]={0},copy[316];memcpy(b,"ACE2",4);alarm_write32(b+4,1);alarm_write32(b+8,2);alarm_write32(b+12,4);alarm_write32(b+16,80);alarm_write32(b+20,100);
 alarm_config config={.revision=2,.deadline=200,.created=100,.kind=1,.enabled=1};alarm_config_encode(&config,b+40);alarm_write32(b+104,80);seal(b);
 clock_participant_record r;memcpy(copy,b,316);assert(clock_participant_import_ace2(&r,b,316));assert(r.phase==2&&r.count==2&&r.rows[0].floor==80&&r.rows[1].provenance==CLOCK_CONFIG_ABSENT);assert(!memcmp(copy,b,316));
 for(size_t n=0;n<316;n++){uint8_t*p=malloc(n?n:1);memcpy(p,b,n);refused(p,n);free(p);}
 for(size_t i=0;i<316;i++){memcpy(copy,b,316);copy[i]^=1;refused(copy,316);}
 memcpy(copy,b,316);alarm_write32(copy+104,79);seal(copy);refused(copy,316);
 /* INTENT is preserved and prior rows are kept, never promoted to verified. */
 alarm_write32(b+8,1);alarm_write32(b+24,2);alarm_write32(b+28,90);alarm_write32(b+32,110);memcpy(b+176,b+40,68);alarm_write32(b+240,90);seal(b);
 assert(clock_participant_import_ace2(&r,b,316));assert(r.phase==1&&r.prior_phase==2&&r.count==4&&r.rows[2].floor==90);
 for(unsigned i=0;i<r.count;i++)assert(r.rows[i].kind!=3); /* no Points authorization */
 /* Unknown new envelope is refused by the historical-format importer. */
 memcpy(copy,b,316);memcpy(copy,"ACM2",4);seal(copy);refused(copy,316);
 puts("ACE2 exact read-only compatibility import passed");return 0;
}
