#include "node_task.h"
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

#include "common.h"

extern uint8_t g_self_mac[MWIFI_ADDR_LEN];
extern uint8_t g_root_mac[MWIFI_ADDR_LEN];

extern void period_feed_dog_in_second(uint32_t sec);

static const char *TAG = "node_task";
static uint8_t * g_node_mesh_buffer = NULL;
static light_color_t g_Led_color = LIGHT_OFF;
//static bool g_node_ping = false;
static bool bTaskRuning = false;

bool node_task_buffer_init(void) 
{
    g_node_mesh_buffer = (uint8_t *)MDF_MALLOC(CHILD_PACKET_LEN);
    if (NULL == g_node_mesh_buffer)
        return false;
    return true;
}
#if 0
bool isNodeGotPing(void)
{
    return g_node_ping;
}

void clearNodePingFlag(void)
{
    g_node_ping = false;
}
#endif
bool getNodeTaskRunStatus(void)
{
    return bTaskRuning;
}

void set_light_color(light_color_t color)
{
    g_Led_color = color;
    switch (color) {
        case LIGHT_OFF:
            gpio_set_level(LED_B_PIN, 0);
            gpio_set_level(LED_R_PIN, 0);
            gpio_set_level(LED_G_PIN, 0);
            break;
        case LIGHT_COLOR_BLUE:
            gpio_set_level(LED_B_PIN, 1);
            gpio_set_level(LED_R_PIN, 0);
            gpio_set_level(LED_G_PIN, 0);
            break;
        case LIGHT_COLOR_RED:
            gpio_set_level(LED_B_PIN, 0);
            gpio_set_level(LED_R_PIN, 1);
            gpio_set_level(LED_G_PIN, 0);
            break;
        case LIGHT_COLOR_GREEN:
            gpio_set_level(LED_B_PIN, 0);
            gpio_set_level(LED_R_PIN, 0);
            gpio_set_level(LED_G_PIN, 1);
            break;
        default:
            break;
    }    
}

light_color_t get_light_color(void)
{
    return g_Led_color;
}

//node send pic to root
static int node_send_pic(node_msg_t * pnodemsg)
{
    mdf_err_t ret                    = MDF_OK;
    mwifi_data_type_t data_type      = {0x0};

    MDF_LOGE("Camera Capture start");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        MDF_LOGE("Camera Capture Failed");
        return MDF_ERR_NO_MEM;
    }
    MDF_LOGE("Camera Capture end");

    uint8_t * buffer = fb->buf;
    size_t size = fb->len;
    uint16_t count = 1;
    uint16_t maxCount = (size + PACKET_BODY_LEN - 1) / PACKET_BODY_LEN;
    node_pic_t * pNodePic = (node_pic_t *)pnodemsg;

    /* count(2) + maxCount(2) + dataLen(2) + data */
    uint16_t dataLen = PACKET_BODY_LEN;
    pNodePic->u16MaxCount = maxCount;
    pNodePic->u16DataLen = dataLen;
    MDF_LOGI("maxcount:%d datalen=%d", maxCount, dataLen);
    period_feed_dog_in_second(10);
    while (count != maxCount)
    {
        pNodePic->u16Count = count;
        memcpy(pNodePic->u8Data, buffer, dataLen);
        //MDF_LOGI("send: max count  = %d, count = %d, datalen = %d", maxCount, count, dataLen);
        ret = mwifi_write(NULL, &data_type, pNodePic, CHILD_PACKET_LEN, true);
        MDF_ERROR_GOTO(ret != MDF_OK, FREE_MEM, "<%s> mwifi_node_write", mdf_err_to_name(ret));

        size -= dataLen;
        buffer += dataLen;
        count ++;
    }
    period_feed_dog_in_second(10);
    pNodePic->u16Count = count;
    pNodePic->u16DataLen = (uint16_t)size;
    memcpy(pNodePic->u8Data, buffer, pNodePic->u16DataLen);
    MDF_LOGI("last send:count = %d datalen=%d", count, size);
    ret = mwifi_write(NULL, &data_type, pNodePic, CHILD_PACKET_LEN, true);
    MDF_ERROR_GOTO(ret != MDF_OK, FREE_MEM, "<%s> mwifi_node_write", mdf_err_to_name(ret));
    MDF_LOGD("Node send one photo over : %u", fb->len);

FREE_MEM:
    esp_camera_fb_return(fb);
    return ret;
}

static int node_handle_led(node_msg_t * pnodemsg)
{
    mwifi_data_type_t data_type = {0x0};
    set_light_color(pnodemsg->tRespLight.u8Resp);
    memset(pnodemsg->u8Header, 0xff, PACKET_HEADER_LEN);
    pnodemsg->u8Type = BUS_TYPE_RESPONSE;
    pnodemsg->u8Cmd = BUS_CMD_LED;
    return mwifi_write(NULL, &data_type, pnodemsg, CHILD_PACKET_LEN, true);
}

//子节点发送给root的ping响应
static int node_handle_ping_from_tcpserver(node_msg_t * pnodemsg)
{
    mwifi_data_type_t data_type = {0x0};
    memset(pnodemsg->u8Header, 0xff, PACKET_HEADER_LEN);
    pnodemsg->u8Type = BUS_TYPE_RESPONSE;
    pnodemsg->u8Cmd = BUS_CMD_PING;
    pnodemsg->tRespLight.u8Resp = get_light_color();
    return mwifi_write(NULL, &data_type, pnodemsg, CHILD_PACKET_LEN, true);
}

static int node_handle_switch(node_msg_t * pnmsg)
{   
    mwifi_data_type_t data_type      = {0x0};
    int mode = pnmsg->tRespSetmode.u8Mode;
    ESP_LOGI(TAG, "work_mode = %d\n", mode);
    char acMode[2] = {0};
    acMode[0] = mode + 0x30;

    if (update_system_config("run_mode", acMode)) {
        MDF_LOGE("Error occured during write file\n");
    }

    memset(pnmsg->u8Header, 0xff, PACKET_HEADER_LEN);
    pnmsg->u8Type = BUS_TYPE_RESPONSE;
    pnmsg->u8Cmd = BUS_CMD_SWITCH;
    return mwifi_write(NULL, &data_type, pnmsg, CHILD_PACKET_LEN, true);
}

static int node_handle_ping_from_root(node_msg_t *pmsg)
{
    int i;
    mwifi_data_type_t data_type = {0x0};
    //子节点接收到root节点发送的ping信息
    //子节点会比较自身mac地址是否在root节点发送过来的设备中，如果不在其中，就向root节点发送自己的注册信息。
    //MDF_LOGI("ping:force =%d", pmsg->tRootPing.u8Force);
    switch(pmsg->tRootPing.u8Force) {
        case QUEUE_NODE_ADD:
        case QUEUE_NODE_PING:
            if (memcmp(g_root_mac, pmsg->tRootPing.u8RootMac, MWIFI_ADDR_LEN) != 0) {
                //root 节点改变，重新发送注册信息
                //root change
                //save root mac to local
                memcpy(g_root_mac, pmsg->tRootPing.u8RootMac, MWIFI_ADDR_LEN);
                goto SEND_REG;
            } else {
                for (i = 0; i < pmsg->tRootPing.u8NodeCount; i++) {
                    if (memcmp(g_self_mac, pmsg->tRootPing.u8NodeMac[i], MWIFI_ADDR_LEN) == 0) {
                        break;
                    }
                }
                if (i == pmsg->tRootPing.u8NodeCount) {
                    goto SEND_REG;
                }               
            }
            break;
        default:
            break;
    }

    return MDF_OK;

SEND_REG:
    //send register msg
    memset(pmsg->u8Header, 0xff, PACKET_HEADER_LEN);
    pmsg->u8Type = BUS_TYPE_REQUEST;
    pmsg->u8Cmd = BUS_CMD_NODE_REGISTER;
    pmsg->tDevVersion.u8Data = 1;
    memcpy(pmsg->tDevVersion.u8Addr, g_self_mac, MWIFI_ADDR_LEN);
    strncpy((char *)pmsg->tDevVersion.u8VersionStr, CONFIG_DEVICE_VERSION, strlen(CONFIG_DEVICE_VERSION) + 1);
    return mwifi_write(NULL, &data_type, pmsg, CHILD_PACKET_LEN, true);
}

//only node run this
void node_task(void *arg)
{
    mdf_err_t ret                       = MDF_OK;
    node_msg_t * pNodeMsg               = (node_msg_t *)g_node_mesh_buffer;
    size_t size;
    mwifi_data_type_t data_type         = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN]    = {0x0};
    bus_cmd_t cmd                       = BUS_CMD_MAX;
    bus_type_t type                     = BUS_TYPE_MAX;

    MDF_LOGI("NODE mesh read task is running on core %d", xPortGetCoreID());
    bTaskRuning = true;

    while (mwifi_is_connected())
    {
        if(esp_mesh_get_layer() == MESH_ROOT)
            break;

        size = CHILD_PACKET_LEN;
        memset(&data_type, 0, sizeof(mwifi_data_type_t));
        ret = mwifi_read(src_addr, &data_type, (uint8_t *)pNodeMsg, &size, 200);
        if (MDF_OK != ret) {
            continue;
        }

        //MDF_ERROR_CONTINUE(ret != MDF_OK, "node_task:mwifi_read err:%s", mdf_err_to_name(ret));

        if (data_type.upgrade) { // This mesh package contains upgrade data.
            ret = mupgrade_handle(src_addr, (uint8_t *)pNodeMsg, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        } else {
            // other business is here
            /*
             * root -> node non-pic
             * 0xFFFFFFFFFFFF(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
             */
            type = pNodeMsg->u8Type;
            cmd = pNodeMsg->u8Cmd;
            MDF_LOGI("===>recv message from root:type=%d, cmd=%d", type, cmd);
            if (BUS_TYPE_REQUEST == type) {
                switch (cmd) {
                    case BUS_CMD_RESTART:
                        MDF_LOGW("The node device will restart after 3 seconds");
                        vTaskDelay(pdMS_TO_TICKS(3000));
                        //system_reboot();
                        period_feed_dog_in_second(0);
                        period_feed_dog_in_second(0);
                        break;
                    case BUS_CMD_PIC:
                        ret = node_send_pic(pNodeMsg);
                        if (MDF_OK != ret) {
                            MDF_LOGE("node send pic error:%s", mdf_err_to_name(ret));
                        }
                        period_feed_dog_in_second(10);
                        break;
                    case BUS_CMD_LED:
                        ret = node_handle_led(pNodeMsg);
                        if (MDF_OK != ret) {
                            MDF_LOGE("node set led error:%s", mdf_err_to_name(ret));
                        }
                        period_feed_dog_in_second(10);
                        break;
#if 1
                    case BUS_CMD_PING:
                        //来自服务器的ping命令的响应动作,该指令用来获取节点的亮灯颜色
                        ret = node_handle_ping_from_tcpserver(pNodeMsg);
                        if (MDF_OK != ret) {
                            MDF_LOGE("node ping error:%s", mdf_err_to_name(ret));
                        }
                        period_feed_dog_in_second(10);
                        break;
#endif
                    case BUS_CMD_SWITCH:
                        // Open renamed file for reading
                        ret = node_handle_switch(pNodeMsg);
                        if( MDF_OK != ret ) {
                            MDF_LOGE("node set workmode error:%s", mdf_err_to_name(ret));
                        }
                        period_feed_dog_in_second(10);
                        break;
                    case BUS_CMD_ROOT_SEND_PING:
                        //来自root的心跳包，不需要发送响应信息。
                        //g_node_ping = true;
                        ret = node_handle_ping_from_root(pNodeMsg);
                        if (MDF_OK != ret) {
                            MDF_LOGE("node handler ping from root：%s", mdf_err_to_name(ret));
                        }
                        period_feed_dog_in_second(10);
                        break;
                    default:
                        MDF_LOGE("NODE mesh read ilegal cmd: %d", cmd);
                        break;
                }
            }   //if (BUS_TYPE_REQUEST == type) end
            else if (BUS_TYPE_RESPONSE == type) {
                MDF_LOGW("===> should never goto here, BUS_TYPE_RESPONSE");
            } else {
                MDF_LOGE("===> should never goto here, type = %d", type);
            }
        }
    }

    MDF_LOGW("NODE mesh read task is exit");
    //*pNodeTaskHandler = NULL;
    bTaskRuning = false;
    vTaskDelete(NULL);
}
