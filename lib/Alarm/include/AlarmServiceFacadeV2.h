#ifndef UTILITIES_ALARM_SERVICE_FACADE_V2_H
#define UTILITIES_ALARM_SERVICE_FACADE_V2_H
#include "AlarmServiceV2.h"
/* Semantic facade over the selected, unchanged scheduler. This is a distinct
 * capability so installed legacy consumers retain their exact API/grants. */
#define ALARM_SERVICE_FACADE_CAPABILITY "alarm.service.facade"
#define ALARM_SERVICE_FACADE_API 2u
#endif
