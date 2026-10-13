#pragma once
/* The optional test identity must have an explicit matching Runtime grant.
 * Default builds keep the existing app and persistent paths unchanged. */
#ifdef OBD_SIDE_BY_SIDE_TEST
#define OBD_APP_DIRECTORY "/Apps/obd-test"
#define OBD_TRIP_DIRECTORY "/OBD-Test"
#define OBD_STARTUP_DIAGNOSTIC "OBD/dashboard version=0.2.3 variant=obd-test transport=BLE clear=confirmed-only"
#else
#define OBD_APP_DIRECTORY "/Apps/obd-dashboard"
#define OBD_TRIP_DIRECTORY "/OBD"
#define OBD_STARTUP_DIAGNOSTIC "OBD/dashboard version=0.2.3 transport=BLE clear=confirmed-only"
#endif
