#ifndef __NODE_TASK_H__
#define __NODE_TASK_H__

#include "common.h"


void set_light_color(light_color_t color);
light_color_t get_light_color(void);
bool node_task_buffer_init(void);
void node_task(void *arg);
#if 0
bool isNodeGotPing(void);
void clearNodePingFlag(void);
#endif
bool getNodeTaskRunStatus(void);
#endif
