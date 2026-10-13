/* Generic read-only inventory client. No discovery, media or product policy. */
static bool cs_catalog_checked(bool ok){
 if(cs_catalog.uncertain){cs_retain();return false;}
 return cs_alive()&&ok;
}
static bool cs_catalog_refresh(void){
 if(!cs_alive())return false;
 return cs_catalog_checked(portable_catalog_refresh(&cs_catalog,ctx_runtime));
}
static bool cs_catalog_close(void){
 if(!cs_alive())return false;
 return cs_catalog_checked(portable_catalog_close(&cs_catalog,ctx_runtime));
}
static const char*cs_catalog_key(unsigned index){
 return index<cs_catalog.count?cs_catalog.entries[index].manifest.file_name:NULL;
}
static bool cs_catalog_select(unsigned index,cr_step*out){
 const char*key=cs_catalog_key(index);
 if(!key||!cs_catalog.entries[index].manifest.compatible){ctx_note("APP UNAVAILABLE / REFRESH");return false;}
 /* Keep the existing rules schema and the shared resolver's exact key.
  * Never truncate a route, invent aliases or substitute a display name. */
 if(strlen(key)>=sizeof(out->text)){ctx_note("APP KEY TOO LONG FOR THIS RULE");return false;}
 portable_app_catalog_launch selected;
 if(!cs_catalog_checked(portable_catalog_launch_prepare(&cs_catalog,index,&selected))){if(cs_alive())ctx_note("APP CHANGED / REFRESH");return false;}
 *out=(cr_step){.kind=CR_LAUNCH};strcpy(out->text,key);return true;
}
static bool cs_catalog_prepare(const char*key){
 cs_launch[0]=0;
 if(!cs_catalog_refresh())return false;
 for(unsigned i=0;i<cs_catalog.count;i++)if(!strcmp(cs_catalog_key(i),key)){
  if(!cs_catalog_checked(portable_catalog_launch_prepare(&cs_catalog,i,&cs_launch_ticket)))return false;
  strcpy(cs_launch,cs_launch_ticket.route);return true;
 }
 return false;
}
