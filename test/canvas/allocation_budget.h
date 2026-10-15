#ifndef CANVAS_TEST_ALLOCATION_BUDGET_H
#define CANVAS_TEST_ALLOCATION_BUDGET_H
#include <stddef.h>
void *canvas_test_malloc(size_t);
void *canvas_test_calloc(size_t,size_t);
void canvas_test_free(void *);
size_t canvas_test_live_bytes(void);
size_t canvas_test_peak_bytes(void);
#endif
