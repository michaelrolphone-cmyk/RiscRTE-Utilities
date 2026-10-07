#pragma once
#include "RiscBluetoothSensorsV1.h"
#define BLE_MAX_DEVICES RISC_BLE_MAX_DEVICES
#define BLE_IDLE RISC_BLE_SENSOR_IDLE
#define BLE_STARTING RISC_BLE_SENSOR_STARTING
#define BLE_SCANNING RISC_BLE_SENSOR_SCANNING
#define BLE_COMPLETE RISC_BLE_SENSOR_COMPLETE
#define BLE_ERROR RISC_BLE_SENSOR_ERROR
typedef risc_ble_sensor_device_v1 ble_device;
typedef struct { ble_device devices[BLE_MAX_DEVICES];unsigned count,dropped,malformed,phase;char error[48]; } ble_scan;
