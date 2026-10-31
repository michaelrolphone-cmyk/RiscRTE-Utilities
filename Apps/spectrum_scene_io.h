#pragma once
/* Invocation-local guards for the retained Spectrum domain. No physical IDs,
 * callbacks or domain memory escape this invocation. */
static twatch_audio_in_api_v1 spectrum_mic_proxy;
static risc_key_value_v1 spectrum_kv_proxy;
static bool spectrum_acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *g){
 if(!spectrum_domain_alive())return false;
 if((g->struct_size&&g->struct_size!=sizeof(*g))||g->api||g->slot||g->generation){spectrum_domain_retain();return false;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 bool ok=runtime->acquire(name,api,instance,g);
 if(!spectrum_domain_alive())return false;
 if(g->struct_size!=sizeof(*g)||(!ok&&(g->api||g->slot||g->generation))||(ok&&(!g->api||!g->slot||!g->generation))){spectrum_domain_retain();return false;}
 return ok;
}
static bool spectrum_release(risc_runtime_capability_v1 *g){
 if(!spectrum_domain_alive())return false;
 if(!g->api&&!g->slot&&!g->generation){if(g->struct_size&&g->struct_size!=sizeof(*g)){spectrum_domain_retain();return false;}return true;}
 if(g->struct_size!=sizeof(*g)||!g->api||!g->slot||!g->generation){spectrum_domain_retain();return false;}
 bool ok=runtime->release(g);
 if(!spectrum_domain_alive())return false;
 if(!ok||g->struct_size!=sizeof(*g)||g->api||g->slot||g->generation){spectrum_domain_retain();return false;}
 return true;
}
static bool spectrum_mic_open(void *ctx,uint32_t rate){
 const twatch_audio_in_api_v1 *p=ctx;
 if(!spectrum_domain_alive())return false;
 bool ok=p->open(p->context,rate);
 return spectrum_domain_alive()&&ok;
}
static bool spectrum_mic_read(void *ctx,int16_t *pcm,size_t cap,size_t *actual){
 const twatch_audio_in_api_v1 *p=ctx;
 if(actual)*actual=0;
 if(!spectrum_domain_alive())return false;
 bool ok=p->read(p->context,pcm,cap,actual);
 return spectrum_domain_alive()&&ok;
}
static bool spectrum_mic_close(void *ctx){
 const twatch_audio_in_api_v1 *p=ctx;
 if(!spectrum_domain_alive())return false;
 bool ok=p->close(p->context);
 return spectrum_domain_alive()&&ok;
}
static int32_t spectrum_kv_result(int32_t result){
 if(!spectrum_domain_alive())return RISC_KEY_VALUE_CONTEXT;
 switch(result){
 case RISC_KEY_VALUE_OK:case RISC_KEY_VALUE_NOT_FOUND:case RISC_KEY_VALUE_INVALID:
 case RISC_KEY_VALUE_IO:case RISC_KEY_VALUE_BUFFER_SMALL:case RISC_KEY_VALUE_BUSY: return result;
 default:spectrum_domain_retain();return RISC_KEY_VALUE_CONTEXT;
 }
}
static int32_t spectrum_kv_get(void *ctx,const char *key,void *out,uint32_t cap,uint32_t *actual){
 const risc_key_value_v1 *p=ctx;
 if(!spectrum_domain_alive()||!spectrum_domain_storage_boundary())return RISC_KEY_VALUE_CONTEXT;
 return spectrum_kv_result(p->get(p->context,key,out,cap,actual));
}
static int32_t spectrum_kv_put(void *ctx,const char *key,const void *value,uint32_t size){
 const risc_key_value_v1 *p=ctx;
 if(!spectrum_domain_alive()||!spectrum_domain_storage_boundary())return RISC_KEY_VALUE_CONTEXT;
 return spectrum_kv_result(p->put(p->context,key,value,size));
}
static const twatch_audio_in_api_v1 *spectrum_guard_microphone(const twatch_audio_in_api_v1 *p){
 spectrum_mic_proxy=(twatch_audio_in_api_v1){.api_version=1,.struct_size=sizeof(spectrum_mic_proxy),.context=(void*)p,.open=spectrum_mic_open,.read=spectrum_mic_read,.close=spectrum_mic_close};
 return &spectrum_mic_proxy;
}
static const risc_key_value_v1 *spectrum_guard_storage(const risc_key_value_v1 *p){
 spectrum_kv_proxy=(risc_key_value_v1){.api_version=2,.struct_size=sizeof(spectrum_kv_proxy),.context=(void*)p,.get=spectrum_kv_get,.put=spectrum_kv_put};
 return &spectrum_kv_proxy;
}
