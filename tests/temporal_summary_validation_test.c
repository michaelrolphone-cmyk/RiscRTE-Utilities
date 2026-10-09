/* Differential oracle frozen from fdd214b6c4bd3da97763234195c65b905064e6c1.
 * Only function names/calls are prefixed old_; do not update these references
 * when changing the production implementation. In particular they retain the
 * original whole-example validator copies and spectrum/RF acceptance differences.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "../Apps/spectrum_temporal_store.h"
#include "../Apps/rf_temporal_store.h"

/* Baseline public model layouts, independently measured at fdd214b6. */
#define ABI_SIZE(type, value) _Static_assert(sizeof(type) == (value), #type " ABI size")
#define ABI_OFFSET(type, member, value) \
 _Static_assert(offsetof(type, member) == (value), #type "." #member " ABI offset")
ABI_SIZE(st_frame, 38); ABI_SIZE(st_example, 2464);
ABI_SIZE(st_label, 14808); ABI_SIZE(st_library, 118472);
ABI_SIZE(rt_frame, 44); ABI_SIZE(rt_example, 2852);
ABI_SIZE(rt_label, 17136); ABI_SIZE(rt_library, 137160);
#define COMMON_EXAMPLE_ABI(type) \
 ABI_OFFSET(type, id, 0); ABI_OFFSET(type, count, 4); \
 ABI_OFFSET(type, pre, 5); ABI_OFFSET(type, kind, 6); \
 ABI_OFFSET(type, flags, 7); ABI_OFFSET(type, onset, 8); \
 ABI_OFFSET(type, end, 9); ABI_OFFSET(type, impacts, 10); \
 ABI_OFFSET(type, impact_at, 11); ABI_OFFSET(type, duration_ms, 20)
COMMON_EXAMPLE_ABI(st_example); COMMON_EXAMPLE_ABI(rt_example);
ABI_OFFSET(st_example, attack_ms, 22); ABI_OFFSET(st_example, decay_ms, 24);
ABI_OFFSET(st_example, peak_hz, 26); ABI_OFFSET(st_example, peak_db, 28);
ABI_OFFSET(st_example, frames, 30);
ABI_OFFSET(rt_example, attack_ms, 24); ABI_OFFSET(rt_example, decay_ms, 28);
ABI_OFFSET(rt_example, peak_coord, 32); ABI_OFFSET(rt_example, peak_db, 34);
ABI_OFFSET(rt_example, frames, 36);
ABI_OFFSET(st_frame, shape, 0); ABI_OFFSET(st_frame, level_db, 32);
ABI_OFFSET(st_frame, peak_hz, 34); ABI_OFFSET(st_frame, flux, 36);
ABI_OFFSET(st_frame, flags, 37);
ABI_OFFSET(rt_frame, shape, 0); ABI_OFFSET(rt_frame, level_db, 32);
ABI_OFFSET(rt_frame, peak_coord, 34); ABI_OFFSET(rt_frame, flux, 36);
ABI_OFFSET(rt_frame, flags, 37); ABI_OFFSET(rt_frame, timestamp_ms, 40);
ABI_OFFSET(st_label, examples, 24); ABI_OFFSET(rt_label, examples, 24);
ABI_OFFSET(st_library, generation, 118464);
ABI_OFFSET(rt_library, generation, 137088); ABI_OFFSET(rt_library, identity, 137096);

static inline bool old_st_frame_valid(const st_frame *f){
 if(!f||f->level_db< -12000||f->level_db>1000||f->peak_hz>8000||(f->flags&~(ST_ACTIVE|ST_TONAL)))return false;
 unsigned peak=0;for(unsigned i=0;i<64;i++){unsigned n=st_nibble(f,(int)i);if(n>peak)peak=n;}
 if(!f->peak_hz)return !peak&&f->level_db==-12000&&!f->flux&&!f->flags;
 return peak==15&&f->level_db> -12000;
}

static inline void old_st_summarize(st_example *e){
 e->peak_db=-12000;e->peak_hz=0;e->onset=e->pre;e->end=e->count?e->count-1:0;e->impacts=0;memset(e->impact_at,0,sizeof(e->impact_at));unsigned peak_index=e->pre,last_impact=0;bool have_impact=false;
 for(unsigned i=e->pre;i<e->count;i++){if(e->frames[i].level_db>e->peak_db){e->peak_db=e->frames[i].level_db;e->peak_hz=e->frames[i].peak_hz;peak_index=i;}if((e->frames[i].flags&ST_ACTIVE)&&e->frames[i].flux>=180&&(!have_impact||i>=last_impact+2u)){last_impact=i;have_impact=true;if(e->impacts<8)e->impact_at[e->impacts]=(uint8_t)i;if(e->impacts<255)++e->impacts;}}
 while(e->onset<e->count&&!(e->frames[e->onset].flags&ST_ACTIVE))++e->onset;
 if(e->onset>=e->count)e->onset=e->pre;
 while(e->end>e->onset&&!(e->frames[e->end].flags&ST_ACTIVE))--e->end;
 unsigned lo=e->onset,hi=e->onset,last_hi=peak_index,last_lo=peak_index;
 for(unsigned i=e->onset;i<=peak_index;i++){if(e->frames[i].level_db<e->peak_db-700)lo=i;if(e->frames[i].level_db<e->peak_db-100)hi=i;}
 for(unsigned i=peak_index;i<=e->end;i++){if(e->frames[i].level_db>=e->peak_db-100)last_hi=i;if(e->frames[i].level_db>=e->peak_db-700)last_lo=i;}
 e->duration_ms=(uint16_t)((e->end-e->onset+1u)*ST_FRAME_MS);e->attack_ms=(uint16_t)((hi>=lo?hi-lo:0)*ST_FRAME_MS);e->decay_ms=(uint16_t)((last_lo>=last_hi?last_lo-last_hi:0)*ST_FRAME_MS);
}

static inline bool old_st_example_valid(const st_example *e){
 if(!e||!e->id||e->kind<1||e->kind>2||e->count<2||e->count>ST_FRAMES||e->pre>ST_PRE||e->pre>=e->count||(e->flags&~(ST_CLIPPED|ST_CONFIRMED_END))||((e->flags&ST_CONFIRMED_END)&&!(e->flags&ST_CLIPPED)))return false;
 bool active=false;for(unsigned i=0;i<e->count;i++){if(!old_st_frame_valid(&e->frames[i]))return false;active|=!!(e->frames[i].flags&ST_ACTIVE);}if(!active)return false;
 st_example copy=*e;old_st_summarize(&copy);
 return copy.onset==e->onset&&copy.end==e->end&&copy.impacts==e->impacts&&copy.duration_ms==e->duration_ms&&copy.attack_ms==e->attack_ms&&copy.decay_ms==e->decay_ms&&copy.peak_hz==e->peak_hz&&copy.peak_db==e->peak_db&&!memcmp(copy.impact_at,e->impact_at,8);
}

static inline bool old_st_label_valid(const st_label *l){
 if(!l||l->shift_limit>ST_MAX_SHIFT)return false;
 if(!l->present){if(l->name[0]||l->next_id||l->shift_limit)return false;for(unsigned i=0;i<ST_EXAMPLES;i++)if(l->examples[i].id)return false;return true;}
 if(!spectrum_signature_name_valid(l->name)||!l->next_id)return false;
 for(unsigned i=0;i<ST_EXAMPLES;i++)if(l->examples[i].id){if(!old_st_example_valid(&l->examples[i])||l->examples[i].id>=l->next_id)return false;for(unsigned j=0;j<i;j++)if(l->examples[i].id==l->examples[j].id)return false;}
 return true;
}

static inline bool old_rt_frame_valid(const rt_frame *f){
 if(!f||f->level_db< -12000||f->level_db>1000||f->level_db%100||(f->flags&~(RT_ACTIVE|RT_TONAL|RT_GAP_BEFORE)))return false;
 unsigned peak=0;for(unsigned i=0;i<64;i++){unsigned n=rt_nibble(f,(int)i);if(n>peak)peak=n;}
 if(!f->peak_coord)return !peak&&f->level_db==-12000&&!f->flux&&!(f->flags&~RT_GAP_BEFORE);
 return peak==15&&f->level_db> -12000;
}

static inline unsigned old_rt_elapsed(const rt_example *e,unsigned first,unsigned last){return e->frames[last].timestamp_ms-e->frames[first].timestamp_ms;}

static inline void old_rt_summarize(rt_example *e){
 e->peak_db=-12000;e->peak_coord=0;e->onset=e->pre;e->end=e->count?e->count-1:0;e->impacts=0;memset(e->impact_at,0,sizeof(e->impact_at));unsigned peak_index=e->pre,last_impact=0;bool have_impact=false;
 for(unsigned i=e->pre;i<e->count;i++){if(e->frames[i].level_db>e->peak_db){e->peak_db=e->frames[i].level_db;e->peak_coord=e->frames[i].peak_coord;peak_index=i;}if((e->frames[i].flags&RT_ACTIVE)&&e->frames[i].flux>=180&&(!have_impact||old_rt_elapsed(e,last_impact,i)>=RT_FRAME_MS)){last_impact=i;have_impact=true;if(e->impacts<8)e->impact_at[e->impacts]=(uint8_t)i;if(e->impacts<255)++e->impacts;}}
 while(e->onset<e->count&&!(e->frames[e->onset].flags&RT_ACTIVE))++e->onset;
 if(e->onset>=e->count)e->onset=e->pre;
 while(e->end>e->onset&&!(e->frames[e->end].flags&RT_ACTIVE))--e->end;
 if(peak_index<e->onset)peak_index=e->onset;
 unsigned lo=e->onset,hi=e->onset,last_hi=peak_index,last_lo=peak_index;
 for(unsigned i=e->onset;i<=peak_index;i++){if(e->frames[i].level_db<e->peak_db-700)lo=i;if(e->frames[i].level_db<e->peak_db-100)hi=i;}
 for(unsigned i=peak_index;i<=e->end;i++){if(e->frames[i].level_db>=e->peak_db-100)last_hi=i;if(e->frames[i].level_db>=e->peak_db-700)last_lo=i;}
 e->duration_ms=(uint32_t)(old_rt_elapsed(e,e->onset,e->end)+1u);e->attack_ms=(uint32_t)(hi>=lo?old_rt_elapsed(e,lo,hi):0);e->decay_ms=(uint32_t)(last_lo>=last_hi?old_rt_elapsed(e,last_hi,last_lo):0);
}

static inline bool old_rt_example_valid(const rt_example *e){
 if(!e||!e->id||e->kind<1||e->kind>2||e->count<2||e->count>RT_FRAMES||e->pre>RT_PRE||e->pre>=e->count||(e->flags&~(RT_CLIPPED|RT_CONFIRMED_END))||((e->flags&RT_CONFIRMED_END)&&!(e->flags&RT_CLIPPED)))return false;
 bool active=false;for(unsigned i=0;i<e->count;i++){if(!old_rt_frame_valid(&e->frames[i]))return false;if(i&&(e->frames[i].timestamp_ms<=e->frames[i-1].timestamp_ms||e->frames[i].timestamp_ms-e->frames[i-1].timestamp_ms>RT_DELTA_MAX_MS))return false;if(i>=e->pre)active|=!!(e->frames[i].flags&RT_ACTIVE);}if(!active)return false;
 rt_example copy=*e;old_rt_summarize(&copy);
 return copy.onset==e->onset&&copy.end==e->end&&copy.impacts==e->impacts&&copy.duration_ms==e->duration_ms&&copy.attack_ms==e->attack_ms&&copy.decay_ms==e->decay_ms&&copy.peak_coord==e->peak_coord&&copy.peak_db==e->peak_db&&!memcmp(copy.impact_at,e->impact_at,8);
}

static inline bool old_rt_label_valid(const rt_label *l){
 if(!l||l->shift_limit>RT_MAX_SHIFT)return false;
 if(!l->present){if(l->name[0]||l->next_id||l->shift_limit)return false;for(unsigned i=0;i<RT_EXAMPLES;i++)if(l->examples[i].id)return false;return true;}
 if(!rf_signature_name_valid(l->name)||!l->next_id)return false;
 for(unsigned i=0;i<RT_EXAMPLES;i++)if(l->examples[i].id){if(!old_rt_example_valid(&l->examples[i])||l->examples[i].id>=l->next_id)return false;for(unsigned j=0;j<i;j++)if(l->examples[i].id==l->examples[j].id)return false;}
 return true;
}

static inline size_t old_st_bank_encode(const st_library *library,unsigned bank,uint8_t *out,size_t capacity){
 if(!library||!out||bank>=2||capacity<ST_BANK_MIN)return 0;
 size_t size=ST_BANK_MIN;for(unsigned i=bank*4;i<bank*4+4;i++){if(!old_st_label_valid(&library->labels[i]))return 0;for(unsigned j=0;j<ST_EXAMPLES;j++)if(library->labels[i].examples[j].id)size+=(size_t)library->labels[i].examples[j].count*38;}
 if(size>capacity||size>ST_BANK_MAX)return 0;
 memset(out,0,size);memcpy(out,"SQT2",4);out[4]=1;out[5]=(uint8_t)bank;out[6]=64;out[7]=6;spectrum_signature_put32(out+8,16000);st_put16(out+12,512);out[14]=SPECTRUM_DSP_HANN;out[15]=64;out[16]=ST_PRE;out[17]=3;out[18]=2;out[19]=1;spectrum_signature_put32(out+20,library->generation[bank]);spectrum_signature_put32(out+24,(uint32_t)size);
 size_t at=64;
 for(unsigned i=bank*4;i<bank*4+4;i++){const st_label*l=&library->labels[i];uint8_t *p=out+at;p[0]=l->present;p[1]=l->shift_limit;if(l->present){memcpy(p+4,l->name,strlen(l->name));spectrum_signature_put32(p+24,l->next_id);}at+=64;
  for(unsigned j=0;j<ST_EXAMPLES;j++){const st_example*e=&l->examples[j];p=out+at;at+=40;if(!e->id)continue;spectrum_signature_put32(p,e->id);p[4]=e->count;p[5]=e->pre;p[6]=e->kind;p[7]=e->flags;p[8]=e->onset;p[9]=e->end;p[10]=e->impacts;memcpy(p+12,e->impact_at,8);st_put16(p+20,e->duration_ms);st_put16(p+22,e->attack_ms);st_put16(p+24,e->decay_ms);st_put16(p+26,e->peak_hz);st_put16(p+28,(unsigned)(e->peak_db+12000));
   for(unsigned k=0;k<e->count;k++){const st_frame*f=&e->frames[k];p=out+at;memcpy(p,f->shape,32);st_put16(p+32,(unsigned)(f->level_db+12000));st_put16(p+34,f->peak_hz);p[36]=f->flux;p[37]=f->flags;at+=38;}
  }
 }
 if(at+4!=size)return 0;
 spectrum_signature_put32(out+at,spectrum_signature_crc(out,at));return size;
}

static inline size_t old_rt_bank_encode(const rt_library *library,unsigned bank,uint8_t *out,size_t capacity){
 if(!library||!out||bank>=2||capacity<RT_BANK_MIN||!rf_identity_valid(&library->identity))return 0;
 size_t size=RT_BANK_MIN;for(unsigned i=bank*4;i<bank*4+4;i++){if(!old_rt_label_valid(&library->labels[i]))return 0;for(unsigned j=0;j<RT_EXAMPLES;j++)if(library->labels[i].examples[j].id)size+=(size_t)library->labels[i].examples[j].count*RT_FRAME_BYTES;}
 if(size>capacity||size>RT_BANK_MAX)return 0;
 memset(out,0,size);memcpy(out,"RFT1",4);out[4]=1;out[5]=(uint8_t)bank;out[6]=RT_FRAMES;out[7]=RT_EXAMPLES;out[8]=RT_BANDS;out[9]=RT_PRE;out[10]=RT_FRAME_BYTES;out[11]=2;/* uint16 exact millisecond deltas; 1dB levels */
 rf_signature_put32(out+20,library->generation[bank]);rf_signature_put32(out+24,(uint32_t)size);rf_identity_encode(&library->identity,out+32);
 size_t at=RT_BANK_HEADER;
 for(unsigned i=bank*4;i<bank*4+4;i++){const rt_label*l=&library->labels[i];uint8_t *p=out+at;p[0]=l->present;p[1]=l->shift_limit;if(l->present){memcpy(p+4,l->name,strlen(l->name));rf_signature_put32(p+24,l->next_id);}at+=64;
  for(unsigned j=0;j<RT_EXAMPLES;j++){const rt_example*e=&l->examples[j];p=out+at;at+=40;if(!e->id)continue;rf_signature_put32(p,e->id);p[4]=e->count;p[5]=e->pre;p[6]=e->kind;p[7]=e->flags;p[8]=e->onset;p[9]=e->end;p[10]=e->impacts;p[11]=(uint8_t)((e->peak_db+12000)/100);memcpy(p+12,e->impact_at,8);rf_signature_put32(p+20,e->duration_ms);rf_signature_put32(p+24,e->attack_ms);rf_signature_put32(p+28,e->decay_ms);rt_put16(p+32,e->peak_coord);rf_signature_put32(p+34,e->frames[0].timestamp_ms);
   for(unsigned k=0;k<e->count;k++){const rt_frame*f=&e->frames[k];p=out+at;memcpy(p,f->shape,32);p[32]=(uint8_t)((f->level_db+12000)/100);rt_put16(p+33,f->peak_coord);p[35]=f->flux;p[36]=f->flags;rt_put16(p+37,k?f->timestamp_ms-e->frames[k-1u].timestamp_ms:0);at+=RT_FRAME_BYTES;}
  }
 }
 if(at+4!=size)return 0;
 rf_signature_put32(out+at,rf_signature_crc(out,at));return size;
}

static uint32_t random_state = UINT32_C(0x74656d70);
static unsigned validations, summaries, corruptions, codec_checks;
static const char *phase;
static unsigned case_number;

#define CHECK(expression) do { \
 if (!(expression)) { \
  fprintf(stderr, "%s case %u, line %d: %s\n", phase, case_number, __LINE__, #expression); \
  abort(); \
 } \
} while (0)

static uint32_t next_random(void) {
 random_state ^= random_state << 13;
 random_state ^= random_state >> 17;
 random_state ^= random_state << 5;
 return random_state;
}

static bool check_st(const st_example *e, int expected) {
 st_example before;
 memcpy(&before, e, sizeof(before));
 bool old = old_st_example_valid(e);
 CHECK(!memcmp(&before, e, sizeof(before)));
 bool current = st_example_valid(e);
 CHECK(!memcmp(&before, e, sizeof(before)));
 CHECK(current == old);
 if (expected >= 0) CHECK(current == (expected != 0));
 ++validations;
 return current;
}

static bool check_rt(const rt_example *e, int expected) {
 rt_example before;
 memcpy(&before, e, sizeof(before));
 bool old = old_rt_example_valid(e);
 CHECK(!memcmp(&before, e, sizeof(before)));
 bool current = rt_example_valid(e);
 CHECK(!memcmp(&before, e, sizeof(before)));
 CHECK(current == old);
 if (expected >= 0) CHECK(current == (expected != 0));
 ++validations;
 return current;
}

static void summarize_st(st_example *e, int expected) {
 st_example reference;
 memcpy(&reference, e, sizeof(reference));
 old_st_summarize(&reference);
 st_summarize(e);
 /* Whole-object comparison also catches changes outside the summary. */
 CHECK(!memcmp(e, &reference, sizeof(reference)));
 ++summaries;
 check_st(e, expected);
}

static void summarize_rt(rt_example *e, int expected) {
 rt_example reference;
 memcpy(&reference, e, sizeof(reference));
 old_rt_summarize(&reference);
 rt_summarize(e);
 CHECK(!memcmp(e, &reference, sizeof(reference)));
 ++summaries;
 check_rt(e, expected);
}

static void make_st(st_example *e, unsigned count, unsigned pre) {
 static const uint8_t flux[] = {0, 1, 179, 180, 181, 254, 255};
 /* Poison all padding and unused frames, which validation must not touch. */
 memset(e, 0xa5, sizeof(*e));
 e->id = 1; e->count = (uint8_t)count; e->pre = (uint8_t)pre;
 e->kind = (uint8_t)(1 + next_random() % 2); e->flags = 0;
 for (unsigned i = 0; i < count; ++i) {
  st_frame *f = &e->frames[i];
  memset(f, 0, sizeof(*f));
  if (i != pre && next_random() % 5 == 0) { f->level_db = -12000; continue; }
  for (unsigned j = 0; j < 32; ++j) f->shape[j] = (uint8_t)next_random();
  st_set_nibble(f, next_random() % 64, 15);
  f->peak_hz = (uint16_t)(1 + next_random() % 8000);
  f->level_db = (int16_t)(-11999 + (int)(next_random() % 13000));
  f->flux = flux[next_random() % (sizeof(flux) / sizeof(flux[0]))];
  f->flags = (uint8_t)(next_random() % 4);
 }
 e->frames[pre].flags |= ST_ACTIVE;
}

static void make_rt(rt_example *e, unsigned count, unsigned pre) {
 static const unsigned deltas[] = {1, 99, 100, 101, 1800, 65534, 65535};
 memset(e, 0x5a, sizeof(*e));
 e->id = 1; e->count = (uint8_t)count; e->pre = (uint8_t)pre;
 e->kind = (uint8_t)(1 + next_random() % 2); e->flags = 0;
 uint32_t timestamp = (next_random() & 1) ?
  UINT32_MAX - (count - 1) * RT_DELTA_MAX_MS : next_random() % 10000;
 for (unsigned i = 0; i < count; ++i) {
  rt_frame *f = &e->frames[i];
  memset(f, 0, sizeof(*f));
  if (i) timestamp += deltas[next_random() % (sizeof(deltas) / sizeof(deltas[0]))];
  f->timestamp_ms = timestamp;
  f->flags = (next_random() & 1) ? RT_GAP_BEFORE : 0;
  if (i != pre && next_random() % 5 == 0) { f->level_db = -12000; continue; }
  for (unsigned j = 0; j < 32; ++j) f->shape[j] = (uint8_t)next_random();
  rt_set_nibble(f, next_random() % 64, 15);
  f->peak_coord = (uint16_t)(1 + next_random() % 65535);
  f->level_db = (int16_t)(-11900 + (int)(next_random() % 130) * 100);
  f->flux = (uint8_t)((next_random() & 1) ? 180 : next_random());
  f->flags |= (uint8_t)(next_random() % 4);
 }
 e->frames[pre].flags |= RT_ACTIVE;
}

struct field { size_t offset, size; };
#define FIELD(type, name) {offsetof(type, name), sizeof(((type *)0)->name)}
static const struct field st_fields[] = {
 FIELD(st_example, onset), FIELD(st_example, end), FIELD(st_example, impacts),
 FIELD(st_example, impact_at), FIELD(st_example, duration_ms),
 FIELD(st_example, attack_ms), FIELD(st_example, decay_ms),
 FIELD(st_example, peak_hz), FIELD(st_example, peak_db)
};
static const struct field rt_fields[] = {
 FIELD(rt_example, onset), FIELD(rt_example, end), FIELD(rt_example, impacts),
 FIELD(rt_example, impact_at), FIELD(rt_example, duration_ms),
 FIELD(rt_example, attack_ms), FIELD(rt_example, decay_ms),
 FIELD(rt_example, peak_coord), FIELD(rt_example, peak_db)
};

static void corrupt_st_summary(const st_example *source) {
 for (unsigned f = 0; f < sizeof(st_fields) / sizeof(st_fields[0]); ++f) {
  for (size_t b = 0; b < st_fields[f].size; ++b) {
   st_example bad;
   memcpy(&bad, source, sizeof(bad));
   ((unsigned char *)&bad)[st_fields[f].offset + b] ^= 1u << ((f + b) % 8);
   check_st(&bad, 0);
   ++corruptions;
  }
 }
}

static void corrupt_rt_summary(const rt_example *source) {
 for (unsigned f = 0; f < sizeof(rt_fields) / sizeof(rt_fields[0]); ++f) {
  for (size_t b = 0; b < rt_fields[f].size; ++b) {
   rt_example bad;
   memcpy(&bad, source, sizeof(bad));
   ((unsigned char *)&bad)[rt_fields[f].offset + b] ^= 1u << ((f + b) % 8);
   check_rt(&bad, 0);
   ++corruptions;
  }
 }
}

static void test_legal_and_random(void) {
 static const uint8_t flags[] = {0, ST_CLIPPED, ST_CLIPPED | ST_CONFIRMED_END};
 phase = "legal count/pre/kind/flags";
 for (unsigned count = 2; count <= ST_FRAMES; ++count) {
  for (unsigned pre = 0; pre <= ST_PRE && pre < count; ++pre) {
   for (unsigned kind = 1; kind <= 2; ++kind) {
    for (unsigned f = 0; f < sizeof(flags); ++f) {
     st_example s; rt_example r;
     make_st(&s, count, pre); make_rt(&r, count, pre);
     s.kind = r.kind = (uint8_t)kind; s.flags = r.flags = flags[f];
     summarize_st(&s, 1); summarize_rt(&r, 1);
     if (case_number % 17 == 0) { corrupt_st_summary(&s); corrupt_rt_summary(&r); }
     ++case_number;
    }
   }
  }
 }
 phase = "deterministic random legal/malformed";
 for (case_number = 0; case_number < 512; ++case_number) {
  unsigned count = 2 + next_random() % 63;
  unsigned pre = next_random() % (count < 5 ? count : 5);
  st_example s; rt_example r;
  make_st(&s, count, pre); make_rt(&r, count, pre);
  summarize_st(&s, 1); summarize_rt(&r, 1);
  corrupt_st_summary(&s); corrupt_rt_summary(&r);
  /* Raw mutations include malformed metadata, summaries, frame payloads and
   * ignored unused bytes. Some remain legal: acceptance must match exactly. */
  for (unsigned mutation = 0; mutation < 8; ++mutation) {
   st_example bad_s; rt_example bad_r;
   memcpy(&bad_s, &s, sizeof(bad_s)); memcpy(&bad_r, &r, sizeof(bad_r));
   ((unsigned char *)&bad_s)[next_random() % sizeof(bad_s)] ^= (uint8_t)(1 + next_random() % 255);
   ((unsigned char *)&bad_r)[next_random() % sizeof(bad_r)] ^= (uint8_t)(1 + next_random() % 255);
   check_st(&bad_s, -1); check_rt(&bad_r, -1);
  }
 }
}

static void test_metadata_and_frames(void) {
 phase = "all metadata byte values";
 CHECK(!old_st_example_valid(NULL) && !st_example_valid(NULL));
 CHECK(!old_rt_example_valid(NULL) && !rt_example_valid(NULL));
 st_example s; rt_example r;
 make_st(&s, 16, 4); make_rt(&r, 16, 4);
 summarize_st(&s, 1); summarize_rt(&r, 1);
 for (case_number = 0; case_number < 256; ++case_number) {
  for (unsigned field = 0; field < 4; ++field) {
   st_example a = s; rt_example b = r;
   switch (field) {
    case 0: a.count = b.count = (uint8_t)case_number; break;
    case 1: a.pre = b.pre = (uint8_t)case_number; break;
    case 2: a.kind = b.kind = (uint8_t)case_number; break;
    default: a.flags = b.flags = (uint8_t)case_number; break;
   }
   check_st(&a, -1); check_rt(&b, -1);
  }
  st_example a = s; rt_example b = r;
  a.frames[a.pre].flags = b.frames[b.pre].flags = (uint8_t)case_number;
  summarize_st(&a, -1); summarize_rt(&b, -1);
 }
 s.id = r.id = 0; check_st(&s, 0); check_rt(&r, 0);
 s.id = r.id = UINT32_MAX; check_st(&s, 1); check_rt(&r, 1);
 phase = "invalid frame encodings";
 for (case_number = 0; case_number < 10; ++case_number) {
  st_example a = s; rt_example b = r;
  st_frame *sf = &a.frames[a.pre]; rt_frame *rf = &b.frames[b.pre];
  switch (case_number) {
   case 0: sf->level_db = rf->level_db = -12001; break;
   case 1: sf->level_db = rf->level_db = 1001; break;
   case 2: sf->peak_hz = rf->peak_coord = 0; break;
   case 3: memset(sf->shape, 0, sizeof(sf->shape)); memset(rf->shape, 0, sizeof(rf->shape)); break;
   case 4: sf->flags |= 128; rf->flags |= 128; break;
   case 5: sf->level_db = rf->level_db = -12000; break;
   case 6: sf->peak_hz = 8001; rf->level_db = -11999; break;
   default:
    memset(sf, 0, sizeof(*sf));
    memset(rf->shape, 0, sizeof(rf->shape));
    sf->level_db = rf->level_db = -12000; rf->peak_coord = 0;
    sf->flux = rf->flux = case_number == 7 ? 1 : 0;
    sf->flags = rf->flags = case_number == 8 ? ST_ACTIVE : ST_TONAL;
    break;
  }
  summarize_st(&a, 0); summarize_rt(&b, 0);
 }
 /* Spectrum accepts activity entirely before pre; RF deliberately does not. */
 phase = "pre-only and absent activity";
 make_st(&s, 8, 4); make_rt(&r, 8, 4);
 for (unsigned i = 0; i < 8; ++i) {
  s.frames[i].flags &= (uint8_t)~ST_ACTIVE;
  r.frames[i].flags &= (uint8_t)~RT_ACTIVE;
 }
 summarize_st(&s, 0); summarize_rt(&r, 0);
 s.frames[0] = s.frames[4]; s.frames[0].flags |= ST_ACTIVE;
 r.frames[0].level_db = -2000; r.frames[0].peak_coord = 500;
 r.frames[0].shape[0] = 15; r.frames[0].flags |= RT_ACTIVE;
 summarize_st(&s, 1); summarize_rt(&r, 0);
 for (unsigned i = s.pre; i < s.count; ++i) {
  memset(&s.frames[i], 0, sizeof(s.frames[i])); s.frames[i].level_db = -12000;
  uint32_t timestamp = r.frames[i].timestamp_ms;
  memset(&r.frames[i], 0, sizeof(r.frames[i])); r.frames[i].level_db = -12000;
  r.frames[i].timestamp_ms = timestamp;
 }
 summarize_st(&s, 1); summarize_rt(&r, 0);
 CHECK(s.onset == s.pre && s.end == s.pre && s.duration_ms == ST_FRAME_MS);
 CHECK(!s.peak_hz && s.peak_db == -12000 && !s.impacts);
}

static void flat_examples(st_example *s, rt_example *r, unsigned count, unsigned pre) {
 memset(s, 0, sizeof(*s)); memset(r, 0, sizeof(*r));
 s->id = r->id = 1; s->count = r->count = (uint8_t)count;
 s->pre = r->pre = (uint8_t)pre; s->kind = r->kind = ST_POSITIVE;
 for (unsigned i = 0; i < count; ++i) {
  s->frames[i].level_db = r->frames[i].level_db = -2000;
  s->frames[i].peak_hz = (uint16_t)(1000 + i);
  r->frames[i].peak_coord = (uint16_t)(1000 + i);
  s->frames[i].shape[0] = r->frames[i].shape[0] = 15;
  s->frames[i].flags = r->frames[i].flags = ST_ACTIVE;
  s->frames[i].flux = r->frames[i].flux = 180;
  r->frames[i].timestamp_ms = 1000 + i * 100;
 }
}

static void test_summary_edges(void) {
 st_example s; rt_example r;
 phase = "peak ties and truncated impacts"; case_number = 0;
 flat_examples(&s, &r, 64, 4);
 summarize_st(&s, 1); summarize_rt(&r, 1);
 CHECK(s.onset == 4 && s.end == 63 && s.peak_hz == 1004 && s.impacts == 30);
 CHECK(r.onset == 4 && r.end == 63 && r.peak_coord == 1004 && r.impacts == 60);
 CHECK(s.duration_ms == 60 * ST_FRAME_MS && r.duration_ms == 5901);
 for (unsigned i = 0; i < 8; ++i) { CHECK(s.impact_at[i] == 4 + i * 2); CHECK(r.impact_at[i] == 4 + i); }
 corrupt_st_summary(&s); corrupt_rt_summary(&r);
 phase = "quiet tails and inactive strongest peak";
 for (case_number = 0; case_number < 6; ++case_number) {
  flat_examples(&s, &r, 16, 2);
  for (unsigned i = 16 - case_number; i < 16; ++i) {
   s.frames[i].flags = 0; r.frames[i].flags = RT_GAP_BEFORE;
  }
  s.frames[2].flags = r.frames[2].flags = 0;
  s.frames[2].level_db = r.frames[2].level_db = -1000;
  summarize_st(&s, 1); summarize_rt(&r, 1);
  CHECK(s.onset == 3 && r.onset == 3);
  CHECK(s.end == 15 - case_number && r.end == 15 - case_number);
  CHECK(s.peak_hz == 1002 && r.peak_coord == 1002);
 }
 phase = "attack decay strict thresholds";
 static const int levels[] = {-2800, -2700, -2699, -2200, -2100, -2099, -2000,
                              -2099, -2100, -2101, -2699, -2700, -2701};
 flat_examples(&s, &r, sizeof(levels) / sizeof(levels[0]), 0);
 for (unsigned i = 0; i < s.count; ++i) {
  s.frames[i].level_db = (int16_t)levels[i];
  r.frames[i].level_db = (int16_t)((levels[i] / 100) * 100);
  r.frames[i].timestamp_ms = 1000 + i * i * 17;
 }
 summarize_st(&s, 1); summarize_rt(&r, 1);
 CHECK(s.attack_ms == 3 * ST_FRAME_MS && s.decay_ms == 3 * ST_FRAME_MS);
 /* A single post-pre active frame still has a nonzero canonical duration. */
 flat_examples(&s, &r, 5, 4);
 summarize_st(&s, 1); summarize_rt(&r, 1);
 CHECK(s.duration_ms == ST_FRAME_MS && r.duration_ms == 1);
 CHECK(!s.attack_ms && !s.decay_ms && !r.attack_ms && !r.decay_ms);
}

static void test_rf_timestamps(void) {
 static const uint32_t deltas[] = {0, 1, 99, 100, 101, 65534, 65535, 65536};
 phase = "RF timestamp boundaries and gaps";
 for (case_number = 0; case_number < sizeof(deltas) / sizeof(deltas[0]); ++case_number) {
  for (unsigned near_limit = 0; near_limit < 2; ++near_limit) {
   st_example unused; rt_example r;
   flat_examples(&unused, &r, 64, 0);
   uint32_t delta = deltas[case_number];
   uint32_t first = near_limit ? UINT32_MAX - delta * 63 : 0;
   for (unsigned i = 0; i < r.count; ++i) {
    r.frames[i].timestamp_ms = first + i * delta;
    r.frames[i].flags |= (i & 1) ? RT_GAP_BEFORE : 0;
   }
   summarize_rt(&r, delta > 0 && delta <= RT_DELTA_MAX_MS);
   if (delta > 0 && delta <= RT_DELTA_MAX_MS) {
    CHECK(r.duration_ms == 63 * delta + 1);
    CHECK(r.impacts == (delta >= RT_FRAME_MS ? 64 : delta == 1 ? 1 : 32));
    if (near_limit) CHECK(r.frames[63].timestamp_ms == UINT32_MAX);
   }
  }
 }
 st_example unused; rt_example r;
 flat_examples(&unused, &r, 8, 4);
 for (unsigned i = 0; i < r.count; ++i) r.frames[i].timestamp_ms = UINT32_MAX - 250u + i * 100u;
 summarize_rt(&r, 0); /* Real uint32 wrap is rejected, even inside pre-context. */
 flat_examples(&unused, &r, 8, 4);
 r.frames[1].timestamp_ms = r.frames[0].timestamp_ms;
 summarize_rt(&r, 0);
 flat_examples(&unused, &r, 8, 4);
 r.frames[1].timestamp_ms = r.frames[0].timestamp_ms - 1;
 summarize_rt(&r, 0);
 flat_examples(&unused, &r, 8, 4);
 r.frames[7].timestamp_ms = r.frames[6].timestamp_ms + RT_DELTA_MAX_MS + 1;
 summarize_rt(&r, 0);
}

/* Keep libraries and codec buffers static: the fixture is about the production
 * validators' semantics, not the host's available automatic-storage budget. */
static st_library st_catalog, st_decoded, st_before;
static rt_library rt_catalog, rt_decoded, rt_before;
static uint8_t encoded[RT_BANK_MAX], reference[RT_BANK_MAX], roundtrip[RT_BANK_MAX];

static void test_codecs(void) {
 phase = "canonical codec bytes and summary rejection";
 rt_catalog.identity = rf_identity_default();
 for (unsigned label = 0; label < ST_LABELS; ++label) {
  st_label *sl = &st_catalog.labels[label]; rt_label *rl = &rt_catalog.labels[label];
  sl->present = rl->present = true; sl->next_id = rl->next_id = 7;
  sl->shift_limit = rl->shift_limit = (uint8_t)(label % 3);
  snprintf(sl->name, sizeof(sl->name), "S%u", label);
  snprintf(rl->name, sizeof(rl->name), "R%u", label);
  for (unsigned j = 0; j < ST_EXAMPLES; ++j) {
   unsigned count = 2 + next_random() % 63;
   unsigned pre = next_random() % (count < 5 ? count : 5);
   make_st(&sl->examples[j], count, pre); make_rt(&rl->examples[j], count, pre);
   sl->examples[j].id = rl->examples[j].id = j + 1;
   sl->examples[j].kind = rl->examples[j].kind = (uint8_t)(1 + j % 2);
   summarize_st(&sl->examples[j], 1); summarize_rt(&rl->examples[j], 1);
  }
 }
 for (unsigned bank = 0; bank < 2; ++bank) {
  st_catalog.generation[bank] = rt_catalog.generation[bank] = 100 + bank;
  case_number = bank;
  size_t n = st_bank_encode(&st_catalog, bank, encoded, sizeof(encoded));
  CHECK(n > ST_BANK_MIN);
  CHECK(old_st_bank_encode(&st_catalog, bank, reference, sizeof(reference)) == n);
  CHECK(!memcmp(encoded, reference, n));
  CHECK(st_bank_decode(&st_decoded, bank, encoded, n));
  CHECK(st_bank_encode(&st_decoded, bank, roundtrip, sizeof(roundtrip)) == n);
  CHECK(!memcmp(encoded, roundtrip, n));
  st_before = st_decoded;
  /* Every persisted summary byte, including unused impact offsets, is checked
   * after recomputing the CRC, so failure cannot be blamed on its checksum. */
  static const unsigned st_offsets[] = {8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19,
                                        20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
  for (unsigned i = 0; i < sizeof(st_offsets) / sizeof(st_offsets[0]); ++i) {
   memcpy(encoded, reference, n); encoded[128 + st_offsets[i]] ^= 1;
   spectrum_signature_put32(encoded + n - 4, spectrum_signature_crc(encoded, n - 4));
   CHECK(!st_bank_decode(&st_decoded, bank, encoded, n));
   CHECK(!memcmp(&st_before, &st_decoded, sizeof(st_before)));
   ++codec_checks;
  }
  n = rt_bank_encode(&rt_catalog, bank, encoded, sizeof(encoded));
  CHECK(n > RT_BANK_MIN);
  CHECK(old_rt_bank_encode(&rt_catalog, bank, reference, sizeof(reference)) == n);
  CHECK(!memcmp(encoded, reference, n));
  CHECK(rt_bank_decode(&rt_decoded, bank, encoded, n));
  CHECK(rt_bank_encode(&rt_decoded, bank, roundtrip, sizeof(roundtrip)) == n);
  CHECK(!memcmp(encoded, roundtrip, n));
  rt_before = rt_decoded;
  for (unsigned offset = 8; offset < 34; ++offset) {
   memcpy(encoded, reference, n); encoded[RT_BANK_HEADER + RT_LABEL_BYTES + offset] ^= 1;
   rf_signature_put32(encoded + n - 4, rf_signature_crc(encoded, n - 4));
   CHECK(!rt_bank_decode(&rt_decoded, bank, encoded, n));
   CHECK(!memcmp(&rt_before, &rt_decoded, sizeof(rt_before)));
   ++codec_checks;
  }
 }
 /* Corrupted summaries must also be rejected by both encoder implementations. */
 st_catalog.labels[0].examples[0].impact_at[7] ^= 1;
 rt_catalog.labels[0].examples[0].decay_ms ^= UINT32_C(0x10000);
 CHECK(!st_bank_encode(&st_catalog, 0, encoded, sizeof(encoded)));
 CHECK(!old_st_bank_encode(&st_catalog, 0, reference, sizeof(reference)));
 CHECK(!rt_bank_encode(&rt_catalog, 0, encoded, sizeof(encoded)));
 CHECK(!old_rt_bank_encode(&rt_catalog, 0, reference, sizeof(reference)));
}

int main(void) {
 test_legal_and_random();
 test_metadata_and_frames();
 test_summary_edges();
 test_rf_timestamps();
 test_codecs();
 printf("Temporal summary differential: %u summaries, %u immutable validation comparisons, "
        "%u summary corruptions, %u atomic codec rejection checks passed\n",
        summaries, validations, corruptions, codec_checks);
 return 0;
}
