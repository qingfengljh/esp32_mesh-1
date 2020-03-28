/* SPIFFS filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "app_spiffs.h"

#define BUFFER_SIZE 128

static const char *TAG = "app_spiffs";
static const char* config_items[] = {"router_ssid", "mesh_id", "server_ip", "server_port", "run_mode", "max_layer", "backoff_rssi"};
static const char* config_values[] = {"Frog", "FrogT12", "192.168.200.34", "8888", "2", "3", "-78"};

int system_config_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return -1;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return -2;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/spiffs/config.txt", &st) != 0) {
        // Create it if it not exists
        ESP_LOGI(TAG, "Create /spiffs/config.txt");

        // Use POSIX and C standard library functions to work with files.
        // First create a file.
        ESP_LOGI(TAG, "Opening file");
        FILE* f = fopen("/spiffs/config.txt", "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return -3;
        }
        size_t size = sizeof(config_items) / sizeof(char *);
        for (int i = 0; i < size; ++i) {
            fprintf(f, "%s=%s\n", config_items[i], config_values[i]);
        }
        fclose(f);
        ESP_LOGI(TAG, "File written");
    }

    ESP_LOGI(TAG, "/spiffs/config.txt already exist");
    return 0;
}
#if 0
int get_system_config(system_config_t * pConfig)
{
    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE * pfile = fopen("/spiffs/config.txt", "r");
    if (pfile == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -1;
    }
    char line[BUFFER_SIZE];

    size_t size = sizeof(config_items) / sizeof(char *);
    char *p;
    for (int i = 0; i < size; ++i) {
        memset(line, 0, sizeof(line));
        fgets(line, sizeof(line), pfile);
        // strip newline
        p = strchr(line, '\n');
        if (p) {
            *p = '\0';
        }
        ESP_LOGI(TAG, "Read from file: '%s'", line);
        p = strchr(line, '=');
        if (!p) {
            ESP_LOGE(TAG, "no =");
            fclose(pfile);
            return -2;
        }
        //router_ssid 最多32字节
        //router_password 64
        //router_bssid 6
        //mesh_id 6
        //mesh_password 64
        if (!strncmp(line, config_items[i], strlen(config_items[i]))) {
            switch (i) {
                case 0:
                    strncpy(pConfig->router_ssid, p + 1, sizeof(pConfig->router_ssid)); 
                    break;
                case 1:
                    strncpy(pConfig->mesh_id, p + 1, sizeof(pConfig->mesh_id)); 
                    break;
                case 2:
                    strncpy(pConfig->server_ip, p + 1, sizeof(pConfig->server_ip)); 
                    break;
                case 3:
                    strncpy(pConfig->server_port, p + 1, sizeof(pConfig->server_port)); 
                    break;
                case 4:
                    strncpy(pConfig->run_mode, p + 1, sizeof(pConfig->run_mode)); 
                    break;
                case 5:
                    strncpy(pConfig->max_layer, p + 1, sizeof(pConfig->max_layer)); 
                    break;
                case 6:
                    strncpy(pConfig->backoff_rssi, p + 1, sizeof(pConfig->backoff_rssi)); 
                    break;
                default:
                    ESP_LOGE(TAG, "code missing");
                    fclose(pfile);
                    return -3;
            }
        }
        else {
            ESP_LOGE(TAG, "not %s", config_items[i]);
            fclose(pfile);
            return -4;
        }
    }
    fclose(pfile);

    // All done, unmount partition and disable SPIFFS
    //esp_vfs_spiffs_unregister(NULL);
    //ESP_LOGI(TAG, "SPIFFS unmounted");
    return 0;
}
#endif

int get_system_config(system_config_t * pConfig) 
{
    // Open renamed file for reading
    ESP_LOGI(TAG, "Reading file");
    FILE * pfile = fopen("/spiffs/config.txt", "r");
    if (pfile == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return -1;
    }
    char line[BUFFER_SIZE];

    size_t size = sizeof(config_items) / sizeof(char *);
    char *p;
    ESP_LOGI(TAG, "===> config file size = %d", size);

    for (int i = 0; i < size; ++i) {
        memset(line, 0, sizeof(line));
        fgets(line, sizeof(line), pfile);
        // strip newline
        p = strchr(line, '\n');
        if (p) {
            *p = '\0';
        }
        ESP_LOGI(TAG, "Read from file: '%s'", line);
        p = strchr(line, '=');
        if (!p) {
            ESP_LOGE(TAG, "no =");
            fclose(pfile);
            return -2;
        }
        //router_ssid 最多32字节
        //router_password 64
        //router_bssid 6
        //mesh_id 6
        //mesh_password 64
        int len;
        for (int j = 0; j < size; j++) {
            if (0 == strncmp(line, config_items[j], strlen(config_items[j]))) {
                switch(j) {
                    case 0: //router_ssid
                        len = strlen(p+1) > sizeof(pConfig->router_ssid) ? sizeof(pConfig->router_ssid) : strlen(p+1);
                        memcpy(pConfig->router_ssid, p + 1, len);
                        pConfig->router_ssid[sizeof(pConfig->router_ssid) - 1] = 0;
                        break;
                    case 1: //mesh_id
                        len = strlen(p+1) > sizeof(pConfig->mesh_id) ? sizeof(pConfig->mesh_id) : strlen(p+1);
                        memcpy(pConfig->mesh_id, p + 1, len);
                        pConfig->mesh_id[sizeof(pConfig->mesh_id) - 1] = 0;
                        break;
                    case 2: //server_ip
                        len = strlen(p+1) > sizeof(pConfig->server_ip) ? sizeof(pConfig->server_ip) : strlen(p+1);
                        memcpy(pConfig->server_ip, p + 1, len);
                        pConfig->server_ip[sizeof(pConfig->server_ip) - 1] = 0;
                        break;
                    case 3: //server_port
                        len = strlen(p+1) > sizeof(pConfig->server_port) ? sizeof(pConfig->server_port) : strlen(p+1);
                        memcpy(pConfig->server_port, p + 1, len);
                        pConfig->server_port[sizeof(pConfig->server_port) - 1] = 0;
                        break;
                    case 4: //run_mode
                        len = strlen(p+1) > sizeof(pConfig->run_mode) ? sizeof(pConfig->run_mode) : strlen(p+1);
                        memcpy(pConfig->run_mode, p + 1, len);
                        pConfig->run_mode[sizeof(pConfig->run_mode) - 1] = 0;
                        break;
                    case 5: //max_layer
                        len = strlen(p+1) > sizeof(pConfig->max_layer) ? sizeof(pConfig->max_layer) : strlen(p+1);
                        memcpy(pConfig->max_layer, p + 1, len);
                        pConfig->max_layer[sizeof(pConfig->max_layer) - 1] = 0;
                        break;
                    case 6: //backoff_rssi
                        len = strlen(p+1) > sizeof(pConfig->backoff_rssi) ? sizeof(pConfig->backoff_rssi) : strlen(p+1);
                        memcpy(pConfig->backoff_rssi, p + 1, len);
                        pConfig->backoff_rssi[sizeof(pConfig->backoff_rssi) - 1] = 0;
                        break;
                    default:
                        ESP_LOGE(TAG, "code missing");
                        fclose(pfile);
                        return -3;
                } 
            }
        }
    }
    fclose(pfile);

    // All done, unmount partition and disable SPIFFS
    return 0;
}

int update_system_config(const char *variable, const char *value)
{
    if (NULL == variable || NULL == value) {
        ESP_LOGE(TAG, "NULL param!");
        return -1;
    }

    /* File pointer to hold reference of input file */
    FILE * fPtr;
    FILE * fTemp;
    char buffer[BUFFER_SIZE];
    char newline[BUFFER_SIZE];

    /*  Open all required files */
    fPtr  = fopen("/spiffs/config.txt", "r");
    fTemp = fopen("/spiffs/replace.tmp", "w");

    /* fopen() return NULL if unable to open file in given mode. */
    if (fPtr == NULL || fTemp == NULL)
    {
        /* Unable to open file hence exit */
        ESP_LOGE(TAG, "\nUnable to open file.\n");
        ESP_LOGE(TAG, "Please check whether file exists and you have read/write privilege.\n");
        return -2;
    }

    snprintf(newline, BUFFER_SIZE, "%s=%s\n", variable, value);

    size_t size = sizeof(config_items) / sizeof(char *);
    ESP_LOGI(TAG, "size = %d", size);
    for (int i = 0; i < size; ++i) {

        fgets(buffer, BUFFER_SIZE, fPtr);

        /*
         * Read line from source file and write to destination
         * file after replacing given line.
         */
        /* If current line is line to replace */
        if (!strncmp(variable, buffer, strlen(variable))) {
            ESP_LOGI(TAG, "newline = %s", newline);
            fputs(newline, fTemp);
        }
        else {
            ESP_LOGI(TAG, "buffer = %s", buffer);
            fputs(buffer, fTemp);
        }
    }

    /* Close all files to release resource */
    fclose(fPtr);
    fclose(fTemp);


    // Check if destination file exists before renaming
    struct stat st;
    if (stat("/spiffs/config.txt", &st) == 0) {
        // Delete it if it exists
        ESP_LOGI(TAG, "Delete /spiffs/config.txt");
        unlink("/spiffs/config.txt");
    }

    // Rename original file
    ESP_LOGI(TAG, "Renaming file");
    if (rename("/spiffs/replace.tmp", "/spiffs/config.txt") != 0) {
        ESP_LOGE(TAG, "Rename failed");
        return -3;
    }
    return 0;
}
