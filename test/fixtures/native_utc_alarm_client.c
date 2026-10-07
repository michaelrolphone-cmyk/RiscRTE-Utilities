extern void test_native_alarm_app(void);
extern void test_native_alarm_lifecycle(void);
__attribute__((visibility("default"))) int app_module_init(void){test_native_alarm_lifecycle();return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){test_native_alarm_lifecycle();}
__attribute__((visibility("default"))) void app_main(void){test_native_alarm_app();}
