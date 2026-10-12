/* Test-only controls around real text and scene providers. */
#include "RiscTextEntryV1.h"
#include "RiscUsbHidV1.h"
extern const risc_driver_v2 *contexts_text_driver_get(uint32_t);
extern uint64_t contexts_test_session(void);
static bool text_test_acquire(risc_runtime_capability_v1*);
static bool text_test_is_grant(const risc_runtime_capability_v1*);
static bool text_test_release(risc_runtime_capability_v1*);
static bool text_test_yield(void);
static void text_test_setup(const char*);
static bool text_test_finish(void);

static bool text_test_exit_requested(void);
extern bool contexts_test_header(bool,int*,int*);

static bool text_test_checkpoint(const risc_resident_request_v1*,risc_resident_reply_v1*,int32_t*);

static void text_test_backend_after(const char*);
static int32_t text_test_backend_storage_result(const char*);
