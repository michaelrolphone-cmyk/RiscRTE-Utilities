/* One app_main-only selection. An older Runtime or a clean documented legacy
 * refusal selects local lifecycle. A malformed successful reply never falls back. */
static bool sdr_resident_select(void) {
 if(native_custody_retained || !native_custody_runtime)return false;
 if(risc_runtime_get_api(1)!=native_custody_runtime) {
  portable_adapter_retain_silent();return false;
 }
 if(rt->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE || !rt->resident_shell) {
  sdr_resident_active=false;return true;
 }
 risc_resident_client_v1 next={.struct_size=sizeof(next)};
 risc_resident_client_v1 empty=next;
 bool got=rt->resident_shell(&next);
 if(native_custody_retained || risc_runtime_get_api(1)!=native_custody_runtime) {
  portable_adapter_retain_silent();return false;
 }
 if(!got) {
  if(memcmp(&next,&empty,sizeof(next))) {portable_adapter_retain_silent();return false;}
  sdr_resident_active=false;return true;
 }
 if(next.api_version!=RISC_RESIDENT_SHELL_API_V1 || next.struct_size<sizeof(next) ||
    !next.invocation || next.role!=RISC_RESIDENT_ROLE_FOREGROUND || next.reserved || !next.checkpoint) {
  portable_adapter_retain_silent();return false;
 }
 resident=next;resident_ready=true;sdr_resident_active=true;return true;
}
