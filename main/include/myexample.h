#ifndef __MY_EXAMPLE__
#define __MY_EXAMPLE__

#if 0
typedef enum {
    BUS_CMD_OTA = 1,
    BUS_CMD_RESTART = 2,
    BUS_CMD_PIC = 3,
    BUS_CMD_LED = 4,
    BUS_CMD_ROOT_REGISTER = 5,
    BUS_CMD_NODE_REGISTER = 6,
    BUS_CMD_NODE_UNREGISTER = 7,
    BUS_CMD_PING = 8,
    BUS_CMD_SWITCH = 9,
    BUS_CMD_ROOT_SEND_PING = 10,
    BUS_CMD_MAX
} bus_cmd_t;

typedef enum {
    BUS_OTA_UPDATING = 100,
    BUS_RESTART_ERROR = 200,
} bus_status_t;

typedef enum {
    BUS_TYPE_REQUEST = 1,
    BUS_TYPE_RESPONSE = 2,
    BUS_TYPE_MAX
} bus_type_t;

typedef enum {
    LIGHT_OFF = 0,
    LIGHT_COLOR_BLUE = 1,
    LIGHT_COLOR_RED = 2,
    LIGHT_COLOR_GREEN = 3,
    LIGHT_COLOR_MAX
} light_color_t;

typedef enum {
    WORK_DEBUG = 1,
    WORK_PRODUCTION = 2,
    WORK_MAX = 3
} work_mode_t;

typedef struct {
   uint8_t mac_addr[MWIFI_ADDR_LEN];
   bool  online;
   bool  valid;
   char version[MAX_VERSION_NAME_LEN + 1];
} node_info_t;
#endif

#endif
