#pragma once
/* Invocation-local synthesis state. Physical output belongs to audio.output;
 * this owner closes only its own attempted stream. Never resumes implicitly. */
#include "tone_core.h"
#include "AlarmOutputV1.h"
typedef struct {
 tone_state tone;unsigned hz,level,step;uint32_t started;
 bool playing,owned,terminal;
 const twatch_audio_out_api_v1 *output;
 bool (*alive)(void*);void (*retain)(void*);void *context;
} frequency_session;
static bool frequency_live(frequency_session*s){return !s->terminal&&s->alive&&s->alive(s->context);}
static bool frequency_fail(frequency_session*s){if(!s->terminal){s->terminal=true;if(s->retain)s->retain(s->context);}return false;}
static void frequency_init(frequency_session*s,bool(*alive)(void*),void(*retain)(void*),void*c){*s=(frequency_session){.hz=440,.level=100,.step=100,.alive=alive,.retain=retain,.context=c};}
static bool frequency_stop(frequency_session*s){
 if(!frequency_live(s))return false;
 s->playing=false;
 if(!s->owned)return true;
 bool ok=s->output->close(s->output->context);
 if(!frequency_live(s))return false;
 if(!ok)return frequency_fail(s);
 s->owned=false;return true;
}
/* False with !terminal is a clean unavailable/failed-open result. */
static bool frequency_start(frequency_session*s,const twatch_audio_out_api_v1*out,uint32_t now){
 if(!frequency_live(s))return false;
 if(s->owned)return frequency_fail(s);
 if(!out||out->api_version!=1||out->struct_size<sizeof(*out)||!out->open||!out->write||!out->close)return false;
 s->output=out;s->tone=(tone_state){0};if(!tone_configure(&s->tone,s->hz,s->level))return false;
 s->owned=true;bool ok=out->open(out->context,TONE_RATE,1);
 if(!frequency_live(s))return false;
 if(!ok){(void)frequency_stop(s);return false;}
 s->playing=true;s->started=now;return true;
}
static bool frequency_edit(frequency_session*s,int hz_delta,int level_delta){
 if(!frequency_live(s))return false;
 s->hz=tone_adjust(s->hz,hz_delta,TONE_MIN_HZ,TONE_MAX_HZ);
 s->level=tone_adjust(s->level,level_delta,0,100);
 return tone_configure(&s->tone,s->hz,s->level);
}
static bool frequency_step(frequency_session*s,uint32_t now){
 if(!frequency_live(s))return false;
 if(!s->playing)return true;
 if((uint32_t)(now-s->started)>=60000u)return frequency_stop(s);
 int16_t pcm[256];tone_generate(&s->tone,pcm,256);
 bool ok=s->output->write(s->output->context,pcm,256);
 if(!frequency_live(s))return false;
 return ok||frequency_stop(s);
}
