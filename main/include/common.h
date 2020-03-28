#ifndef __COMMON__H__
#define __COMMON__H__

#include "mdf_common.h"
#include "mwifi.h"

#define CHILD_PACKET_LEN        1430
#define ROOT_PACKET_LEN         (CHILD_PACKET_LEN + MWIFI_ADDR_LEN)
#define PACKET_HEADER_LEN       6
#define PACKET_BODY_LEN         (CHILD_PACKET_LEN - PACKET_HEADER_LEN)
#define TCP_CLIENT_RECV_LEN     101
#define PHOTO_PACKET_MAX_COUNT  400
#define OTA_NAME_LEN            31
#define MAX_VERSION_NAME_LEN    15
#define MAX_MESH_SIZE           20
#define HEART_BEAT_TRY_MAX      18
#define MAX_MESH_DISCONNECT_CNT 18
#define MAX_NO_TASK_CNT         18

#define WTD_PIN                 22
#define LED_G_PIN               GPIO_NUM_5
#define LED_R_PIN               GPIO_NUM_19
#define LED_B_PIN               GPIO_NUM_21

#define ROOT_SEND_PING_INTERVAL 8
#define MAX_NODE_COUNT          45

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

#pragma pack(1)

typedef struct node_info_t {
    struct node_info_t *next;
    uint8_t mac_addr[MWIFI_ADDR_LEN];
    char version[MAX_VERSION_NAME_LEN + 1];
} node_info_t;

//根节点注册
//根节点mac(6) + 0xff(6) + 请求编号『1』(1) + 根节点注册类型编号『5』(1) + 版本号(不定长) + 结束符『0』(1) + 字节流补充(不定长)
typedef struct {
    char u8Version[64];
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 64];
} root_node_register_t;

//子节点注册
//根节点MAC(6) + 0xff(6) + 请求编号『1』(1) + 子节点注册类型编号『6』(1) + 子节点注册数量(1) + [子节点MAC(6) + 子节点版本号(不定长) + 结束符『0』(1)] * 子节点注册数量 + 字节流补充(不定长)
typedef struct {
    uint8_t u8Number;
    uint8_t u8RegPayload[MAX_MESH_SIZE][MWIFI_ADDR_LEN + MAX_VERSION_NAME_LEN];
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 3 - (MAX_MESH_SIZE * (MWIFI_ADDR_LEN + MAX_VERSION_NAME_LEN))];
} child_node_register_t;

//子节点反注册
//根节点MAC(6) + 0xff(6) + 请求编号『1』(1) + 子节点反注册类型编号『7』(1) + 子节点反注册数量(1) + 子节点MAC(6) * 子节点反注册数量 + 字节流补充(不定长)
typedef struct {
    uint8_t u8Number;
    uint8_t u8DevMac[MAX_MESH_SIZE][MWIFI_ADDR_LEN];
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 3 - (MWIFI_ADDR_LEN * MAX_MESH_SIZE)];
} child_node_unregister_t;

//重启响应
//节点MAC(6) + 0xff(6) + 响应编号『2』(1) + 重启类型编号『2』(1) + 失败编号正在升级『200』(1) + 字节流补充(不定长)
//根节点MAC(6) + 0xff(6) + 响应编号『2』(1) + 重启类型编号『2』(1) + 失败编号正在升级『200』(1) + 重启失败数量(1) + 重启失败节点MAC(6) * 重启失败数量 + 字节流补充(不定长)
typedef struct {
    uint8_t u8Response;
    uint8_t u8Number;
    uint8_t u8DevAddr[MAX_MESH_SIZE][MWIFI_ADDR_LEN];
    uint8_t u8Dummy01[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 -2 - (MWIFI_ADDR_LEN * MAX_MESH_SIZE)];
} resp_restart_t;

//拉取图片响应
//上传节点MAC(6) + 当前包索引号(2) + 包大小(2) + 内容字节长度(2) + 内容字节流(1424)


//亮灯响应
//节点MAC(6) + 0xff(6) + 响应编号『2』(1) + 亮灯类型编号『4』(1) + 亮灯编号(1) + 字节流补充(不定长)
typedef struct {
    uint8_t u8Resp;
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 1];
} resp_light_t;

//模式切换响应
typedef struct {
    uint8_t u8Mode;
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 1];
} resp_setmode_t;

//发送给root节点的版本信息
typedef struct {
    uint8_t u8Data;
    uint8_t u8Addr[MWIFI_ADDR_LEN];
    uint8_t u8VersionStr[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 1 - MWIFI_ADDR_LEN];
} device_version_t;

//ota
typedef struct {
    uint8_t u8OtaStatus;
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 1];
} resp_ota_t;

typedef struct {
    uint8_t u8UncertainData[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2];
} resp_ota_last_t;

typedef struct {
    uint8_t u8Force;
    uint8_t u8RootMac[6];
    uint8_t u8NodeCount;
    uint8_t u8NodeMac[20][6];
    uint8_t u8Dummy[CHILD_PACKET_LEN - PACKET_HEADER_LEN - 2 - 1 - 6 - 1 - 120];
} root_ping_t;
#if 0
typedef struct {
    uint8_t u8NodeMac[6];
    uint8_t u8Dummy[CHILD_PACKET_LEN - CHILD_PACKET_LEN - 2 - 6];
} node_resp_ping_t;
#endif
typedef struct {
    uint8_t u8Header[PACKET_HEADER_LEN];
    uint8_t u8Type;
    uint8_t u8Cmd;
    union {
        root_node_register_t        tRootNodeRegister;
        child_node_register_t       tChilNodeRegister;
        child_node_unregister_t     tChildNodeUnregister;
        resp_restart_t              tRespRestart;
        resp_light_t                tRespLight;
        resp_setmode_t              tRespSetmode;
        device_version_t            tDevVersion;
        resp_ota_t                  tRespOta;
        resp_ota_last_t             tRespOtaLast;
        root_ping_t                 tRootPing;
        //node_resp_ping_t            tNodeRespPing;
    };
} node_msg_t;

typedef struct {
    uint8_t u8Addr[MWIFI_ADDR_LEN];
    node_msg_t tNode;
} root_msg_t;

//服务端发送给客户端的包长度：101
//OTA升级请求
//根节点MAC(6) + 请求编号(1) + OTA升级类型编号(1) + 升级文件名(32) + 数量(1) + MAC列表(60)
typedef struct {
    char name[32];
    uint8_t u8Number;
    uint8_t tOtaAddr[10][6];
    uint8_t u8Dummy[TCP_CLIENT_RECV_LEN - PACKET_HEADER_LEN - 2 - 32 - 1 - 60];
} device_ota_t;

//重启请求
//根节点MAC(6) + 请求编号(1) + 重启类型编号(1) + 数量(1) + MAC列表(60) + 字节流补充(不定长)
typedef struct {
    uint8_t u8Number;
    uint8_t tDevAddr[10][6];
    uint8_t u8Dummy[TCP_CLIENT_RECV_LEN - PACKET_HEADER_LEN - 2 - 1 - 60];
} device_restart_t;

//拉去图片
//节点MAC(6) + 请求编号(1) + 拉取图片类型编号(1) + 字节流补充(不定长)
//不需要单独定义结构体

//亮灯
//节点MAC(6) + 请求编号(1) + 亮灯类型编号(1) + 亮灯编号(1) + 字节流补充(不定长)
typedef struct {
    uint8_t u8LedColor;
    uint8_t u8Dummy[TCP_CLIENT_RECV_LEN - PACKET_HEADER_LEN - 2 - 1];
} device_light_t;

//查询亮灯
//节点MAC(6) + 请求编号『1』(1) + 查询亮灯类型编号『8』(1) + 字节流补充(不定长)
//不需要单独定义结构体

//模式切换
//节点MAC(6) + 请求编号(1) + 模式切换类型编号『9』(1) + 模式编号(1) + 字节流补充(不定长)
typedef struct {
    uint8_t u8Mode;
    uint8_t u8Dummy[TCP_CLIENT_RECV_LEN - PACKET_HEADER_LEN - 2 - 1];
} device_setmode_t;

//服务端发送给客户端的包长度：101
typedef struct {
    uint8_t u8Header[PACKET_HEADER_LEN];
    uint8_t u8Type;
    uint8_t u8Cmd;
    union {
        device_restart_t    tDevRestart;
        device_ota_t        tDevOta;
        device_light_t      tDevLight;
        device_setmode_t    tDevSetmode;
    };
} server_cmd_t;

typedef struct {
    uint16_t u16Count;
    uint16_t u16MaxCount;
    uint16_t u16DataLen;
    uint8_t  u8Data[CHILD_PACKET_LEN - 6];
} node_pic_t;

typedef enum {
    QUEUE_NONE = 0,
    QUEUE_NODE_ADD = 1,
    QUEUE_NODE_REMOVE = 2,
    QUEUE_NODE_PING = 3,
} msgqueue_type_t;

#pragma pack()

typedef struct {
    uint8_t     u8cmd;
    //uint8_t     u8flag;
    //uint32_t    u32timestamp;
} queue_msg_t;

typedef struct {
    bool bAddrValid;
    uint8_t u8addr[6];
    server_cmd_t tServerCmd;
} serverqueue_msg_t;

#endif
