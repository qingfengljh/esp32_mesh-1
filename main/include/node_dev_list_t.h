#ifndef __NODE_DEV_LIST_T_H__
#define __NODE_DEV_LIST_T_H__

#include "mdf_common.h"
#include "common.h"

node_info_t * get_nodedev_list_head(void);
int add_newnode_to_nodedev_list(uint8_t * pAddr, uint8_t * pVersion);
void del_node_from_nodedev_list(uint8_t * pAddr);
bool check_node_is_exist(uint8_t * pnodeaddr);

#endif
