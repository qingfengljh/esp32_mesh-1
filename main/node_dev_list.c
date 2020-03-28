#include "node_dev_list_t.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static node_info_t * g_node_info_ptr = NULL;

node_info_t * get_nodedev_list_head(void){
    return g_node_info_ptr;
}

int add_newnode_to_nodedev_list(uint8_t * pAddr, uint8_t * pVersion)
{
    node_info_t *cur, *pre;
    if (NULL == g_node_info_ptr) {
        g_node_info_ptr = (node_info_t *)malloc(sizeof(node_info_t));
        if (NULL == g_node_info_ptr)
            return ENOMEM;
        //the first node
        g_node_info_ptr->next = NULL;
        memcpy(g_node_info_ptr->mac_addr, pAddr, sizeof(g_node_info_ptr->mac_addr));
        if (NULL != pVersion)
            memcpy(g_node_info_ptr->version,  pVersion, sizeof(g_node_info_ptr->version));

        return 0;
    }

    cur = g_node_info_ptr;
    pre = g_node_info_ptr;
    
    //2,3,4.....node
    while(1) {
#if 0
        if (NULL == cur->next) {
            break;    
        } else {
            if (memcmp(cur->mac_addr, pAddr, sizeof(cur->mac_addr)) == 0) {
                if (NULL != pVersion)
                    memcpy(cur->version, pVersion, sizeof(cur->version));
                return 0;
            }
                
            cur= cur->next;
        }
#else
        if (NULL == cur)
            break;
        if (memcmp(cur->mac_addr, pAddr, sizeof(cur->mac_addr)) == 0) {
            if (NULL != pVersion) 
                memcpy(cur->version, pVersion, sizeof(cur->version));
            return 0;
        } else {
            pre = cur;
            cur = cur->next;
        }
#endif
    }

    node_info_t *new = (node_info_t *)malloc(sizeof(node_info_t));
    if (NULL == new) 
        return -1;
    memcpy(new->mac_addr, pAddr, sizeof(new->mac_addr));
    memcpy(new->version, pVersion, sizeof(new->version));
    new->next = NULL;

    pre->next = new;

    return 0;
}

void del_node_from_nodedev_list(uint8_t * pAddr)
{
    node_info_t * cur = g_node_info_ptr;
    node_info_t * pre = NULL;

    while(1) {
        
        if (NULL == cur)
            break;

        if (memcmp(cur->mac_addr, pAddr, sizeof(cur->mac_addr)) == 0) {
            //找到了这个节点
            if (NULL == pre) {
                //改节点是头节点
                g_node_info_ptr = cur->next;
            } else {
                pre->next = cur->next;
            }

            free(cur);
            break;
        }

        pre = cur;     
        cur = cur->next;
    }
}

bool check_node_is_exist(uint8_t * pnodeaddr)
{ 
    node_info_t * cur = g_node_info_ptr;

    while(1) {
        
        if (NULL == cur) 
            break;

        if (memcmp(cur->mac_addr, pnodeaddr, sizeof(cur->mac_addr)) == 0) {
            return true;
        }

        cur = cur->next;
    }

    return false;
}
