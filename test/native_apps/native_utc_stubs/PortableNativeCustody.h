#ifndef PORTABLE_NATIVE_CUSTODY_H
#define PORTABLE_NATIVE_CUSTODY_H
/* Host test declaration of the required external adapter integration hook.
 * Production builds must use the verified System header and implementation. */
#include <stdbool.h>
void portable_adapter_retain(void);
bool portable_adapter_retained(void);
#endif
