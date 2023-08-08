#ifndef __CONFIG_H
#define __CONFIG_H

#define TAG "app"
#define DEBUG_SERIAL_SPEED 115200

#define DEVICE_PREFIX "esp32-"
#define DEVICE_WIFI_KEY "0123456789"

#include <LittleFS.h>
#define LOCALFS LittleFS
#define FORMAT_FS_IF_FAILED true

#endif