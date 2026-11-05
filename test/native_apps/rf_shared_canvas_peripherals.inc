/* Real canvas provider over explicit host display/input peripherals.
 * No production coordinates or device identifiers are replaced. */
static void sc_dimensions(unsigned *w,unsigned *h) {
 unsigned r=sc_rotation;
 *w=(r&1?sc_h:sc_w)-sc_insets[r]-sc_insets[(r+2)%4];
 *h=(r&1?sc_w:sc_h)-sc_insets[(r+1)%4]-sc_insets[(r+3)%4];
}
static void sc_physical(unsigned x,unsigned y,unsigned *px,unsigned *py) {
 x+=sc_insets[sc_rotation];y+=sc_insets[(sc_rotation+1)%4];
 switch(sc_rotation) {
 case 1:*px=sc_w-1-y;*py=x;break;
 case 2:*px=sc_w-1-x;*py=sc_h-1-y;break;
 case 3:*px=y;*py=sc_h-1-x;break;
 default:*px=x;*py=y;break;
 }
}
static bool sc_info(void *c,risc_display_info_v1 *s) {
 (void)c;
 *s=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*s),
 .width=sc_w,.height=sc_h,.supported_formats=RISC_DISPLAY_FORMAT_BIT(sc_format),
 .flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|(sc_brightness?RISC_DISPLAY_INFO_BRIGHTNESS:0)};
 s->safe_insets.left=sc_insets[0];s->safe_insets.top=sc_insets[1];
 s->safe_insets.right=sc_insets[2];s->safe_insets.bottom=sc_insets[3];
 return true;
}
static bool sc_frame(void*c,uint32_t f,risc_display_surface_v1*s) {
 (void)c;assert(!fx_frames&&!fx_frame_pending&&f==sc_format);fx_frames=1;
 *s=(risc_display_surface_v1){.frame=1,.pixels=fx_raster.pixels,
 .width=sc_w,.height=sc_h,.stride_bytes=FX_STRIDE,
 .size_bytes=sizeof(fx_raster.pixels),.pixel_format=f};return true;
}
static unsigned sc_pixel(unsigned x,unsigned y) {
 assert(x<sc_w&&y<sc_h);
 if(sc_format==RISC_DISPLAY_FORMAT_MONO1)
  return fx_raster.pixels[(size_t)y*FX_STRIDE+x/8]&(0x80u>>(x&7))?0:0xffffff;
 uint16_t v;memcpy(&v,fx_raster.pixels+(size_t)y*FX_STRIDE+x*2,2);
 return (v>>11)*255/31<<16|((v>>5)&63)*255/63<<8|(v&31)*255/31;
}
static void sc_guards(void) {
 for(unsigned i=0;i<32;i++)assert(fx_raster.before[i]==0xa5&&fx_raster.after[i]==0xa5);
 /* Provider legitimately initializes stride padding. The unused rows must remain untouched. */
 for(size_t i=(size_t)sc_h*FX_STRIDE;i<sizeof(fx_raster.pixels);i++)assert(fx_raster.pixels[i]==0xa5);
}
static bool sc_white(uint16_t v,unsigned x,unsigned y) {
 static const unsigned threshold[]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
 unsigned r=(v>>11)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
 return (77*r+150*g+29*b+128)/256>threshold[(y%4)*4+x%4]*16+7;
}
static void sc_verify_raster(void) {
 unsigned w,h;sc_dimensions(&w,&h);
 assert(sc_provider.frames.mapping.width==w&&sc_provider.frames.mapping.height==h);
 assert(sc_provider.frames.encoding==RISC_CANVAS_RGB565_ADAPTIVE);
 for(unsigned py=0;py<sc_h;py++)for(unsigned px=0;px<sc_w;px++) {
  unsigned x=px,y=py;
  switch(sc_rotation) {
  case 1:x=py;y=sc_w-1-px;break;
  case 2:x=sc_w-1-px;y=sc_h-1-py;break;
  case 3:x=sc_h-1-py;y=px;break;
  default:break;
  }
  unsigned l=sc_insets[sc_rotation],t=sc_insets[(sc_rotation+1)%4];
  uint16_t expected=0xffff;
  if(x>=l&&y>=t&&x-l<w&&y-t<h) {
   const unsigned char *p=sc_provider.frames.writable+(size_t)(y-t)*sc_provider.frames.stride+2*(x-l);
   expected=(uint16_t)(p[0]|(uint16_t)p[1]<<8);
  }
  const unsigned char *p=fx_raster.pixels+(size_t)py*FX_STRIDE;
  if(sc_format==RISC_DISPLAY_FORMAT_RGB565) {
   uint16_t got;memcpy(&got,p+2*px,2);assert(got==expected);
  } else assert(((p[px/8]&(0x80u>>(px&7)))==0)==sc_white(expected,px,py));
 }
 sc_frames_checked++;
}
static void sc_report(risc_touch_snapshot_v1 *s) {
 unsigned lw=sc_rotation&1?sc_h:sc_w,lh=sc_rotation&1?sc_w:sc_h;
 unsigned rw=sc_touch_rotation&1?lh:lw,rh=sc_touch_rotation&1?lw:lh;
 *s=(risc_touch_snapshot_v1){.width=rw,.height=rh,.sequence=fx_polls,.buttons=fx_touch_buttons};
 if(fx_touch_x<0)return;
 unsigned x=(unsigned)fx_touch_x+sc_insets[sc_rotation];
 unsigned y=(unsigned)fx_touch_y+sc_insets[(sc_rotation+1)%4],rx=x,ry=y;
 switch(sc_touch_rotation) {
 case 1:rx=y;ry=rh-1-x;break;
 case 2:rx=rw-1-x;ry=rh-1-y;break;
 case 3:rx=rw-1-y;ry=x;break;
 default:break;
 }
 assert(rx<rw&&ry<rh);
 s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=rx,.y=ry};
}
static bool sc_bright(void*c,uint16_t v,uint16_t m){(void)c;assert(sc_brightness&&m&&v<=m);return true;}
static int32_t sc_input_activate(void*c,uint64_t s) {
 (void)c;assert(sc_input_mode);
 return canvas_provider_bind_inputs(&sc_provider,s,sc_input_mode==2?&fx_touch_api:NULL,&fx_nav_api);
}
static int32_t sc_input_deactivate(void*c,uint64_t s){(void)c;return canvas_provider_unbind_inputs(&sc_provider,s);}
static const risc_canvas_input_v1 sc_input={.api_version=1,.struct_size=sizeof(sc_input),
 .tag=RISC_CANVAS_INPUT_TAG,.activate=sc_input_activate,.deactivate=sc_input_deactivate};
static int32_t sc_next(void*c,uint64_t s,risc_canvas_event_v1*out) {
 if(!sc_cut) {
  assert(++fx_polls<20000);fx_ticks+=fx_poll_ms;fx_script();fx_raw_capture();sc_cut=true;
 }
 int32_t rc=canvas_provider_next(c,s,out);
 if(rc==RISC_CANVAS_IDLE||rc==RISC_CANVAS_AGAIN)sc_cut=false;
 if(rc==RISC_CANVAS_OK) {
  if(out->kind==RISC_CANVAS_INPUT_GAP)sc_gaps++;
  if(out->kind==RISC_CANVAS_CONTACT)sc_contacts++;
  if(out->kind==RISC_CANVAS_GEOMETRY_CHANGED)sc_geometries++;
 }
 return rc;
}
/* Optional settings are absent by default; RF records still use the original
 * fixture's checked store. A new optional preference name is not a test failure. */
static int32_t sc_get(void*c,const char*k,void*out,uint32_t cap,uint32_t*size) {
 if(!strncmp(k,"rf_",3)||!strcmp(k,"context_filter"))return fx_get(c,k,out,cap,size);
 *size=0;return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_key_value_v1 sc_preferences={1,sizeof(sc_preferences),NULL,sc_get,fx_put};
static bool sc_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g,int *handled) {
 (void)id;*handled=1;
 if(v==1&&!strcmp(n,RISC_CANVAS_RASTER_CAPABILITY))g->api=&sc_canvas_api;
 else if(v==1&&!strcmp(n,RISC_CANVAS_INPUT_CAPABILITY)) {
  if(!sc_input_mode)return false;g->api=&sc_input;
 } else if(!strcmp(n,"display.output")) {
  if(!sc_brightness)return false;
  static risc_display_output_api_v1 controls;
  controls=(risc_display_output_api_v1){.api_version=1,.struct_size=sizeof(controls),.get_info=sc_info,.set_brightness=sc_bright};
  g->api=&controls;
 } else if(!strcmp(n,"input.touch.raw")||!strcmp(n,"input.navigation")) {
  assert(!"App requested physical input outside canvas");return false;
 } else if(v==1&&!strcmp(n,"storage.key-value"))g->api=&sc_preferences;
 else if(!strcmp(n,"alarm.service"))return false; /* Optional tagged alarm absent. */
 else {*handled=0;return false;}
 g->slot=1;g->generation=1;fx_grants++;return true;
}
static void sc_corners(void) {
 const t5_app_api_v1 *a=t5_app_get_api(1);unsigned w,h;sc_dimensions(&w,&h);
 assert((unsigned)a->screen_width()==w&&(unsigned)a->screen_height()==h);
 a->clear();a->fill_rect(0,0,2,2,true);a->fill_rect((int)w-2,(int)h-2,2,2,true);a->present(true);
 unsigned px,py;sc_physical(0,0,&px,&py);assert(sc_pixel(px,py)==0);
 sc_physical(w-1,h-1,&px,&py);assert(sc_pixel(px,py)==0);fx_checks++;
}
static bool sc_command(const char*op) {
 if(!strcmp(op,"sc_rejection_begin")) {
  assert(!contact_down);sc_before_rejection_contacts=sc_contacts;return true;
 }
 if(!strcmp(op,"sc_rejection_end")) {
  assert(sc_contacts==sc_before_rejection_contacts&&!contact_down);
  assert(!sc_provider.captured[1]);fx_checks++;return true;
 }
 if(!strcmp(op,"sc_geometry")) {
  unsigned w,h;sc_dimensions(&w,&h);
  const t5_app_api_v1 *a=t5_app_get_api(1);
  assert((unsigned)a->screen_width()==w&&(unsigned)a->screen_height()==h);
  assert(PLOT_W==(int)w-12&&PLOT_H==(int)h-110);
  assert(rf_host_canvas_contract()==((sc_input_mode?1u:0u)|(sc_brightness?2u:0u)));
  assert(sc_provider.companion_bound==(sc_input_mode!=0));
  assert((sc_provider.touch!=NULL)==(sc_input_mode==2));
  assert(fx_subs==(sc_input_mode==2?1u:0u));fx_checks++;return true;
 }
 if(!strcmp(op,"sc_resize")) {
  assert(!fx_frames&&!fx_frame_pending&&!sc_provider.frames.lease&&!sc_provider.frames.receipts);
  unsigned w,h;assert(sscanf(fx_line,"%*s %u %u",&w,&h)==2&&w<=FX_W&&h<=FX_H);
  sc_before_resize_gaps=sc_gaps;sc_before_resize_geometry=sc_geometries;
  sc_w=w;sc_h=h;memset(&fx_raster,0xa5,sizeof(fx_raster));return true;
 }
 if(!strcmp(op,"sc_gap")) {
  assert(sc_geometries>sc_before_resize_geometry);
  assert(sc_gaps>=sc_before_resize_gaps);
  assert(!contact_down&&!contact_drag);fx_checks++;return true;
 }
 return false;
}
