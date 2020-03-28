/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is MDF_FREE of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * tcp server -> root
 * dest_mac(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : TCP_CLIENT_RECV_LEN
 */

/*
 * root -> tcp server non-pic
 * src_mac(6) +  0xFFFFFFFFFFFF(6) + CMD_RESPONSE(1) + cmd(1) + payload(?) ---- total : ROOT_PACKET_LEN
 */

/*
 * root -> tcp server pic
 * src_mac(6) + count(2) + maxCount(2) + dataLen(2) + data ---- total : ROOT_PACKET_LEN
 */

/*
 * node -> root pic
 * count(2) + maxCount(2) + dataLen(2) + data ---- total : MWIFI_ADDR_LEN
 */

/*
 * node -> root non-pic
 * 0xFFFFFFFFFFFF(6) + CMD_RESPONSE(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
 */

/*
 * root -> node/root non-pic
 * 0xFFFFFFFFFFFF(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
 */

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

#define CAMERA_MODEL_AI_SELF
#include "camera_pins.h"
#include "lwip/netif.h"

#include <sys/select.h>
#include <fcntl.h>

#include "common.h"
#include "node_dev_list_t.h"
#include "node_task.h"

static void camera_setup();
static void watchdog_setup();
static void light_setup();
static int socket_tcp_client_create(const char *ip, uint16_t port);
static void root_send_photo(root_msg_t * data);

static void tcp_client_read_task(void *arg);
static void root_mesh_read_task(void *arg);
static void root_ota_read_task(void *arg);
static void ota_task(void *arg);
//SemaphoreHandle_t LockOfWtd;

uint8_t g_self_mac[MWIFI_ADDR_LEN] = {0};
uint8_t g_root_mac[MWIFI_ADDR_LEN] = {0};

static int g_sockfd                                                             = -1; // tcp client socket
static const char *TAG                                                          = "router_example";
static TaskHandle_t g_tcp_client_read_task_handle                               = NULL; // root task
static TaskHandle_t g_root_mesh_read_task_handle                                = NULL; // root task
static TaskHandle_t g_root_ota_read_task_handle                                 = NULL; // root task
static uint8_t * g_tcp_client_read_data                                         = NULL;
static uint8_t * g_root_mesh_read_data                                          = NULL;
static char url_ota[128]                            							= {0};
static bool g_ota_read_task_flag                                                = false;
static char g_ota_firmware_name[OTA_NAME_LEN + 1]                               = {0};
static const char *ANDROID_HOTSPOT_SSID                                         = "AndroidAP_9699";
static const char *ANDROID_HOTSPOT_PASSWORD                                     = "FrogsHealth@32";
static char ConfigServerIp[16]                                                  = "192.168.199.200";
static unsigned int ConfigServerPort                                            = 8888;

static QueueHandle_t MeshQueue = NULL;
static QueueHandle_t tcpQueue = NULL;
static QueueHandle_t ServerQueue = NULL;

static bool bDeviceOtaFlag = false;

static inline int get_two_timestamp_interval(uint32_t curtime, uint32_t lasttime)
{
    return ((curtime - lasttime) * portTICK_RATE_MS / 1000);
}



void feed_dog(void)
{
    MDF_LOGI("===>feed dog!");
    gpio_set_level(22, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(22, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(22, 0);
}

void system_reboot(void) {
    // need restart
    MDF_LOGI("===========> i am will restart!!!!!");
    //xTimerStop(g_timer, 0); 
    vTaskDelay(3000);
    feed_dog();
    feed_dog();
}

static void camera_setup()
{
#ifdef CAMERA_MODEL_AI_SELF
    /* IO13, IO14 is designed for JTAG by default,
     * to use it as generalized input,
     * firstly declair it as pullup input */
    gpio_config_t conf;
    conf.mode = GPIO_MODE_INPUT;
    conf.pull_up_en = GPIO_PULLUP_ENABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    conf.pin_bit_mask = 1LL << 13;
    gpio_config(&conf);
    conf.pin_bit_mask = 1LL << 14;
    gpio_config(&conf);
#endif

    camera_config_t camera_config;
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.pixel_format = PIXFORMAT_JPEG;
    camera_config.frame_size = FRAMESIZE_UXGA;
    //camera_config.frame_size = FRAMESIZE_SVGA;
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 2;

    // camera init
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        MDF_LOGE("Camera init failed with error 0x%x", err);
        //vTaskDelay(3000 / portTICK_PERIOD_MS);
        //esp_restart();
        system_reboot();
        while(true);
        return;
    }
    sensor_t * s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_UXGA);

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Test Camera capture failed");
        //xTimerStop(g_timer, 0);
        system_reboot();
        while(true);
        return;
    }
    esp_camera_fb_return(fb);

    //vTaskDelay(3000 / portTICK_PERIOD_MS);
    MDF_LOGI("Camera init suc...");
}

/* hardware watchdog 30s reset */
static void watchdog_setup()
{
    gpio_config_t conf;
    conf.mode = GPIO_MODE_OUTPUT;
    conf.pull_up_en = GPIO_PULLUP_ENABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    conf.pin_bit_mask = 1LL << WTD_PIN;
    gpio_config(&conf);
}

static void light_setup()
{
    gpio_config_t conf;
    conf.mode = GPIO_MODE_OUTPUT;
    //conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    conf.pull_up_en = GPIO_PULLUP_ENABLE;
    conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    conf.intr_type = GPIO_INTR_DISABLE;
    conf.pin_bit_mask = (1LL << LED_G_PIN) | ( 1LL << LED_R_PIN) | (1LL << LED_B_PIN); 
    gpio_config(&conf);
}

static void set_work_mode(work_mode_t mode)
{
    mdf_err_t ret = MDF_OK;
    ESP_LOGI(TAG, "work_mode = %d\n", mode);
    char acMode[2] = {0};
    acMode[0] = mode + 0x30;
    ret = update_system_config("run_mode", acMode);
    if(ret) {
        MDF_LOGE("Error occured during write file\n");
    } else {
        MDF_LOGI("write file suc\n");
    }
}

/**
 * @brief Create a tcp client
 */
static int socket_tcp_client_create(const char *ip, uint16_t port)
{
    MDF_PARAM_CHECK(ip);

    MDF_LOGI("Create a tcp client, ip: %s, port: %d", ip, port);

    int sockfd    = -1;
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip),
    };

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        MDF_LOGI("socket create, sockfd: %d", sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)) < 0) {
        MDF_LOGI("connect server[%s]:port[%d] failed", ip, port);
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

static int root_send_register(int socket_fd, root_msg_t * pRootMsg)
{
    //send root register message
    memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
    memset(pRootMsg->tNode.u8Header, 0xff, PACKET_HEADER_LEN);
    pRootMsg->tNode.u8Type = BUS_TYPE_REQUEST;
    pRootMsg->tNode.u8Cmd = BUS_CMD_ROOT_REGISTER;
    strncpy(pRootMsg->tNode.tRootNodeRegister.u8Version, CONFIG_DEVICE_VERSION, strlen(CONFIG_DEVICE_VERSION) + 1);
    if (send(socket_fd, pRootMsg, sizeof(root_msg_t), 0) < 0) {
        MDF_LOGE("send root register message failed");
        return -1;
    }
    
    //send child register message
    uint8_t node_num = 0;
    int offset = 0;
    node_info_t * head = get_nodedev_list_head();

    for (; NULL != head; head = head->next) {
        memcpy(pRootMsg->tNode.tChilNodeRegister.u8RegPayload + offset, head->mac_addr, sizeof(head->mac_addr));
        memcpy(pRootMsg->tNode.tChilNodeRegister.u8RegPayload + MWIFI_ADDR_LEN + offset, head->version, strlen(head->version) + 1);
        MDF_LOGI("root send register node[%02x:%02x:%02x:%02x:%02x:%02x] version : %s", head->mac_addr[0],head->mac_addr[1],
                head->mac_addr[2],head->mac_addr[3],head->mac_addr[4],head->mac_addr[5],head->version);

        offset += MWIFI_ADDR_LEN + strlen(head->version) + 1;
        node_num++;
    }

    if (node_num) {
        memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
        memset(pRootMsg->tNode.u8Header, 0xff, PACKET_HEADER_LEN);
        pRootMsg->tNode.u8Type = BUS_TYPE_REQUEST;
        pRootMsg->tNode.u8Cmd = BUS_CMD_NODE_REGISTER;
        pRootMsg->tNode.tChilNodeRegister.u8Number = node_num;
        if(send(socket_fd, pRootMsg, sizeof(root_msg_t), 0) < 0) {
            MDF_LOGE("send node register message failed");
            return -1;
        }
    }
    
    return 0;
}

static void root_do_child_offline(int socket_fd, queue_msg_t * pQMsg, root_msg_t *pRootMsg)
{
    //table list 利用root包的没用上的内存空间
    mesh_addr_t *pTableList = (mesh_addr_t *)pRootMsg->tNode.tChildNodeUnregister.u8Dummy;
    mesh_addr_t *pOriginTL = pTableList;

    size_t table_size;
    int i, ret;
    int rm_num = 0;

    node_info_t * head = get_nodedev_list_head();
    node_info_t * next = head;
    
    if (pQMsg->u8cmd == QUEUE_NODE_REMOVE) {
        table_size = esp_mesh_get_routing_table_size();
        ret = esp_mesh_get_routing_table(pTableList, table_size * sizeof(mesh_addr_t), (int *)&table_size);
        MDF_LOGI("offline node number = %d", table_size);
        if (MDF_OK != ret)
            return;
        //找出不在设备路由表中的设备mac地址，发送给tcpserver，并删除nodedev_list对应的节点
        //此处应该对比的是本地的nodedev链表中的设备是否在路由表中
        while(NULL != next) {
            pTableList = pOriginTL;
            for (i = 0; i < table_size; i++) {
                if (memcmp(head->mac_addr, pTableList->addr, sizeof(head->mac_addr)) == 0) {
                    break; 
                }
                pTableList++;
            }
            next = head->next;
            if (i == table_size) {
                memcpy(pRootMsg->tNode.tChildNodeUnregister.u8DevMac[rm_num], head->mac_addr, sizeof(head->mac_addr));
                del_node_from_nodedev_list(head->mac_addr);
                rm_num++;
                MDF_LOGI("offline devaddr:%02x:%02x:%02x:%02x:%02x:%02x", 
                        head->mac_addr[0],head->mac_addr[1],head->mac_addr[2],head->mac_addr[3],head->mac_addr[4],head->mac_addr[5]);
            }
            head = next;
        }

        if (rm_num) {
            memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
            memset(pRootMsg->tNode.u8Header, 0xff, MWIFI_ADDR_LEN);
            pRootMsg->tNode.u8Type = BUS_TYPE_REQUEST;
            pRootMsg->tNode.u8Cmd = BUS_CMD_NODE_UNREGISTER;
            pRootMsg->tNode.tChildNodeUnregister.u8Number = rm_num;

            ret = send(socket_fd, pRootMsg, sizeof(root_msg_t), 0);
            if (ret < 0) {
                MDF_LOGE("root_do_child_offline: errno %d", errno);
            }
        }
    }
}

void period_feed_dog_in_second(uint32_t sec)
{
    static uint32_t u32LastTimeStamp = 0;
    uint32_t u32CurTimeStamp = xTaskGetTickCount();
    uint32_t u32Total;

    if (u32CurTimeStamp > u32LastTimeStamp) {
        if (get_two_timestamp_interval(u32CurTimeStamp, u32LastTimeStamp) >= sec) {
            u32LastTimeStamp = u32CurTimeStamp;
            feed_dog();
        }
    } else {
        //可能发生了溢出，计时器翻转
        u32Total = u32CurTimeStamp + (0xffffffff - u32LastTimeStamp);
        if (get_two_timestamp_interval(u32Total, 0) >= sec) {
            u32LastTimeStamp = u32CurTimeStamp;
            feed_dog();
        }
    }
}
/*
 * read command data from tcp server
 * transport command data to children or root itself
 */
static void tcp_client_read_task(void *arg)
{
    mdf_err_t ret           = MDF_OK;
    server_cmd_t *pSCmd     = (server_cmd_t *)g_tcp_client_read_data;
    bool bVaild             = false;
    MDF_LOGI("TCP client read task is running on core %d", xPortGetCoreID());
    serverqueue_msg_t   sqmsg;

    fd_set rd;
    struct timeval tv;
    int alreadReadNumber = 0;
    int socket_maybe_close = 0;

    while (mwifi_is_connected()) {
        
        if (esp_mesh_get_layer() != MESH_ROOT) {
            MDF_LOGI("this node not root, exit tcp thread");
            break;
        }
        
        period_feed_dog_in_second(10);

        if (bDeviceOtaFlag == true) {
            vTaskDelay(500);
            continue;
        }

        if (g_sockfd == -1) {
            vTaskDelay(200);
            continue;
        }

        /*
         * tcp server -> root
         * dest_mac(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : TCP_CLIENT_RECV_LEN
         */
        FD_ZERO(&rd);
        FD_SET(g_sockfd, &rd);
        tv.tv_sec = 0;
        tv.tv_usec = 200*1000;

        ret = select(g_sockfd + 1, &rd, NULL, NULL, &tv);
        if (ret == -1) {
            MDF_LOGE("epoll failed");
            close(g_sockfd);
            g_sockfd = -1;
            continue;
        } else if (ret == 0) {
            continue;
        } else {
            if (FD_ISSET(g_sockfd, &rd)) {
                
                if (alreadReadNumber >= TCP_CLIENT_RECV_LEN)
                    alreadReadNumber = 0;

                ret = read(g_sockfd, pSCmd + alreadReadNumber, TCP_CLIENT_RECV_LEN - alreadReadNumber);
                if (ret > 0) {
                    socket_maybe_close = 0;
                    alreadReadNumber += ret;
                    MDF_LOGI("get socket data:%d %d", alreadReadNumber, ret);
                    if (TCP_CLIENT_RECV_LEN != alreadReadNumber) {
                        continue;
                    } else {
                        alreadReadNumber = 0;
                        MDF_LOGI("===>socket recv:[%02x:%02x:%02x:%02x:%02x:%02x] type=%d cmd=%d",
                                pSCmd->u8Header[0],pSCmd->u8Header[1],pSCmd->u8Header[2],
                                pSCmd->u8Header[3],pSCmd->u8Header[4],pSCmd->u8Header[5],
                                pSCmd->u8Type, pSCmd->u8Cmd);
                    }
                } else if (ret == 0) {
                    socket_maybe_close++;
                    if (socket_maybe_close > 20) {
                        socket_maybe_close = 0;
                        MDF_LOGE("socket server maybe close!");
                        close(g_sockfd);
                        g_sockfd = -1;
                    }
                } else {
                    socket_maybe_close = 0;
                    MDF_LOGW("<%s> TCP read", strerror(errno));
                    close(g_sockfd);
                    g_sockfd = -1;
                }

                //对服务器发来所有数据不进行任何校验，全部认为是有效的数据?
                /*
                 * root -> node/root non-pic
                 * 0xFFFFFFFFFFFF(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
                 */
                bVaild = false;
                if (0 == memcmp(g_self_mac, pSCmd->u8Header, MWIFI_ADDR_LEN)) {
                    //to root
                    sqmsg.bAddrValid = false;
                    bVaild = true;
                } else {
                    //to node
                    if (check_node_is_exist(pSCmd->u8Header)) {
                        memcpy(sqmsg.u8addr, pSCmd->u8Header, MWIFI_ADDR_LEN);
                        sqmsg.bAddrValid = true;
                        bVaild = true;
                    }
                }

                if (true == bVaild) {
                    if (ServerQueue) {
                        //memset(pSCmd->u8Header, 0xff, MWIFI_ADDR_LEN);
                        memset(sqmsg.tServerCmd.u8Header, 0xff, MWIFI_ADDR_LEN);
                        memcpy(&sqmsg.tServerCmd.u8Type, &pSCmd->u8Type, sizeof(server_cmd_t) - MWIFI_ADDR_LEN);
                        xQueueSend(ServerQueue, &sqmsg, 0);
                    }
                }           
            }
        }

    }

//EXIT:
    period_feed_dog_in_second(10);
    MDF_LOGW("TCP client read task is exit");
    // need restart
    close(g_sockfd);
    g_sockfd = -1;
    g_tcp_client_read_task_handle = NULL;
    vTaskDelete(NULL);
}

static void root_send_photo(root_msg_t * pRootMsg)
{
    if (-1 == g_sockfd) {
        return;
    }
    
    MDF_LOGI("root_send_photo");
    uint16_t dataLen;
    node_pic_t *pNodePic = (node_pic_t *)pRootMsg->tNode.u8Header;
    camera_fb_t * fb = esp_camera_fb_get();
    if (NULL == fb) {
        MDF_LOGW("Camera Capture Failed");
        system_reboot();
    }
    MDF_LOGW("Camera Capture Over");

    uint8_t * buffer = fb->buf;
    size_t size = fb->len;
    // MDF_LOGI("fb->len = %u", size);
    uint16_t count = 1;
    uint16_t maxCount = (size + PACKET_BODY_LEN - 1) / PACKET_BODY_LEN;

    /* root_mac_addr(6) + count(1) + maxCount(1) + dataLen(2) + data */
    dataLen = PACKET_BODY_LEN;
    memcpy(pRootMsg->u8Addr, g_self_mac, sizeof(pRootMsg->u8Addr));
    while (count < maxCount)
    {
        //fill_root_packet(data, count, maxCount, buffer, dataLen);
        pNodePic->u16Count = count;
        pNodePic->u16MaxCount = maxCount;
        pNodePic->u16DataLen = dataLen;
        memcpy(pNodePic->u8Data, buffer, sizeof(pNodePic->u8Data));
        //MDF_LOGI("Root send: count: %u, maxCount: %u, size: %u", count, maxCount, dataLen);
        if ( send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0) < 0) {
            MDF_LOGE("Error occured during sending: errno %d", errno);
            size = 0;
            break;
        }
        size -= dataLen;
        buffer += dataLen;
        count ++;
    }

    if (size) {
        dataLen = size;
        pNodePic->u16Count = count;
        pNodePic->u16MaxCount = maxCount;
        pNodePic->u16DataLen = dataLen;
        memcpy(pNodePic->u8Data, buffer, sizeof(pNodePic->u8Data));
        MDF_LOGD("Root send: maxCount: %u", maxCount);
        if (send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0) < 0) {
            MDF_LOGE("Error occured during sending: errno %d", errno);
        }
    }
    esp_camera_fb_return(fb);
}

//download update firmware thread task
static void ota_task(void *arg)
{
    mdf_err_t ret                       = MDF_OK;
    uint8_t *data                       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t total_size                   = 0;
    int start_time                      = 0;
    root_msg_t *pRootMsg                = MDF_MALLOC(ROOT_PACKET_LEN);
    mupgrade_result_t upgrade_result    = {0};
    uint8_t u8retrycount = 0;
    /**
     * @note If you need to upgrade all devices, pass MWIFI_ADDR_ANY;
     *       If you upgrade the incoming address list to the specified device
     */
    // uint8_t dest_addr[][MWIFI_ADDR_LEN] = {{0x1, 0x1, 0x1, 0x1, 0x1, 0x1}, {0x2, 0x2, 0x2, 0x2, 0x2, 0x2},};
    uint8_t * dest_data = (uint8_t *)arg;
    int num = dest_data[0];
    MDF_LOGI("Ota task get tmp_data num is: %d\n", num);
    uint8_t dest_addr[MAX_MESH_SIZE][MWIFI_ADDR_LEN] = {0};    
    if (num == 0) {             // upgrade all devices
        for (int i = 0; i < MWIFI_ADDR_LEN; i ++) {
            dest_addr[num][i] = 0xFF;
        }
        num = 1;                // 作为节点数量
    } else if (num > 0) {       // upgrade dest addr devices
        int m = 1;
        for (int i = 0; i < num; i++) {
            for (int j = 0; j < MWIFI_ADDR_LEN; j++) {
                dest_addr[i][j] = dest_data[m];
                m++;
            }
        }
    }

    /**
     * @brief In order to allow more nodes to join the mesh network for firmware upgrade,
     *      in the example we will start the firmware upgrade after 30 seconds.
     */
    vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);

    esp_http_client_config_t config = {
        .url            = url_ota,
        .transport_type = HTTP_TRANSPORT_UNKNOWN,
    };

    MDF_LOGW("show url ota is: %s", url_ota);
    /**
     * @brief 1. Connect to the server
     */
    esp_http_client_handle_t client = esp_http_client_init(&config);
    MDF_ERROR_GOTO(!client, EXIT, "Initialise HTTP connection");

    start_time = xTaskGetTickCount();
    MDF_LOGI("Open HTTP connection: %s", url_ota);

    /**
     * @brief First, the firmware is obtained from the http server and stored on the root node.
     */
    do {
        ret = esp_http_client_open(client, 0);

        if (ret != MDF_OK) {
            if (!esp_mesh_is_root()) {
                goto EXIT;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            MDF_LOGW("<%s> Connection service failed", mdf_err_to_name(ret));
        }
        u8retrycount++;
    } while ((ret != MDF_OK) && (u8retrycount < 20));
    
    if (u8retrycount == 20)
        goto EXIT;

    total_size = esp_http_client_fetch_headers(client);
    MDF_LOGI("Down HTTP frame package name is: %s", g_ota_firmware_name);

    if (total_size <= 0) {
        MDF_LOGW("Please check the address of the server");
        ret = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(ret < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        MDF_LOGW("Recv data: %.*s", ret, data);
        goto EXIT;
    }

    /**
     * @brief 2. Initialize the upgrade status and erase the upgrade partition.
     */
    ret = mupgrade_firmware_init(g_ota_firmware_name, total_size);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Initialize the upgrade status", mdf_err_to_name(ret));

    /**
     * @brief 3. Read firmware from the server and write it to the flash of the root node
     */
    for (ssize_t size = 0, recv_size = 0; recv_size < total_size; recv_size += size) {
        size = esp_http_client_read(client, (char *)data, MWIFI_PAYLOAD_LEN);
        MDF_ERROR_GOTO(size < 0, EXIT, "<%s> Read data from http stream", mdf_err_to_name(ret));

        if (size > 0) {
            /* @brief  Write firmware to flash */
            ret = mupgrade_firmware_download(data, size);
            MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> Write firmware to flash, size: %d, data: %.*s",
                           mdf_err_to_name(ret), size, size, data);
            MDF_LOGI("Write firmware to flash, size: %d, total: %d", size, total_size);
        } else {
            MDF_LOGW("<%s> esp_http_client_read", mdf_err_to_name(ret));
            goto EXIT;
        }
    }

    MDF_LOGI("The service download firmware is complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);

    start_time = xTaskGetTickCount();

    /**
     * @brief 4. The firmware will be sent to each node.
     */
    ret = mupgrade_firmware_send((uint8_t *)dest_addr, num, &upgrade_result);
    MDF_ERROR_GOTO(ret != MDF_OK, EXIT, "<%s> mupgrade_firmware_send", mdf_err_to_name(ret));

    if (upgrade_result.successed_num == 0) {
        MDF_LOGW("Devices upgrade failed, unfinished_num: %d", upgrade_result.unfinished_num);
        goto EXIT;
    }

    MDF_LOGI("Firmware is sent to the device to complete, Spend time: %ds",
             (xTaskGetTickCount() - start_time) * portTICK_RATE_MS / 1000);
    MDF_LOGI("Devices upgrade completed, successed_num: %d, unfinished_num: %d", upgrade_result.successed_num, upgrade_result.unfinished_num);

EXIT:
    /**
     * @brief 5. send upgrade_result to server
     */
    memset(pRootMsg, 0, ROOT_PACKET_LEN);
    //memcpy(send_data, g_self_mac, MWIFI_ADDR_LEN);
    memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
    //memset(send_data + MWIFI_ADDR_LEN, 0xFF, MWIFI_ADDR_LEN);
    memset(pRootMsg->tNode.u8Header, 0xff, MWIFI_ADDR_LEN);
    pRootMsg->tNode.u8Type = BUS_TYPE_RESPONSE;
    pRootMsg->tNode.u8Cmd = BUS_CMD_OTA;
    //send_data[12] = BUS_TYPE_RESPONSE;
    //send_data[13] = BUS_CMD_OTA;
    pRootMsg->tNode.tRespOtaLast.u8UncertainData[0] = upgrade_result.successed_num;
    //send_data[14] = upgrade_result.successed_num;
    //memcpy(send_data + 15, upgrade_result.successed_addr, MWIFI_ADDR_LEN * upgrade_result.successed_num);
    memcpy(&pRootMsg->tNode.tRespOtaLast.u8UncertainData[1], upgrade_result.successed_addr, MWIFI_ADDR_LEN * upgrade_result.successed_num );

    //send_data[15 + MWIFI_ADDR_LEN * upgrade_result.successed_num] = upgrade_result.unfinished_num;
    pRootMsg->tNode.tRespOtaLast.u8UncertainData[1 + MWIFI_ADDR_LEN * upgrade_result.successed_num] = upgrade_result.unfinished_num;
    //memcpy(send_data + 16 + MWIFI_ADDR_LEN * upgrade_result.successed_num, upgrade_result.unfinished_addr, MWIFI_ADDR_LEN * upgrade_result.unfinished_num);
    memcpy(&pRootMsg->tNode.tRespOtaLast.u8UncertainData[2 + MWIFI_ADDR_LEN * upgrade_result.successed_num],
                upgrade_result.unfinished_addr,
                MWIFI_ADDR_LEN * upgrade_result.unfinished_num );
    
    // send successed_addr to server
    send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0);
    MDF_LOGI("==== upgrade successed_num is: %d", upgrade_result.successed_num);
    MDF_LOGI("==== Devices upgrade completed, just waiting cmd and restart! ====");

    MDF_FREE(data);
    MDF_FREE(pRootMsg);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    mupgrade_result_free(&upgrade_result);
    g_ota_read_task_flag = false;
    vTaskDelete(NULL);

    bDeviceOtaFlag = false;
    MDF_FREE(dest_data);
}

static int root_do_ota(root_msg_t *pRootMessage, server_cmd_t * pServerCmd) 
{
    MDF_LOGI("root do ota");
    uint8_t * dest_data = (uint8_t *)MDF_MALLOC(sizeof(uint8_t) * 61); //61 = 10 * macaddr + 1
    root_msg_t *pRootmsg = pRootMessage;

    if (NULL == pRootmsg || NULL == dest_data || NULL == pServerCmd) {
        MDF_LOGE("root_do_ota MDF_MALLOC failed");
        return -1;
    }

    bDeviceOtaFlag = true;

    memcpy(g_ota_firmware_name, pServerCmd->tDevOta.name, OTA_NAME_LEN + 1);
    sprintf(url_ota, "http://%s/firmware_name/%s", ConfigServerIp, g_ota_firmware_name);

    memset(pRootmsg->tNode.u8Header, 0xff, MWIFI_ADDR_LEN);
    pRootmsg->tNode.u8Type = BUS_TYPE_RESPONSE;

    if (true == g_ota_read_task_flag) {
        // firmware is ota running, return server message
        pRootmsg->tNode.u8Cmd = BUS_CMD_OTA;
        pRootmsg->tNode.tRespOta.u8OtaStatus = BUS_OTA_UPDATING;
        send(g_sockfd, pRootmsg, ROOT_PACKET_LEN, 0);
    } else {
        // get dest addr num and copy to dest_addr
        uint8_t num = ((pServerCmd->tDevOta.u8Number > 10) ? 10 : pServerCmd->tDevOta.u8Number);
        memcpy(dest_data, (uint8_t *)&pServerCmd->tDevOta.u8Number, (num * MWIFI_ADDR_LEN) + 1);
        MDF_LOGI("ota dev:%02x:%02x:%02x:%02x:%02x:%02x", 
                dest_data[1], dest_data[2], dest_data[3], dest_data[4], dest_data[5], dest_data[6]);
        g_ota_read_task_flag = true;
#if 1
        if(!g_root_ota_read_task_handle) {
            xTaskCreate(root_ota_read_task, "root_ota_read_task", 4 * 1024, NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, &g_root_ota_read_task_handle);
        }    
        xTaskCreate(ota_task, "ota_task", 4 * 1024, dest_data, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
#endif
    }

    //dest_data在任务中使用，此处不释放

    return 0;
}

static int root_do_restart(root_msg_t *pRootMessage, server_cmd_t * pServerCmd) 
{
    bool restart_self_flag = false;
    int send_data_error_num = 0; 
    int ret;

    MDF_LOGI("root do restart");
    root_msg_t * pRootmsg = pRootMessage;
    if (NULL == pRootmsg || NULL == pServerCmd) {
        MDF_LOGE("root_do_restart failed");
        return -1;
    }
    node_msg_t * resp = (node_msg_t *)pRootmsg->tNode.u8Header;
    uint8_t dest_addr[MWIFI_ADDR_LEN] = MWIFI_ADDR_BROADCAST;
    mwifi_data_type_t data_type = {0x0};

    memset(pRootmsg->tNode.u8Header, 0xff, sizeof(pRootmsg->tNode.u8Header));
    pRootmsg->tNode.u8Type = BUS_TYPE_REQUEST;
    pRootmsg->tNode.u8Cmd = BUS_CMD_RESTART;

    uint8_t num = pServerCmd->tDevRestart.u8Number;
    if (num == 0) { 
        MDF_LOGI("==== start All restart ====");
        data_type.communicate = MWIFI_COMMUNICATE_MULTICAST;
        mwifi_root_write(dest_addr, sizeof(dest_addr) / MWIFI_ADDR_LEN, &data_type, resp, CHILD_PACKET_LEN, true);
        MDF_LOGW("The device will restart after 3 seconds");
        system_reboot();
    } else if (num > 0) { 
        MDF_LOGI("==== start %d num restart ====", num);
        for (int i = 0; i < num; i++) {
            MDF_LOGI("===>restart dev: %02x:%02x:%02x:%02x:%02x:%02x", 
                    pServerCmd->tDevRestart.tDevAddr[i][0], pServerCmd->tDevRestart.tDevAddr[i][1], pServerCmd->tDevRestart.tDevAddr[i][2],
                    pServerCmd->tDevRestart.tDevAddr[i][3], pServerCmd->tDevRestart.tDevAddr[i][4], pServerCmd->tDevRestart.tDevAddr[i][5]);
            if (0 == memcmp(g_self_mac, pServerCmd->tDevRestart.tDevAddr[i], MWIFI_ADDR_LEN)) {
                MDF_LOGI("Start num restart but include myself");
                restart_self_flag = true;
            } else {
                memcpy(dest_addr, pServerCmd->tDevRestart.tDevAddr[i], MWIFI_ADDR_LEN);
                data_type.communicate = MWIFI_COMMUNICATE_UNICAST;
                //xSemaphoreTake(Lock, portMAX_DELAY);
                ret = mwifi_write(dest_addr, &data_type, resp, CHILD_PACKET_LEN, true);
                //xSemaphoreGive(Lock);
                if (ret != MDF_OK) {
                    MDF_LOGE("<%s> mwifi_node_write", mdf_err_to_name(ret));
                    send_data_error_num++;
                    MDF_LOGE("send restart error_num, the ret is: %d", ret);
                    memcpy(pRootmsg->tNode.tRespRestart.u8DevAddr[send_data_error_num - 1], dest_addr, MWIFI_ADDR_LEN);
                }
            }
            // add the delay in case the loop to fast to call mwifi_write, will make the root restart
            // vTaskDelay(pdMS_TO_TICKS(1000));        
        }

        if (send_data_error_num >= 1 && g_sockfd != -1) {
            pRootmsg->tNode.u8Type = BUS_TYPE_RESPONSE;
            pRootmsg->tNode.tRespRestart.u8Response = BUS_RESTART_ERROR;
            pRootmsg->tNode.tRespRestart.u8Number = send_data_error_num;
            send(g_sockfd, pRootmsg, ROOT_PACKET_LEN, 0);
        }

        if (true == restart_self_flag) {
            MDF_LOGW("The device will restart after 3 seconds");
            system_reboot();
        }
    }

    return 0;
}

static int do_process_data_from_tcpser_or_root(root_msg_t * pRootMsg)
{
    light_color_t color;
    work_mode_t mode;
    server_cmd_t * pServerCmd = (server_cmd_t *)pRootMsg->tNode.u8Header;
    uint8_t type = pServerCmd->u8Type;
    uint8_t cmd = pServerCmd->u8Cmd;

    //MDF_LOGI("ROOT-ROOT : %d, %d", type, cmd);
    //解析服务器发送来的命令
    if (BUS_TYPE_REQUEST == type) {
        switch (cmd) {
            case BUS_CMD_OTA:
                // get OTA firmware name and splice ota url
                MDF_LOGI("==== start ota ====");
                root_do_ota(pRootMsg, pServerCmd);
                break;
            case BUS_CMD_RESTART:
                root_do_restart(pRootMsg, pServerCmd);
                break;
            case BUS_CMD_PIC:
                root_send_photo(pRootMsg);
                break;
            case BUS_CMD_LED:
                //MDF_LOGI("root set led");
                color = pServerCmd->tDevLight.u8LedColor;
                set_light_color(color);
                if (-1 != g_sockfd) {
                    pRootMsg->tNode.u8Cmd = BUS_CMD_LED;
                    pRootMsg->tNode.tRespLight.u8Resp = color;
                    if ( send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0) < 0) {
                        MDF_LOGE("Error occured during sending: errno %d", errno);
                    }
                }
                break;
            case BUS_CMD_PING:
                //MDF_LOGI("root do ping");
                if (-1 != g_sockfd) {
                    pRootMsg->tNode.u8Cmd = BUS_CMD_PING;
                    pRootMsg->tNode.tRespLight.u8Resp = get_light_color();
                    MDF_LOGI("led color = %d", get_light_color());
                    if ( send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0) < 0) {
                        MDF_LOGE("Error occured during sending: errno %d", errno);
                    }
                }
                break;
            case BUS_CMD_SWITCH:
                // Open renamed file for reading
                //MDF_LOGI("root do switch mode");
                mode = pRootMsg->tNode.tRespSetmode.u8Mode;
                set_work_mode(mode);
                if (-1 != g_sockfd) {
                    pRootMsg->tNode.u8Cmd = BUS_CMD_SWITCH;
                    pRootMsg->tNode.tRespSetmode.u8Mode = mode;
                    if ( send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0) < 0) {
                        MDF_LOGE("Error occured during sending: errno %d", errno);
                    }
                }
                break;
            default:
                MDF_LOGE("cmd = %d", cmd);
                break;
        }
    } else if (BUS_TYPE_RESPONSE == type) {
        MDF_LOGW("BUS_TYPE_RESPONSE");
    } else {
        MDF_LOGE("type = %d", type);
    }

    return 0;
}

static int root_do_process_register(uint8_t *which, root_msg_t * pRootMsg) 
{
    int ret;
    node_msg_t * pNodeMsg = (node_msg_t *)pRootMsg->tNode.u8Header;

    memset(pNodeMsg->u8Header, 0xff, PACKET_HEADER_LEN);
    pNodeMsg->u8Type = BUS_TYPE_REQUEST;
    pNodeMsg->u8Cmd = BUS_CMD_NODE_REGISTER;

    add_newnode_to_nodedev_list(which, pNodeMsg->tDevVersion.u8VersionStr);
    // transparent node data to tcp server
    if (-1 != g_sockfd) {
        memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
        ret = send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0);
        if (ret < 0) {
            MDF_LOGE("Error occured during sending: errno %d", errno);
        }
    }

    return 0;
}

static int do_process_data_from_node(uint8_t * which, root_msg_t * pRootMsg) 
{
    node_msg_t *pNodeMsg = (node_msg_t *)pRootMsg->tNode.u8Header;
    uint8_t type = pNodeMsg->u8Type;
    uint8_t cmd = pNodeMsg->u8Cmd;
    int ret;

    if (BUS_TYPE_REQUEST == type) {
        MDF_LOGW("BUS_TYPE_REQUEST");
        if (BUS_CMD_NODE_REGISTER == cmd) {
            root_do_process_register(which, pRootMsg); 
        }
    } else if (BUS_TYPE_RESPONSE == type) {
        switch (cmd) {
            case BUS_CMD_RESTART:
                // todo
                break;
            case BUS_CMD_LED:
            case BUS_CMD_PING:
            case BUS_CMD_SWITCH:
                // transparent node data to tcp server
                if (-1 != g_sockfd) {
                    memcpy(pRootMsg->u8Addr, which, MWIFI_ADDR_LEN);
                    ret = send(g_sockfd, pRootMsg, ROOT_PACKET_LEN, 0);
                    if (ret < 0) {
                        MDF_LOGE("Error occured during sending: errno %d", errno);
                    }
                }
                break;
            default:
                MDF_LOGE("cmd = %d", cmd);
                break;
        }
    } else {
        MDF_LOGE("type = %d", type);
    }

    return 0;
}

static int do_process_image_data_from_root_or_node(uint8_t *src_addr, root_msg_t **pRMsg, server_cmd_t **pSCmd)
{
    static int err_count = 0;
    const int MAX_ERR_COUNT = 10;
    uint16_t count = 0;
    uint16_t max_count = 0;
    static bool bStartGetImage = false;
    root_msg_t *pRootMsg = *pRMsg;

    // node -> root pic
    node_pic_t * pNodePic = (node_pic_t *)*pSCmd;
    max_count = pNodePic->u16MaxCount;
    //MDF_LOGI("max count = %d", max_count);
    //MDF_LOGI("pNodePic = %p", pNodePic);
    if (max_count > PHOTO_PACKET_MAX_COUNT) {
        // too large pic, drop
        err_count++;
        if(err_count > MAX_ERR_COUNT) {
            err_count = 0;
            bStartGetImage = false;
            pRootMsg = (root_msg_t *)g_root_mesh_read_data;
            pNodePic = (node_pic_t *)(pRootMsg->tNode.u8Header);
        }
        //continue;
        goto EXIT;
    }
#if 1
    count = pNodePic->u16Count;
    if (1 == count) {
        // new photo,
        // last photo has not been sent, drop the last photo
        //MDF_LOGI("start recv image");
        bStartGetImage = true;
        pRootMsg = (root_msg_t *)g_root_mesh_read_data;
        memcpy(pRootMsg->u8Addr, src_addr, MWIFI_ADDR_LEN);
        err_count = 0;
    }
    //MDF_LOGI("pic:count=%d maxcount=%d", count, max_count);

    if (bStartGetImage == false) {
        //MDF_LOGI("bStartGetImage = false");
        goto EXIT;
    }

    if (count == max_count) {
        // last pic packet
        if (-1 != g_sockfd) {
            if (send(g_sockfd, g_root_mesh_read_data, ROOT_PACKET_LEN * count, 0) < 0) {
                MDF_LOGE("Error occured during sending: errno %d", errno);
            }
            MDF_LOGD("root transport send: max_count: %u", max_count);
        }
        pRootMsg = (root_msg_t *)g_root_mesh_read_data;
        pNodePic = (node_pic_t *)(pRootMsg->tNode.u8Header);
        goto EXIT;
    } else {
        if (memcmp(g_root_mesh_read_data, src_addr, PACKET_HEADER_LEN) == 0) {
            //判断是不是该自该地址的数据
            err_count = 0;
            pRootMsg += 1;
            pNodePic = (node_pic_t *)(pRootMsg->tNode.u8Header);
        } else {
            err_count++;
            if (err_count > MAX_ERR_COUNT) {
                bStartGetImage = false;
                pRootMsg = (root_msg_t *)g_root_mesh_read_data;
                pNodePic= (node_pic_t *)(pRootMsg->tNode.u8Header);
                err_count = 0;
            } 
        }
    }
#endif
EXIT:
    *pSCmd = (server_cmd_t *)pNodePic;
    *pRMsg = pRootMsg;

    return 0;
}

//root send ping to node
static void root_send_ping(QueueHandle_t queue, node_msg_t * pNodeBuffer)
{
    if (pNodeBuffer == NULL)
        return;

    static uint32_t u32LastTimeStamp = 0;
    mdf_err_t ret = MDF_OK;
    mwifi_data_type_t data_type_write = {0};;
    uint8_t u8Target[MWIFI_ADDR_LEN] = MWIFI_ADDR_BROADCAST;
    uint32_t u32CurTimeStamp = xTaskGetTickCount();
    queue_msg_t qmsg;

    //判断有无子节点，如果没有就不发送
    if ( esp_mesh_get_total_node_num() == 1)
        return;
 
    //如果没有收到新加节点消息，并且与上次发送时间间隔小于ROOT_SEND_PING_INTERVAL，则返回。
    if (xQueueReceive(queue, &qmsg, 0) != pdTRUE) {
        if (get_two_timestamp_interval(u32CurTimeStamp, u32LastTimeStamp) < ROOT_SEND_PING_INTERVAL) {
            return;
        }
    }
#if 0
    else {
        switch(pQMsg->u8cmd) {
            case QUEUE_NODE_ADD:
                //如果发现有新节点加入，则立即发送一次广播ping包，并更新路由表
                //该消息只有新加入的节点需要发送响应信息
            case QUEUE_NODE_PING:
                //root的广播ping包，根据时间戳保持固定间隔。
                ;;
                node_info_t * head = get_nodedev_list_head();
                for (int i = 0; head != NULL; head = head->next, i++) {
                    memcpy(pNodeBuffer->tRootPing.u8NodeMac[i], head->mac_addr, sizeof(head->mac_addr));
                }
                break;
            default:
                break;
        }
    }
#endif
    node_info_t * head = get_nodedev_list_head();
    for (int i = 0; head != NULL; head = head->next, i++) {
        memcpy(pNodeBuffer->tRootPing.u8NodeMac[i], head->mac_addr, sizeof(head->mac_addr));
    }

    memset(pNodeBuffer->u8Header, 0xff, PACKET_HEADER_LEN);
    pNodeBuffer->u8Type = BUS_TYPE_REQUEST;
    pNodeBuffer->u8Cmd  = BUS_CMD_ROOT_SEND_PING;
    pNodeBuffer->tRootPing.u8Force = qmsg.u8cmd;
    memcpy(pNodeBuffer->tRootPing.u8RootMac, g_self_mac, MWIFI_ADDR_LEN);

    data_type_write.communicate = MWIFI_COMMUNICATE_BROADCAST;
    
    u32LastTimeStamp = u32CurTimeStamp;
    ret = mwifi_write(u8Target, &data_type_write, pNodeBuffer, CHILD_PACKET_LEN, true);
    if (MDF_OK != ret) {
        MDF_LOGE("<%s> mwifi_node_write", mdf_err_to_name(ret));
    }
}

/*
 * read from node or root itself
 */
static void root_mesh_read_task(void *arg)
{
    mdf_err_t ret                   = MDF_OK;
    root_msg_t * pRootMsg           = (root_msg_t *)g_root_mesh_read_data; 
    server_cmd_t * pServerCmd       = (server_cmd_t *)(pRootMsg->tNode.u8Header);
    size_t size                     = CHILD_PACKET_LEN;
    uint8_t src_addr[MWIFI_ADDR_LEN]= {0x0}; // mesh network sender
    mwifi_data_type_t data_type     = {0x0};
    uint8_t u8NormalHeader[PACKET_HEADER_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    queue_msg_t qmsg;
    node_msg_t * pPingMsg = (node_msg_t *)pRootMsg->tNode.u8Header;
    serverqueue_msg_t sqmsg;
    MDF_LOGI("ROOT mesh read task is running on core %d", xPortGetCoreID());

    while (mwifi_is_connected()) {

        if (esp_mesh_get_layer() != MESH_ROOT)
            break;

        //send it every 8 seconds,this will feed the dog on child node
        root_send_ping(MeshQueue, pPingMsg);

        if (bDeviceOtaFlag == true) {
            vTaskDelay(500);
            continue;
        }

        if (g_sockfd == -1) {
            g_sockfd = socket_tcp_client_create(ConfigServerIp, ConfigServerPort);
            if (g_sockfd == -1) {
                vTaskDelay(5000 / portTICK_RATE_MS);
                continue;
            }
            if (root_send_register(g_sockfd, pRootMsg) < 0) {
                MDF_LOGD("root_send_registea failed");
                vTaskDelay(5000 / portTICK_RATE_MS);
                continue;
            }
        }

        if (xQueueReceive(tcpQueue, &qmsg, 0) == pdTRUE) {
            root_do_child_offline(g_sockfd, &qmsg, pRootMsg); 
        }

        if (xQueueReceive(ServerQueue, &sqmsg, 0) == pdTRUE) {
            if (true == sqmsg.bAddrValid) {
                //tcp send to node message
                memcpy(pServerCmd, &sqmsg.tServerCmd, sizeof(server_cmd_t));
                if (mwifi_write(sqmsg.u8addr, &data_type, pServerCmd, sizeof(node_msg_t), true) != MDF_OK) {
                    MDF_LOGE("mwifi_write err:%s", mdf_err_to_name(ret));
                }
            } else {
                //tcp send to root message
                memcpy(pRootMsg->u8Addr, g_self_mac, MWIFI_ADDR_LEN);
                memcpy(pServerCmd, &sqmsg.tServerCmd, sizeof(server_cmd_t));
                do_process_data_from_tcpser_or_root(pRootMsg);
            }
        }

        //非阻塞读取mwifi
        size = CHILD_PACKET_LEN;
        memset(&data_type, 0, sizeof(mwifi_data_type_t));
        ret = mwifi_root_read(src_addr, &data_type, (uint8_t *)pServerCmd, &size, 200);
        if (MDF_OK != ret) {
            //MDF_LOGI("<%s> mwifi_root_read", mdf_err_to_name(ret));
            continue;
        }

        // ota root read process
        if (data_type.upgrade) { // This mesh package contains upgrade data.
            MDF_LOGI("root task: update firmware: src[%02x:%02x:%02x:%02x:%02x:%02x]",
                    src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
            ret = mupgrade_root_handle(src_addr, (uint8_t *)pServerCmd, size);
            if (MDF_OK != ret) {
                MDF_LOGE("===>mupgrade_root_handle target err:%s", mdf_err_to_name(ret));
            }
            continue;
        }

        // other business is here
        /*
         * root -> root non-pic
         * 0xFFFFFFFFFFFF(6) + CMD_REQUEST(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
         *
         * node -> root pic
         * count(2) + maxCount(2) + dataLen(2) + data ---- total : MWIFI_ADDR_LEN
         *
         * node -> root non-pic
         * 0xFFFFFFFFFFFF(6) + CMD_RESPONSE(1) + cmd(1) + payload(?) ---- total : MWIFI_ADDR_LEN
         */
        if (0 == memcmp(pServerCmd->u8Header, u8NormalHeader, PACKET_HEADER_LEN)) {
            memcpy(pRootMsg->u8Addr, src_addr, MWIFI_ADDR_LEN);
            // node -> root non-pic or root -> root non-pic
            if (0 == memcmp(src_addr, g_self_mac, MWIFI_ADDR_LEN)) {
                // root to root from tcp server
                do_process_data_from_tcpser_or_root(pRootMsg);
            } else {
                // node to root non-pic
                do_process_data_from_node(src_addr, pRootMsg);
            }
        } else {
            do_process_image_data_from_root_or_node(src_addr, &pRootMsg, &pServerCmd);
        }
    }
    
    MDF_LOGW("ROOT mesh read task is exit");
    // need restart
    g_root_mesh_read_task_handle = NULL;
    vTaskDelete(NULL);
}
#if 0
static void my_test_task(void *arg)
{
    //uint8_t u8TaskCount = 0;
    char pWriteBuffer[2048];
    node_info_t * head = NULL;
    while (1)
    {
        //u8TaskCount = uxTaskGetNumberOfTasks();
        vTaskList((char *)&pWriteBuffer);
        //MDF_LOGI("===>my_test_task get u8TaskCount=%d", u8TaskCount);
        MDF_LOGI("\n%s", pWriteBuffer);
        MDF_LOGI("====self mac[%02x:%02x:%02x:%02x:%02x:%02x]====", 
                g_self_mac[0],g_self_mac[1], g_self_mac[2], g_self_mac[3], g_self_mac[4], g_self_mac[5]);
        head = get_nodedev_list_head();
        MDF_LOGI("============dev list================");
        for (int i = 0; NULL != head; head = head->next, i++) {
            MDF_LOGI("dev[%d]:%02x:%02x:%02x:%02x:%02x:%02x", i,
                    head->mac_addr[0],head->mac_addr[1],head->mac_addr[2],
                    head->mac_addr[3],head->mac_addr[4],head->mac_addr[5]);
        }
        MDF_LOGI("====================================");
        sleep(5);
    }

    vTaskDelete(NULL);
}
#endif
#if 1
//root updatefirmware thread task
static void root_ota_read_task(void *arg) {
    mdf_err_t ret = MDF_OK;
    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
    size_t size   = MWIFI_PAYLOAD_LEN;
    mwifi_data_type_t data_type      = {0x0};
    uint8_t src_addr[MWIFI_ADDR_LEN] = {0};

    MDF_LOGI("root_ota_read_task is running");

    while (mwifi_is_connected() && esp_mesh_get_layer() == MESH_ROOT) {
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        size = MWIFI_PAYLOAD_LEN;
        ret = mwifi_read(src_addr, &data_type, data, &size, portMAX_DELAY);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_recv", mdf_err_to_name(ret));

        MDF_LOGI("root_ota_read_task:src addr[%02x:%02x:%02x:%02x:%02x:%02x]",
                    src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5]);
        if (data_type.upgrade) { // This mesh package contains upgrade data.
            ret = mupgrade_handle(src_addr, data, size);
            MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mupgrade_handle", mdf_err_to_name(ret));
        }
    }

    MDF_LOGW("root_ota_read_task task is exit!");

    MDF_FREE(data);
    g_root_ota_read_task_handle = NULL;
    vTaskDelete(NULL);
}
#endif
static mdf_err_t wifi_init()
{
    mdf_err_t ret          = nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        MDF_ERROR_ASSERT(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    MDF_ERROR_ASSERT(ret);

    tcpip_adapter_init();
    MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);
    queue_msg_t qmsg;

    switch (event) {
        case MDF_EVENT_MWIFI_STARTED:   //event 0
            MDF_LOGI("MESH is started");
            break;

        case MDF_EVENT_MWIFI_PARENT_CONNECTED:  //event 7
            MDF_LOGI("Parent is connected on station interface");
            if (esp_mesh_get_layer() != MESH_ROOT) {
                //if (!g_node_task_handle) {
                if (false == getNodeTaskRunStatus()) {
                    xTaskCreatePinnedToCore(node_task, "node_task", 4 * 1024,
                        NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY + 1, NULL, 1);
                }
            }
            break;

        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:   //event 8
            MDF_LOGI("Parent is disconnected on station interface");
            break;

        case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD: //event 5
            MDF_LOGI("MDF_EVENT_MWIFI_ROUTING_TABLE_ADD, total_num: %d", esp_mesh_get_total_node_num());
            qmsg.u8cmd = QUEUE_NODE_ADD;
            if (MeshQueue)
                xQueueSend(MeshQueue, &qmsg, 0);
            break;

        case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE: //event 6
            MDF_LOGI("MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE, total_num: %d", esp_mesh_get_total_node_num());
            qmsg.u8cmd = QUEUE_NODE_REMOVE;
            if (tcpQueue)
                xQueueSend(tcpQueue, &qmsg, 0);
            //if (esp_mesh_is_root()) {
            //    node_left_notify_server();
            //}
            break;

        case MDF_EVENT_MWIFI_ROOT_GOT_IP: { //event 17
            MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
            if (esp_mesh_get_layer() == MESH_ROOT) {
                if (!g_tcp_client_read_task_handle) {
                    xTaskCreatePinnedToCore(tcp_client_read_task, "tcp_client_read_task", 4 * 1024,
                            NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY + 1, &g_tcp_client_read_task_handle, 1);
                }
                if (!g_root_mesh_read_task_handle) {
                    xTaskCreatePinnedToCore(root_mesh_read_task, "root_mesh_read_task", 4 * 1024,
                            NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY + 1, &g_root_mesh_read_task_handle, 1);
                }
            }

            break;
        }
        case MDF_EVENT_MUPGRADE_STARTED: {
            mupgrade_status_t status = {0x0};
            mupgrade_get_status(&status);
            MDF_LOGI("MDF_EVENT_MUPGRADE_STARTED, name: %s, size: %d", status.name, status.total_size);
            break;
        }
        case MDF_EVENT_MUPGRADE_STATUS:
            MDF_LOGI("Upgrade progress: %d%%", (int)ctx);
            if((int)ctx >= 100) {
                MDF_LOGE("system update firmware complete! and i will restart!");
                vTaskDelay(2000 / portTICK_RATE_MS);
                feed_dog();
                feed_dog();
                feed_dog();
            }
            break;

        default:
            break;
    }

    return MDF_OK;
}


/**
 * @brief Timed printing system information
 */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        ESP_LOGI(TAG,"retry to connect to the AP");
        ESP_LOGI(TAG,"connect to the AP fail");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void app_main()
{
    watchdog_setup();
    system_config_init(); 
    system_config_t system_config = {0};
    get_system_config(&system_config);

    if(!strcmp(system_config.run_mode, "1")) {
#if 0
        g_timer = xTimerCreate("watchdog", 10000 / portTICK_RATE_MS,
                                           true, NULL, feed_dog);
        xTimerStart(g_timer, 0);
        feed_dog(g_timer);
#endif
        camera_setup();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        tcpip_adapter_init();
        MDF_ERROR_ASSERT(esp_event_loop_init(event_handler, NULL));
        MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
        MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        snprintf((char*)wifi_config.sta.ssid, 32, "%s", ANDROID_HOTSPOT_SSID);
        snprintf((char*)wifi_config.sta.password, 64, "%s", ANDROID_HOTSPOT_PASSWORD);

        MDF_ERROR_ASSERT(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

        MDF_LOGI("wifi_init_sta finished.");
        MDF_ERROR_ASSERT(esp_wifi_start());
        app_httpd_main();
    } else {
        MDF_LOGI("app_main() running on core %d", xPortGetCoreID());
        MDF_LOGI("================================");
        MDF_LOGI("system.router_ssid = %s", system_config.router_ssid);
        MDF_LOGI("system.mesh_id = %s", system_config.mesh_id);
        MDF_LOGI("system.server_ip = %s", system_config.server_ip);
        MDF_LOGI("system.backoff_rssi = %s", system_config.backoff_rssi);
        MDF_LOGI("system.run_mode = %s", system_config.run_mode);
        MDF_LOGI("system.server_port = %s", system_config.server_port);
        MDF_LOGI("system.max_layer = %s", system_config.max_layer);
        MDF_LOGI("---------------------------------");
#if 0
        LockOfWtd = xSemaphoreCreateMutex();
        if(!LockOfWtd) {
            return;
        }
        MDF_LOGI("semaphore[LockOfWtd] create success!");
        vSemaphoreCreateBinary(LockOfWtd);
        if (LockOfWtd == NULL) {
            MDF_LOGI("semaphore binary create failed");
            return;
        }
#endif

        node_task_buffer_init();
        //g_tcp_client_read_data  = (uint8_t *)MDF_MALLOC(TCP_CLIENT_RECV_LEN);
        g_tcp_client_read_data  = (uint8_t *)MDF_MALLOC(ROOT_PACKET_LEN);
        //g_node_mesh_read_data   = (uint8_t *)MDF_MALLOC(CHILD_PACKET_LEN);
        g_root_mesh_read_data   = (uint8_t *)MDF_MALLOC(PHOTO_PACKET_MAX_COUNT * ROOT_PACKET_LEN);
        //g_root_mesh_write_data  = (uint8_t *)MDF_MALLOC(CHILD_PACKET_LEN);
        //g_node_info_array       = (node_info_t *)MDF_MALLOC(sizeof(node_info_t) * MAX_MESH_SIZE);
        //memset(g_node_info_array, 0, sizeof(node_info_t) * MAX_MESH_SIZE);
#if 0
        g_timer = xTimerCreate("watchdog", 10000 / portTICK_RATE_MS,
                                           true, NULL, feed_dog);
        xTimerStart(g_timer, 0);
        feed_dog(g_timer);
#endif
        camera_setup();
        light_setup();
        
        tcpQueue = xQueueCreate(3, sizeof(queue_msg_t));
        MeshQueue = xQueueCreate(3, sizeof(queue_msg_t));
        ServerQueue = xQueueCreate(3, sizeof(server_cmd_t));

        mwifi_init_config_t cfg   = MWIFI_INIT_CONFIG_DEFAULT();
        mwifi_config_t mconfig;
        memset(&mconfig, 0, sizeof(mwifi_config_t));
        /**
         * @brief Set the log level for serial port printing.
         */
        //mconfig 初始化,根据配置文件来填充
        //sizeof -1 是为了保证数组结尾处是字符串结束符0
        memset(ConfigServerIp, 0, sizeof(ConfigServerIp));
#if 1
        memcpy(mconfig.router_ssid, system_config.router_ssid, strlen(system_config.router_ssid));
        memcpy(mconfig.router_password, CONFIG_ROUTER_PASSWORD, strlen(CONFIG_ROUTER_PASSWORD));
        memcpy(ConfigServerIp, system_config.server_ip, strlen(system_config.server_ip));
        //memcpy(ConfigServerIp, "192.168.2.1", strlen("192.168.2.6"));
#else
        memcpy(mconfig.router_ssid, "HiWiFi_1F4514", strlen("HiWiFi_1F4514"));
        memcpy(mconfig.router_password, "19870209duan", strlen("19870209duan"));
        memcpy(ConfigServerIp, "192.168.199.119", strlen("192.168.199.119"));
#endif
        memcpy(mconfig.mesh_id, system_config.mesh_id, strlen(system_config.mesh_id));
        memcpy(mconfig.mesh_password, CONFIG_MESH_PASSWORD, strlen(CONFIG_MESH_PASSWORD));
        ConfigServerPort = atoi(system_config.server_port);
        
        esp_log_level_set("*", ESP_LOG_INFO);
        esp_log_level_set(TAG, ESP_LOG_DEBUG);
        /**
         * @brief Initialize wifi mesh.
         */

        MDF_LOGI("connect to ap SSID:%s password:%s mesh_id:%s mesh_password=%s",
             mconfig.router_ssid, mconfig.router_password, mconfig.mesh_id, mconfig.mesh_password);

        MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
        MDF_ERROR_ASSERT(wifi_init());
        MDF_ERROR_ASSERT(mwifi_init(&cfg));
        MDF_ERROR_ASSERT(mwifi_set_config(&mconfig));
        MDF_ERROR_ASSERT(mwifi_start());

        esp_wifi_get_mac(ESP_IF_WIFI_STA, g_self_mac);
        MDF_LOGI("self mac: " MACSTR , MAC2STR(g_self_mac));

        char hostname[9] = {0};
        sprintf(hostname, "%s%02x%02x", "esp_", g_self_mac[4], g_self_mac[5]);
        ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));
        //xTaskCreate(my_test_task, "mytesttak", 4*1024, NULL, 10, NULL);
    }
}
