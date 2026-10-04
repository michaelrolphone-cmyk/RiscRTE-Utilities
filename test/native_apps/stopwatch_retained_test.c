/* The real app must not close private grants after the shared adapter reports
 * native retention. The paired adapter fixture proves the -2 latch itself. */
#define PORTABLE_ALARM_CLIENT
#define main ordinary_stopwatch_fixture_main
#include "stopwatch_test.c"
#undef main
bool portable_app_sleep_retained(void) { return true; }
int main(void) {
 setup(true);app_main();assert(live&&releases==0&&acquires==2);
 /* Native retention can also occur on the dependency-error screen, after a
    partial open. That path must retain its successfully acquired grant too. */
 setup(true);deny_rtc=true;app_main();assert(live&&releases==0&&acquires==2);
 puts("Production Stopwatch native retention preserves private grants on both paths");
 return 0;
}
