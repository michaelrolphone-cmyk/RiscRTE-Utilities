static unsigned shared_kind=CF_ROOM,shared_offset;
static bool shared_discard_confirm;
static int shared_selected=-1;
static char shared_name[17];
static unsigned shared_focus,shared_targets;
static bool shared_name_valid(const char*n){if(!n||!n[0]||n[0]==' ')return false;unsigned i=0;for(;i<17&&n[i];i++)if((unsigned char)n[i]<32||(unsigned char)n[i]>126)return false;return i>0&&i<17&&n[i-1]!=' ';}
static void shared_open(unsigned kind){
 shared_kind=kind;shared_offset=shared_focus=shared_targets=0;shared_selected=-1;shared_keyboard=shared_discard_confirm=false;
 cfa_shared_names();toast_message=NULL;set_page(PAGE_SHARED);
}
static void shared_keyboard_done(void){
 memcpy(shared_name,editing.name,17);shared_keyboard=false;set_page(PAGE_SHARED_EDIT);
}
static bool shared_back(void){
 if(page==PAGE_SHARED_CAPTURE){notify("SAVE OR CANCEL LEARNING");return true;}
 if(page==PAGE_SHARED_EDIT){set_page(PAGE_SHARED);return true;}
 if(page==PAGE_SHARED){set_page(PAGE_MAIN);return true;}
 return false;
}
static unsigned shared_rows(unsigned slots[CF_PROFILES]){
 unsigned n=0;for(unsigned i=0;i<CF_PROFILES;i++)if(cfa_bank.profiles[i].kind==shared_kind)slots[n++]=i;return n;
}
