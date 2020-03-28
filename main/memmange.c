#include "mdf_common.h"
#include "mwifi.h"
#include "driver/uart.h"
#include "esp_camera.h"
#include "mupgrade.h"

#include "esp_spiffs.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "app_httpd.h"
#include "app_spiffs.h"

//#define CAMERA_MODEL_AI_THINKER
#define CAMERA_MODEL_AI_SELF
#include "camera_pins.h"
#include "lwip/netif.h"

//my define
#include "common.h"

int dummy(){

    return 0;
}
