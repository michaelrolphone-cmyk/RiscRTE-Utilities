
int main(int argc,char**argv) {
 assert(argc==15);fx_directory=argv[1];fx_commands=fopen(argv[2],"r");assert(fx_commands);
 sc_w=(unsigned)atoi(argv[3]);sc_h=(unsigned)atoi(argv[4]);sc_format=(unsigned)atoi(argv[5]);
 sc_rotation=(unsigned)atoi(argv[6]);sc_touch_rotation=(unsigned)atoi(argv[7]);
 for(unsigned i=0;i<4;i++)sc_insets[i]=(unsigned)atoi(argv[8+i]);
 sc_input_mode=(unsigned)atoi(argv[12]);sc_brightness=(unsigned)atoi(argv[13]);
 bool admitted=atoi(argv[14])!=0;
 assert(sc_w<=FX_W&&sc_h<=FX_H&&sc_rotation<4&&sc_touch_rotation<4);
 memset(&fx_raster,0xa5,sizeof(fx_raster));fx_frame_delay=0;
 canvas_provider_init(&sc_provider,&fx_display_api,NULL,sc_rotation);
 sc_provider.touch_rotation=sc_touch_rotation;sc_canvas_api=sc_provider.api;sc_canvas_api.next=sc_next;
 int initialized=app_module_init();
 if(admitted) {
  assert(initialized==0);sc_corners();fx_app_api_phase=true;
  app_main();assert(fx_done&&fx_checks&&sc_frames_checked);
 } else {
  if(initialized==0){app_main();assert(!fx_captures&&!fx_done);}
  assert(!fx_captures);
 }
 assert(!sc_retention_count); /* Admission and geometry refusal must both be clean. */
 app_module_fini();
 assert(!sc_retention_count);
 assert(!fx_grants&&!fx_frames&&!fx_subs&&!sc_provider.frames.session&&!sc_provider.frames.receipts);
 assert(!sc_provider.frames.lease&&!sc_provider.frames.writable&&!sc_provider.frames.seeded.frame);
 assert(!sc_provider.companion_bound&&!sc_provider.subscription&&!sc_provider.claimed);
 assert(!sc_provider.touch&&!sc_provider.navigation);
 sc_guards();assert(!fclose(fx_commands));
 printf("shared-canvas admitted=%u checks=%u frames=%u gaps=%u geometry=%u\n",
        admitted,fx_checks,sc_frames_checked,sc_gaps,sc_geometries);
 return 0;
}
