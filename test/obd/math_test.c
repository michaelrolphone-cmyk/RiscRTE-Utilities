#include <stdint.h>
#include <assert.h>
#include <stdio.h>
uint64_t obd_unsigned_divide(uint64_t,uint64_t);
int64_t obd_signed_divide(int64_t,int64_t);
int main(void){
 uint64_t seed=0x5963657281346915ULL;
 const uint64_t cases[]={0,1,2,3,0x7fffffff,0x80000000,UINT32_MAX,UINT64_C(1)<<63,INT64_MAX,UINT64_MAX};
 for(unsigned i=0;i<10;i++)for(unsigned j=1;j<10;j++)assert(obd_unsigned_divide(cases[i],cases[j])==cases[i]/cases[j]);
 for(unsigned i=0;i<100000;i++){
  seed=seed*6364136223846793005ULL+1;uint64_t a=seed;seed=seed*6364136223846793005ULL+1;uint64_t b=seed?seed:1;
  assert(obd_unsigned_divide(a,b)==a/b);int64_t sa=(int64_t)a,sb=(int64_t)b;
  if(sa!=INT64_MIN||sb!=-1)assert(obd_signed_divide(sa,sb)==sa/sb);
 }
 assert(obd_unsigned_divide(1,0)==0);assert(obd_signed_divide(INT64_MIN,-1)==INT64_MIN);
 puts("PIC division: boundary values and 100000 signed/unsigned reference comparisons PASS");return 0;
}
